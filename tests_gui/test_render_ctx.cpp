// Regression tests for the offscreen render (efrender::renderSurface / renderPng) while a GUI-like ImGui context with an
// initialised sdlrenderer3 backend already exists. Headless: a software renderer over an SDL_Surface, no window, no display.
// Any IM_ASSERT (src/imgui_config/ef_imconfig.h) terminates the process with exit code 3 and prints file:line:expression.
#include "doctest/doctest.h"

#include <SDL3/SDL.h>

#include <cmath>
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

// v2.6 phase 2: the same dipole computed at an explicit view scale (0 = legacy).
ef::FieldScene dipoleSceneAt(double viewScale) {
  const ef::ChargeSystem field = ef::makeSystem(ef::presets().at(0));
  ef::ComputeParams p = lightParams();
  p.viewScale = viewScale;
  ef::ComputeSession s(field, p);
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

  bool ok = false;  // construction succeeded; tests REQUIRE(gui.ok) (a REQUIRE inside a constructor would leak the partial state)
  bool backend = false;

  GuiLikeContext() {
    surface = SDL_CreateSurface(320, 240, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return;
    renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) return;
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);  // the GUI does exactly this once at start-up (first context: CreateContext keeps it current)
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(320.f, 240.f);
    io.DeltaTime = 1.f / 60.f;
    ImGui::GetStyle().CurveTessellationTol = 0.7f;  // a value the export must not leak into (its own value is 0.3)
    backend = ImGui_ImplSDLRenderer3_Init(renderer);
    if (!backend) return;
    frame();
    frame();
    ok = true;
  }
  ~GuiLikeContext() {
    if (ctx) ImGui::SetCurrentContext(ctx);
    if (backend) ImGui_ImplSDLRenderer3_Shutdown();
    if (ctx) ImGui::DestroyContext(ctx);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (surface) SDL_DestroySurface(surface);
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

// Count of non-background samples on a circle of `radius` device px around (cx, cy): majority sampling
// over the whole ring, deliberately tolerant (anti-aliased ink counts; no byte-exact comparison).
int ringInk(const SDL_Surface* px, float cx, float cy, float radius) {
  const int samples = 360;
  int ink = 0;
  for (int i = 0; i < samples; ++i) {
    const double a = static_cast<double>(i) * 3.14159265358979323846 / 180.0;
    const int x = static_cast<int>(cx + radius * std::cos(a));
    const int y = static_cast<int>(cy + radius * std::sin(a));
    if (x < 0 || y < 0 || x >= px->w || y >= px->h) continue;
    const auto* p = static_cast<const unsigned char*>(px->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(px->pitch) +
                    static_cast<std::size_t>(x) * 4;
    if (p[0] < 240 || p[1] < 240 || p[2] < 240) ++ink;
  }
  return ink;
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
  REQUIRE(gui.ok);
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
  REQUIRE(gui.ok);
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
  REQUIRE(gui.ok);
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

// v2.6 phase 2: the zoom-adapted computation closes the white disk around a charge when zoomed in.
// Dipole computed with viewScale = 800 and rendered at ppu 800 centred on the + charge: a sampling ring
// just outside the 25 px drawn dot must show line pixels. The legacy computation starts its lines at
// the fixed 0.25 world seed circle (200 px at this scale), so the same ring is pure background there.
TEST_CASE("zoom-adapted computation: field lines hug the charge at ppu 800 (renderSurface sampling)") {
  const ef::FieldScene legacyScene = dipoleSceneAt(0.0);
  const ef::FieldScene adaptedScene = dipoleSceneAt(800.0);

  ef::View view;
  view.widthPx = 400;
  view.heightPx = 300;
  view.pixelsPerUnit = 800.0;
  view.centre = ef::Vec2{-2.0, 0.0};  // the + charge sits at the canvas centre

  std::string err;
  SDL_Surface* legacy = efrender::renderSurface(legacyScene, ef::DrawStyle{}, view, err);
  REQUIRE(legacy != nullptr);
  SDL_Surface* adapted = efrender::renderSurface(adaptedScene, ef::DrawStyle{}, view, err);
  REQUIRE(adapted != nullptr);

  const int legacyInk = ringInk(legacy, 200.f, 150.f, 30.f);
  const int adaptedInk = ringInk(adapted, 200.f, 150.f, 30.f);
  MESSAGE("ring ink of 360 samples at 30 px: legacy=", legacyInk, " adapted=", adaptedInk);
  CHECK(legacyInk <= 4);          // essentially the whole ring is background-white before adaptation
  CHECK(adaptedInk > legacyInk);  // the adapted computation strictly adds ink on the ring
  CHECK(adaptedInk >= 4);         // the + charge's field lines must cross the ring

  SDL_DestroySurface(legacy);
  SDL_DestroySurface(adapted);
}

// v2.6 phase 2 (WR-04): the command/script exports (exportConfigurationTo / exportCollectionTo) recompute
// synchronously with the DEFAULT view scale instead of rendering the tab's cached scene - after a zoom
// settle the cache is view-adapted, so exports would depend on the tab's zoom history. Doctested at the
// compute level (the App paths are ImGui-bound): two fresh legacy computes are bit-identical regardless
// of history, and neither equals the zoomed cache.
TEST_CASE("legacy recompute is zoom-history independent (the export recompute contract, WR-04)") {
  const ef::ChargeSystem field = ef::makeSystem(ef::presets().at(1));  // two equal + charges: electric lines record points
  const auto compute = [&field](double viewScale) {
    ef::ComputeParams p = lightParams();
    p.viewScale = viewScale;
    ef::ComputeSession s(field, p);
    REQUIRE_FALSE(s.wait().failed);
    ef::FieldResult r;
    REQUIRE(s.takeResult(r));
    return r;
  };
  const ef::FieldResult zoomed = compute(800.0);  // what a zoom-settle recalc leaves in the tab's cache
  const ef::FieldResult fresh1 = compute(0.0);    // the export recompute: the DEFAULT view scale
  const ef::FieldResult fresh2 = compute(0.0);    // ...from any zoom history: the same output
  REQUIRE(fresh1.electric.size() > 0);
  REQUIRE(fresh1.potential.size() > 0);
  CHECK(fresh2.electric.size() == fresh1.electric.size());
  CHECK(fresh2.potential.size() == fresh1.potential.size());
  CHECK(fresh2.electric[0].points.size() == fresh1.electric[0].points.size());
  CHECK(fresh2.electric[0].points.front().X == fresh1.electric[0].points.front().X);  // deterministic (D-04)
  CHECK(fresh2.electric[0].points.front().Y == fresh1.electric[0].points.front().Y);
  CHECK(fresh2.potential[0].potential == fresh1.potential[0].potential);
  // the zoomed cache really differs from the legacy result (adapted seed circle 0.02 vs the legacy
  // 0.25 world from the charge): the recompute is not silently a no-op
  CHECK((zoomed.electric[0].points.front() - field.charges[0].position).Length() * 5.0 <
        (fresh1.electric[0].points.front() - field.charges[0].position).Length());
}
