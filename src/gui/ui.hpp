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

// 261012-h26: one File > Recent Collections entry: label for display (stem, unique in the list, ImGui
// '&' already escaped), path = the collection file's UTF-8 path.
struct RecentCollection {
  std::string label;
  std::string path;
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
  bool calculateAll = false;     // Calculate All (v2.2 COL-03): close tabs, open every configuration of the collection
  bool openLeaveConfirm = false, leaveConfirmVisible = false;  // v2.2: unsaved-changes prompt
  std::string leaveText;         // body text of the prompt
  std::string leaveButton;       // "Quit anyway" / "Switch anyway"
  int leaveDecision = 0;         // 1 = leave anyway (consumed by App::frame)
  bool undoRequest = false, redoRequest = false;  // v2.3 EDIT-12 (Edit menu / Ctrl+Z, Ctrl+Y)
  bool openChooserAfterSwitch = false;  // after a UI-initiated switch to an empty workspace, show the chooser
  bool saveCollectionRequest = false;       // File > Save Collection (Ctrl+S, v2.4 COL-06)
  bool saveConfigurationRequest = false;    // File > Save Configuration (v2.5 CFG-04; needs a source configuration)
  bool closeCollectionRequest = false;      // File > Close Collection (v2.4 COL-08; v2.5: no collection open after)
  // 261012-h26: the recents mirror (the App owns the master list) and a clicked entry's request.
  std::vector<RecentCollection> recentCollections;
  std::string openRecentPath;
  bool renameOpen = false, renameVisible = false;  // tab rename modal (v2.4 TAB-03)
  unsigned renameSerial = 0;
  char renameBuf[128] = "";
  bool closePopups = false;  // v2.4 BUG-02: a collection change closes every open modal exactly once
  std::string removeConfigName;  // v2.3 COL-05: right-clicked configuration awaiting confirmation
  bool removeConfigOpen = false; // the confirm popup was requested this frame
  std::string removeConfigApply; // confirmed removal; consumed by App::frame
  bool openConfigFolder = false; // Help > Open Configuration Folder (v2.1 SES-03)
  // v2.2 collections: the App owns the master list; ui_ mirrors names for the switcher.
  std::vector<std::string> collections;
  std::string currentCollection;    // "" = no collection open (v2.5 OPEN-01); the App mirrors it every frame
  std::string switchToCollection;   // set by the switcher; consumed by App::frame
  // v2.1 editor panel: Add-charge section and the per-tab rename buffer
  int addValue = 1;
  double addX = 0.0, addY = 0.0;
  char tabNameBuf[128] = "";
  unsigned tabNameBufSerial = 0;
  // v2.6 phase 3: the selected charge's name buffer (the tabNameBuf pattern, keyed on the active tab
  // AND the selection - it re-syncs whenever either changes). IN-03: 256 bytes with UTF-8-safe
  // truncation on copy, so a long API-set name is never cut mid-code point (128 silently truncated
  // at 127 bytes AND at an arbitrary byte).
  char chargeNameBuf[256] = "";
  unsigned chargeNameBufSerial = 0;
  int chargeNameBufSelected = -1;
  int exportRequest = 0;     // File > Export / dialog kinds (consumed by App::frame): 1 = PNG, 2 = SVG, 3 = PDF
                             // (the active tab) and 10 = Open Collection..., 11 = Save Collection As...,
                             // 12 = New Collection..., 13 = Export Collection (v2.5 OPEN-01/EXP-03)
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
