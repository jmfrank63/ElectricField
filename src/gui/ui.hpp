#pragma once
// Dear ImGui layer of the GUI shell (Phase 3): host window, menu bar, tab bar, canvas, modals.
// Everything decision-like lives in src/app (std-only, unit-tested); this file only maps it onto ImGui widgets.
#include <SDL3/SDL.h>

#include <optional>
#include <string>
#include <vector>

#include "app/configs.hpp"
#include "app/options.hpp"
#include "app/workspace.hpp"
#include "imgui.h"

namespace efgui {

struct UiEnv {
  float k = 1.f;            // display scale (clamped)
  bool mac = false;         // shortcut labels read Cmd+ instead of Ctrl+
  SDL_Texture* aboutIcon = nullptr;
  const char* semver = "";
  const char* gitHash = "";
  ef::DrawStyle drawStyle;                  // user draw options (Phase 4), unscaled; the canvas draws scaledStyle(drawStyle, k)
  const efapp::Options* options = nullptr;  // the current options, owned by App; the dialog resets its edits from here
};

struct UiState {
  // requests (set by menu / shortcuts / startup, consumed by drawUi)
  bool openChooser = false, openAbout = false, requestQuit = false;
  bool openOptions = false;  // View > Options...
  bool openSaveAs = false;   // File > Save As... (v2 CFG-03)
  int openPresetNow = -1;    // File > New > <preset>
  int openUserConfig = -1;   // chooser: index into userConfigs (v2 CFG-02)
  bool openEmpty = false;    // chooser: Empty Configuration (v2 CFG-02)
  bool calculateActive = false;  // F5 / panel button (v2 CALC-01); consumed by drawUi via Workspace::calculate
  bool openConfigFolder = false; // Help > Open Configuration Folder (v2.1 SES-03)
  // v2.1 editor panel: Add-charge section and the per-tab rename buffer
  int addValue = 1;
  double addX = 0.0, addY = 0.0;
  char tabNameBuf[128] = "";
  unsigned tabNameBufSerial = 0;
  int exportRequest = 0;     // File > Export: 1 = PNG..., 2 = SVG... (consumed by App::frame, D-06)
  unsigned closeSerial = 0;  // request to close a tab (0 = none)
  // state carried between frames
  bool chooserVisible = false, aboutVisible = false, optionsVisible = false;  // a modal was drawn last frame
  bool saveAsVisible = false, saveAsConfirmVisible = false;                   // Save As modal (v2)
  int chooserSel = 0;
  efapp::OptionsEdit optionsEdit;                 // working copy behind the Options dialog (D-04)
  std::optional<efapp::Options> optionsToApply;   // set on OK after validation; consumed by App::frame (D-05)
  std::vector<efapp::UserConfig> userConfigs;     // mirror of the App's user configurations (v2, one owner: App)
  char saveAsName[128] = "";                      // Save As name field
  std::string saveAsError;                        // inline Save As error (EMPTY/RESERVED name)
  std::optional<efapp::UserConfig> saveAsApply;   // validated result consumed by App::frame
  unsigned activeSerial = 0;   // selected tab during the last frame
  unsigned selectSerial = 0;   // programmatic selection request (new tab)
  bool canvasDrag = false;     // v2 editor: a charge drag that started on the canvas is in progress
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
