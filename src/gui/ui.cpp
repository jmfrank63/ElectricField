#include "gui/ui.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "app/ui_logic.hpp"
#include "app/configs.hpp"
#include "app/editor.hpp"
#include "ef/presets.hpp"
#include "render/imgui_painter.hpp"

namespace efgui {

namespace {

constexpr const char* kBlogUrl = "https://compphys.go.ro/electric-field-lines/";
constexpr const char* kRepoUrl = "https://github.com/jmfrank63/ElectricField";
constexpr const char* kUpstreamUrl = "https://github.com/aromanro/ElectricField";
constexpr float kEditorPanelWidth = 230.f;    // v2 editor panel, logical px
constexpr double kEditorHitRadius = 0.30;     // world units: charge radius 0.25 + slack (D-06)

ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

// Centered text at an explicit font size (PushFont(nullptr, size) is scaled by style.FontScaleDpi).
void centeredText(const char* text, float size, const ImVec4* color) {
  ImGui::PushFont(nullptr, size);
  if (color) ImGui::PushStyleColor(ImGuiCol_Text, *color);
  const float w = ImGui::CalcTextSize(text).x;
  const float avail = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (avail - w) * 0.5f));
  ImGui::TextUnformatted(text);
  if (color) ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawBusyOverlay(ImDrawList* dl, ImVec2 canvasMin, ImVec2 canvasMax, const efapp::Tab& t, float k, UiState& ui) {
  const std::string text = efapp::overlayText(t.linesReceived, t.expectedLines);
  const float pad = 8.f * k, inset = 16.f * k, r = 8.f * k, gap = 4.f * k;
  const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
  const ImVec2 pmin(canvasMin.x + inset, canvasMin.y + inset);
  const ImVec2 pmax(pmin.x + pad + 2 * r + gap + ts.x + pad, pmin.y + pad + std::max(2 * r, ts.y) + pad);
  ui.overlayMin = pmin;
  ui.overlayMax = pmax;
  dl->PushClipRect(canvasMin, canvasMax, true);  // tiny canvas: never draw outside the tab content region
  dl->AddRectFilled(pmin, pmax, rgba(240, 240, 240, 230), 4.f * k);
  dl->AddRect(pmin, pmax, rgba(0, 0, 0, 77), 4.f * k, 0, 1.f);
  const ImVec2 c(pmin.x + pad + r, (pmin.y + pmax.y) * 0.5f);
  const float a0 = static_cast<float>(ImGui::GetTime() * 6.283185307179586);  // one revolution per second
  dl->PathArcTo(c, r, a0, a0 + 1.5f * 3.14159265f, 24);
  dl->PathStroke(rgba(0x42, 0x96, 0xFA), ImDrawFlags_None, 2.f * k);
  dl->AddText(ImVec2(c.x + r + gap, (pmin.y + pmax.y) * 0.5f - ts.y * 0.5f), rgba(0, 0, 0), text.c_str());
  dl->PopClipRect();
}

void drawCanvas(efapp::Tab& t, UiState& ui, const UiEnv& env) {
  ImVec2 avail = ImGui::GetContentRegionAvail();
  avail.x = std::max(1.f, avail.x);
  avail.y = std::max(1.f, avail.y);
  ImVec2 p = ImGui::GetCursorScreenPos();
  p = ImVec2(std::floor(p.x), std::floor(p.y));  // whole logical pixels keep 1 px lines aligned
  ImGui::SetCursorScreenPos(p);
  ImGui::InvisibleButton("##canvas", avail);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  const ef::View view = efapp::canvasView(avail.x, avail.y, env.k);
  const ef::DrawStyle style = efapp::scaledStyle(env.drawStyle, env.k);  // user draw options (Phase 4), scaled for HiDPI
  {
    efrender::ImGuiPainter painter(dl, p);
    ef::renderScene(painter, t.scene, style, view);
  }
  ui.canvasMin = p;
  ui.canvasMax = ImVec2(p.x + static_cast<float>(view.widthPx), p.y + static_cast<float>(view.heightPx));

  // Charge editor (v2, EDIT-01..04): all mutations go through efapp::editor, nothing recomputes implicitly.
  const bool canEdit = t.session == nullptr;
  const ImGuiIO& io = ImGui::GetIO();
  const ef::Vec2 mouseWorld = efapp::worldFromCanvas(view, p.x, p.y, io.MousePos.x, io.MousePos.y);
  if (canEdit && hovered) {
    if (ImGui::IsMouseDoubleClicked(0)) {
      if (efapp::editor::hitCharge(t.field, mouseWorld, kEditorHitRadius) < 0) efapp::editor::addCharge(t, mouseWorld, 1);
    } else if (ImGui::IsMouseClicked(0)) {
      t.selected = efapp::editor::hitCharge(t.field, mouseWorld, kEditorHitRadius);
    }
  }
  // A drag that started on the canvas keeps moving the charge even past the edge until the button is released.
  if (canEdit && t.selected >= 0 && ImGui::IsMouseDragging(0) && (hovered || ui.canvasDrag)) {
    efapp::editor::moveCharge(t, static_cast<std::size_t>(t.selected), mouseWorld);
    ui.canvasDrag = ImGui::IsMouseDown(0);
  } else {
    ui.canvasDrag = false;
  }
  if (canEdit && t.selected >= 0 && (hovered || ImGui::IsItemFocused())) {
    const double step = ImGui::IsKeyDown(ImGuiMod_Shift) ? 0.5 : 0.1;  // EDIT-02
    ef::Vec2 d{0, 0};
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) d.X -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) d.X += step;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) d.Y += step;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) d.Y -= step;
    if (d.X != 0 || d.Y != 0) efapp::editor::stepSelected(t, d);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) efapp::editor::removeSelected(t);  // EDIT-04: keeps the last charge
  }

  // Selection ring (v2): over the drawn scene, only when the charge exists.
  if (t.state != efapp::TabState::Computing && t.selected >= 0 && static_cast<std::size_t>(t.selected) < t.scene.charges.size()) {
    const ef::Vec2 c = efapp::canvasFromWorld(view, p.x, p.y, t.scene.charges[static_cast<std::size_t>(t.selected)].position);
    dl->AddCircle(ImVec2(static_cast<float>(c.X), static_cast<float>(c.Y)), style.chargeRadiusPx + 3.f * env.k, rgba(0x42, 0x96, 0xFA), 0, 1.5f * env.k);
  }

  if (t.state == efapp::TabState::Computing) {
    drawBusyOverlay(dl, ui.canvasMin, ui.canvasMax, t, env.k, ui);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Wait);
  } else if (t.state == efapp::TabState::Failed) {
    ui.failedShown = true;
    const float m = 16.f * env.k;
    ImGui::SetCursorScreenPos(ImVec2(p.x + m, p.y + std::max(m, avail.y * 0.5f - 3 * ImGui::GetFrameHeight())));
    ImGui::PushTextWrapPos(p.x + avail.x - m);
    ImGui::TextColored(ImVec4(0.75f, 0.f, 0.f, 1.f), "Could not compute this configuration");
    ImGui::TextUnformatted(efapp::failureDetail(t.error).c_str());
    ImGui::TextDisabled("Close this tab and open it again from File > New.");
    ImGui::PopTextWrapPos();
  }
}

