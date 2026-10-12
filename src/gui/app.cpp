#include "gui/app.hpp"

#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "app/session_file.hpp"
#include "app/ui_logic.hpp"
#include "app/ini_text.hpp"
#include "app/configs.hpp"
#include "app/options.hpp"
#include "app/editor.hpp"
#include "ef/presets.hpp"
#include "ef/svg.hpp"
#include "ef/pdf.hpp"
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

// Text file write through SDL's I/O: the same UTF-8-safe path handling as SDL_SavePNG (D-07), so SVG export
// behaves like PNG export for non-ASCII destinations.
std::string writeTextFile(const std::string& path, const std::string& text) {
  SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "w");
  if (!io) return std::string("SDL_IOFromFile failed: ") + SDL_GetError();
  const bool written = SDL_WriteIO(io, text.data(), text.size()) == text.size();
  const bool closed = SDL_CloseIO(io);
  if (!written || !closed) return "Could not write " + path;
  return {};
}

// Binary byte-exact writes ("wb": the PDF's object offsets and stream lengths must survive newline
// handling untouched, which the text mode above must not be trusted with).
std::string writeBinaryFile(const std::string& path, const std::string& data) {
  SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "wb");
  if (!io) return std::string("SDL_IOFromFile failed: ") + SDL_GetError();
  const bool written = SDL_WriteIO(io, data.data(), data.size()) == data.size();
  const bool closed = SDL_CloseIO(io);
  if (!written || !closed) return "Could not write " + path;
  return {};
}
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
  io.IniFilename = nullptr;  // always: window geometry stays session-local; user options persist via efapp::saveOptions (Phase 4)
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
  // User options (Phase 4, D-03), user configurations (v2 CFG-01) and the open-tab session (v2.1 SES-01)
  // live in the per-user config directory. Smoke/screenshot never touch user files; script mode always
  // sets them up, in a sandbox by default and under --user-dir <dir> when given (v2.4 API-04).
  if (opt_.usesUserDir()) {
    std::filesystem::path dir;
    if (!opt_.userDir.empty()) {
      dir = std::filesystem::path(opt_.userDir);
    } else if (!opt_.script) {
      if (char* pref = SDL_GetPrefPath("ElectricField", "ElectricField")) {
        dir = std::filesystem::u8path(pref);  // SDL hands out UTF-8; u8path keeps non-ASCII intact
        SDL_free(pref);
      }
    } else {
      // Script sandbox: a throwaway directory under the temp folder.
      std::ostringstream sandbox;
      sandbox << "ef-script-" << SDL_GetTicks();
      dir = std::filesystem::temp_directory_path() / sandbox.str();
      opt_.userDir = dir.string();
    }
    if (!dir.empty()) {
      std::filesystem::create_directories(dir);
      configDir_ = dir;
      optionsPath_ = dir / "options.ini";
      options_ = efapp::loadOptions(optionsPath_);
      configsPath_ = dir / "configurations.ini";
      userConfigs_ = efapp::loadConfigs(configsPath_);
      sessionPath_ = dir / "session.ini";
      sessionsDir_ = dir / "Sessions";
      std::filesystem::create_directories(sessionsDir_);
      // v2.2 COL-01: the Original collection is seeded once with the built-ins + any legacy configurations.
      collectionsDir_ = dir / "Collections";
      std::filesystem::create_directories(collectionsDir_);
      const std::filesystem::path original = collectionsDir_ / "Original.ini";
      if (!std::filesystem::exists(original)) {
        std::vector<efapp::UserConfig> seed;
        for (const ef::Preset& p : ef::presets()) {
          efapp::UserConfig u;
          u.name = std::string(p.displayName);
          for (int k = 0; k < p.count; ++k) u.charges.push_back(p.charges[static_cast<std::size_t>(k)]);
          seed.push_back(std::move(u));
        }
        for (const efapp::UserConfig& u : efapp::loadConfigs(configsPath_)) seed.push_back(u);
        std::string migErr;
        efapp::saveConfigs(original, seed, migErr);
      }
      // COL-09 (v2.4): session.ini is the pointer; the tabs live in Sessions/<Collection>.ini. A legacy
      // session.ini that still carries tabs migrates once into Sessions/<its collection>.ini.
      const efapp::SavedSession legacy = efapp::loadSessionFull(sessionPath_);
      if (!legacy.tabs.empty()) {
        std::string migErr;
        if (efapp::saveSession(sessionsDir_ / (legacy.collection + ".ini"), legacy.tabs, legacy.collection, migErr))
          efapp::saveSession(sessionPath_, {}, legacy.collection, migErr);  // now a pure pointer
      }
      // v2.5 OPEN-01: the app starts with no collection open - a collection is a document file that
      // the user opens explicitly (File > Open Collection..., Ctrl+O; any path). The seeded
      // Collections\ folder is the picker's starting point, and sessions stay keyed by file stem.
      currentCollection_.clear();
      currentCollectionPath_.clear();
      userConfigs_.clear();
      ui_.userConfigs.clear();
      refreshCollections();
    }
  }
  env_.options = &options_;
  env_.drawStyle = options_.draw;
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
  ws_.autoRecalcTick(ImGui::GetTime());  // v2.2 CALC-03: fire due continuous recalculations
  ui_.userConfigs = userConfigs_;  // the chooser reads the mirror; the App keeps the master list
  ui_.collections = collections_;
  ui_.currentCollection = currentCollection_;
  if (scaleDirty_) refreshScale();
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  // Both HiDPI corrections below derive from one ground-truth ratio: the renderer's true output size vs the window
  // size. They only ever fire on a pixel-density > 1 framebuffer (macOS Retina / Wayland) and are no-ops elsewhere.
  {
    ImGuiIO& io = ImGui::GetIO();
    int winW = 0, winH = 0, outW = 0, outH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);
    SDL_GetRenderOutputSize(renderer_, &outW, &outH);
    if (winW > 0 && winH > 0 && outW > 0 && outH > 0) {
      // Geometry: imgui_impl_sdlrenderer3 submits vertices (and, whenever the SDL render scale differs from 1, the
      // clip rectangles) in ImGui unit space and leaves the scaling to SDL's renderer view scale - which nothing
      // sets (SDL initialises it to 1 and never updates it itself). On a density-2 framebuffer the whole UI then
      // lands at half size in the top-left quadrant of the framebuffer, and hover lands half a UI away from the
      // drawn widgets (a click opens a menu whose highlight sits at twice the pointer's offset). Setting the view
      // scale to the density scales vertices AND clip rects consistently. Recomputed every frame from the output
      // size, so it can never go stale (e.g. window moved between displays); SDL_SetRenderScale early-outs when
      // the value is unchanged.
      const float density = std::max(outW / static_cast<float>(winW), outH / static_cast<float>(winH));
      SDL_SetRenderScale(renderer_, density, density);
      // Font rasterizer density: with viewports disabled ImGui reads the main viewport's FramebufferScale, which
      // the backend never fills (it stays 0), and falls back to io.DisplayFramebufferScale. The platform backend
      // derives that from SDL_GetWindowDisplayScale() (= density x content scale), which can go stale on macOS -
      // glyphs would then rasterize at the wrong density (blurry). Quarter-quantised to avoid font-atlas churn
      // during resizes.
      const float fb = std::round(density * 4.f) / 4.f;
      if (fb >= 1.f && (std::abs(fb - io.DisplayFramebufferScale.x) > 0.01f || std::abs(fb - io.DisplayFramebufferScale.y) > 0.01f))
        io.DisplayFramebufferScale = ImVec2(fb, fb);
    }
  }
  ImGui::NewFrame();
  drawUi(ws_, ui_, env_);
  ui_.closePopups = false;  // the modal guards saw it for exactly one frame (v2.4 BUG-02)
  if (ui_.optionsToApply) {  // Options accepted (Phase 4, D-05): draw options re-render every open tab next frame,
    // computation options reach tabs opened afterwards; persist immediately (MFC parity: saved on Apply, D-03).
    options_ = *ui_.optionsToApply;
    ui_.optionsToApply.reset();
    ws_.setDefaults(options_.compute);
    env_.drawStyle = options_.draw;
    pacer_.notifyActivity();
    if (!optionsPath_.empty()) {
      std::string err;
      if (!efapp::saveOptions(optionsPath_, options_, err)) std::fprintf(stderr, "options: %s\n", err.c_str());
    }
  }
  exportStep();  // EXP-01/02: start the save dialog / run a chosen export on the UI thread between frames
  if (ui_.saveAsApply) {  // v2 CFG-03 / v2.4 COL: Save Configuration As accepted (the modal pre-validated;
    // overwriting an existing name was confirmed in the modal, and saveConfigurationAs upserts in place)
    const std::string err = saveConfigurationAs(ui_.saveAsApply->name);
    if (!err.empty()) std::fprintf(stderr, "configurations: %s\n", err.c_str());
    ui_.saveAsApply.reset();
    pacer_.notifyActivity();
  }
  if (ui_.saveCollectionRequest) {  // v2.4 COL-06
    ui_.saveCollectionRequest = false;
    std::string err = saveCollection();
    if (err.rfind("error: ", 0) == 0) err = err.substr(7);
    if (!err.empty()) exportFailed(err);
  }
  if (ui_.saveConfigurationRequest) {  // v2.5 CFG-04
    ui_.saveConfigurationRequest = false;
    std::string err = saveConfiguration();
    if (err.rfind("error: ", 0) == 0) err = err.substr(7);
    if (!err.empty()) exportFailed(err);
  }
  if (ui_.closeCollectionRequest) {  // v2.4 COL-08 (v2.5: leaves no collection open)
    ui_.closeCollectionRequest = false;
    const std::string err = closeCollection();
    if (!err.empty()) std::fprintf(stderr, "collection: %s\n", err.c_str());
  }
  // v2.2 COL-02/03: collection switching (gated by the unsaved-changes prompt) and Calculate All.
  if (ui_.leaveDecision != 0) {
    if (ui_.leaveDecision == 1) {
      leaveConfirmed_ = true;
      if (!pendingCollection_.empty()) {
        switchCollection(pendingCollection_);
        pendingCollection_.clear();
        leaveConfirmed_ = false;
        if (ui_.openChooserAfterSwitch && ws_.tabs().empty()) ui_.openChooser = true;  // v2.5: then a configuration
        ui_.openChooserAfterSwitch = false;
      } else if (!pendingCollectionFile_.empty()) {
        const std::filesystem::path file = pendingCollectionFile_;
        pendingCollectionFile_.clear();
        leaveConfirmed_ = false;
        switchCollectionAt(file);
        if (ui_.openChooserAfterSwitch && ws_.tabs().empty()) ui_.openChooser = true;  // v2.5: then a configuration
        ui_.openChooserAfterSwitch = false;
      } else {
        quit_ = true;  // the next iterate() leaves the loop and the session is saved
        ui_.closePopups = true;  // the prompt closes with the answer (v2.4 BUG-02; the app may still render a frame)
      }
    } else {
      leaveConfirmed_ = false;
      pendingCollection_.clear();
      pendingCollectionFile_.clear();
    }
    ui_.leaveDecision = 0;
  }
  if (!ui_.switchToCollection.empty()) {
    if (ws_.anyDirty() && !leaveConfirmed_) {
      pendingCollection_ = ui_.switchToCollection;
      ui_.leaveText = "Some tabs have changes that are not saved to a collection.";
      ui_.leaveButton = "Switch anyway";
      ui_.openLeaveConfirm = true;
      leaveConfirmed_ = true;  // the prompt is up; do not re-open every frame
    } else {
      switchCollection(ui_.switchToCollection);
      leaveConfirmed_ = false;
      if (ui_.openChooserAfterSwitch && ws_.tabs().empty()) ui_.openChooser = true;  // v2.5: then a configuration
      ui_.openChooserAfterSwitch = false;
    }
    ui_.switchToCollection.clear();
  }
  if (ui_.calculateAll) {  // v2.2 COL-03: open every configuration of the current collection in tabs
    ui_.calculateAll = false;
    ws_.closeAll();
    for (const efapp::UserConfig& c : userConfigs_) ws_.openUser(c.name, c.charges);
    pacer_.notifyActivity();
  }
  if (!ui_.removeConfigApply.empty()) {  // v2.3 COL-05 / v2.4: removal through the shared action
    const std::string err = removeConfiguration(ui_.removeConfigApply);
    ui_.removeConfigApply.clear();
    if (!err.empty()) std::fprintf(stderr, "configurations: %s\n", err.c_str());
  }
  if (ui_.openConfigFolder) {  // v2.1 SES-03
    ui_.openConfigFolder = false;
    if (!configDir_.empty()) {
      std::string url = "file:///" + configDir_.generic_u8string();
      for (std::size_t sp = url.find(' '); sp != std::string::npos; sp = url.find(' '))  // spaces must be percent-encoded
        url.replace(sp, 1, "%20");
      if (!SDL_OpenURL(url.c_str())) SDL_SetClipboardText(configDir_.u8string().c_str());
    }
  }
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
  const bool modalBefore = ui_.chooserVisible || ui_.aboutVisible || ui_.optionsVisible;
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
  // v2.2: quitting with unsaved tab edits asks first (once; the prompt modal collects the answer).
  if ((quit_ || ui_.requestQuit) && ws_.anyDirty() && !leaveConfirmed_) {
    ui_.leaveText = "Some tabs have changes that are not saved to a collection. They are restored on the next start, but quitting now will not store them in any collection file.";
    ui_.leaveButton = "Quit anyway";
    ui_.openLeaveConfirm = true;
    quit_ = false;
    ui_.requestQuit = false;
    leaveConfirmed_ = true;
    pacer_.notifyActivity();
  }
  if (quit_) return false;
  if (ui_.requestQuit) return false;
  if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {  // sessions keep being polled, nothing is drawn
    ws_.pollAll();
    SDL_Delay(16);
    return true;
  }
  frame();
  if (ws_.opened() != openedBefore || computing != ws_.anyComputing()) pacer_.notifyActivity();  // tab opened / finished: show it
  if ((ui_.chooserVisible || ui_.aboutVisible || ui_.optionsVisible) != modalBefore) pacer_.notifyActivity(efapp::FramePacer::kModalFadeFrames);
  if (!vsync_) SDL_Delay(16);
  return !ui_.requestQuit;
}

