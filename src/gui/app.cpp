#include "gui/app.hpp"

#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "app/ui_logic.hpp"
#include "ef/presets.hpp"
#include "ef_build_info.h"
#include "ef_git_hash.h"
#include "gui/icon.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "render/png_export.hpp"

namespace efgui {

namespace {
SDL_Surface* loadPng(const unsigned char* data, std::size_t size) {
  SDL_IOStream* io = SDL_IOFromConstMem(data, size);
  return io ? SDL_LoadPNG_IO(io, true) : nullptr;
}

// A session that never finishes: lets smoke draw the busy overlay deterministically.
struct EndlessSession final : efapp::ISession {
  void cancel() override {}
  ef::SessionStatus poll(ef::PollBatch&) override { return ef::SessionStatus{}; }
  bool takeResult(ef::FieldResult&) override { return false; }
};
}  // namespace

bool App::init(const AppOptions& opt, std::string& step, std::string& message) {
  opt_ = opt;
  if (opt_.headless()) {  // headless paths must not depend on the environment
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  }
  auto fail = [&](const char* s) {
    step = s;
    message = SDL_GetError();
    shutdown();  // release whatever was created, in reverse order
    return false;
  };

  if (!SDL_Init(SDL_INIT_VIDEO)) return fail("SDL initialisation");
  sdlInit_ = true;

  // Initial size: (1024 x 824 logical) * k, clamped to 90% of the usable area of the primary display.
  const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
  float k0 = efapp::clampScale(primary ? SDL_GetDisplayContentScale(primary) : 1.f);
  if (opt_.scaleOverride > 0.f) k0 = efapp::clampScale(opt_.scaleOverride);
  SDL_Rect usable{0, 0, 0, 0};
  if (primary) SDL_GetDisplayUsableBounds(primary, &usable);
  efapp::IntSize size = efapp::defaultWindowSize(k0, usable.w, usable.h);
  if (opt_.width > 0 && opt_.height > 0) size = {opt_.width, opt_.height};

  window_ = SDL_CreateWindow("Electric Field", size.w, size.h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window_) return fail("Window creation");
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  if (SDL_Surface* icon = loadPng(icon256Png(), icon256PngSize())) {
    SDL_SetWindowIcon(window_, icon);  // SDL copies the pixels (ignored by macOS, whose Dock icon comes from the bundle)
    SDL_DestroySurface(icon);
  }

  renderer_ = SDL_CreateRenderer(window_, nullptr);
  if (!renderer_) return fail("Renderer creation");
  vsync_ = SDL_SetRenderVSync(renderer_, 1);  // not fatal; without vsync the loop caps itself with SDL_Delay

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();  // first context: becomes current (see png_export.cpp for the rule when one already exists)
  imguiCtx_ = true;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // always: window geometry / options persistence belongs to Phase 4
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.Fonts->AddFontDefaultVector();
  baseStyle_ = makeBaseStyle();
  if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) return fail("Dear ImGui initialisation");
  platformInit_ = true;
  if (!ImGui_ImplSDLRenderer3_Init(renderer_)) return fail("Dear ImGui initialisation");
  rendererInit_ = true;

  if (SDL_Surface* s = loadPng(icon96Png(), icon96PngSize())) {
    aboutIcon_ = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_DestroySurface(s);
    if (aboutIcon_) SDL_SetTextureScaleMode(aboutIcon_, SDL_SCALEMODE_LINEAR);
  }

  env_.mac = io.ConfigMacOSXBehaviors;
  env_.aboutIcon = aboutIcon_;
  env_.semver = EF_VERSION;
  env_.gitHash = EF_GIT_HASH;
  refreshScale(true);  // also sets the minimum window size
  return true;
}

void App::shutdown() {
  ws_.closeAll();  // cancel every session first (overlapping joins), then destroy, THEN tear down SDL / ImGui
  if (rendererInit_) ImGui_ImplSDLRenderer3_Shutdown();
  if (platformInit_) ImGui_ImplSDL3_Shutdown();
  if (imguiCtx_) ImGui::DestroyContext();
  rendererInit_ = platformInit_ = imguiCtx_ = false;
  if (aboutIcon_) SDL_DestroyTexture(aboutIcon_);
  aboutIcon_ = nullptr;
  if (renderer_) SDL_DestroyRenderer(renderer_);
  renderer_ = nullptr;
  if (window_) SDL_DestroyWindow(window_);
  window_ = nullptr;
  if (sdlInit_) SDL_Quit();
  sdlInit_ = false;
}