void drawEmptyState(const UiEnv& env) {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float blockH = 24.f * env.k * 2 + 4.f * env.k;
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.f, (avail.y - blockH) * 0.5f));
  centeredText("No configuration open", 20.f, nullptr);
  const std::string hint = std::string("File > New or ") + efapp::shortcutLabel("N", env.mac) + " to open a configuration";
  const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  centeredText(hint.c_str(), 16.f, &muted);
}

void aboutModal(UiState& ui, const UiEnv& env) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  if (ui.openAbout) {
    ImGui::OpenPopup("About Electric Field##about");
    ui.openAbout = false;
    ui.linkNote.clear();
  }
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(400.f * env.k, 0.f), ImVec2(400.f * env.k, FLT_MAX));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * env.k, 16.f * env.k));
  const bool open = ImGui::BeginPopupModal("About Electric Field##about", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  ImGui::PopStyleVar();
  ui.aboutVisible = open;
  if (!open) return;

  const float icon = 48.f * env.k;
  if (env.aboutIcon)
    ImGui::Image(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(env.aboutIcon))), ImVec2(icon, icon));
  else
    ImGui::Dummy(ImVec2(icon, icon));
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::PushFont(nullptr, 20.f);
  ImGui::TextUnformatted("Electric Field");
  ImGui::PopFont();
  ImGui::PushFont(nullptr, 14.f);
  ImGui::TextDisabled("%s", efapp::versionLine(env.semver, env.gitHash).c_str());
  ImGui::PopFont();
  ImGui::EndGroup();
  ImGui::Dummy(ImVec2(0.f, 32.f * env.k - ImGui::GetStyle().ItemSpacing.y));

  ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 8.f * env.k);
  ImGui::TextWrapped("Electric field lines and equipotentials of point charges, computed with Runge-Kutta methods.");
  ImGui::TextWrapped("Original application by Adrian Roman.");
  ImGui::TextWrapped("Cross-platform port by Johannes Maria Frank.");
  ImGui::PopStyleVar();
  ImGui::PushFont(nullptr, 14.f);
  ImGui::TextWrapped("Licensed under the GNU General Public License v3.0.");  // TextWrapped: the unwrapped line overflows 400 x k
  ImGui::PopFont();
  ImGui::Spacing();

  struct Link {
    const char* label;
    const char* url;
  };
  const Link links[] = {{"Blog description", kBlogUrl}, {"GitHub repository", kRepoUrl}, {"Original repository", kUpstreamUrl}};
  bool linkActivated = false;  // Enter / Space on a focused link activates it; that same Enter must not also close the dialog
  for (const Link& l : links) {
    if (ImGui::TextLink(l.label)) {
      linkActivated = true;
      if (!SDL_OpenURL(l.url)) {
        SDL_SetClipboardText(l.url);
        ui.linkNote = "Could not open a browser. The link was copied to the clipboard.";
      }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", l.url);
  }
  if (!ui.linkNote.empty()) {
    ImGui::PushFont(nullptr, 14.f);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%s", ui.linkNote.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
  }
  ImGui::Dummy(ImVec2(0.f, 8.f * env.k));

  const float bw = ImGui::CalcTextSize("Close About").x + 2 * ImGui::GetStyle().FramePadding.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bw);
  const bool close = ImGui::Button("Close About");
  ImGui::SetItemDefaultFocus();
  const bool enterClose = !linkActivated && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
  if (close || ImGui::IsKeyPressed(ImGuiKey_Escape) || enterClose) {
    ui.linkNote.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void chooserModal(UiState& ui, const UiEnv& env) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  if (ui.openChooser) {
    ImGui::OpenPopup("New##chooser");
    ui.openChooser = false;
    ui.chooserSel = 0;
  }
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(360.f * env.k, 0.f), ImVec2(360.f * env.k, FLT_MAX));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * env.k, 16.f * env.k));
  const bool open = ImGui::BeginPopupModal("New##chooser", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  ImGui::PopStyleVar();
  ui.chooserVisible = open;
  if (!open) return;

  // Row order (v2 CFG-02): the 9 built-ins, then - when any exist - a separator and the user configurations
  // in file order, then "Empty Configuration". The separator is not a Selectable, so keyboard focus skips it.
  const auto& presets = ef::presets();
  const std::size_t presetCount = presets.size();
  const std::size_t userCount = ui.userConfigs.size();
  const bool hasUsers = userCount > 0;
  const std::size_t rowCount = presetCount + (hasUsers ? 1 + userCount : 0) + 1;
  // kind: -1 = separator, 0 = preset, 1 = user configuration, 2 = empty configuration
  auto rowKind = [&](std::size_t i) {
    if (i < presetCount) return 0;
    if (hasUsers && i == presetCount) return -1;  // separator
    if (hasUsers && i > presetCount && i <= presetCount + userCount) return 1;
    return 2;  // "Empty Configuration" (always the last row)
  };
  auto userIndex = [&](std::size_t i) { return static_cast<int>(i - presetCount - 1); };

  ImGui::TextUnformatted("Choose a charge configuration:");
  ImGui::Dummy(ImVec2(0.f, 4.f * env.k));

  bool openSelected = false;
  const float rowH = ImGui::GetFrameHeight();
  // A plain framed child (NOT BeginListBox): without ImGuiChildFlags_NavFlattened the child is a separate nav scope that must first be
  // entered with Enter, so Up/Down would never reach the rows.
  const bool listOpen = ImGui::BeginChild("##presets", ImVec2(-FLT_MIN, rowH * static_cast<float>(rowCount) + 2.f * ImGui::GetStyle().FramePadding.y), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (listOpen) {
    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.f);
    for (std::size_t i = 0; i < rowCount; ++i) {
      const int kind = rowKind(i);
      if (kind < 0) {
        ImGui::Separator();
        ImGui::TextDisabled("User configurations");
        continue;
      }
      std::string label;
      if (kind == 0)
        label = std::string(presets[i].displayName);
      else if (kind == 1)
        label = ui.userConfigs[static_cast<std::size_t>(userIndex(i))].name;
      else
        label = "Empty Configuration";
      if (ImGui::Selectable(label.c_str(), ui.chooserSel == static_cast<int>(i), ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.f, rowH))) {
        ui.chooserSel = static_cast<int>(i);
        if (ImGui::IsMouseDoubleClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) openSelected = true;
      } else if (ImGui::IsItemFocused()) {
        ui.chooserSel = static_cast<int>(i);  // selection follows keyboard focus (Up/Down)
      }
      if (ui.chooserSel == static_cast<int>(i)) ImGui::SetItemDefaultFocus();
    }
    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0.f, 32.f * env.k - ImGui::GetStyle().ItemSpacing.y));

  const ImGuiStyle& st = ImGui::GetStyle();
  const float w1 = ImGui::CalcTextSize("Open Configuration").x + 2 * st.FramePadding.x;
  const float w2 = ImGui::CalcTextSize("Close Chooser").x + 2 * st.FramePadding.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - (w1 + 8.f * env.k + w2));
  if (ImGui::Button("Open Configuration")) openSelected = true;
  // no SetItemDefaultFocus here: initial keyboard focus stays on the preselected list row so Up/Down work at once (Enter on a row opens it)
  ImGui::SameLine(0.f, 8.f * env.k);
  bool close = ImGui::Button("Close Chooser");
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;

  if (openSelected && ui.chooserSel >= 0 && static_cast<std::size_t>(ui.chooserSel) < rowCount) {
    const int kind = rowKind(static_cast<std::size_t>(ui.chooserSel));
    if (kind == 0)
      ui.openPresetNow = ui.chooserSel;
    else if (kind == 1)
      ui.openUserConfig = userIndex(static_cast<std::size_t>(ui.chooserSel));
    else
      ui.openEmpty = true;
    ImGui::CloseCurrentPopup();
  } else if (close) {
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

// File > Save As (v2 CFG-03, D-07): name field prefilled from the active tab, inline errors for an empty or
// reserved name, and an overwrite confirmation for an existing user configuration. The actual file write is
// the App's job (saveAsApply).
void saveAsModal(efapp::Workspace& ws, UiState& ui, const UiEnv& env) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  if (ui.openSaveAs) {
    ImGui::OpenPopup("Save Configuration As##saveas");
    ui.openSaveAs = false;
    ui.saveAsError.clear();
    const efapp::Tab* t = ws.find(ui.activeSerial);
    const std::string base = t ? efapp::baseTitle(t->title) : "";
    std::snprintf(ui.saveAsName, sizeof(ui.saveAsName), "%s", base.c_str());
  }
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(380.f * env.k, 0.f), ImVec2(520.f * env.k, FLT_MAX));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * env.k, 16.f * env.k));
  const bool open = ImGui::BeginPopupModal("Save Configuration As##saveas", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  ImGui::PopStyleVar();
  ui.saveAsVisible = open;
  if (!open) {
    ui.saveAsConfirmVisible = false;
    return;
  }
  const efapp::Tab* t = ws.find(ui.activeSerial);
  if (!t) {  // the tab vanished under the dialog: nothing to save
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }

  ImGui::TextUnformatted("Save the charges of this tab as a user configuration:");
  ImGui::Dummy(ImVec2(0.f, 4.f * env.k));
  ImGui::InputText("##name", ui.saveAsName, sizeof(ui.saveAsName));
  if (!ui.saveAsError.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.f, 0.f, 1.f));
    ImGui::TextWrapped("%s", ui.saveAsError.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0.f, 8.f * env.k));

  const ImGuiStyle& st = ImGui::GetStyle();
  const float w1 = ImGui::CalcTextSize("Save").x + 2 * st.FramePadding.x;
  const float w2 = ImGui::CalcTextSize("Cancel").x + 2 * st.FramePadding.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - (w1 + 8.f * env.k + w2));
  bool close = false;
  if (ImGui::Button("Save")) {
    std::vector<std::string> reserved;
    for (const ef::Preset& p : ef::presets()) reserved.emplace_back(p.displayName);
    efapp::UserConfig candidate;
    candidate.name = ui.saveAsName;
    candidate.charges.reserve(t->field.charges.size());
    for (const ef::Charge& c : t->field.charges) candidate.charges.push_back(ef::PresetCharge{c.charge, c.position.X, c.position.Y});
    // Check on a scratch copy: upsert is the single decision core (also used by App::frame for the real write).
    std::vector<efapp::UserConfig> scratch = ui.userConfigs;
    const efapp::UpsertResult res = efapp::upsertUserConfig(scratch, candidate, reserved);
    if (res == efapp::UpsertResult::EMPTY_NAME)
      ui.saveAsError = "Enter a name.";
    else if (res == efapp::UpsertResult::RESERVED_NAME)
      ui.saveAsError = "'" + candidate.name + "' is a built-in preset and cannot be overwritten.";
    else if (efapp::findUserConfig(ui.userConfigs, candidate.name) >= 0)
      ImGui::OpenPopup("Overwrite?##saveas");
    else {
      ui.saveAsApply = candidate;
      close = true;
    }
  }
  ImGui::SetItemDefaultFocus();
  ImGui::SameLine(0.f, 8.f * env.k);
  if (ImGui::Button("Cancel")) close = true;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;

  // Overwrite confirmation for an existing user configuration name.
  if (ImGui::BeginPopupModal("Overwrite?##saveas", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ui.saveAsConfirmVisible = true;
    ImGui::Text("A configuration named '%s' already exists. Overwrite it?", ui.saveAsName);
    if (ImGui::Button("Overwrite")) {
      efapp::UserConfig candidate;
      candidate.name = ui.saveAsName;
      candidate.charges.reserve(t->field.charges.size());
      for (const ef::Charge& c : t->field.charges) candidate.charges.push_back(ef::PresetCharge{c.charge, c.position.X, c.position.Y});
      ui.saveAsApply = candidate;
      ImGui::CloseCurrentPopup();
      close = true;
    }
    ImGui::SameLine(0.f, 8.f * env.k);
    if (ImGui::Button("Keep Existing") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  if (close) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

// View > Options (Phase 4, D-04): the Computation / Draw property pages as a tabbed modal. Ranges are
// validated on OK through efapp::OptionsEdit and never clamped; an invalid input keeps the dialog open.
void optionsModal(UiState& ui, const UiEnv& env) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  if (ui.openOptions) {
    ImGui::OpenPopup("Options##options");
    ui.openOptions = false;
    if (env.options) ui.optionsEdit.reset(*env.options);
  }
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(430.f * env.k, 0.f), ImVec2(560.f * env.k, FLT_MAX));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * env.k, 16.f * env.k));
  const bool open = ImGui::BeginPopupModal("Options##options", nullptr, ImGuiWindowFlags_NoCollapse);
  ImGui::PopStyleVar();
  ui.optionsVisible = open;
  if (!open) return;
  efapp::OptionsEdit& e = ui.optionsEdit;

  if (ImGui::BeginTabBar("##optiontabs")) {
    if (ImGui::BeginTabItem("Computation")) {
      const int mIdx = e.methodIndex < 0 ? 0 : (e.methodIndex >= static_cast<int>(ef::kMethodCount) ? static_cast<int>(ef::kMethodCount) - 1 : e.methodIndex);
      const std::string mCurrent(ef::kMethods[static_cast<std::size_t>(mIdx)].display);
      if (ImGui::BeginCombo("Integration method", mCurrent.c_str())) {
        for (int i = 0; i < static_cast<int>(ef::kMethodCount); ++i) {
          const std::string label(ef::kMethods[static_cast<std::size_t>(i)].display);
          if (ImGui::Selectable(label.c_str(), i == e.methodIndex)) e.methodIndex = i;
          if (i == e.methodIndex) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::Checkbox("Calculate equipotentials", &e.equipotentials);
      ImGui::InputInt("Lines per unit charge", &e.lines);
      ImGui::InputInt("Threads", &e.threads);
      float interval = static_cast<float>(e.interval);
      if (ImGui::SliderFloat("Potential interval", &interval, 0.01f, 0.50f, "%.2f")) e.interval = interval;
      ImGui::TextDisabled("0.01 - 0.50");
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Draw")) {
      ImGui::ColorEdit3("Positive charge", e.positive);
      ImGui::ColorEdit3("Negative charge", e.negative);
      ImGui::ColorEdit3("Field lines", e.electric);
      ImGui::ColorEdit3("Equipotentials", e.potential);
      ImGui::InputInt("Field line thickness", &e.electricThickness);
      ImGui::InputInt("Equipotential thickness", &e.potentialThickness);
      ImGui::TextDisabled("1 - 64");
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (!e.error.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.f, 0.f, 1.f));
    ImGui::TextWrapped("%s", e.error.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0.f, 8.f * env.k));

  const ImGuiStyle& st = ImGui::GetStyle();
  const float w1 = ImGui::CalcTextSize("OK").x + 2 * st.FramePadding.x;
  const float w2 = ImGui::CalcTextSize("Cancel").x + 2 * st.FramePadding.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - (w1 + 8.f * env.k + w2));
  bool close = false;
  if (ImGui::Button("OK")) {
    if (e.validate()) {
      ui.optionsToApply = e.apply();
      close = true;
    }  // invalid input: the error is shown and the dialog stays open (D-04)
  }
  ImGui::SetItemDefaultFocus();
  ImGui::SameLine(0.f, 8.f * env.k);
  if (ImGui::Button("Cancel")) close = true;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
  if (close) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

// Press-and-hold repeat for the panel's -/+ buttons (v2 EDIT-03): one step on click, then steps every
// 60 ms after a 300 ms delay, while the button is held.
bool stepButton(const char* label) {
  const bool clicked = ImGui::Button(label, ImVec2(22.f, 0.f));
  static std::unordered_map<ImGuiID, double> s_nextRepeat;
  const ImGuiID id = ImGui::GetID(label);
  if (ImGui::IsItemActive()) {
    const double now = ImGui::GetTime();
    if (clicked) {
      s_nextRepeat[id] = now + 0.30;
      return true;
    }
    const auto it = s_nextRepeat.find(id);
    if (it != s_nextRepeat.end() && now >= it->second) {
      it->second = now + 0.06;
      return true;
    }
  } else {
    s_nextRepeat.erase(id);
  }
  return clicked;
}

// The editor panel (v2, D-05): selection fields with up/down buttons, Remove, Method, Calculate, stale hint.
void drawEditorPanel(efapp::Tab& t, UiState& ui, const UiEnv& env) {
  ImGui::BeginChild("##editor", ImVec2(kEditorPanelWidth * env.k, 0.f), ImGuiChildFlags_None);
  const bool canEdit = t.session == nullptr;
  ImGui::TextUnformatted("Charge editor");
  ImGui::Separator();
  ImGui::BeginDisabled(!canEdit);

  const bool have = t.selected >= 0 && static_cast<std::size_t>(t.selected) < t.field.charges.size();
  if (!have) {
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("Click a charge to select it. Double-click empty canvas to add one. Arrow keys nudge, Delete removes.");
    ImGui::PopTextWrapPos();
  } else {
    const std::size_t at = static_cast<std::size_t>(t.selected);
    ef::Charge& c = t.field.charges[at];

    int value = c.charge;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * (22.f + 8.f * env.k));
    if (ImGui::InputInt("##value", &value, 0, 0)) efapp::editor::stepValue(t, value - c.charge);  // realtime per keystroke
    ImGui::SameLine(0.f, 8.f * env.k);
    if (stepButton("##value-")) efapp::editor::stepValue(t, -1);
    ImGui::SameLine(0.f, 4.f * env.k);
    if (stepButton("##value+")) efapp::editor::stepValue(t, +1);
    ImGui::SameLine(0.f, 8.f * env.k);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Value");

    double x = c.position.X;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * (22.f + 8.f * env.k));
    if (ImGui::InputDouble("##x", &x, 0.0, 0.0, "%.3f")) efapp::editor::moveCharge(t, at, ef::Vec2{x, c.position.Y});
    ImGui::SameLine(0.f, 8.f * env.k);
    if (stepButton("##x-")) efapp::editor::stepSelected(t, ef::Vec2{-0.1, 0});
    ImGui::SameLine(0.f, 4.f * env.k);
    if (stepButton("##x+")) efapp::editor::stepSelected(t, ef::Vec2{0.1, 0});
    ImGui::SameLine(0.f, 8.f * env.k);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("X");

    double y = c.position.Y;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * (22.f + 8.f * env.k));
    if (ImGui::InputDouble("##y", &y, 0.0, 0.0, "%.3f")) efapp::editor::moveCharge(t, at, ef::Vec2{c.position.X, y});
    ImGui::SameLine(0.f, 8.f * env.k);
    if (stepButton("##y-")) efapp::editor::stepSelected(t, ef::Vec2{0, -0.1});
    ImGui::SameLine(0.f, 4.f * env.k);
    if (stepButton("##y+")) efapp::editor::stepSelected(t, ef::Vec2{0, 0.1});
    ImGui::SameLine(0.f, 8.f * env.k);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Y");

    ImGui::BeginDisabled(t.field.charges.size() <= 1);  // EDIT-04: at least one charge remains
    if (ImGui::Button("Remove charge")) efapp::editor::removeSelected(t);
    ImGui::EndDisabled();
  }

  ImGui::Separator();
  const int mIdx = static_cast<int>(t.params.method);
  const std::string mCurrent(ef::kMethods[static_cast<std::size_t>(mIdx)].display);
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::BeginCombo("Method", mCurrent.c_str())) {
    for (std::size_t i = 0; i < ef::kMethodCount; ++i) {
      const std::string label(ef::kMethods[i].display);
      if (ImGui::Selectable(label.c_str(), static_cast<int>(i) == mIdx) && static_cast<int>(i) != mIdx) {
        t.params.method = ef::kMethods[i].method;  // CALC-02: applies at the next Calculate
        t.stale = true;
      }
      if (static_cast<int>(i) == mIdx) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Calculate (F5)", ImVec2(-FLT_MIN, 0.f))) ui.calculateActive = true;

  if (t.stale && t.state == efapp::TabState::Done) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.45f, 0.f, 1.f));
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted("Edited - the lines show the previous calculation. Press Calculate (F5).");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  ImGui::EndDisabled();
  ImGui::EndChild();
}

}  // namespace

