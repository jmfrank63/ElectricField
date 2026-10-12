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

// Rebuilds the whole charge mirror (undo/redo/revert change several charges at once). Lines are untouched.
void syncAllScene(Tab& t) {
  t.scene.charges.clear();
  for (const auto& c : t.field.charges) t.scene.charges.push_back(ef::SceneCharge{c.position, c.charge});
}

// EDIT-12: push the pre-mutation state (charges + method) onto the undo stack; a new branch clears redo.
void recordHistory(Tab& t) {
  t.undo.push_back({t.field, t.params.method});
  if (t.undo.size() > kHistoryLimit) t.undo.erase(t.undo.begin());
  t.redo.clear();
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
  const ef::Vec2 target = clamped(world);
  ef::Charge& c = t.field.charges[index];
  if (c.position.X == target.X && c.position.Y == target.Y) return true;  // nothing to do, no history entry
  // v2.5 (261012-h24): while a drag transaction is open for this index the moves are silent for history —
  // commitDrag records the whole drag as ONE entry (pre-drag state). Every other index keeps per-move
  // history, and everything after this line stays unconditional (realtime-draw semantics untouched).
  if (t.dragIndex < 0 || t.dragIndex != static_cast<int>(index)) recordHistory(t);
  c.position = target;
  syncScene(t, index);
  t.stale = true;
  t.dirty = true;
  t.pendingRecalcAt = -2;
  return true;
}

// v2.5 (261012-h24): begin a mouse-drag transaction by capturing the pre-drag state. No-ops when the
// tab is computing (no NEW drags — D-02), the index is out of range, or a transaction is already open
// (a second beginDrag never overwrites the captured snapshot).
void beginDrag(Tab& t, std::size_t index) {
  if (!editable(t) || index >= t.field.charges.size() || t.dragIndex >= 0) return;
  t.dragIndex = static_cast<int>(index);
  t.dragPre = Tab::HistoryEntry{t.field, t.params.method};  // a value copy, no aliasing
}

// v2.5: close the transaction — the captured pre-drag state becomes ONE undo entry when the drag moved
// the charge. Nothing is recorded when no transaction is open, when the dragged charge was removed
// mid-drag (removals record no history entry in this codebase — the out-of-range index is simply
// discarded, T-h24-03), or when the charge is exactly at its pre-drag position (no net movement /
// returned to the start — the same exact X/Y comparison moveCharge uses).
// commitDrag touches none of stale / dirty / pendingRecalcAt / scene / computed: the per-move semantics
// already did their job, so the undo stack alone carries the coalescing.
void commitDrag(Tab& t) {
  if (t.dragIndex < 0) return;  // no open transaction
  const std::size_t index = static_cast<std::size_t>(t.dragIndex);
  Tab::HistoryEntry pre = std::move(t.dragPre);
  t.dragIndex = -1;  // close first (exception-safe ordering), then decide what to record
  if (index >= t.field.charges.size()) return;  // the dragged charge was removed mid-drag
  const ef::Vec2& now = t.field.charges[index].position;
  const ef::Vec2& was = pre.field.charges[index].position;
  if (now.X == was.X && now.Y == was.Y) return;  // no net movement, nothing to record
  t.undo.push_back(std::move(pre));
  if (t.undo.size() > kHistoryLimit) t.undo.erase(t.undo.begin());  // same cap-erase-oldest rule as recordHistory
  t.redo.clear();  // a new edit branch invalidates redo
}

bool addCharge(Tab& t, ef::Vec2 world, int value) {
  if (!editable(t)) return false;
  t.field.charges.push_back(ef::Charge{value, clamped(world)});
  t.scene.charges.push_back(ef::SceneCharge{clamped(world), value});
  t.selected = static_cast<int>(t.field.charges.size()) - 1;
  t.stale = true;
  t.dirty = true;
  t.pendingRecalcAt = -2;
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
  t.pendingRecalcAt = -2;
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
  const int target = std::clamp(t.field.charges[at].charge + delta, -1000000, 1000000);
  if (target == t.field.charges[at].charge) return true;  // nothing to do, no history entry
  recordHistory(t);
  t.field.charges[at].charge = target;
  syncScene(t, at);
  t.stale = true;
  t.dirty = true;
  t.pendingRecalcAt = -2;
  return true;
}

bool flipSign(Tab& t) {
  if (!editable(t) || t.selected < 0 || static_cast<std::size_t>(t.selected) >= t.field.charges.size()) return false;
  const std::size_t at = static_cast<std::size_t>(t.selected);
  if (t.field.charges[at].charge == 0) return true;  // nothing to flip, no history entry
  recordHistory(t);
  t.field.charges[at].charge = -t.field.charges[at].charge;  // +10 -> -10 and back; 0 stays 0
  syncScene(t, at);
  t.stale = true;
  t.dirty = true;
  t.pendingRecalcAt = -2;
  return true;
}

bool undo(Tab& t) {
  if (!editable(t) || t.undo.empty()) return false;
  t.redo.push_back({t.field, t.params.method});
  if (t.redo.size() > kHistoryLimit) t.redo.erase(t.redo.begin());
  const Tab::HistoryEntry e = std::move(t.undo.back());
  t.undo.pop_back();
  t.field = e.field;
  t.params.method = e.method;
  t.pendingRecalcAt = -2;
  syncAllScene(t);
  if (t.selected >= static_cast<int>(t.field.charges.size())) t.selected = -1;
  t.stale = true;
  t.dirty = true;
  return true;
}

bool redo(Tab& t) {
  if (!editable(t) || t.redo.empty()) return false;
  t.undo.push_back({t.field, t.params.method});
  if (t.undo.size() > kHistoryLimit) t.undo.erase(t.undo.begin());
  const Tab::HistoryEntry e = std::move(t.redo.back());
  t.redo.pop_back();
  t.field = e.field;
  t.params.method = e.method;
  t.pendingRecalcAt = -2;
  syncAllScene(t);
  if (t.selected >= static_cast<int>(t.field.charges.size())) t.selected = -1;
  t.stale = true;
  t.dirty = true;
  return true;
}

bool revertToComputed(Tab& t) {
  if (!editable(t) || !t.stale) return false;  // nothing uncalculated to revert (EDIT-11)
  recordHistory(t);  // the reverted edits stay reachable through Undo
  t.field = t.computed;
  syncAllScene(t);
  if (t.selected >= static_cast<int>(t.field.charges.size())) t.selected = -1;
  t.stale = false;
  t.dirty = false;
  t.pendingRecalcAt = -1.0;  // reverted: nothing pending
  return true;
}

ef::Vec2 snapPoint(const GridSettings& g, ef::Vec2 world) {
  if (!g.active || !(g.spacing > 0.0)) return world;
  const ef::Vec2 rel = world - g.origin;
  if (!g.polar) {
    const ef::Vec2 snapped{std::round(rel.X / g.spacing) * g.spacing, std::round(rel.Y / g.spacing) * g.spacing};
    return g.origin + snapped;
  }
  const double stepRad = g.angleStepDeg * 3.1415926535897932346 / 180.0;
  double r = std::sqrt(rel * rel);
  const double theta = std::atan2(rel.Y, rel.X);
  r = std::round(r / g.spacing) * g.spacing;
  const double thetaSnapped = std::round(theta / stepRad) * stepRad;
  return g.origin + ef::Vec2{r * std::cos(thetaSnapped), r * std::sin(thetaSnapped)};
}

double arrowStep(const GridSettings& g) { return g.active && g.spacing > 0.0 ? g.spacing : 0.1; }

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
