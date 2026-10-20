#pragma once
// SDL3 + Dear ImGui application shell (Phase 3): window, renderer, loop, DPI, smoke / screenshot modes.
#include <SDL3/SDL.h>

#include <filesystem>
#include <mutex>
#include <string>

#include "app/options.hpp"
#include "app/ui_logic.hpp"
#include "app/workspace.hpp"
#include "gui/ui.hpp"
#include "imgui.h"

namespace efgui {

struct AppOptions {
  bool smoke = false;
  bool screenshot = false;   // --screenshot <slug> <png> [--size WxH] [--scale k] [--modal about|chooser]
  bool exportMode = false;   // --export <dir>: headless PNG + SVG export of the finished Dipole (EXP-01/02, CI)
  bool script = false;       // --script <file>: headless command API runner (v2.4 API-01/02)
  bool lenient = false;      // --lenient: keep executing after a failed command
  std::string slug, out;
  std::string exportDir;
  std::string scriptFile;    // the command file
  std::string resultsFile;   // optional --out: per-command results (default stdout)
  std::string userDir;       // --user-dir <dir>: redirect all user-file paths (options/Collections/sessions)
  std::string modal;         // open that modal before the screenshot is taken
  float scaleOverride = 0.f; // headless HiDPI check: forces k
  int width = 0, height = 0;
  bool headless() const { return smoke || screenshot || exportMode || script; }
  // Script mode sets its user-file paths up (sandbox by default); smoke/screenshot never touch them.
  bool usesUserDir() const { return !headless() || script; }
};

class App {
 public:
  App() = default;
  App(const App&) = delete;
  App& operator=(const App&) = delete;
  ~App() { shutdown(); }

  // On failure `step` is one of "SDL initialisation", "Window creation", "Renderer creation", "Dear ImGui initialisation";
  // everything created so far has been released again.
  bool init(const AppOptions& opt, std::string& step, std::string& message);
  int runInteractive();
  int runSmoke();
  int runScreenshot();
  int runExportMode();
  int runScript();
  // The command API (v2.4): every menu action and editor/collection operation, callable by name.
  std::string command(const std::string& line);
  void shutdown();  // idempotent: cancel sessions, then ImGui backends, context, renderer, window, SDL

 private:
  bool iterate();                     // one pass of the main loop (events -> frame); false = quit requested
  void frame();                       // poll sessions, NewFrame, drawUi, Render, present
  void persistCoupleValueSize();      // 261020-ef2: save options when the panel's Couple toggle flipped
  void handleEvent(const SDL_Event& e);
  void refreshScale(bool initial = false);  // initial: clamp the minimum window size to the creation size (small --size / small displays)
  unsigned lastSerial() const;  // serial of the newest tab, 0 if there is none
  bool pumpUntilDone(unsigned serial, unsigned timeoutMs);
  void frames(int n);
  void chord(ImGuiKeyChord mods, ImGuiKey key);  // inject a key chord through ImGui's input queue, with the frames it needs
  bool checkCanvas(unsigned serial, int w, int h, std::string& report);
  bool smokeExtras(std::string& report);
  // Export (Phase 4, D-06): menu request -> native save dialog; the chosen path is exported on the UI
  // thread at a frame boundary (renderPng's ImGui-context rule).
  void exportStep();
  void runExport(const std::string& path);
  void exportFailed(const std::string& what);
  std::filesystem::path collectionFile(const std::string& name) const;  void refreshCollections();
  void switchCollection(const std::string& name);
  void createCollection(const std::string& name);
  void saveSessionNow();
  void writeSessionPointer();
  // The common body of switching/closing/starting: become `name`'s collection (load its configurations)
  // and, when restoreTabs, reopen its saved tabs from Sessions/<name>.ini.
  void enterCollection(const std::string& name, bool restoreTabs);
  // v2.5 OPEN-01: a collection is a document file - anything under Collections\ OR any path a file
  // dialog picked. The path-based core; the name-based calls above resolve through collectionFile().
  void enterCollectionAt(const std::filesystem::path& file, bool restoreTabs);
  void switchCollectionAt(const std::filesystem::path& file);
  void createCollectionAt(const std::filesystem::path& file);
  // Collection-level actions behind the commands and the menu (v2.4 COL-06..08).
  std::string saveCollection();               // merge matching edited tabs, write the collection file
  std::string saveCollectionAs(const std::string& name);
  std::string saveCollectionAsFile(const std::filesystem::path& file);  // v2.5: the path a dialog chose
  std::string closeCollection();
  std::string removeConfiguration(const std::string& name);
  std::string saveConfigurationAs(const std::string& name);
  std::string saveConfiguration();          // v2.5 CFG-04: the active tab's source configuration, in place
  void mergeDirtyTabsInto(std::vector<efapp::UserConfig>& configs);
  // v2.5 EXP-03: script/file-exports with deterministic fitted views (the GUI export keeps the canvas view).
  std::string exportConfigurationTo(const std::string& path);
  std::string exportCollectionTo(const std::string& path);

