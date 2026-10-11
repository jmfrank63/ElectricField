#include "app/editor.hpp"

#include <algorithm>
#include <cmath>

namespace efapp {
namespace editor {
namespace {

constexpr double kPositionLimit = 1e5;  // sane guard, matches the config parser's value range

bool editable(const Tab& t) { return t.session == nullptr; }  // Done/Failed only (D-02)

// The scene mirror of one charge: index-aligned by buildScene (scene.cpp pushes field order 1:1).
void syncScene(Tab& t, std::size_t index) {
  if (index < t.scene.charges.size()) {
    t.scene.charges[index].position = t.field.charges[index].position;
    t.scene.charges[index].charge = t.field.charges[index].charge;
  }
}

ef::Vec2 clamped(ef::Vec2 v) {
  v.X = std::clamp(v.X, -kPositionLimit, kPositionLimit);
  v.Y = std::clamp(v.Y, -kPositionLimit, kPositionLimit);
  return v;
}

}  // namespace

int hitCharge(const ef::ChargeSystem& field, ef::Vec2 world, const double radius) {
  int best = -1;
  double bestDist = radius;
  for (std::size_t i = 0; i < field.charges.size(); ++i) {
    const ef::Vec2 d = field.charges[i].position - world;
    const double dist = std::sqrt(d * d);
    if (dist <= bestDist) {
      bestDist = dist;
      best = static_cast<int>(i);
    }
  }
  return best;
}

bool moveCharge(Tab& t, std::size_t index, ef::Vec2 world) {
  if (!editable(t) || index >= t.field.charges.size()) return false;
  t.field.charges[index].position = clamped(world);
  syncScene(t, index);
  t.stale = true;
  t.dirty = true;
  return true;
}

bool addCharge(Tab& t, ef::Vec2 world, int value) {
  if (!editable(t)) return false;
  t.field.charges.push_back(ef::Charge{value, clamped(world)});
  t.scene.charges.push_back(ef::SceneCharge{clamped(world), value});
  t.selected = static_cast<int>(t.field.charges.size()) - 1;
  t.stale = true;
  t.dirty = true;
  return true;
}

bool removeSelected(Tab& t) {
  if (!editable(t) || t.selected < 0 || static_cast<std::size_t>(t.selected) >= t.field.charges.size()) return false;
  if (t.field.charges.size() <= 1) return false;  // EDIT-04: at least one charge remains
  const std::size_t at = static_cast<std::size_t>(t.selected);
  t.field.charges.erase(t.field.charges.begin() + static_cast<std::ptrdiff_t>(at));
  t.scene.charges.erase(t.scene.charges.begin() + static_cast<std::ptrdiff_t>(at));
  t.selected = -1;
  t.stale = true;
  t.dirty = true;
  return true;
}

bool stepSelected(Tab& t, ef::Vec2 delta) {
  if (!editable(t) || t.selected < 0 || static_cast<std::size_t>(t.selected) >= t.field.charges.size()) return false;
  const std::size_t at = static_cast<std::size_t>(t.selected);
  return moveCharge(t, at, clamped(t.field.charges[at].position + delta));
}

bool stepValue(Tab& t, int delta) {
  if (!editable(t) || t.selected < 0 || static_cast<std::size_t>(t.selected) >= t.field.charges.size()) return false;
  const std::size_t at = static_cast<std::size_t>(t.selected);
  t.field.charges[at].charge = std::clamp(t.field.charges[at].charge + delta, -1000000, 1000000);
  syncScene(t, at);
  t.stale = true;
  t.dirty = true;
  return true;
}

bool flipSign(Tab& t) {
  if (!editable(t) || t.selected < 0 || static_cast<std::size_t>(t.selected) >= t.field.charges.size()) return false;
  const std::size_t at = static_cast<std::size_t>(t.selected);
  t.field.charges[at].charge = -t.field.charges[at].charge;  // +10 -> -10 and back; 0 stays 0
  syncScene(t, at);
  t.stale = true;
  t.dirty = true;
  return true;
}

bool zoomAt(Tab& t, const double factor, const ef::Vec2 anchor) {
  if (!(factor > 0.0) || !std::isfinite(factor)) return false;
  const double z = std::clamp(t.zoom * factor, kZoomMin, kZoomMax);
  if (z == t.zoom) return false;
  // Keep the anchor world point at the same screen position: centre = anchor + (centre - anchor) / factor.
  t.centre = ef::Vec2{anchor.X + (t.centre.X - anchor.X) / factor, anchor.Y + (t.centre.Y - anchor.Y) / factor};
  t.zoom = z;
  return true;
}

bool pan(Tab& t, const ef::Vec2 screenDeltaPx, const double pixelsPerUnit) {
  if (!(pixelsPerUnit > 0.0)) return false;
  t.centre = ef::Vec2{t.centre.X - screenDeltaPx.X / pixelsPerUnit, t.centre.Y + screenDeltaPx.Y / pixelsPerUnit};
  return true;
}

bool resetView(Tab& t) {
  t.zoom = 1.0;
  t.centre = ef::Vec2{0, 0};
  return true;
}

}  // namespace editor
}  // namespace efapp