void App::refreshScale(bool initial) {
  // k = display CONTENT scale only. Never SDL_GetWindowDisplayScale (= content scale x pixel density): on macOS / Wayland the
  // pixel density is already applied by the ImGui backend as io.DisplayFramebufferScale, so using it here would double the UI.
  float k = efapp::clampScale(SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window_)));
  if (opt_.scaleOverride > 0.f) k = efapp::clampScale(opt_.scaleOverride);
  k_ = k;
  env_.k = k;
  applyScale(baseStyle_, k);
  scaleDirty_ = false;
  // The minimum window size follows the scale (a move to a 2.0 display must not allow shrinking to half the logical minimum).
  // Only at creation is it clamped to the window size, so a small --size or a small display does not force the window to grow.
  const efapp::IntSize minSize = efapp::minimumWindowSize(k);
  int w = 0, h = 0;
  if (initial) SDL_GetWindowSize(window_, &w, &h);
  SDL_SetWindowMinimumSize(window_, initial ? std::min(minSize.w, w) : minSize.w, initial ? std::min(minSize.h, h) : minSize.h);
}

void App::handleEvent(const SDL_Event& e) {
  ImGui_ImplSDL3_ProcessEvent(&e);
  switch (e.type) {
    case SDL_EVENT_QUIT:
      quit_ = true;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      if (e.window.windowID == SDL_GetWindowID(window_)) quit_ = true;
      break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
      scaleDirty_ = true;
      break;
    default:
      break;
  }
}

void App::frame() {
  ws_.pollAll();
  if (scaleDirty_) refreshScale();
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawUi(ws_, ui_, env_);
  ImGui::Render();
  if (ui_.wantedTitle != lastTitle_) {
    SDL_SetWindowTitle(window_, ui_.wantedTitle.c_str());
    lastTitle_ = ui_.wantedTitle;
  }
  SDL_SetRenderDrawColor(renderer_, 240, 240, 240, 255);
  SDL_RenderClear(renderer_);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  SDL_RenderPresent(renderer_);
  ++frames_;
  pacer_.frameRendered();
}

void App::frames(int n) {
  for (int i = 0; i < n; ++i) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);
    frame();
  }
}

bool App::iterate() {
  const bool computing = ws_.anyComputing();
  const bool modalBefore = ui_.chooserVisible || ui_.aboutVisible;
  const unsigned openedBefore = ws_.opened();
  SDL_Event e;
  bool gotEvent = false;
  if (!pacer_.shouldBlock(computing)) {
    while (SDL_PollEvent(&e)) {
      handleEvent(e);
      gotEvent = true;
    }
  } else if (SDL_WaitEventTimeout(&e, efapp::FramePacer::kIdleWaitMs)) {  // idle: no busy loop (Phase 1 WR-02 / Phase 2 IN-03)
    handleEvent(e);
    gotEvent = true;
    while (SDL_PollEvent(&e)) handleEvent(e);
  }
  if (gotEvent) pacer_.notifyActivity();
  if (quit_) return false;
  if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {  // sessions keep being polled, nothing is drawn
    ws_.pollAll();
    SDL_Delay(16);
    return true;
  }
  frame();
  if (ws_.opened() != openedBefore || computing != ws_.anyComputing()) pacer_.notifyActivity();  // tab opened / finished: show it
  if ((ui_.chooserVisible || ui_.aboutVisible) != modalBefore) pacer_.notifyActivity(efapp::FramePacer::kModalFadeFrames);
  if (!vsync_) SDL_Delay(16);
  return !ui_.requestQuit;
}

int App::runInteractive() {
  ui_.openChooser = true;  // D-03: the preset chooser is shown at startup
  pacer_.notifyActivity(efapp::FramePacer::kModalFadeFrames);
  while (iterate()) {
  }
  return 0;
}

