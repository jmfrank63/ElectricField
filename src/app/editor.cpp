#include "app/editor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "app/ini_text.hpp"

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
    t.scene.charges[index].sizePx = t.field.charges[index].sizePx;
  }
}

// Rebuilds the whole charge mirror (undo/redo/revert change several charges at once). Lines are untouched.
void syncAllScene(Tab& t) {
  t.scene.charges.clear();
  for (const auto& c : t.field.charges) t.scene.charges.push_back(ef::SceneCharge{c.position, c.charge, c.sizePx});
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

std::uint64_t nextChargeId(const ef::ChargeSystem& field) {
  std::uint64_t maxId = 0;
  for (const auto& c : field.charges)
    if (c.id > maxId) maxId = c.id;
  return maxId + 1;  // empty system -> 1
}

void assignChargeIds(ef::ChargeSystem& field) {
  for (auto& c : field.charges)
    if (c.id == 0) {
      const std::uint64_t next = nextChargeId(field);  // each assignment raises the max: monotonic within the call
      // WR-02: never mint an id the parsers would reject on the next load - saturate to unassigned.
      if (next > ef::kMaxChargeId) return;
      c.id = next;
    }
}

int hitCharge(const ef::ChargeSystem& field, ef::Vec2 world, const double pixelsPerUnit, const double defaultRadiusPx) {
  const double ppu = pixelsPerUnit > 0.0 ? pixelsPerUnit : 1.0;  // degenerate view scale: 1 px per unit
  int best = -1;
  double bestDist = 0;
  for (std::size_t i = 0; i < field.charges.size(); ++i) {
    const ef::Charge& c = field.charges[i];
    const ef::Vec2 d = c.position - world;
    const double dist = std::sqrt(d * d);
    const double bound = (c.sizePx > 0.f ? static_cast<double>(c.sizePx) : defaultRadiusPx) / ppu + kEditorHitSlack;
    if (dist <= bound && (best < 0 || dist <= bestDist)) {  // nearest wins among each charge's own bound
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

bool addCharge(Tab& t, ef::Vec2 world, int value, float defaultSizePx) {
  if (!editable(t)) return false;
  const ef::Vec2 at = clamped(world);
  // Identity is model logic: max over the system, and never below the tab's high-water mark, so a
  // removed charge's id is not reused (the watermark deliberately survives undo/redo snapshots).
  // WR-02: past the parsers' shared ceiling the id saturates to 0 (unassigned - derived display,
  // still addressable by index) instead of an id a save->load round-trip would drop.
  const std::uint64_t next = std::max(nextChargeId(t.field), t.chargeIdWater + 1);
  t.chargeIdWater = next;  // the watermark advances regardless: the saturated range stays retired
  const std::uint64_t id = next <= ef::kMaxChargeId ? next : 0;
  const float sizePx = defaultSizePx == 0.f || (defaultSizePx >= 2.f && defaultSizePx <= 120.f) ? defaultSizePx : 0.f;
  t.field.charges.push_back(ef::Charge{value, at, id, std::string(), sizePx});
  t.scene.charges.push_back(ef::SceneCharge{at, value, sizePx});
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

// v2.6 phase 3: identity edits - dirty only, never stale, never a pending recalculation (the drawn
// lines are not a function of a name or a size). Same refusal/no-op discipline as moveCharge.
bool setChargeName(Tab& t, std::size_t index, const std::string& name) {
  if (!editable(t) || index >= t.field.charges.size()) return false;
  std::string clean = name;
  clean.erase(std::remove(clean.begin(), clean.end(), ','), clean.end());  // ',' is the field separator on disk
  // WR-03: splitCsv trims every field on read, so a trimmable-only difference would mutate on
  // save->load (" Pad" -> "Pad", whitespace-only -> the derived display). Trim at the entry point
  // instead: the model can never hold it, and the round-trip is the identity.
  clean = ini::trim(clean);
  if (clean == t.field.charges[index].name) return true;                   // nothing to do, no history entry
  recordHistory(t);
  t.field.charges[index].name = std::move(clean);
  t.dirty = true;  // collection-visible state, NOT a recalculation request
  return true;
}

bool setChargeSize(Tab& t, std::size_t index, float sizePx) {
  if (!editable(t) || index >= t.field.charges.size()) return false;
  if (!(sizePx == 0.f || (sizePx >= 2.f && sizePx <= 120.f))) return false;  // 0 = inherit, else 2..120 px
  if (sizePx == t.field.charges[index].sizePx) return true;                  // nothing to do, no history entry
  recordHistory(t);
  t.field.charges[index].sizePx = sizePx;
  syncScene(t, index);  // the canvas redraws the dot at once (sizePx is a draw property only)
  t.dirty = true;
  return true;
}

// WR-04: the physics slice of a ChargeSystem - the only part the drawn lines depend on. Names,
// sizes and ids are excluded: undoing them leaves the lines correct, so the tab must not claim
// "the lines show the previous calculation". Exact == comparisons, the same convention moveCharge
// and commitDrag use for no-op detection.
bool samePhysics(const ef::ChargeSystem& a, const ef::ChargeSystem& b) {
  if (a.charges.size() != b.charges.size()) return false;
  for (std::size_t i = 0; i < a.charges.size(); ++i)
    if (a.charges[i].charge != b.charges[i].charge || a.charges[i].position.X != b.charges[i].position.X ||
        a.charges[i].position.Y != b.charges[i].position.Y)
      return false;
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
  // WR-04: stale only when the restored PHYSICS differs from the computed charges the lines show.
  // Undoing a name/size-only edit restores byte-identical physics: no false "press Calculate" state.
  t.stale = !samePhysics(t.field, t.computed);
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
  t.stale = !samePhysics(t.field, t.computed);  // WR-04: same rule as undo
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

double zoomCeiling(const double ppuBase, const double dotRadiusPx) {
  if (!(ppuBase > 0.0) || !std::isfinite(ppuBase) || !(dotRadiusPx > 0.0) || !std::isfinite(dotRadiusPx))
    return kZoomMax;
  // v2.6 phase 2 (WR-02): the ceiling derives from the APPLIED dot - at the ceiling the floored seed
  // circle (kMinSeedWorld) exactly fills the drawn dot on screen. A small "from a dot" charge size
  // therefore caps zoom-in earlier instead of re-exposing the untraced white annulus.
  return std::clamp(dotRadiusPx / ef::ComputeParams::kMinSeedWorld / ppuBase, kZoomMin, kZoomMax);
}

// 261020-ef1: the SMALLEST effective dot in the tab drives the ceiling (see editor.hpp).
double tabZoomCeiling(const Tab& t, const double ppuBase, const double defaultDotPx) {
  double dot = defaultDotPx;
  for (const auto& c : t.field.charges) {
    const double s = c.sizePx > 0.f ? static_cast<double>(c.sizePx) : defaultDotPx;
    if (s > 0.0 && (dot <= 0.0 || s < dot)) dot = s;
  }
  return zoomCeiling(ppuBase, dot);
}

bool zoomAt(Tab& t, const double factor, const ef::Vec2 anchor, const double zoomMax) {
  if (!(factor > 0.0) || !std::isfinite(factor)) return false;
  const double ceiling = std::clamp(zoomMax, kZoomMin, kZoomMax);
  const double z = std::clamp(t.zoom * factor, kZoomMin, ceiling);
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

bool armViewRecalc(Tab& t, const double logicalPpu) {
  if (t.state != TabState::Done || t.session != nullptr) return false;  // arming is a Done-tab activity
  if (!(logicalPpu > 0.0) || !std::isfinite(logicalPpu)) return false;  // defensive: zoom is always clamped > 0
  double ppu = logicalPpu;
  // WR-01: a settled ppu a hair above the reference is the legacy regime - snap the stamp down so the
  // guard and any compute this tab later runs (zoom settle, edit recalc, F5) stay regime-stable.
  if (ppu > ef::ComputeParams::kRefPpu && ppu <= ef::ComputeParams::kRefPpuEps) ppu = ef::ComputeParams::kRefPpu;
  const bool sameRegime = ppu <= ef::ComputeParams::kRefPpu &&
                          (t.params.viewScale == 0 || t.params.viewScale <= ef::ComputeParams::kRefPpuEps);
  bool armed = false;
  if (t.armedViewPpu != 0 && ppu != t.armedViewPpu) {
    t.pendingViewRecalcAt = sameRegime ? -1.0 : -2;
    armed = !sameRegime;
  }
  t.viewPpu = ppu;
  t.armedViewPpu = ppu;
  return armed;
}

}  // namespace editor
}  // namespace efapp
