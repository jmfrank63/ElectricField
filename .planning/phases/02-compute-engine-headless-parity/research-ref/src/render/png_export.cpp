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
}  // namespace

std::string renderPng(const ef::FieldScene& scene, const ef::DrawStyle& style, const ef::View& view, const char* path) {
  std::unique_ptr<SDL_Surface, SurfaceDeleter> surface(SDL_CreateSurface(view.widthPx, view.heightPx, SDL_PIXELFORMAT_RGBA32));
  if (!surface) return std::string("SDL_CreateSurface failed: ") + SDL_GetError();
  std::unique_ptr<SDL_Renderer, RendererDeleter> renderer(SDL_CreateSoftwareRenderer(surface.get()));
  if (!renderer) return std::string("SDL_CreateSoftwareRenderer failed: ") + SDL_GetError();

  IMGUI_CHECKVERSION();
  ImGuiContext* const prev = ImGui::GetCurrentContext();  // the GUI (Phase 4 export) has its own context: restore it afterwards
  ImGuiContext* ctx = ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // never write imgui.ini
  io.DisplaySize = ImVec2(static_cast<float>(view.widthPx), static_cast<float>(view.heightPx));
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::GetStyle().CurveTessellationTol = 0.3f;  // default 1.25 is visibly faceted for large Beziers
  // Texture-based AA lines are sampled with nearest filtering by SDL's software renderer (jagged 1 px lines); the
  // vertex-alpha "fringe" AA path renders like Direct2D. Use the same setting in the GUI so both paths look the same.
  ImGui::GetStyle().AntiAliasedLinesUseTex = false;

  std::string err;
  if (!ImGui_ImplSDLRenderer3_Init(renderer.get())) {
    err = "ImGui_ImplSDLRenderer3_Init failed";
  } else {
    // 1st frame creates the font atlas texture; draw in the 2nd (same as src/gui/main.cpp --smoke)
    for (int frame = 0; frame < 2; ++frame) {
      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui::NewFrame();
      ImGuiPainter painter(ImGui::GetBackgroundDrawList(), ImVec2(0, 0));
      ef::renderScene(painter, scene, style, view);
      ImGui::Render();
      SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
      SDL_RenderClear(renderer.get());
      ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
    }
    std::unique_ptr<SDL_Surface, SurfaceDeleter> pixels(SDL_RenderReadPixels(renderer.get(), nullptr));
    if (!pixels)
      err = std::string("SDL_RenderReadPixels failed: ") + SDL_GetError();
    else if (!SDL_SavePNG(pixels.get(), path))
      err = std::string("SDL_SavePNG failed: ") + SDL_GetError();
    ImGui_ImplSDLRenderer3_Shutdown();
  }
  ImGui::DestroyContext(ctx);
  ImGui::SetCurrentContext(prev);
  return err;
}

}  // namespace efrender