unsigned App::lastSerial() const { return ws_.tabs().empty() ? 0u : ws_.tabs().back()->serial; }  // 0 = no tab (no valid serial)

bool App::pumpUntilDone(unsigned serial, unsigned timeoutMs) {
  const Uint64 t0 = SDL_GetTicks();
  for (;;) {
    frames(1);
    const efapp::Tab* t = ws_.find(serial);
    if (!t) return false;
    if (t->state != efapp::TabState::Computing) return t->state == efapp::TabState::Done;
    if (SDL_GetTicks() - t0 > timeoutMs) return false;
    SDL_Delay(5);
  }
}

void App::chord(ImGuiKeyChord mods, ImGuiKey key) {
  // Injected through ImGui's queue like a backend would. On macOS ImGui swaps Ctrl<->Super inside AddKeyEvent, so the
  // physical modifier that yields the logical "Ctrl" (= Cmd) of Shortcut(ImGuiMod_Ctrl | ...) is ImGuiMod_Super there.
  ImGuiIO& io = ImGui::GetIO();
  const ImGuiKey mod = (mods & ImGuiMod_Ctrl) ? (io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl) : ImGuiKey_None;
  if (mod != ImGuiKey_None) io.AddKeyEvent(mod, true);
  frames(2);
  io.AddKeyEvent(key, true);
  frames(3);
  io.AddKeyEvent(key, false);
  frames(2);
  if (mod != ImGuiKey_None) io.AddKeyEvent(mod, false);
  frames(2);
}

