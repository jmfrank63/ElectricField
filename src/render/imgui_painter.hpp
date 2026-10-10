#pragma once
// ImGui ImDrawList backend of ef::Painter. Shared by the interactive GUI (Phase 3: draw into a window/child draw list)
// and the headless PNG export (this phase: draw into the background draw list of an offscreen ImGui frame), so both
// produce the same pixels from the same code path (D-11).
#include "ef/scene.hpp"
#include "imgui.h"

namespace efrender {

class ImGuiPainter final : public ef::Painter {
 public:
  // `origin` = screen position of the scene's top-left corner (0,0 for the offscreen export).
  ImGuiPainter(ImDrawList* dl, ImVec2 origin) : dl_(dl), o_(origin) {}

  void begin(int w, int h, ef::Color bg) override {
    dl_->PushClipRect(o_, ImVec2(o_.x + static_cast<float>(w), o_.y + static_cast<float>(h)), true);
    dl_->AddRectFilled(o_, ImVec2(o_.x + static_cast<float>(w), o_.y + static_cast<float>(h)), col(bg));
  }

  void strokePath(const ef::PointF* p, std::size_t n, std::size_t bezierSegments, bool closed, ef::Color c, float width) override {
    if (n < 2) return;
    dl_->PathLineTo(pt(p[0]));
    std::size_t i = 1;
    for (std::size_t s = 0; s < bezierSegments; ++s, i += 3) dl_->PathBezierCubicCurveTo(pt(p[i]), pt(p[i + 1]), pt(p[i + 2]), 0);
    for (; i < n; ++i) dl_->PathLineTo(pt(p[i]));
    dl_->PathStroke(col(c), closed ? ImDrawFlags_Closed : ImDrawFlags_None, width);
  }

  void fillCircle(ef::PointF center, float radius, ef::Color c) override { dl_->AddCircleFilled(pt(center), radius, col(c), 0); }

  void end() override { dl_->PopClipRect(); }

 private:
  ImVec2 pt(ef::PointF p) const { return ImVec2(o_.x + p.x, o_.y + p.y); }
  static ImU32 col(ef::Color c) { return IM_COL32(c.r, c.g, c.b, c.a); }

  ImDrawList* dl_;
  ImVec2 o_;
};

}  // namespace efrender
