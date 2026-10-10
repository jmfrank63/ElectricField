#include "gui/ui.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "app/ui_logic.hpp"
#include "ef/presets.hpp"
#include "render/imgui_painter.hpp"

namespace efgui {

namespace {

constexpr const char* kBlogUrl = "https://compphys.go.ro/electric-field-lines/";
constexpr const char* kRepoUrl = "https://github.com/jmfrank63/ElectricField";
constexpr const char* kUpstreamUrl = "https://github.com/aromanro/ElectricField";

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
  const ef::DrawStyle style = efapp::scaledStyle(env.k);
  {
    efrender::ImGuiPainter painter(dl, p);
    ef::renderScene(painter, t.scene, style, view);
  }
  ui.canvasMin = p;
  ui.canvasMax = ImVec2(p.x + static_cast<float>(view.widthPx), p.y + static_cast<float>(view.heightPx));

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

  const auto& presets = ef::presets();
  ImGui::TextUnformatted("Choose a charge configuration:");
  ImGui::Dummy(ImVec2(0.f, 4.f * env.k));

  bool openSelected = false;
  const bool appearing = ImGui::IsWindowAppearing();
  const float rowH = ImGui::GetFrameHeight();
  // A plain framed child (NOT BeginListBox): without ImGuiChildFlags_NavFlattened the child is a separate nav scope that must first be
  // entered with Enter, so Up/Down would never reach the rows.
  const bool listOpen = ImGui::BeginChild("##presets", ImVec2(-FLT_MIN, rowH * static_cast<float>(presets.size()) + 2.f * ImGui::GetStyle().FramePadding.y), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (listOpen) {
    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.f);
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
      const std::string name(presets[static_cast<std::size_t>(i)].displayName);
      if (ImGui::Selectable(name.c_str(), ui.chooserSel == i, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.f, rowH))) {
        ui.chooserSel = i;
        if (ImGui::IsMouseDoubleClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) openSelected = true;
      } else if (ImGui::IsItemFocused()) {
        ui.chooserSel = i;  // selection follows keyboard focus (Up/Down)
      }
      if (ui.chooserSel == i) ImGui::SetItemDefaultFocus();  // canonical list pattern: the preselected row is the default nav focus on appearing
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

  if (openSelected) {
    ui.openPresetNow = ui.chooserSel;
    ImGui::CloseCurrentPopup();
  } else if (close) {
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
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
  const bool modalOpen = ui.chooserVisible || ui.aboutVisible || ui.openChooser || ui.openAbout;
  const bool hasTabs = !ws.tabs().empty();

  // Shortcuts (UI-SPEC "Keyboard"): ignored while a modal is open.
  if (!modalOpen) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N)) ui.openChooser = true;
    if (hasTabs && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W)) ui.closeSerial = ui.activeSerial;
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
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", efapp::shortcutLabel("Q", env.mac).c_str())) ui.requestQuit = true;
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
          drawCanvas(t, ui, env);
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

  ImGui::End();

  ui.wantedTitle = efapp::windowTitle(ws.find(ui.activeSerial));
}

}  // namespace efgui
