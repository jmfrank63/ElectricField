#include "render/png_export.hpp"

#include <SDL3/SDL.h>

#include <memory>

#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "render/imgui_painter.hpp"

namespace efrender {

namespace {
struct SurfaceDeleter {
  void operator()(SDL_Surface* s) const { SDL_DestroySurface(s); }
};
struct RendererDeleter {
  void operator()(SDL_Renderer* r) const { SDL_DestroyRenderer(r); }
};

// The export borrows the process-wide "current ImGui context" slot: whatever happens (including a throw from renderScene),
// the temporary context must be destroyed and the caller's (e.g. the GUI's) context made current again. Destruction order
// is the reverse of declaration, so BackendGuard (needs the temp context current) runs before CtxGuard.
struct CtxGuard {
  ImGuiContext* prev;
  ImGuiContext* ctx;
  CtxGuard(ImGuiContext* previous, ImGuiContext* created) : prev(previous), ctx(created) {}
  ~CtxGuard() {
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(prev);
  }
  CtxGuard(const CtxGuard&) = delete;
  CtxGuard& operator=(const CtxGuard&) = delete;
};
struct BackendGuard {  // only constructed after a successful ImGui_ImplSDLRenderer3_Init
  ~BackendGuard() { ImGui_ImplSDLRenderer3_Shutdown(); }
};
}  // namespace

SDL_Surface* renderSurface(const ef::FieldScene& scene, const ef::DrawStyle& style, const ef::View& view, std::string& err) {
  err.clear();
  std::unique_ptr<SDL_Surface, SurfaceDeleter> surface(SDL_CreateSurface(view.widthPx, view.heightPx, SDL_PIXELFORMAT_RGBA32));
  if (!surface) {
    err = std::string("SDL_CreateSurface failed: ") + SDL_GetError();
    return nullptr;
  }
  std::unique_ptr<SDL_Renderer, RendererDeleter> renderer(SDL_CreateSoftwareRenderer(surface.get()));
  if (!renderer) {
    err = std::string("SDL_CreateSoftwareRenderer failed: ") + SDL_GetError();
    return nullptr;
  }

  IMGUI_CHECKVERSION();
  // CONTEXT RULE (Phase 3): ImGui::CreateContext() makes the new context current ONLY when no context exists (imgui.cpp:4258-4259 "Restore
  // previous context if any, else keep new one"). Inside the running GUI it returns a context that is NOT current, so every call below
  // (GetIO, GetStyle, the backend Init, NewFrame, Shutdown) would hit the GUI's context: it trips IM_ASSERT(io.BackendRendererUserData
  // == nullptr) in ImGui_ImplSDLRenderer3_Init and, with asserts compiled out, corrupts the GUI's io, style and backend data.
  // Hence: create, guard (restores the caller's context on every exit path), THEN SetCurrentContext explicitly.
  ImGuiContext* const prev = ImGui::GetCurrentContext();
  ImGuiContext* const ctx = ImGui::CreateContext();
  if (!ctx) {
    err = "ImGui::CreateContext failed";
    return nullptr;
  }
  CtxGuard ctxGuard(prev, ctx);
  ImGui::SetCurrentContext(ctx);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // never write imgui.ini
  io.DisplaySize = ImVec2(static_cast<float>(view.widthPx), static_cast<float>(view.heightPx));
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::GetStyle().CurveTessellationTol = 0.3f;
  ImGui::GetStyle().AntiAliasedLinesUseTex = false;

  if (!ImGui_ImplSDLRenderer3_Init(renderer.get())) {
    err = "ImGui_ImplSDLRenderer3_Init failed";
    return nullptr;
  }
  BackendGuard backendGuard;
  for (int frame = 0; frame < 2; ++frame) {  // 1st frame creates the font atlas texture; draw in the 2nd
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();
    ImGuiPainter painter(ImGui::GetBackgroundDrawList(), ImVec2(0, 0));
    ef::renderScene(painter, scene, style, view);
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
    SDL_RenderClear(renderer.get());
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
  }
  SDL_Surface* pixels = SDL_RenderReadPixels(renderer.get(), nullptr);
  if (!pixels) err = std::string("SDL_RenderReadPixels failed: ") + SDL_GetError();
  return pixels;
}

std::string renderPng(const ef::FieldScene& scene, const ef::DrawStyle& style, const ef::View& view, const char* path) {
  std::string err;
  std::unique_ptr<SDL_Surface, SurfaceDeleter> pixels(renderSurface(scene, style, view, err));
  if (!pixels) return err;
  if (!SDL_SavePNG(pixels.get(), path)) return std::string("SDL_SavePNG failed: ") + SDL_GetError();
  return std::string();
}

}  // namespace efrender
