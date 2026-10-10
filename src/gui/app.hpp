#pragma once
// SDL3 + Dear ImGui application shell (Phase 3): window, renderer, loop, DPI, smoke / screenshot modes.
#include <SDL3/SDL.h>

#include <string>

#include "app/ui_logic.hpp"
#include "app/workspace.hpp"
#include "gui/ui.hpp"
#include "imgui.h"

namespace efgui {

struct AppOptions {
  bool smoke = false;
  bool screenshot = false;   // --screenshot <slug> <png> [--size WxH] [--scale k] [--modal about|chooser]
  std::string slug, out;
  std::string modal;         // open that modal before the screenshot is taken
  float scaleOverride = 0.f; // headless HiDPI check: forces k
  int width = 0, height = 0;
  bool headless() const { return smoke || screenshot; }
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
  void shutdown();  // idempotent: cancel sessions, then ImGui backends, context, renderer, window, SDL

 private:
  bool iterate();                     // one pass of the main loop (events -> frame); false = quit requested
  void frame();                       // poll sessions, NewFrame, drawUi, Render, present
  void handleEvent(const SDL_Event& e);
  void refreshScale(bool initial = false);  // initial: clamp the minimum window size to the creation size (small --size / small displays)
  unsigned lastSerial() const;  // serial of the newest tab, 0 if there is none
  bool pumpUntilDone(unsigned serial, unsigned timeoutMs);
  void frames(int n);
  void chord(ImGuiKeyChord mods, ImGuiKey key);  // inject a key chord through ImGui's input queue, with the frames it needs
  bool checkCanvas(unsigned serial, int w, int h, std::string& report);
  bool smokeExtras(std::string& report);

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
  UiState ui_;
  UiEnv env_;
  std::string lastTitle_;
  unsigned frames_ = 0;
};

}  // namespace efgui
