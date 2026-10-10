#pragma once
// Dear ImGui layer of the GUI shell (Phase 3): host window, menu bar, tab bar, canvas, modals.
// Everything decision-like lives in src/app (std-only, unit-tested); this file only maps it onto ImGui widgets.
#include <SDL3/SDL.h>

#include <string>

#include "app/workspace.hpp"
#include "imgui.h"

namespace efgui {

struct UiEnv {
  float k = 1.f;            // display scale (clamped)
  bool mac = false;         // shortcut labels read Cmd+ instead of Ctrl+
  SDL_Texture* aboutIcon = nullptr;
  const char* semver = "";
  const char* gitHash = "";
};

struct UiState {
  // requests (set by menu / shortcuts / startup, consumed by drawUi)
  bool openChooser = false, openAbout = false, requestQuit = false;
  int openPresetNow = -1;    // File > New > <preset>
  unsigned closeSerial = 0;  // request to close a tab (0 = none)
  // state carried between frames
  bool chooserVisible = false, aboutVisible = false;  // a modal was drawn last frame
  int chooserSel = 0;
  unsigned activeSerial = 0;   // selected tab during the last frame
  unsigned selectSerial = 0;   // programmatic selection request (new tab)
  std::string linkNote;        // About: "Could not open a browser..." (cleared when the modal closes)
  // outputs for the loop / tests
  std::string wantedTitle;     // SDL window title for this frame
  ImVec2 canvasMin{0, 0}, canvasMax{0, 0};  // last drawn canvas rect (screen coordinates)
  ImVec2 tabMin{0, 0}, tabMax{0, 0};        // rect of the active tab item (smoke injects a middle click there)
  ImVec2 overlayMin{0, 0}, overlayMax{0, 0};// busy overlay rect of the last frame that drew one (zero when none)
  bool failedShown = false;                 // the failed-session copy was drawn last frame
};

// Between ImGui::NewFrame() and ImGui::Render(). Mutates the workspace only through open/close.
void drawUi(efapp::Workspace& ws, UiState& ui, const UiEnv& env);

// Base style = StyleColorsLight + the UI-SPEC overrides at scale 1; apply(k) = base, ScaleAllSizes(k), FontScaleDpi = k.
ImGuiStyle makeBaseStyle();
void applyScale(const ImGuiStyle& base, float k);

}  // namespace efgui