bool App::checkCanvas(unsigned serial, int w, int h, std::string& report) {
  SDL_SetWindowSize(window_, w, h);
  SDL_SyncWindow(window_);
  frames(4);
  const efapp::Tab* t = ws_.find(serial);
  if (!t) return false;
  const int cx0 = static_cast<int>(ui_.canvasMin.x), cy0 = static_cast<int>(ui_.canvasMin.y);
  const int cw = static_cast<int>(ui_.canvasMax.x - ui_.canvasMin.x), ch = static_cast<int>(ui_.canvasMax.y - ui_.canvasMin.y);

  SDL_Surface* raw = SDL_RenderReadPixels(renderer_, nullptr);
  if (!raw) return false;
  SDL_Surface* gui = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(raw);
  if (!gui) return false;

  // The reference is rendered by the SAME function ef-cli uses (efrender::renderSurface), with its own ImGui context, while the
  // GUI context (with its initialised backend) is live: this is also the in-process regression check for the context rule.
  std::string err;
  const ef::View view = efapp::canvasView(static_cast<float>(cw), static_cast<float>(ch), k_);
  SDL_Surface* ref = efrender::renderSurface(t->scene, efapp::scaledStyle(k_), view, err);
  if (!ref) {
    SDL_DestroySurface(gui);
    report = err;
    return false;
  }
  // 1) canvas rectangle equals the headless render: count pixels that differ by more than 24 in any channel
  long diff = 0;
  for (int y = 0; y < ch && y + cy0 < gui->h; ++y) {
    const Uint8* a = static_cast<const Uint8*>(gui->pixels) + static_cast<std::size_t>(y + cy0) * static_cast<std::size_t>(gui->pitch) + static_cast<std::size_t>(cx0) * 4;
    const Uint8* b = static_cast<const Uint8*>(ref->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(ref->pitch);
    for (int x = 0; x < cw && x + cx0 < gui->w; ++x)
      for (int c = 0; c < 3; ++c)
        if (std::abs(static_cast<int>(a[x * 4 + c]) - static_cast<int>(b[x * 4 + c])) > 24) {
          ++diff;
          break;
        }
  }
  // 2) clipping backstop: no scene ink in the chrome right of the tab text (x >= 400 x k), above the canvas
  long bleed = 0;
  const int x0 = static_cast<int>(400.f * k_);
  for (int y = 0; y < cy0; ++y) {
    const Uint8* a = static_cast<const Uint8*>(gui->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(gui->pitch);
    for (int x = x0; x < gui->w; ++x)
      if (a[x * 4] < 128 && a[x * 4 + 1] < 128 && a[x * 4 + 2] < 128) ++bleed;
  }
  // 3) the origin stays at the centre of the canvas: the pixel at the centre of the dipole's two charges' midpoint is background,
  //    and the scene is symmetric about the canvas centre column for the (symmetric) dipole -> check the charge colours exist
  long blue = 0, red = 0;
  for (int y = 0; y < ch && y + cy0 < gui->h; ++y) {
    const Uint8* a = static_cast<const Uint8*>(gui->pixels) + static_cast<std::size_t>(y + cy0) * static_cast<std::size_t>(gui->pitch) + static_cast<std::size_t>(cx0) * 4;
    for (int x = 0; x < cw; ++x) {
      if (a[x * 4 + 2] > 200 && a[x * 4] < 40 && a[x * 4 + 1] < 40) ++blue;
      if (a[x * 4] > 200 && a[x * 4 + 1] < 40 && a[x * 4 + 2] < 40) ++red;
    }
  }
  report = "canvas " + std::to_string(cw) + "x" + std::to_string(ch) + " at (" + std::to_string(cx0) + "," + std::to_string(cy0) + ") window " +
           std::to_string(gui->w) + "x" + std::to_string(gui->h) + " diff_px=" + std::to_string(diff) + " of " + std::to_string(cw * ch) +
           " chrome_bleed_px=" + std::to_string(bleed) + " blue_px=" + std::to_string(blue) + " red_px=" + std::to_string(red);
  const bool ok = diff <= static_cast<long>(cw) * ch / 1000 && bleed == 0 && blue > 1000 && red > 1000;
  SDL_DestroySurface(gui);
  SDL_DestroySurface(ref);
  return ok;
}

// Everything beyond the UI-SPEC smoke sequence that CI can check without a display: keyboard contract through ImGui's input
// queue, the failed-session surface, the busy overlay, the idle loop, quit with busy sessions.
bool App::smokeExtras(std::string& report) {
  auto fail = [&](const std::string& what) {
    report = what;
    return false;
  };
  SDL_SetWindowSize(window_, 1024, 824);
  SDL_SyncWindow(window_);
  frames(4);
  const unsigned openedBase = ws_.opened(), closedBase = ws_.closed();

  // Keyboard: Ctrl+N opens the chooser, Down selects row 2, Enter opens "Two Equal Charges 1"; Ctrl+W closes it.
  chord(ImGuiMod_Ctrl, ImGuiKey_N);
  if (!ui_.chooserVisible) return fail("Ctrl+N did not open the chooser");
  if (ui_.chooserSel != 0) return fail("chooser did not preselect Dipole");
  chord(0, ImGuiKey_DownArrow);
  if (ui_.chooserSel != 1) return fail("Down did not move the chooser selection to row 2 (sel=" + std::to_string(ui_.chooserSel) + ")");
  chord(0, ImGuiKey_Enter);
  if (ui_.chooserVisible || ws_.tabs().size() != 1 || ws_.tabs().back()->title != "Two Equal Charges 1")
    return fail("chooser Down+Enter did not open Two Equal Charges 1 (tabs=" + std::to_string(ws_.tabs().size()) + " title=" + (ws_.tabs().empty() ? std::string("-") : ws_.tabs().back()->title) + " chooser=" + std::to_string(ui_.chooserVisible) + " sel=" + std::to_string(ui_.chooserSel) + ")");
  chord(ImGuiMod_Ctrl, ImGuiKey_W);
  if (!ws_.tabs().empty()) return fail("Ctrl+W did not close the active tab");
  // Esc closes the chooser without opening anything; Ctrl+N is ignored while About is open.
  chord(ImGuiMod_Ctrl, ImGuiKey_N);
  chord(0, ImGuiKey_Escape);
  if (ui_.chooserVisible || !ws_.tabs().empty()) return fail("Esc did not close the chooser cleanly");
  ui_.openAbout = true;
  frames(4);
  chord(ImGuiMod_Ctrl, ImGuiKey_N);
  if (ui_.chooserVisible || !ui_.aboutVisible) return fail("Ctrl+N was not ignored while About is open");
  chord(0, ImGuiKey_Escape);
  if (ui_.aboutVisible) return fail("Esc did not close About");

  // Middle click on a tab closes it (ImGui default, we do not set NoCloseWithMiddleMouseButton).
  ui_.openPresetNow = 0;
  frames(4);
  {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((ui_.tabMin.x + ui_.tabMax.x) * 0.5f - 12.f * k_, (ui_.tabMin.y + ui_.tabMax.y) * 0.5f);
    frames(3);
    io.AddMouseButtonEvent(2, true);
    frames(2);
    io.AddMouseButtonEvent(2, false);
    frames(3);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    frames(2);
  }
  if (!ws_.tabs().empty()) return fail("middle click on the tab did not close it");

  // Failed surface: a session that cannot start yields a Failed tab and the failure copy is drawn.
  ws_.setFactory([](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> { throw std::runtime_error("injected failure"); });
  ui_.openPresetNow = 3;
  frames(4);
  if (ws_.tabs().size() != 1 || ws_.tabs().back()->state != efapp::TabState::Failed || !ui_.failedShown) return fail("failed tab not shown");
  ui_.closeSerial = lastSerial();
  frames(2);

  // Busy overlay at the minimum window size: stays inside the canvas.
  ws_.setFactory([](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> { return std::make_unique<EndlessSession>(); });
  const efapp::IntSize m = efapp::minimumWindowSize(k_);
  SDL_SetWindowSize(window_, m.w, m.h);
  SDL_SyncWindow(window_);
  ui_.openPresetNow = 5;
  frames(4);
  if (ui_.overlayMax.x <= ui_.overlayMin.x || ui_.overlayMin.x < ui_.canvasMin.x || ui_.overlayMin.y < ui_.canvasMin.y ||
      ui_.overlayMax.x > ui_.canvasMax.x || ui_.overlayMax.y > ui_.canvasMax.y)
    return fail("busy overlay outside the canvas at the minimum window size");
  ui_.closeSerial = lastSerial();
  frames(2);
  ws_.setFactory(efapp::defaultSessionFactory());

  // Idle loop: with one finished tab and no input the loop must block (<= ~4 frames/s + the settle frames), not spin.
  SDL_SetWindowSize(window_, 1024, 824);
  SDL_SyncWindow(window_);
  ui_.openPresetNow = 0;
  frames(2);
  if (!pumpUntilDone(lastSerial(), 60000)) return fail("idle test: session did not finish");
  frames(4);
  while (iterate() && pacer_.pendingFrames() > 0) {
  }
  const unsigned f0 = frames_;
  const Uint64 t0 = SDL_GetTicks();
  while (SDL_GetTicks() - t0 < 1300) iterate();
  const unsigned idleFrames = frames_ - f0;
  if (idleFrames > 10) return fail("idle loop is spinning: " + std::to_string(idleFrames) + " frames in 1.3 s");
  ui_.closeSerial = lastSerial();
  frames(2);

  // Quit with busy sessions: cancel-all then destroy must be quick.
  for (std::size_t i = 0; i < 3; ++i) {
    ui_.openPresetNow = static_cast<int>(i + 4);
    frames(1);
  }
  const Uint64 q0 = SDL_GetTicks();
  ws_.closeAll();
  const Uint64 quitMs = SDL_GetTicks() - q0;
  if (quitMs > 1000) return fail("closeAll with 3 busy sessions took " + std::to_string(quitMs) + " ms");
  frames(2);
  if (ws_.opened() - openedBase != ws_.closed() - closedBase) return fail("tab leak: opened != closed");
  report = "keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=" + std::to_string(idleFrames) + "/1.3s quit_ms=" + std::to_string(quitMs);
  return true;
}

int App::runSmoke() {
  std::printf("video=%s renderer=%s presets=%zu\n", SDL_GetCurrentVideoDriver(), SDL_GetRendererName(renderer_), ef::presets().size());
  std::printf("about: %s\n", efapp::versionLine(EF_VERSION, EF_GIT_HASH).c_str());
  // Software frames of a finished scene take ~100 ms, so a key held for 3 injected frames outlasts ImGui key-repeat delay (0.275 s) and
  // Down moves the chooser row several times. Injected chords must be a single press: switch repeat off for the whole smoke run.
  ImGui::GetIO().KeyRepeatDelay = 1e9f;
  // 1) a busy tab is opened and closed again within the first two frames (cancel path).
  ui_.openPresetNow = 5;  // Four Charges Same Sign
  frames(2);
  ui_.closeSerial = lastSerial();
  frames(1);
  // 2) Dipole: run until its session is done, draw the finished scene.
  ui_.openPresetNow = 0;
  frames(1);
  const unsigned serial = lastSerial();
  if (!pumpUntilDone(serial, 60000)) {
    std::fprintf(stderr, "smoke: the dipole session did not finish\n");
    return 1;
  }
  frames(2);
  const efapp::Tab* t = ws_.find(serial);
  const std::size_t lines = t ? t->scene.electric.size() : 0;
  if (lines == 0) {
    std::fprintf(stderr, "smoke: the finished scene has no field lines\n");
    return 1;
  }
  const int sizes[][2] = {{640, 480}, {1024, 820}, {1280, 1024}};
  for (const auto& sz : sizes) {
    std::string report;
    const bool ok = checkCanvas(serial, sz[0], sz[1], report);
    std::printf("canvas check %dx%d: %s (%s)\n", sz[0], sz[1], ok ? "ok" : "FAIL", report.c_str());
    if (!ok) return 1;
  }
  ui_.closeSerial = serial;
  frames(1);
  // 3) empty state, About modal, chooser modal (each closed with Esc through the input queue)
  frames(1);
  ui_.openAbout = true;
  frames(3);
  chord(0, ImGuiKey_Escape);
  ui_.openChooser = true;
  frames(3);
  chord(0, ImGuiKey_Escape);
  const unsigned opened = ws_.opened(), closed = ws_.closed();  // the UI-SPEC sequence: 2 tabs opened, 2 closed
  if (opened != 2 || closed != 2) {
    std::fprintf(stderr, "smoke: expected tabs_opened=2 closed=2, got %u / %u\n", opened, closed);
    return 1;
  }
  std::string extras;
  if (!smokeExtras(extras)) {
    std::fprintf(stderr, "smoke: extras failed: %s\n", extras.c_str());
    return 1;
  }
  std::printf("smoke extras: %s\n", extras.c_str());
  ws_.closeAll();
  shutdown();
  std::printf("smoke ok frames=%u tabs_opened=%u lines=%zu closed=%u\n", frames_, opened, lines, closed);
  return 0;
}

int App::runScreenshot() {
  const auto& presets = ef::presets();
  std::size_t idx = presets.size();
  for (std::size_t i = 0; i < presets.size(); ++i)
    if (presets[i].slug == opt_.slug) idx = i;
  if (idx == presets.size()) {
    std::fprintf(stderr, "unknown preset '%s'\n", opt_.slug.c_str());
    return 2;
  }
  ui_.openPresetNow = static_cast<int>(idx);
  frames(1);
  const unsigned serial = lastSerial();
  if (!pumpUntilDone(serial, 60000)) {
    std::fprintf(stderr, "screenshot: the '%s' session did not finish (timeout, failure or no tab)\n", opt_.slug.c_str());
    return 1;
  }
  if (opt_.modal == "about") ui_.openAbout = true;
  if (opt_.modal == "chooser") ui_.openChooser = true;
  frames(14);  // modal dim fade takes ~10 frames
  SDL_Surface* px = SDL_RenderReadPixels(renderer_, nullptr);
  if (!px) {
    std::fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
    return 1;
  }
  const bool ok = SDL_SavePNG(px, opt_.out.c_str());
  std::printf("screenshot %s %dx%d canvas=(%.0f,%.0f)-(%.0f,%.0f) k=%.2f ok=%d\n", opt_.slug.c_str(), px->w, px->h, static_cast<double>(ui_.canvasMin.x),
              static_cast<double>(ui_.canvasMin.y), static_cast<double>(ui_.canvasMax.x), static_cast<double>(ui_.canvasMax.y), static_cast<double>(k_),
              ok ? 1 : 0);
  SDL_DestroySurface(px);
  return ok ? 0 : 1;
}

}  // namespace efgui