  AppOptions opt_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* aboutIcon_ = nullptr;
  bool sdlInit_ = false, imguiCtx_ = false, platformInit_ = false, rendererInit_ = false;
  bool vsync_ = false;
  bool quit_ = false;
  bool scaleDirty_ = true;
  float k_ = 1.f;
  ImGuiStyle baseStyle_;
  efapp::Workspace ws_;
  efapp::FramePacer pacer_;
  efapp::Options options_;            // user options (Phase 4); loaded at startup, saved when the dialog is accepted
  bool persistedCoupleValueSize_ = false;  // 261020-ef2: the shadow for the panel toggle's persist-on-flip
  std::filesystem::path optionsPath_; // empty in headless modes (nothing is loaded or persisted there)
  std::vector<efapp::UserConfig> userConfigs_;  // user configurations (v2 CFG-01); master list, mirrored into ui_
  std::filesystem::path configsPath_;           // configurations.ini; empty in headless modes
  std::filesystem::path configDir_;             // the user config directory (v2.1 SES-03: Help > Open Configuration Folder)
  std::filesystem::path sessionPath_;           // session.ini (v2.4: the POINTER file, collection = <name>)
  std::filesystem::path sessionsDir_;           // Sessions\ (v2.4 COL-09: one open-tab file per collection)
  std::filesystem::path collectionsDir_;        // Collections\ (v2.2 COL-01); empty in headless modes
  std::string currentCollection_ = "Original";  // v2.2 COL: the collection being worked on ("" = none open, v2.5)
  std::filesystem::path currentCollectionPath_;  // v2.5 OPEN-01: the open collection's file (empty = none)
  std::vector<std::string> collections_;        // known collection names (switcher)
  // 261012-h26: recently opened collection files, newest first (UTF-8 paths, v2.5 OPEN-01), persisted in
  // the session pointer (session.ini `recent` lines) and listed by File > Recent Collections.
  std::vector<std::string> recents_;
  void noteRecentCollection(const std::filesystem::path& file);  // dedupe to the front, cap at efapp::kMaxRecentCollections
  bool leaveConfirmed_ = false;                 // v2.2: the unsaved-changes prompt was answered with "leave"
  std::string pendingCollection_;               // v2.2: collection to switch to once confirmed
  std::filesystem::path pendingCollectionFile_;  // v2.5 OPEN-01: collection file to open once confirmed
  // Export state (Phase 4). The SDL save-dialog callback may run on any thread (portald/zenity on Linux), so the
  // chosen path crosses into the UI thread behind a mutex and is exported at the next frame boundary.
  // v2.5: one dialog machinery for every file pick - exportFormat_: 1 = PNG, 2 = SVG, 3 = PDF (the active
  // tab), 10 = Open Collection..., 11 = Save Collection As..., 12 = New Collection..., 13 = Export Collection...
  std::mutex exportMutex_;
  std::string pendingExportPath_;   // guarded by exportMutex_; empty = nothing to do
  std::string pendingExportError_;  // guarded by exportMutex_; dialog failure text (SDL_GetError read in the callback)
  int exportFormat_ = 0;            // format of the dialog in flight (see above)
  unsigned exportSerial_ = 0;       // tab to export once the path arrives
  UiState ui_;
  UiEnv env_;
  std::string lastTitle_;
  unsigned frames_ = 0;
};

}  // namespace efgui