int App::runInteractive() {
  // v2.5 OPEN-01: the app starts with no collection open (a collection is a document: File >
  // Open Collection..., Ctrl+O); the configuration menu entries grey out until one is open.
  pacer_.notifyActivity(efapp::FramePacer::kModalFadeFrames);
  while (iterate()) {
  }
  // SES-01: persist the open tabs (charges + per-tab method) for the next start.
  if (!sessionPath_.empty()) {
    std::vector<efapp::SavedTab> saved;
    for (const auto& tp : ws_.tabs()) {
      efapp::SavedTab s;
      s.title = tp->title;
      s.preset = tp->preset;
      s.source = tp->sourceConfig;  // h22: the tab's configuration provenance survives the restart
      s.method = std::string(ef::info(tp->params.method).name);
      s.charges.reserve(tp->field.charges.size());
      for (const ef::Charge& c : tp->field.charges) s.charges.push_back(ef::PresetCharge{c.charge, c.position.X, c.position.Y});
      saved.push_back(std::move(s));
    }
    std::string err;
    if (!efapp::saveSession(sessionPath_, saved, err)) std::fprintf(stderr, "session: %s\n", err.c_str());
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
  // The comparison below maps logical canvas coordinates 1:1 onto the read-back surface, which is only exact at pixel density 1
  // (always the case with the headless dummy video driver this check runs under).
  if (SDL_GetWindowPixelDensity(window_) != 1.f) {
    report = "pixel density " + std::to_string(static_cast<double>(SDL_GetWindowPixelDensity(window_))) + " != 1: the canvas comparison needs density 1";
    return false;
  }
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
  SDL_Surface* ref = efrender::renderSurface(t->scene, efapp::scaledStyle(options_.draw, k_), view, err);
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
  // 2) clipping backstop: no scene ink in the chrome right of the tab text (x >= 400 x k). The scan covers
  //    the TAB-BAR band only (the lower half of the strip above the canvas): since v2.2 the MENU bar hosts
  //    the collection switcher and Calculate All on the right, whose dark text is legitimate chrome.
  long bleed = 0;
  const int x0 = static_cast<int>(400.f * k_);
  for (int y = cy0 / 2; y < cy0; ++y) {
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
  // v2.2 COL: the chooser lists the CURRENT COLLECTION; headless runs have no collection files, so the
  // Original seed (the built-ins) is created in memory for the keyboard test below.
  for (const ef::Preset& p : ef::presets()) {
    efapp::UserConfig u;
    u.name = std::string(p.displayName);
    for (int k = 0; k < p.count; ++k) u.charges.push_back(p.charges[static_cast<std::size_t>(k)]);
    userConfigs_.push_back(std::move(u));
  }
  ui_.userConfigs = userConfigs_;

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

  // Options dialog (Phase 4): it opens, Esc closes it, an accepted apply reaches the app (new draw style,
  // new tabs pick up the new computation params). Headless runs have no options file, so nothing persists.
  ui_.openOptions = true;
  frames(3);
  if (!ui_.optionsVisible) return fail("Options did not open");
  chord(0, ImGuiKey_Escape);
  frames(2);
  if (ui_.optionsVisible) return fail("Esc did not close Options");
  {
    efapp::OptionsEdit e;
    e.reset(options_);
    e.lines = 32;
    e.electricThickness = 4;
    ui_.optionsToApply = e.apply();
    frames(2);
    if (options_.draw.electricThickness != 4.f) return fail("accepted options were not applied");
  }
  ui_.openPresetNow = 2;
  frames(2);
  if (ws_.tabs().empty() || ws_.tabs().back()->params.linesPerUnitCharge != 32)
    return fail("a new tab did not pick up the new computation options");
  ui_.closeSerial = lastSerial();
  frames(2);
  {
    efapp::OptionsEdit e;
    e.reset(efapp::Options{});  // defaults for the rest of the run
    ui_.optionsToApply = e.apply();
    frames(2);
    if (options_.draw.electricThickness != 1.f) return fail("options were not restored to the defaults");
  }

  // User configurations (v2): headless has no file, so the list is populated in memory; a user config and
  // the empty entry must open and compute like preset tabs.
  userConfigs_.push_back(efapp::UserConfig{"Smoke Pair", {{1, -2, 0}, {-1, 2, 0}}});
  ui_.userConfigs = userConfigs_;
  ui_.openUserConfig = 9;  // after the nine seeded built-ins
  frames(2);
  if (ws_.tabs().empty() || ws_.tabs().back()->title != "Smoke Pair 1") return fail("the user configuration did not open");
  if (!pumpUntilDone(lastSerial(), 60000)) return fail("the user configuration session did not finish");
  ui_.closeSerial = lastSerial();
  frames(2);
  ui_.openEmpty = true;
  frames(2);
  if (ws_.tabs().empty() || ws_.tabs().back()->title != "Empty Configuration 1") return fail("the empty configuration did not open");
  ui_.closeSerial = lastSerial();
  frames(2);

  // Charge editor (v2): the full loop through the injected input queue - select, drag, add, delete, calculate.
  ui_.openEmpty = true;
  frames(2);
  if (ws_.tabs().empty()) return fail("editor test: the empty configuration did not open");
  const unsigned es = lastSerial();
  frames(3);
  ImGuiIO& eio = ImGui::GetIO();
  const float cx = (ui_.canvasMin.x + ui_.canvasMax.x) * 0.5f, cy = (ui_.canvasMin.y + ui_.canvasMax.y) * 0.5f;
  eio.AddMousePosEvent(cx, cy);
  frames(2);
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMouseButtonEvent(0, false);
  frames(2);
  if (ws_.find(es)->selected != 0) return fail("editor: clicking the origin charge did not select it");
  // Wheel zoom at the cursor (v2.2): zoom in, then out.
  eio.AddMouseWheelEvent(0.f, 1.f);
  frames(2);
  if (!(ws_.find(es)->zoom > 1.0)) return fail("editor: the mouse wheel did not zoom in");
  eio.AddMouseWheelEvent(0.f, -1.f);
  frames(2);
  if (ws_.find(es)->zoom < 0.99 || ws_.find(es)->zoom > 1.01) return fail("editor: the mouse wheel did not zoom back out");
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMousePosEvent(cx + 60.f, cy);
  frames(2);
  eio.AddMouseButtonEvent(0, false);
  frames(2);
  if (ws_.find(es)->field.charges[0].position.X < 0.4) return fail("editor: dragging did not move the charge");
  eio.AddMousePosEvent(cx + 150.f, cy - 80.f);
  frames(2);
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMouseButtonEvent(0, false);
  frames(2);
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMouseButtonEvent(0, false);
  frames(3);
  if (ws_.find(es)->field.charges.size() != 2)
    return fail("editor: double-click did not add a charge (count=" + std::to_string(ws_.find(es)->field.charges.size()) +
                " canvas=(" + std::to_string(ui_.canvasMin.x) + "," + std::to_string(ui_.canvasMin.y) + ")-(" +
                std::to_string(ui_.canvasMax.x) + "," + std::to_string(ui_.canvasMax.y) + ") click=(" + std::to_string(cx + 150.f) + "," +
                std::to_string(cy - 80.f) + ") selected=" + std::to_string(ws_.find(es)->selected) + ")");
  chord(0, ImGuiKey_Delete);
  if (ws_.find(es)->field.charges.size() != 1) return fail("editor: Delete did not remove the added charge");
  if (!ws_.find(es)->stale) return fail("editor: edits did not mark the tab stale");
  chord(0, ImGuiKey_F5);
  if (!pumpUntilDone(es, 60000)) return fail("editor: calculate did not finish");
  if (ws_.find(es)->state != efapp::TabState::Done || ws_.find(es)->scene.electric.empty() || ws_.find(es)->stale)
    return fail("editor: calculate did not produce fresh lines");
  ws_.rename(es, "Renamed");
  frames(2);
  if (ws_.find(es)->title != "Renamed") return fail("editor: rename did not set the title");
  if (ui_.wantedTitle != "Electric Field \xE2\x80\x94 Original \xE2\x80\x94 Renamed") return fail("editor: the title does not show the collection");

  // v2.3 CALC-04 regression: with continuous calculation on, a canvas edit recomputes on its own (the
  // debounce deadline must not be re-armed by idle frames).
  ws_.find(es)->autoRecalc = true;
  eio.AddMousePosEvent(cx + 60.f, cy);
  frames(2);
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMouseButtonEvent(0, false);
  frames(2);
  if (ws_.find(es)->selected != 0) return fail("editor: re-selecting the charge failed");
  eio.AddMouseButtonEvent(0, true);
  frames(1);
  eio.AddMousePosEvent(cx + 90.f, cy);
  frames(2);
  eio.AddMouseButtonEvent(0, false);
  frames(1);
  for (int i = 0; i < 60; ++i) {  // let time pass: the debounce fires, then the recompute runs
    frames(1);
    if (ws_.find(es)->stale == false && ws_.find(es)->state == efapp::TabState::Done) break;
  }
  if (!pumpUntilDone(es, 60000)) return fail("editor: continuous calculation did not finish");
  {
    const efapp::Tab* dt = ws_.find(es);
    if (!dt) return fail("editor: the tab vanished");
    if (dt->stale || dt->state != efapp::TabState::Done)
      return fail("editor: continuous calculation did not recompute (stale=" + std::to_string(dt->stale) + " state=" +
                  std::to_string(static_cast<int>(dt->state)) + " auto=" + std::to_string(dt->autoRecalc) + " pending=" +
                  std::to_string(dt->pendingRecalcAt) + " t=" + std::to_string(ImGui::GetTime()) + " pos.X=" +
                  std::to_string(dt->field.charges[0].position.X) + ")");
  }
  ws_.find(es)->autoRecalc = false;

  // v2.4 BUG-02: the leave prompt must CLOSE after its answer (it used to stay open and lock the UI).
  ws_.find(es)->dirty = true;  // make quitting "unsaved"
  quit_ = true;                // iterate() opens the leave prompt instead of quitting
  iterate();
  if (!ui_.leaveConfirmVisible) return fail("the leave prompt did not open for a dirty tab");
  ui_.leaveDecision = 1;  // "Quit anyway": the pending quit proceeds and the prompt must close
  iterate();
  frames(2);  // the closePopups guard draws once
  if (ui_.leaveConfirmVisible || ui_.openLeaveConfirm) return fail("BUG-02: the leave prompt stayed open after its answer");
  quit_ = false;  // the prompt flow is done; the smoke continues
  leaveConfirmed_ = false;
  if (!ws_.find(es)) return fail("BUG-02: the tab vanished during the leave-prompt flow");
  ws_.find(es)->dirty = false;
  ui_.closeSerial = es;
  frames(2);

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
  report = "keys=ok middle_click=ok failed_tab=ok overlay=ok options=ok user_configs=ok editor=ok idle_frames=" + std::to_string(idleFrames) + "/1.3s quit_ms=" + std::to_string(quitMs);
  return true;
}

int App::runSmoke() {
  std::printf("video=%s renderer=%s presets=%zu\n", SDL_GetCurrentVideoDriver(), SDL_GetRendererName(renderer_), ef::presets().size());
  std::printf("about: %s\n", efapp::versionLine(EF_VERSION, EF_GIT_HASH).c_str());
  // Software frames of a finished scene take ~100 ms, so a key held for 3 injected frames outlasts ImGui key-repeat delay (0.275 s) and
  // Down moves the chooser row several times. Injected chords must be a single press: switch repeat off for the whole smoke run.
  ImGui::GetIO().KeyRepeatDelay = 1e9f;
  // Same idea for double-clicks (the editor's add-by-double-click): two injected clicks span several 100 ms frames, far beyond the
  // 0.3 s real window, so widen the window for the smoke run (the assert requires SingleClickDelay > DoubleClickTime).
  ImGui::GetIO().MouseDoubleClickTime = 10.0f;
  ImGui::GetIO().MouseSingleClickDelay = 11.0f;
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

// --- Export (Phase 4, EXP-01/EXP-02, D-06) ---------------------------------------------------------------

// Consumed every frame after drawUi: a menu request opens the native save dialog; a path chosen in an
// earlier frame is exported here — on the UI thread, between frames (renderPng's ImGui-context rule).
void App::exportStep() {
  // The five dialog kinds share this machinery; exportFormat_ says which dialog was started (and
  // therefore what to do with the chosen path below):
  //   1 PNG | 2 SVG | 3 PDF: the active tab           (GUI export, the on-screen canvas view)
  //   10 Open Collection...: a collection FILE        (v2.5: any path; the chooser list is gone)
  //   11 Save Collection As... / 12 New Collection...: a collection FILE
  //   13 Export Collection: one PDF, one page per configuration
  if (ui_.exportRequest != 0) {
    exportFormat_ = ui_.exportRequest;
    exportSerial_ = ui_.activeSerial;
    // The callback may run on any thread: everything crosses through the mutex fields below.
    const SDL_DialogFileCallback callback = [](void* userdata, const char* const* filelist, int) {
      auto* self = static_cast<App*>(userdata);
      std::string path, error;
      if (filelist == nullptr)
        error = SDL_GetError();  // the dialog itself failed (e.g. no portal/zenity on Linux)
      else if (filelist[0] != nullptr)
        path = filelist[0];  // an empty list is the user cancelling
      std::lock_guard<std::mutex> lock(self->exportMutex_);
      self->pendingExportPath_ = std::move(path);
      self->pendingExportError_ = std::move(error);
    };
    if (exportFormat_ >= 1 && exportFormat_ <= 3) {
      const SDL_DialogFileFilter filters[] = {{"PNG image", "png"}, {"SVG image", "svg"}, {"PDF document", "pdf"}};
      SDL_ShowSaveFileDialog(callback, this, window_, filters, 3, nullptr);
    } else if (exportFormat_ == 10) {
      const SDL_DialogFileFilter filters[] = {{"Collection files", "ini"}, {"All files", "*"}};
      SDL_ShowOpenFileDialog(callback, this, window_, filters, 2, collectionsDir_.string().c_str(), false);
    } else if (exportFormat_ == 13) {  // Export Collection: the whole collection as one PDF
      const std::filesystem::path def =
          collectionsDir_ / (currentCollection_.empty() ? std::string("collection") : currentCollection_ + ".pdf");
      const SDL_DialogFileFilter filters[] = {{"PDF document", "pdf"}};
      SDL_ShowSaveFileDialog(callback, this, window_, filters, 1, def.string().c_str());
    } else {  // 11 Save Collection As... / 12 New Collection...: a collection FILE (.ini)
      const std::filesystem::path def =
          exportFormat_ == 12 ? collectionsDir_ / "New Collection.ini" : currentCollectionPath_;
      const SDL_DialogFileFilter filters[] = {{"Collection files", "ini"}};
      SDL_ShowSaveFileDialog(callback, this, window_, filters, 1, def.string().c_str());
    }
    ui_.exportRequest = 0;
    pacer_.notifyActivity();  // an OS dialog sits on top: keep the loop rendering
  }

  std::string path, error;
  {
    std::lock_guard<std::mutex> lock(exportMutex_);
    path = std::move(pendingExportPath_);
    error = std::move(pendingExportError_);
    pendingExportPath_.clear();
    pendingExportError_.clear();
  }
  if (!error.empty()) exportFailed("The file dialog failed: " + error + ".");
  if (path.empty()) return;

  switch (exportFormat_) {
    case 1:
    case 2:
    case 3:
      runExport(path);
      break;
    case 10: {  // v2.5: open the chosen file (the current collection is switched away, prompt first)
      if (ws_.anyDirty() && !leaveConfirmed_) {
        pendingCollectionFile_ = std::filesystem::u8path(path);
        ui_.openChooserAfterSwitch = true;  // then a configuration (honoured once the switch happens)
        ui_.leaveText = "Some tabs have changes that are not saved to a collection.";
        ui_.leaveButton = "Switch anyway";
        ui_.openLeaveConfirm = true;
        leaveConfirmed_ = true;  // the prompt is up; do not re-fire every frame
      } else {
        switchCollectionAt(std::filesystem::u8path(path));
        if (ws_.tabs().empty()) ui_.openChooser = true;  // then a configuration (v2.5 OPEN-01 flow)
      }
      break;
    }
    case 11: {
      std::string e = saveCollectionAsFile(std::filesystem::u8path(path));
      if (e.rfind("error: ", 0) == 0) e = e.substr(7);
      if (!e.empty()) exportFailed(e);
      break;
    }
    case 12: {  // v2.5: a new collection is an empty file at the chosen path
      createCollectionAt(std::filesystem::u8path(path));
      if (ws_.tabs().empty()) ui_.openChooser = true;  // then a configuration (v2.5 OPEN-01 flow)
      break;
    }
    case 13: {
      std::string e = exportCollectionTo(path);
      if (e.rfind("error: ", 0) == 0) e = e.substr(7);
      if (!e.empty()) exportFailed(e);
      break;
    }
    default:
      break;
  }
}

// The export equals the tab: the cached scene, the current draw options and exactly the on-screen canvas view.
void App::runExport(const std::string& path) {
  const efapp::Tab* t = ws_.find(exportSerial_);
  if (!t || t->state != efapp::TabState::Done) {
    exportFailed("The configuration is not finished.");  // defensive: the menu only enables a finished tab
    return;
  }
  const ef::View view = efapp::canvasView(ui_.canvasMax.x - ui_.canvasMin.x, ui_.canvasMax.y - ui_.canvasMin.y, k_);
  const ef::DrawStyle style = efapp::scaledStyle(options_.draw, k_);
  std::string err;
  if (exportFormat_ == 1)
    err = efrender::renderPng(t->scene, style, view, path.c_str());
  else if (exportFormat_ == 3)
    err = writeBinaryFile(path, ef::renderPdf(t->scene, style, view));  // v2.5 EXP-03
  else
    err = writeTextFile(path, ef::renderSvg(t->scene, style, view));
  if (!err.empty()) exportFailed(err);
}

// --- v2.5 EXP-03: deterministic exports through the command API / script runner --------------------------

namespace {

// A view that fits a charge arrangement into a fixed canvas, centred on the charges: bounded, so the
// same collection produces the same pages anywhere (the script exports must be deterministic).
ef::View fittedView(const std::vector<ef::SceneCharge>& charges, int widthPx, int heightPx) {
  double minX = 0, maxX = 0, minY = 0, maxY = 0;
  bool any = false;
  for (const ef::SceneCharge& c : charges) {
    if (!any) {
      minX = maxX = c.position.X;
      minY = maxY = c.position.Y;
      any = true;
    } else {
      minX = std::min(minX, c.position.X);
      maxX = std::max(maxX, c.position.X);
      minY = std::min(minY, c.position.Y);
      maxY = std::max(maxY, c.position.Y);
    }
  }
  const double spanX = std::max(maxX - minX, 3.0);  // a single charge keeps a sensible minimum span
  const double spanY = std::max(maxY - minY, 3.0);
  const double margin = 64.0;                       // device px
  const double ppu = std::clamp(std::min((widthPx - 2 * margin) / spanX, (heightPx - 2 * margin) / spanY), 5.0, 100.0);
  ef::View view{widthPx, heightPx, ppu};
  view.centre = ef::Vec2{(minX + maxX) / 2.0, (minY + maxY) / 2.0};
  return view;
}

}  // namespace

std::string App::exportConfigurationTo(const std::string& path) {
  const efapp::Tab* t = ws_.find(ui_.activeSerial);
  if (!t) return "error: no active tab";
  if (t->state != efapp::TabState::Done) return "error: the active configuration is still computing";
  const ef::DrawStyle style = options_.draw;
  return writeBinaryFile(path, ef::renderPdf(t->scene, style, fittedView(t->scene.charges, 1024, 768)));
}

// One PDF with one page per configuration (the whole collection, EXP-03). Each page is computed
// synchronously with the current options - a script/quick action, not the frame loop.
std::string App::exportCollectionTo(const std::string& path) {
  if (currentCollectionPath_.empty()) return "error: no collection open";
  if (userConfigs_.empty()) return "error: the collection has no configurations";
  const ef::DrawStyle style = options_.draw;
  ef::PdfPainter painter;
  for (const efapp::UserConfig& c : userConfigs_) {
    ef::ChargeSystem field;
    field.charges.reserve(c.charges.size());
    for (const ef::PresetCharge& ch : c.charges) field.charges.push_back(ef::Charge{ch.charge, ef::Vec2{ch.x, ch.y}});
    ef::ComputeSession session(field, options_.compute);
    if (session.wait().failed) return "error: the computation of '" + c.name + "' failed";
    ef::FieldResult result;
    if (!session.takeResult(result)) return "error: no result for '" + c.name + "'";
    const ef::FieldScene scene = ef::buildScene(field, result);
    ef::renderScene(painter, scene, style, fittedView(scene.charges, 1024, 768));  // one renderScene = one page
    painter.setTitle(c.name);  // the page rendered above (renderScene owns begin/end)
  }
  return writeBinaryFile(path, painter.build());
}

void App::exportFailed(const std::string& what) {
  if (opt_.headless()) {
    std::fprintf(stderr, "export: %s\n", what.c_str());
    return;
  }
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Electric Field", what.c_str(), window_);
}

// --- Collections (v2.2, COL-01..04) ----------------------------------------------------------------------

// One file per collection under Collections\; characters that are illegal in file names become '_'.
std::filesystem::path App::collectionFile(const std::string& name) const {
  std::string safe;
  for (const char c : name) {
    const bool bad = c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
    safe += bad ? '_' : c;
  }
  if (efapp::ini::trim(safe).empty()) safe = "Collection";
  // A name that already carries the extension is kept verbatim (API save-as with "Sandbox.ini").
  if (safe.size() >= 4 && efapp::ini::toLower(safe.substr(safe.size() - 4)) == ".ini") return collectionsDir_ / safe;
  return collectionsDir_ / (safe + ".ini");
}

void App::refreshCollections() {
  collections_.clear();
  std::error_code ec;
  for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(collectionsDir_, ec)) {
    if (e.is_regular_file() && e.path().extension() == ".ini") collections_.push_back(e.path().stem().string());
  }
  std::sort(collections_.begin(), collections_.end());
  // Original first (the seeded collection), then everything else alphabetically.
  for (std::size_t i = 0; i < collections_.size(); ++i)
    if (collections_[i] == "Original") {
      std::swap(collections_[0], collections_[i]);
      break;
    }
  if (!currentCollection_.empty() && std::find(collections_.begin(), collections_.end(), currentCollection_) == collections_.end())
    collections_.push_back(currentCollection_);
}

void App::writeSessionPointer() {
  if (sessionPath_.empty() || currentCollection_.empty()) return;
  std::string err;
  if (!efapp::saveSession(sessionPath_, {}, currentCollection_, err)) std::fprintf(stderr, "session: %s\n", err.c_str());
}

void App::enterCollection(const std::string& name, const bool restoreTabs) {
  enterCollectionAt(collectionFile(name), restoreTabs);
}

// v2.5 OPEN-01: the path-based core. `file` may live anywhere; the collection's display name and the
// session key are the file's stem.
void App::enterCollectionAt(const std::filesystem::path& file, const bool restoreTabs) {
  currentCollection_ = file.stem().string();
  currentCollectionPath_ = file;
  userConfigs_ = efapp::loadConfigs(file);
  ui_.userConfigs = userConfigs_;
  if (restoreTabs) {
    const efapp::SavedSession ses = efapp::loadSessionFull(sessionsDir_ / (currentCollection_ + ".ini"));
    for (const efapp::SavedTab& s : ses.tabs) {
      ef::ComputeParams params = options_.compute;
      if (const std::optional<ef::Method> m = ef::methodFromName(s.method)) params.method = *m;
      const unsigned serial = ws_.openRestored(s.title, s.preset, params, s.charges, s.autoRecalc, s.grid, s.source);
      ui_.activeSerial = ui_.selectSerial = serial;  // the newest restored tab is active (TAB/commands resolve "active")
    }
    pacer_.notifyActivity();
  }
  writeSessionPointer();
  ui_.closePopups = true;  // a collection change invalidates every open modal (v2.4 BUG-02)
}

void App::switchCollection(const std::string& name) {
  switchCollectionAt(collectionFile(name));
}

void App::switchCollectionAt(const std::filesystem::path& file) {
  if (file == currentCollectionPath_) return;
  saveSessionNow();  // the tabs of the collection being left -> Sessions/<old>.ini
  ws_.closeAll();
  enterCollectionAt(file, true);  // the target's tabs come back
  refreshCollections();
}

void App::createCollection(const std::string& name) {
  createCollectionAt(collectionFile(name));
}

void App::createCollectionAt(const std::filesystem::path& file) {
  std::string err;
  efapp::saveConfigs(file, {}, err);  // empty collection
  saveSessionNow();  // the tabs of the collection being left
  ws_.closeAll();    // a new collection starts with no tabs and no configurations
  enterCollectionAt(file, false);
  refreshCollections();
}

void App::saveSessionNow() {
  if (sessionsDir_.empty() || currentCollection_.empty()) return;
  std::vector<efapp::SavedTab> saved;
  for (const auto& tp : ws_.tabs()) {
    efapp::SavedTab s;
    s.title = tp->title;
    s.preset = tp->preset;
    s.source = tp->sourceConfig;  // h22: the tab's configuration provenance survives the restart
    s.method = std::string(ef::info(tp->params.method).name);
    s.autoRecalc = tp->autoRecalc;
    s.grid = tp->grid;
    s.charges.reserve(tp->field.charges.size());
    for (const ef::Charge& c : tp->field.charges) s.charges.push_back(ef::PresetCharge{c.charge, c.position.X, c.position.Y});
    saved.push_back(std::move(s));
  }
  std::string err;
  if (!efapp::saveSession(sessionsDir_ / (currentCollection_ + ".ini"), saved, currentCollection_, err)) std::fprintf(stderr, "session: %s\n", err.c_str());
}

// --- Collection-level actions (v2.4 COL-06..08, API-01) --------------------------------------------------

// Open tabs whose edits are not stored yet get merged into matching configurations (the tab title's
// base name = the configuration name); the merged tabs are marked clean.
void App::mergeDirtyTabsInto(std::vector<efapp::UserConfig>& configs) {
  for (const auto& tp : ws_.tabs()) {
    efapp::Tab& t = *tp;
    if (!t.dirty) continue;
    const std::string name = efapp::baseTitle(t.title);
    const int at = efapp::findUserConfig(configs, name);
    if (at < 0) continue;
    efapp::UserConfig& c = configs[static_cast<std::size_t>(at)];
    c.charges.clear();
    c.charges.reserve(t.field.charges.size());
    for (const ef::Charge& ch : t.field.charges) c.charges.push_back(ef::PresetCharge{ch.charge, ch.position.X, ch.position.Y});
    t.dirty = false;
  }
}

std::string App::saveCollection() {
  if (currentCollectionPath_.empty()) return "error: no collection open";
  mergeDirtyTabsInto(userConfigs_);
  std::string err;
  if (!efapp::saveConfigs(currentCollectionPath_, userConfigs_, err)) return "error: " + err;
  return {};
}

// API-compatible form: a bare name resolves to Collections\<name>.ini (v2.5: anything else is a path).
std::string App::saveCollectionAs(const std::string& name) {
  const std::string trimmed = efapp::ini::trim(name);
  if (trimmed.empty()) return "error: the collection name is empty";
  return saveCollectionAsFile(collectionFile(trimmed));
}

// v2.5 OPEN-01: Save Collection As with a path chosen in the native save dialog (or given by the API).
// The tabs stay open and belong to the new file now.
std::string App::saveCollectionAsFile(const std::filesystem::path& file) {
  if (currentCollectionPath_.empty()) return "error: no collection open";
  mergeDirtyTabsInto(userConfigs_);
  std::string err;
  if (!efapp::saveConfigs(file, userConfigs_, err)) return "error: " + err;
  currentCollection_ = file.stem().string();  // the tabs stay open; they belong to the new file now
  currentCollectionPath_ = file;
  refreshCollections();
  saveSessionNow();    // Sessions/<new>.ini = the open tabs
  writeSessionPointer();
  return {};
}

// v2.5: Close Collection leaves NO collection open (the configuration menu entries grey out until
// File > Open Collection... picks one). The closing collection's tabs stay in its session file.
std::string App::closeCollection() {
  if (currentCollectionPath_.empty()) return "error: no collection open";
  const std::string err = saveCollection();
  saveSessionNow();  // the closing collection keeps its tabs for the next open
  ws_.closeAll();
  currentCollection_.clear();
  currentCollectionPath_.clear();
  userConfigs_.clear();
  ui_.userConfigs.clear();
  refreshCollections();
  pacer_.notifyActivity();
  return err;
}

std::string App::removeConfiguration(const std::string& name) {
  if (currentCollectionPath_.empty()) return "error: no collection open";
  const int at = efapp::findUserConfig(userConfigs_, name);
  if (at < 0) return "error: no configuration named '" + name + "'";
  userConfigs_.erase(userConfigs_.begin() + static_cast<std::ptrdiff_t>(at));
  std::string err;
  if (!efapp::saveConfigs(currentCollectionPath_, userConfigs_, err)) return "error: " + err;
  ui_.userConfigs = userConfigs_;
  return {};
}

std::string App::saveConfigurationAs(const std::string& name) {
  const efapp::Tab* t = ws_.find(ui_.activeSerial);
  if (!t) return "error: no active tab";
  if (currentCollectionPath_.empty()) return "error: no collection open";
  std::vector<std::string> reserved;
  for (const ef::Preset& p : ef::presets()) reserved.emplace_back(p.displayName);
  efapp::UserConfig candidate;
  candidate.name = name;
  candidate.charges.reserve(t->field.charges.size());
  for (const ef::Charge& c : t->field.charges) candidate.charges.push_back(ef::PresetCharge{c.charge, c.position.X, c.position.Y});
  if (efapp::upsertUserConfig(userConfigs_, candidate, reserved) != efapp::UpsertResult::OK) return "error: invalid or reserved name";
  std::string err;
  if (!efapp::saveConfigs(currentCollectionPath_, userConfigs_, err)) return "error: " + err;
  if (efapp::Tab* saved = ws_.find(ui_.activeSerial)) {
    saved->dirty = false;
    saved->sourceConfig = efapp::ini::trim(name);  // the tab now belongs to this configuration (v2.5 CFG-04)
    ws_.rename(ui_.activeSerial, name);            // h22: the tab takes the saved name too - its base title is the identity Save Configuration writes to
  }
  ui_.userConfigs = userConfigs_;
  refreshCollections();
  return {};
}

// v2.5 CFG-04 + h22: File > Save Configuration - the active tab's charges go to the tab's CURRENT name
// (the tab's name is the configuration's identity): the source entry is renamed in place (position and
// other entries kept) when it exists; preset-sourced or vanished sources append under the current name;
// a name owned by a DIFFERENT configuration is refused - Save Configuration As owns confirmed overwrites.
std::string App::saveConfiguration() {
  efapp::Tab* t = ws_.find(ui_.activeSerial);
  if (!t) return "error: no active tab";
  if (currentCollectionPath_.empty()) return "error: no collection open";
  if (t->sourceConfig.empty()) return "error: the active tab is not a saved configuration (use Save Configuration As)";
  std::vector<std::string> reserved;
  for (const ef::Preset& p : ef::presets()) reserved.emplace_back(p.displayName);
  efapp::UserConfig candidate;
  candidate.name = efapp::baseTitle(t->title);
  candidate.charges.reserve(t->field.charges.size());
  for (const ef::Charge& ch : t->field.charges) candidate.charges.push_back(ef::PresetCharge{ch.charge, ch.position.X, ch.position.Y});
  const efapp::SaveIntoResult r = efapp::saveIntoUserConfig(userConfigs_, t->sourceConfig, candidate, reserved);
  switch (r) {
    case efapp::SaveIntoResult::EMPTY_NAME: return "error: the configuration name is empty";
    case efapp::SaveIntoResult::RESERVED_NAME:
      return "error: '" + candidate.name + "' is a built-in preset and cannot be overwritten (use Save Configuration As)";
    case efapp::SaveIntoResult::NAME_TAKEN:
      return "error: a configuration named '" + candidate.name + "' already exists (use Save Configuration As to overwrite it)";
    case efapp::SaveIntoResult::OK: break;
  }
  std::string err;
  if (!efapp::saveConfigs(currentCollectionPath_, userConfigs_, err)) return "error: " + err;
  if (efapp::Tab* saved = ws_.find(ui_.activeSerial)) {
    saved->dirty = false;
    saved->sourceConfig = efapp::ini::trim(candidate.name);  // the tab now belongs to this configuration under its current name
  }
  ui_.userConfigs = userConfigs_;
  return {};
}

// --- The command API (v2.4 API-01/02) --------------------------------------------------------------------

namespace {

// Whitespace-separated tokens; double quotes group a token (configuration/collection names with spaces).
std::vector<std::string> apiTokens(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  bool inQuote = false, started = false;
  for (const char c : line) {
    if (inQuote) {
      if (c == '"')
        inQuote = false;
      else
        cur += c;
    } else if (c == '"') {
      inQuote = true;
      started = true;
    } else if (c == ' ' || c == '\t') {
      if (!cur.empty() || started) {
        out.push_back(std::move(cur));
        cur.clear();
        started = false;
      }
    } else {
      cur += c;
    }
  }
  if (!cur.empty() || started) out.push_back(std::move(cur));
  return out;
}

bool parseApiInt(const std::string& s, long long& out) {
  try {
    std::size_t pos = 0;
    out = std::stoll(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}

bool parseApiDouble(const std::string& s, double& out) {
  try {
    std::size_t pos = 0;
    out = std::stod(s, &pos);
    return pos == s.size() && std::isfinite(out);
  } catch (...) {
    return false;
  }
}

bool apiOnOff(const std::string& s, bool& out) {
  const std::string v = efapp::ini::toLower(s);
  if (v == "on" || v == "true" || v == "1") {
    out = true;
    return true;
  }
  if (v == "off" || v == "false" || v == "0") {
    out = false;
    return true;
  }
  return false;
}

}  // namespace

// The single dispatcher behind the script runner (and step by step the menus, v2.4 D-01). Returns "ok"
// or "error: <why>"; app.state returns a multi-line dump of the whole UI state.
std::string App::command(const std::string& line) {
  const std::vector<std::string> tok = apiTokens(line);
  if (tok.empty()) return "ok";  // blank lines are not errors
  const std::string& cmd = tok[0];
  std::string e;  // shared error slot for the helpers below
  auto need = [&](std::size_t n) -> std::string {
    if (tok.size() - 1 < n) return "error: " + cmd + " needs " + std::to_string(n) + " argument(s)";
    return {};
  };
  auto activeTab = [&]() -> efapp::Tab* {
    efapp::Tab* t = ws_.find(ui_.activeSerial);
    if (!t) e = "error: no active tab";
    return t;
  };
  // A typed position lands on the grid when snapping is active (GRID-04).
  auto typedWorld = [&](efapp::Tab& t, double x, double y) { return t.grid.active ? efapp::editor::snapPoint(t.grid, ef::Vec2{x, y}) : ef::Vec2{x, y}; };

  if (cmd == "app.state") {
    std::ostringstream os;
    os << "collection=" << currentCollection_ << '\n';
    os << "collections=";
    for (std::size_t i = 0; i < collections_.size(); ++i) os << (i ? "|" : "") << collections_[i];
    os << '\n';
    os << "configs=";
    for (std::size_t i = 0; i < userConfigs_.size(); ++i) os << (i ? "|" : "") << userConfigs_[i].name;
    os << '\n';
    const auto& tabs = ws_.tabs();
    os << "tabs=" << tabs.size() << '\n';
    for (const auto& tp : tabs) {
      const efapp::Tab& t = *tp;
      os << "tab serial=" << t.serial << " title=" << t.title << " state="
         << (t.state == efapp::TabState::Computing ? "computing" : t.state == efapp::TabState::Done ? "done" : "failed")
         << " method=" << ef::info(t.params.method).name << " autocalc=" << (t.autoRecalc ? 1 : 0)
         << " stale=" << (t.stale ? 1 : 0) << " dirty=" << (t.dirty ? 1 : 0) << " selected=" << t.selected << " lines=" << t.scene.electric.size()
         << " zoom=" << t.zoom << " centre=" << t.centre.X << "," << t.centre.Y << " grid=" << (t.grid.active ? 1 : 0) << ','
         << (t.grid.visible ? 1 : 0) << ',' << (t.grid.polar ? 1 : 0) << ',' << (t.grid.intersectionsOnly ? 1 : 0) << ',' << t.grid.origin.X << ',' << t.grid.origin.Y << ','
         << t.grid.spacing << ',' << t.grid.angleStepDeg << " charges=";
      for (std::size_t i = 0; i < t.field.charges.size(); ++i)
        os << (i ? ";" : "") << t.field.charges[i].charge << ',' << t.field.charges[i].position.X << ',' << t.field.charges[i].position.Y;
      os << '\n';
    }
    return os.str();
  }
  if (cmd == "app.quit") {
    if (tok.size() >= 2 && tok[1] == "force") {
      quit_ = true;
      return "ok";
    }
    if (ws_.anyDirty()) return "error: unsaved changes (use: app.quit force)";
    quit_ = true;
    return "ok";
  }
  if (cmd == "app.frames") {
    if (const std::string e = need(1); !e.empty()) return e;
    long long n = 0;
    if (!parseApiInt(tok[1], n) || n < 0 || n > 10000) return "error: invalid frame count";
    frames(static_cast<int>(n));
    return "ok";
  }
  if (cmd == "app.wait-done") {
    unsigned serial = ui_.activeSerial;
    unsigned timeoutMs = 60000;
    if (tok.size() >= 2 && tok[1] != "active") {
      long long s = 0;
      if (!parseApiInt(tok[1], s) || s <= 0) return "error: invalid serial";
      serial = static_cast<unsigned>(s);
    }
    if (tok.size() >= 3) {
      long long ms = 0;
      if (!parseApiInt(tok[2], ms) || ms <= 0) return "error: invalid timeout";
      timeoutMs = static_cast<unsigned>(ms);
    }
    if (!pumpUntilDone(serial, timeoutMs)) return "error: the tab did not finish in " + std::to_string(timeoutMs) + " ms";
    return "ok";
  }
  if (cmd == "app.open-config-folder") return "ok";  // a real desktop action; headless scripts record the intent

  if (cmd == "collection.new") {
    if (const std::string e = need(1); !e.empty()) return e;
    const std::string name = efapp::ini::trim(tok[1]);
    if (name.empty()) return "error: the collection name is empty";
    createCollection(name);
    return "ok";
  }
  if (cmd == "collection.switch" || cmd == "collection.open") {  // v2.5 OPEN-01: a bare name resolves to
    // Collections\<name>.ini; anything containing a separator or ':' is a path (opened from anywhere).
    if (const std::string e = need(1); !e.empty()) return e;
    const std::string arg = efapp::ini::trim(tok[1]);
    std::filesystem::path file;
    if (arg.find('/') == std::string::npos && arg.find('\\') == std::string::npos && arg.find(':') == std::string::npos) {
      if (std::find(collections_.begin(), collections_.end(), arg) != collections_.end() || std::filesystem::exists(collectionFile(arg)))
        file = collectionFile(arg);
    } else {
      file = std::filesystem::u8path(arg);
      if (file.is_relative() && !opt_.userDir.empty()) file = std::filesystem::u8path(opt_.userDir) / file;
    }
    if (file.empty() || !std::filesystem::exists(file)) return "error: no collection '" + arg + "'";
    switchCollectionAt(file);
    return "ok";
  }
  if (cmd == "collection.save") {
    const std::string err = saveCollection();
    return err.empty() ? "ok" : err;
  }
  if (cmd == "collection.save-as") {  // v2.5: a bare name (Collections\<name>.ini) or a path
    if (const std::string e = need(1); !e.empty()) return e;
    const std::string arg = efapp::ini::trim(tok[1]);
    if (arg.find('/') == std::string::npos && arg.find('\\') == std::string::npos && arg.find(':') == std::string::npos)
      return saveCollectionAs(arg);
    std::filesystem::path file = std::filesystem::u8path(arg);
    if (file.is_relative() && !opt_.userDir.empty()) file = std::filesystem::u8path(opt_.userDir) / file;
    return saveCollectionAsFile(file);
  }
  if (cmd == "collection.close") {
    const std::string err = closeCollection();
    return err.empty() ? "ok" : err;
  }

  if (cmd == "config.open") {
    if (const std::string e = need(1); !e.empty()) return e;
    const int at = efapp::findUserConfig(userConfigs_, tok[1]);
    if (at < 0) return "error: no configuration named '" + tok[1] + "'";
    const efapp::UserConfig& c = userConfigs_[static_cast<std::size_t>(at)];
    ui_.selectSerial = ws_.openUser(c.name, c.charges);
    ui_.activeSerial = ui_.selectSerial;
    pacer_.notifyActivity();
    return "ok";
  }
  if (cmd == "config.remove") {
    if (const std::string e = need(1); !e.empty()) return e;
    return removeConfiguration(tok[1]);
  }
  if (cmd == "config.save") {  // v2.5 CFG-04: the active tab's charges -> its source configuration, in place
    return saveConfiguration();
  }
  if (cmd == "config.save-as") {
    if (const std::string e = need(1); !e.empty()) return e;
    return saveConfigurationAs(tok[1]);
  }
  if (cmd == "export.configuration" || cmd == "export.collection") {  // v2.5 EXP-03: PDF by path
    if (const std::string e = need(1); !e.empty()) return e;
    std::filesystem::path file = std::filesystem::u8path(efapp::ini::trim(tok[1]));
    if (file.is_relative() && !opt_.userDir.empty()) file = std::filesystem::u8path(opt_.userDir) / file;
    const std::string err =
        cmd == "export.configuration" ? exportConfigurationTo(file.string()) : exportCollectionTo(file.string());
    return err.empty() ? "ok" : err;
  }

  if (cmd == "tab.new-empty") {
    if (currentCollectionPath_.empty()) return "error: no collection open";  // mirrors the greyed menu (v2.5 CFG-04)
    ui_.selectSerial = ws_.openEmpty();
    ui_.activeSerial = ui_.selectSerial;
    pacer_.notifyActivity();
    return "ok";
  }
  if (cmd == "tab.close") {
    unsigned serial = ui_.activeSerial;
    if (tok.size() >= 2 && tok[1] != "active") {
      long long s = 0;
      if (!parseApiInt(tok[1], s) || s <= 0) return "error: invalid serial";
      serial = static_cast<unsigned>(s);
    }
    if (serial == 0) return "error: no active tab";
    if (!ws_.close(serial)) return "error: no tab with serial " + std::to_string(serial);
    if (ui_.activeSerial == serial) ui_.activeSerial = ws_.tabs().empty() ? 0 : ws_.tabs().back()->serial;
    return "ok";
  }
  if (cmd == "tab.select") {
    if (const std::string e = need(1); !e.empty()) return e;
    long long s = 0;
    if (!parseApiInt(tok[1], s) || s <= 0) return "error: invalid serial";
    if (!ws_.find(static_cast<unsigned>(s))) return "error: no tab with serial " + tok[1];
    ui_.selectSerial = ui_.activeSerial = static_cast<unsigned>(s);
    return "ok";
  }
  if (cmd == "tab.rename") {
    if (const std::string e = need(1); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    if (!ws_.rename(ui_.activeSerial, tok[1])) return "error: the name is empty";
    return "ok";
  }
  if (cmd == "tab.calculate" || cmd == "tab.calculate-all") {
    if (cmd == "tab.calculate-all") {
      ws_.closeAll();
      for (const efapp::UserConfig& c : userConfigs_) ws_.openUser(c.name, c.charges);
      pacer_.notifyActivity();
      return "ok";
    }
    unsigned serial = ui_.activeSerial;
    if (tok.size() >= 2 && tok[1] != "active") {
      long long s = 0;
      if (!parseApiInt(tok[1], s) || s <= 0) return "error: invalid serial";
      serial = static_cast<unsigned>(s);
    }
    if (serial == 0) return "error: no active tab";
    if (!ws_.calculate(serial)) return "error: the tab is computing or unknown";
    return "ok";
  }
  if (cmd == "tab.esc") {
    efapp::Tab* t = activeTab();
    if (!t) return e;
    if (!efapp::editor::revertToComputed(*t)) return "error: nothing to revert (the tab is not stale)";
    return "ok";
  }
  if (cmd == "editor.undo" || cmd == "editor.redo") {
    efapp::Tab* t = activeTab();
    if (!t) return e;
    if (cmd == "editor.undo" ? !efapp::editor::undo(*t) : !efapp::editor::redo(*t)) return "error: nothing to " + (cmd == "editor.undo" ? std::string("undo") : std::string("redo"));
    return "ok";
  }

  if (cmd == "charge.add") {
    if (const std::string e = need(3); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    long long v = 0;
    double x = 0, y = 0;
    if (!parseApiInt(tok[1], v) || !parseApiDouble(tok[2], x) || !parseApiDouble(tok[3], y)) return "error: charge.add needs <value> <x> <y>";
    if (!efapp::editor::addCharge(*t, typedWorld(*t, x, y), static_cast<int>(v))) return "error: the tab is computing";
    return "ok";
  }
  auto selectedOrArg = [&](std::string& err) -> int {
    efapp::Tab* t = activeTab();
    if (!t) {
      err = e;
      return -1;
    }
    if (tok.size() >= 2) {
      long long i = 0;
      if (!parseApiInt(tok[1], i) || i < 0) {
        err = "error: invalid charge index";
        return -1;
      }
      return static_cast<int>(i);
    }
    if (t->selected < 0) {
      err = "error: no charge selected";
      return -1;
    }
    return t->selected;
  };
  if (cmd == "charge.select") {
    if (const std::string e = need(1); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    long long i = 0;
    if (!parseApiInt(tok[1], i) || i < 0 || static_cast<std::size_t>(i) >= t->field.charges.size()) return "error: invalid charge index";
    t->selected = static_cast<int>(i);
    return "ok";
  }
  if (cmd == "charge.move") {
    if (const std::string e = need(3); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    long long i = 0;
    double x = 0, y = 0;
    if (!parseApiInt(tok[1], i) || i < 0 || static_cast<std::size_t>(i) >= t->field.charges.size()) return "error: invalid charge index";
    if (!parseApiDouble(tok[2], x) || !parseApiDouble(tok[3], y)) return "error: charge.move needs <index> <x> <y>";
    if (!efapp::editor::moveCharge(*t, static_cast<std::size_t>(i), typedWorld(*t, x, y))) return "error: the tab is computing";
    return "ok";
  }
  if (cmd == "charge.value") {
    if (const std::string e = need(2); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    const int at = selectedOrArg(e);
    if (at < 0) return e;
    t->selected = at;  // stepValue works on the selection; an explicit index selects it (v2.5: after removals)
    long long v = 0;
    if (!parseApiInt(tok[at == t->selected && tok.size() < 3 ? 1 : 2], v)) return "error: invalid value";
    const int delta = static_cast<int>(v) - t->field.charges[static_cast<std::size_t>(at)].charge;
    if (!efapp::editor::stepValue(*t, delta)) return "error: the tab is computing";
    return "ok";
  }
  if (cmd == "charge.sign") {
    efapp::Tab* t = activeTab();
    if (!t) return e;
    const int at = selectedOrArg(e);
    if (at < 0) return e;
    t->selected = at;
    if (!efapp::editor::flipSign(*t)) return "error: the tab is computing";
    return "ok";
  }
  if (cmd == "charge.remove") {
    efapp::Tab* t = activeTab();
    if (!t) return e;
    const int at = selectedOrArg(e);
    if (at < 0) return e;
    t->selected = at;
    if (!efapp::editor::removeSelected(*t)) return "error: cannot remove the last charge";
    return "ok";
  }

  if (cmd == "view.zoom") {
    if (const std::string e = need(1); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    double f = 0;
    if (!parseApiDouble(tok[1], f) || f <= 0) return "error: invalid zoom factor";
    efapp::editor::zoomAt(*t, f, t->centre);
    return "ok";
  }
  if (cmd == "view.pan") {
    if (const std::string e = need(2); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    double dx = 0, dy = 0;
    if (!parseApiDouble(tok[1], dx) || !parseApiDouble(tok[2], dy)) return "error: view.pan needs <dpx> <dpy>";
    const double ppu = 100.0 * static_cast<double>(efapp::clampScale(k_)) * t->zoom;
    efapp::editor::pan(*t, ef::Vec2{dx, dy}, ppu);
    return "ok";
  }
  if (cmd == "view.reset") {
    efapp::Tab* t = activeTab();
    if (!t) return e;
    efapp::editor::resetView(*t);
    return "ok";
  }

  if (cmd == "grid.set") {
    if (const std::string e = need(2); !e.empty()) return e;
    efapp::Tab* t = activeTab();
    if (!t) return e;
    efapp::GridSettings& g = t->grid;
    const std::string what = tok[1];
    if (what == "active" || what == "visible" || what == "polar" || what == "intersections") {  // v2.5: intersections-only mode
      bool v = false;
      if (tok.size() < 3 || !apiOnOff(tok[2], v)) return "error: grid.set " + what + " needs on|off";
      if (what == "active") g.active = v;
      if (what == "visible") g.visible = v;
      if (what == "polar") g.polar = v;
      if (what == "intersections") g.intersectionsOnly = v;
      return "ok";
    }
    if (what == "origin") {
      if (const std::string e2 = need(3); !e2.empty()) return e2;
      double x = 0, y = 0;
      if (!parseApiDouble(tok[2], x) || !parseApiDouble(tok[3], y)) return "error: grid.set origin needs <x> <y>";
      g.origin = ef::Vec2{x, y};
      return "ok";
    }
    if (what == "spacing") {
      if (const std::string e2 = need(2); !e2.empty()) return e2;
      double s = 0;
      if (!parseApiDouble(tok[2], s) || s <= 0) return "error: invalid spacing";
      g.spacing = s;
      return "ok";
    }
    if (what == "angle") {
      if (const std::string e2 = need(2); !e2.empty()) return e2;
      double a = 0;
      if (!parseApiDouble(tok[2], a) || a <= 0 || a > 180) return "error: invalid angle step";
      g.angleStepDeg = a;
      return "ok";
    }
    return "error: unknown grid property '" + what + "'";
  }

  if (cmd == "options.set") {
    if (const std::string e = need(2); !e.empty()) return e;
    const std::string key = tok[1];
    ef::ComputeParams& c = options_.compute;
    if (key == "lines") {
      long long v = 0;
      if (!parseApiInt(tok[2], v)) return "error: invalid value";
      c.linesPerUnitCharge = static_cast<unsigned>(v);
    } else if (key == "threads") {
      long long v = 0;
      if (!parseApiInt(tok[2], v)) return "error: invalid value";
      c.numThreads = static_cast<unsigned>(v);
    } else if (key == "interval") {
      double v = 0;
      if (!parseApiDouble(tok[2], v)) return "error: invalid value";
      c.potentialInterval = v;
    } else if (key == "equipotentials") {
      bool v = false;
      if (!apiOnOff(tok[2], v)) return "error: options.set equipotentials needs on|off";
      c.calculateEquipotentials = v;
    } else if (key == "method") {
      const std::optional<ef::Method> m = ef::methodFromName(efapp::ini::toLower(tok[2]));
      if (!m) return "error: unknown method '" + tok[2] + "'";
      c.method = *m;
    } else if (key == "charge-size") {  // v2.5 SIZE-01: the DRAWN charge radius, 2-120 px (physics unchanged)
      double v = 0;
      if (!parseApiDouble(tok[2], v) || v < 2.0 || v > 120.0) return "error: charge-size needs a value in 2..120 px";
      options_.draw.chargeRadiusPx = static_cast<float>(v);
      env_.drawStyle = options_.draw;
    } else {
      return "error: unknown option '" + key + "'";
    }
    efapp::Options o;
    o.compute = c;
    o.draw = options_.draw;
    const std::string verr = efapp::validate(o);
    if (!verr.empty()) return "error: " + verr;
    ws_.setDefaults(c);
    if (!optionsPath_.empty()) {
      std::string err;
      if (!efapp::saveOptions(optionsPath_, options_, err)) return "error: " + err;
    }
    return "ok";
  }

  if (cmd == "app.expect") {  // script assertion: the state dump must contain the text
    if (const std::string e2 = need(1); !e2.empty()) return e2;
    const std::string state = command("app.state");
    if (state.find(tok[1]) == std::string::npos) return "error: app.state does not contain '" + tok[1] + "'";
    return "ok";
  }
  if (cmd == "app.expect-no") {  // script assertion: the state dump must NOT contain the text
    if (const std::string e2 = need(1); !e2.empty()) return e2;
    const std::string state = command("app.state");
    if (state.find(tok[1]) != std::string::npos) return "error: app.state unexpectedly contains '" + tok[1] + "'";
    return "ok";
  }
  if (cmd == "app.expect-error") {  // script assertion: the command starting at tok[2] must fail
    if (const std::string e2 = need(2); !e2.empty()) return e2;
    std::string line2;
    for (std::size_t i = 2; i < tok.size(); ++i) {
      if (i > 2) line2 += ' ';
      line2 += tok[i];
    }
    const std::string result = command(line2);
    if (result.rfind("error:", 0) != 0) return "error: expected '" + tok[1] + "' to fail: '" + line2 + "' returned '" + result + "'";
    if (result.find(tok[1]) == std::string::npos) return "error: '" + line2 + "' failed differently: " + result;
    return "ok";
  }

  return "error: unknown command '" + cmd + "' (see docs/api.md)";
}

// --script <file> (v2.4 API-02): headless command runner. One result line per command ("[ok]" /
// "[error] ..."); app.state results are printed verbatim. Strict by default: the first error stops the
// run with exit 1; --lenient continues.
int App::runScript() {
  std::ifstream in(opt_.scriptFile);
  if (!in) {
    std::fprintf(stderr, "script: cannot open %s\n", opt_.scriptFile.c_str());
    return 2;
  }
  std::ofstream resultsFile;
  if (!opt_.resultsFile.empty()) {
    resultsFile.open(opt_.resultsFile);
    if (!resultsFile) {
      std::fprintf(stderr, "script: cannot open the results file %s\n", opt_.resultsFile.c_str());
      return 2;
    }
  }
  std::ostream& out = opt_.resultsFile.empty() ? static_cast<std::ostream&>(std::cout) : static_cast<std::ostream&>(resultsFile);
  std::printf("script sandbox userDir=%s collection=%s\n", opt_.userDir.c_str(), currentCollection_.c_str());
  out << "script sandbox userDir=" << opt_.userDir << " collection=" << currentCollection_ << '\n';
  out.flush();

  std::string lineRaw;
  bool failed = false;
  while (!failed && std::getline(in, lineRaw)) {
    while (!lineRaw.empty() && (lineRaw.back() == '\r' || lineRaw.back() == ' ' || lineRaw.back() == '\t')) lineRaw.pop_back();
    if (lineRaw.empty() || lineRaw[0] == '#') continue;
    const std::string result = command(lineRaw);
    if (result.rfind("error:", 0) == 0) {
      out << "[error] " << lineRaw << " -> " << result << '\n';
      out.flush();
      std::printf("[error] %s -> %s\n", lineRaw.c_str(), result.c_str());
      std::fflush(stdout);
      failed = true;
    } else {
      out << "[ok] " << lineRaw << '\n';
      if (result != "ok") out << result;  // app.state dumps its body
      out.flush();
    }
  }
  // Leave the workspace consistent. A clean quit (app.quit) keeps the tabs for the next start; an error
  // abort leaves nothing computing but preserves the session as it was.
  if (!quit_) ws_.closeAll();
  saveSessionNow();
  return failed ? 1 : 0;
}

// --export <dir>: the export path CI exercises on all three OSes (D-06). Fixed inputs — the Dipole preset with
// default options and the 1024x768 @ 100 px/unit view — make the PNG and SVG byte-identical to the ef-cli
// parity oracle for the same preset, which the CI step proves with cmp.
int App::runExportMode() {
  std::error_code ec;
  std::filesystem::create_directories(opt_.exportDir, ec);  // exists / creatable is a precondition of the writes below
  ui_.openPresetNow = 0;  // dipole
  frames(1);
  const unsigned serial = lastSerial();
  if (!pumpUntilDone(serial, 60000)) {
    std::fprintf(stderr, "export: the dipole session did not finish\n");
    return 1;
  }
  const efapp::Tab* t = ws_.find(serial);
  if (!t) {
    std::fprintf(stderr, "export: the dipole tab disappeared\n");
    return 1;
  }
  const ef::DrawStyle style;              // headless runs load no options file: the defaults are the oracle's style
  const ef::View view{1024, 768, 100.};
  const std::string base = opt_.exportDir + "/dipole";
  std::string err = efrender::renderPng(t->scene, style, view, (base + ".png").c_str());
  if (err.empty()) err = writeTextFile(base + ".svg", ef::renderSvg(t->scene, style, view));
  if (err.empty()) err = writeBinaryFile(base + ".pdf", ef::renderPdf(t->scene, style, view));  // v2.5 EXP-03
  if (!err.empty()) {
    std::fprintf(stderr, "export: %s\n", err.c_str());
    return 1;
  }
  std::printf("export ok png=%s.png svg=%s.svg pdf=%s.pdf scene lines=%zu equipotentials=%zu\n", base.c_str(), base.c_str(),
              base.c_str(), t->scene.electric.size(), t->scene.potential.size());
  ws_.closeAll();
  shutdown();
  return 0;
}

}  // namespace efgui