ImGuiStyle makeBaseStyle() {
  ImGuiStyle s;
  ImGui::StyleColorsLight(&s);
  s.FontSizeBase = 16.f;
  s.WindowPadding = ImVec2(8, 8);
  s.FramePadding = ImVec2(8, 4);
  s.ItemSpacing = ImVec2(8, 4);
  s.ItemInnerSpacing = ImVec2(4, 4);
  s.CellPadding = ImVec2(4, 4);
  s.ScrollbarSize = 16;
  s.IndentSpacing = 16;
  s.WindowBorderSize = 0;
  s.PopupBorderSize = 1;
  s.FrameBorderSize = 0;
  s.TabBarBorderSize = 1;
  s.TabBarOverlineSize = 0;
  s.WindowRounding = 0;
  s.FrameRounding = 4;
  s.PopupRounding = 4;
  s.TabRounding = 4;
  s.CurveTessellationTol = 0.3f;
  s.AntiAliasedLinesUseTex = false;
  s.Colors[ImGuiCol_TextDisabled] = ImVec4(0x59 / 255.f, 0x59 / 255.f, 0x59 / 255.f, 1.f);
  s.Colors[ImGuiCol_TextLink] = ImVec4(0.f, 0x54 / 255.f, 0xCC / 255.f, 1.f);
  s.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.f, 0.f, 0.f, 0.25f);
  return s;
}

