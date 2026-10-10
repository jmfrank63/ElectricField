// Regression tests for the offscreen render (efrender::renderSurface / renderPng) while a GUI-like ImGui context with an
// initialised sdlrenderer3 backend already exists. Headless: a software renderer over an SDL_Surface, no window, no display.
// Any IM_ASSERT (src/imgui_config/ef_imconfig.h) terminates the process with exit code 3 and prints file:line:expression.
#include "doctest/doctest.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <filesystem>
#include <string>

#include "ef/charge_system.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "render/png_export.hpp"
#include "test_params.hpp"

namespace {

ef::FieldScene dipoleScene() {
  const ef::ChargeSystem field = ef::makeSystem(ef::presets().at(0));
  ef::ComputeSession s(field, lightParams());
  REQUIRE_FALSE(s.wait().failed);
  ef::FieldResult r;
  REQUIRE(s.takeResult(r));
  return ef::buildScene(field, r);
}

// Stand-in for the running GUI: its own ImGuiContext (made current), an initialised sdlrenderer3 backend, a few frames drawn.
struct GuiLikeContext {
  SDL_Surface* surface = nullptr;
  SDL_Renderer* renderer = nullptr;
  ImGuiContext* ctx = nullptr;

  GuiLikeContext() {
    surface = SDL_CreateSurface(320, 240, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr);
    renderer = SDL_CreateSoftwareRenderer(surface);
    REQUIRE(renderer != nullptr);
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);  // the GUI does exactly this once at start-up (first context: CreateContext keeps it current)
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(320.f, 240.f);
    io.DeltaTime = 1.f / 60.f;
    ImGui::GetStyle().CurveTessellationTol = 0.7f;  // a value the export must not leak into (its own value is 0.3)
    REQUIRE(ImGui_ImplSDLRenderer3_Init(renderer));
    frame();
    frame();
  }
  ~GuiLikeContext() {
    ImGui::SetCurrentContext(ctx);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui::DestroyContext(ctx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
  }
  void frame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("host");
    ImGui::TextUnformatted("gui frame");
    ImGui::End();
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
  }
};

SDL_Surface* render(const ef::FieldScene& scene, int w, int h) {
  std::string err;
  ef::View view;
  view.widthPx = w;
  view.heightPx = h;
  SDL_Surface* px = efrender::renderSurface(scene, ef::DrawStyle{}, view, err);
  INFO(err);
  REQUIRE(px != nullptr);
  return px;
}

bool samePixels(const SDL_Surface* a, const SDL_Surface* b) {
  if (a->w != b->w || a->h != b->h || a->format != b->format) return false;
  for (int y = 0; y < a->h; ++y) {
    const auto* ra = static_cast<const unsigned char*>(a->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(a->pitch);
    const auto* rb = static_cast<const unsigned char*>(b->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(b->pitch);
    if (std::memcmp(ra, rb, static_cast<std::size_t>(a->w) * 4) != 0) return false;
  }
  return true;
}

}  // namespace

TEST_CASE("renderSurface with no ImGui context at all (ef-cli path) leaves no context behind") {
  REQUIRE(ImGui::GetCurrentContext() == nullptr);
  const ef::FieldScene scene = dipoleScene();
  SDL_Surface* px = render(scene, 200, 150);
  CHECK(ImGui::GetCurrentContext() == nullptr);
  SDL_DestroySurface(px);
}

TEST_CASE("renderSurface inside a running GUI context: no assert, caller context/io/style/backend untouched") {
  const ef::FieldScene scene = dipoleScene();
  SDL_Surface* alone = render(scene, 200, 150);  // reference: same call without any GUI context

  GuiLikeContext gui;
  ImGuiIO& io = ImGui::GetIO();
  void* const backend = io.BackendRendererUserData;
  REQUIRE(backend != nullptr);
  const ImVec2 displaySize = io.DisplaySize;

  SDL_Surface* inside = render(scene, 200, 150);  // before the fix: IM_ASSERT "Already initialized a renderer backend!" (exit 3)

  CHECK(ImGui::GetCurrentContext() == gui.ctx);                    // the caller's context is current again
  CHECK(ImGui::GetIO().BackendRendererUserData == backend);        // its backend was neither re-initialised nor shut down
  CHECK(ImGui::GetIO().DisplaySize.x == displaySize.x);            // the export did not resize the GUI's display
  CHECK(ImGui::GetIO().DisplaySize.y == displaySize.y);
  CHECK(ImGui::GetStyle().CurveTessellationTol == doctest::Approx(0.7f));  // nor rewrite its style
  CHECK(samePixels(alone, inside));                                // the pixels do not depend on whether a GUI exists

  gui.frame();  // and the GUI can keep rendering afterwards
  SDL_DestroySurface(alone);
  SDL_DestroySurface(inside);
}

TEST_CASE("renderSurface repeatedly inside a GUI context (export button pressed many times)") {
  const ef::FieldScene scene = dipoleScene();
  GuiLikeContext gui;
  for (int i = 0; i < 5; ++i) {
    SDL_Surface* px = render(scene, 160 + i, 120);
    SDL_DestroySurface(px);
    CHECK(ImGui::GetCurrentContext() == gui.ctx);
    gui.frame();
  }
}

TEST_CASE("renderPng inside a GUI context writes a PNG and restores the context") {
  const ef::FieldScene scene = dipoleScene();
  GuiLikeContext gui;
  const std::filesystem::path out = std::filesystem::temp_directory_path() / "ef-render-ctx-test.png";
  std::filesystem::remove(out);
  ef::View view;
  view.widthPx = 200;
  view.heightPx = 150;
  const std::string err = efrender::renderPng(scene, ef::DrawStyle{}, view, out.string().c_str());
  CHECK(err.empty());
  CHECK(std::filesystem::exists(out));
  CHECK(std::filesystem::file_size(out) > 100);
  std::filesystem::remove(out);
  CHECK(ImGui::GetCurrentContext() == gui.ctx);
  gui.frame();
}