void applyScale(const ImGuiStyle& base, float k) {
  ImGuiStyle& s = ImGui::GetStyle();
  s = base;
  s.ScaleAllSizes(k);
  s.FontScaleDpi = k;
}

void drawUi(efapp::Workspace& ws, UiState& ui, const UiEnv& env) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
  const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar;
  ImGui::Begin("##host", nullptr, hostFlags);
  ImGui::PopStyleVar(3);

  ui.failedShown = false;
  ui.overlayMin = ui.overlayMax = ImVec2(0, 0);
  const bool modalOpen = ui.chooserVisible || ui.aboutVisible || ui.optionsVisible || ui.saveAsVisible || ui.openChooser || ui.openAbout ||
                         ui.openOptions || ui.openSaveAs;
  const bool hasTabs = !ws.tabs().empty();

  // Shortcuts (UI-SPEC "Keyboard"): ignored while a modal is open.
  if (!modalOpen) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N)) ui.openChooser = true;
    if (hasTabs && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W)) ui.closeSerial = ui.activeSerial;
    if (hasTabs && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) ui.openSaveAs = true;
    if (hasTabs && ImGui::Shortcut(ImGuiKey_F5)) ui.calculateActive = true;  // v2 CALC-01
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q)) ui.requestQuit = true;
  }

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::BeginMenu("New")) {
        const auto& presets = ef::presets();
        for (std::size_t i = 0; i < presets.size(); ++i) {
          const std::string name(presets[i].displayName);
          if (ImGui::MenuItem(name.c_str())) ui.openPresetNow = static_cast<int>(i);
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("New...", efapp::shortcutLabel("N", env.mac).c_str())) ui.openChooser = true;
      ImGui::Separator();
      if (ImGui::MenuItem("Close Tab", efapp::shortcutLabel("W", env.mac).c_str(), false, hasTabs)) ui.closeSerial = ui.activeSerial;
      if (ImGui::MenuItem("Save As...", efapp::shortcutLabel("S", env.mac).c_str(), false, hasTabs)) ui.openSaveAs = true;
      ImGui::Separator();
      // EXP-01/02 (D-06): export replaces MFC's printing. Only a finished tab has a complete scene, so a busy or
      // failed tab greys the submenu out.
      const efapp::Tab* active = ws.find(ui.activeSerial);
      if (ImGui::BeginMenu("Export", active != nullptr && active->state == efapp::TabState::Done)) {
        if (ImGui::MenuItem("PNG...", nullptr, false, active != nullptr && active->state == efapp::TabState::Done)) ui.exportRequest = 1;
        if (ImGui::MenuItem("SVG...", nullptr, false, active != nullptr && active->state == efapp::TabState::Done)) ui.exportRequest = 2;
        ImGui::EndMenu();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", efapp::shortcutLabel("Q", env.mac).c_str())) ui.requestQuit = true;
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Options...")) ui.openOptions = true;
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About Electric Field...")) ui.openAbout = true;
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  // Open requests (menu entry or chooser result) are applied before the tab bar is submitted so the new tab shows this frame.
  if (ui.openPresetNow >= 0) {
    ui.selectSerial = ws.open(static_cast<std::size_t>(ui.openPresetNow));
    ui.openPresetNow = -1;
  }
  if (ui.openUserConfig >= 0) {  // v2 CFG-02: a user configuration from the chooser
    if (static_cast<std::size_t>(ui.openUserConfig) < ui.userConfigs.size()) {
      const efapp::UserConfig& c = ui.userConfigs[static_cast<std::size_t>(ui.openUserConfig)];
      ui.selectSerial = ws.openUser(c.name, c.charges);
    }
    ui.openUserConfig = -1;
  }
  if (ui.openEmpty) {
    ui.selectSerial = ws.openEmpty();
    ui.openEmpty = false;
  }
  if (ui.calculateActive) {  // v2 CALC-01: explicit recalculation of the active tab
    ws.calculate(ui.activeSerial);
    ui.calculateActive = false;
  }

  if (!ws.tabs().empty()) {
    const ImGuiTabBarFlags tbf = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll |
                                 ImGuiTabBarFlags_TabListPopupButton;
    std::vector<unsigned> toClose;
    unsigned seenActive = 0;
    if (ImGui::BeginTabBar("##tabs", tbf)) {
      for (const auto& tp : ws.tabs()) {
        efapp::Tab& t = *tp;
        bool open = true;
        const std::string label = efapp::tabLabel(t, ImGui::GetTime());
        const ImGuiTabItemFlags f = (ui.selectSerial == t.serial) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem(label.c_str(), &open, f)) {
          seenActive = t.serial;
          ui.tabMin = ImGui::GetItemRectMin();
          ui.tabMax = ImGui::GetItemRectMax();
          // v2 editor: canvas (left) + editor panel (right, D-05)
          const float panelW = kEditorPanelWidth * env.k;
          const ImVec2 avail = ImGui::GetContentRegionAvail();
          ImGui::BeginChild("##canvasarea", ImVec2(std::max(1.f, avail.x - panelW), 0.f), ImGuiChildFlags_None);
          drawCanvas(t, ui, env);
          ImGui::EndChild();
          ImGui::SameLine(0.f, 0.f);
          drawEditorPanel(t, ui, env);
          ImGui::EndTabItem();
        }
        if (!open) toClose.push_back(t.serial);
      }
      ImGui::EndTabBar();
    }
    ui.selectSerial = 0;
    if (seenActive != 0) ui.activeSerial = seenActive;
    if (ui.closeSerial != 0) toClose.push_back(ui.closeSerial);
    ui.closeSerial = 0;
    for (unsigned s : toClose) ws.close(s);  // cancel + join on the UI thread (<= 200 ms contract)
  } else {
    ui.closeSerial = 0;
    ui.activeSerial = 0;
    drawEmptyState(env);
  }

  chooserModal(ui, env);
  aboutModal(ui, env);
  optionsModal(ui, env);
  saveAsModal(ws, ui, env);

  ImGui::End();

  ui.wantedTitle = efapp::windowTitle(ws.find(ui.activeSerial));
}

}  // namespace efgui
