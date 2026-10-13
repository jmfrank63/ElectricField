#pragma once
// The charge editor core (v2 Phase 2, EDIT-01..05): pure, std-only mutations of a tab's charges with
// realtime-draw semantics. Every mutator keeps scene.charges index-aligned with field.charges (buildScene
// order), marks the tab stale (the drawn lines belong to the pre-edit charges) and REFUSES while the tab
// has a live session (computing): editing is a Done/Failed activity. Recomputation happens exclusively
// through Workspace::calculate.
#include <cstddef>
#include <cstdint>
#include <string>

#include "app/workspace.hpp"
#include "ef/vec2.hpp"

namespace efapp {

namespace editor {

// Selecting: the nearest charge within its own bound (world units). Each charge's bound is its
// effective radius converted to world units plus a fixed slack:
//   (sizePx > 0 ? sizePx : defaultRadiusPx) / pixelsPerUnit + kEditorHitSlack
// `defaultRadiusPx` and `pixelsPerUnit` must be in the SAME (device) space: ui.cpp passes
// view.pixelsPerUnit with defaultPx * k. At the defaults this is byte-identical to the old fixed
// 0.30 bound (25 px / 100 ppu + 0.05). pixelsPerUnit <= 0 degenerates to 1.0. -1 = no hit.
inline constexpr double kEditorHitSlack = 0.05;
int hitCharge(const ef::ChargeSystem& field, ef::Vec2 world, double pixelsPerUnit, double defaultRadiusPx);

// The effective display name (v2.6 phase 3): the stored name, or the derived "Charge N" (1-based
// index). The model never stores the derived form - this is the one derivation site.
inline std::string chargeDisplayName(const ef::Charge& c, std::size_t index) {
  return c.name.empty() ? "Charge " + std::to_string(index + 1) : c.name;
}

// All mutators return false when the tab is computing or the request is invalid (unknown index, last
// charge, nothing selected). Positions are clamped to [-1e5, 1e5] world units.

bool moveCharge(Tab& t, std::size_t index, ef::Vec2 world);
// v2.6 phase 3: charge identity. nextChargeId is the pure "max over the system + 1" (empty -> 1);
// assignChargeIds fills ONLY the id == 0 charges, monotonically (persisted ids are never touched).
// makeSystem stays field-carrying (id 0) so the ef-cli parity output is untouched.
std::uint64_t nextChargeId(const ef::ChargeSystem& field);
void assignChargeIds(ef::ChargeSystem& field);
// Appends a charge at `world`, selects it, marks stale. Assigns the id itself (max over the system,
// and never below the tab's high-water mark: a removed charge's id is not reused) and stores
// `defaultSizePx` after validating (0 or 2..120 px; anything else degrades to 0 = inherit the
// Options default - the caller passes it).
bool addCharge(Tab& t, ef::Vec2 world, int value, float defaultSizePx = 0.f);
// Removes the selected charge; refuses when it is the last one (EDIT-04: at least one charge remains).
bool removeSelected(Tab& t);
// Nudges the selected charge by `delta` (arrow keys; the caller owns the step size incl. Shift).
bool stepSelected(Tab& t, ef::Vec2 delta);
// Value +/- 1 on the selected charge (editor panel buttons).
bool stepValue(Tab& t, int delta);
// Flips the sign of the selected charge's value (+10 -> -10); 0 stays 0 (v2.2 EDIT-09).
bool flipSign(Tab& t);

// v2.6 phase 3: identity edits. Name/size are collection-visible state: both mark the tab DIRTY only
// (never stale, never pendingRecalcAt - the drawn lines are not a function of a name or size) and
// update the scene in place where it mirrors the field (setChargeSize syncs sizePx; the name has no
// scene copy - the tooltip reads field.charges). Undo/redo ride the existing ChargeSystem snapshots.
// setChargeName strips ',' (the persistence field separator); an identical name is a no-op that
// returns true without a history entry. setChargeSize validates 0 or 2..120 px (else false) and is a
// no-op for an identical size.
bool setChargeName(Tab& t, std::size_t index, const std::string& name);
bool setChargeSize(Tab& t, std::size_t index, float sizePx);

// v2.3 EDIT-12: history. Every successful charge/method mutation records the pre-state (capped);
// undo/redo restore it. Both refuse while the tab is computing.
inline constexpr std::size_t kHistoryLimit = 100;
bool undo(Tab& t);
bool redo(Tab& t);
// v2.5 (261012-h24): mouse-drag transactions. beginDrag captures the pre-drag state; while the
// transaction is open, moveCharge of that same index records nothing (the moves themselves keep the
// realtime-draw semantics untouched). commitDrag turns the captured state into ONE undo entry when the
// drag moved the charge — a no-op when no transaction is open, when the dragged charge was removed
// mid-drag, or when the charge is exactly at its pre-drag position (no net movement / returned to the
// start, the same exact X/Y comparison moveCharge uses). Pure std-only, no ImGui/SDL types.
void beginDrag(Tab& t, std::size_t index);
void commitDrag(Tab& t);
// True while a drag transaction is open on the tab (inline like kHistoryLimit, one definition site).
inline bool dragActive(const Tab& t) { return t.dragIndex >= 0; }
// v2.3 EDIT-11: restore the charges of the last (re)computation (Esc). False when the tab has no
// uncalculated changes.
bool revertToComputed(Tab& t);

// v2.3 GRID: snap a world position to the grid lattice (no-op when the grid is inactive).
ef::Vec2 snapPoint(const GridSettings& g, ef::Vec2 world);
// The arrow-key step in world units: the grid spacing when snapping is on, else 0.1.
double arrowStep(const GridSettings& g);

// View controls (v2.2 zoom/pan). View-only: they never mark the tab stale and never touch the computation.
inline constexpr double kZoomMin = 0.05;
inline constexpr double kZoomMax = 40.0;
// v2.6 phase 2: zooming IN is limited by the adapted calculation (the h25 "at least 1 world unit across
// the canvas" rule is superseded - the seed/hit radii now adapt to the view). At the ceiling the drawn
// dot - the user's charge size (Options draw.chargeRadiusPx, logical px) - must still cover the floored
// seed circle (kMinSeedWorld = 0.01 world units), or the floored circle pokes out as a white ring.
// The largest legal zoom for a view whose base scale is `ppuBase` px per world unit WITHOUT the zoom
// factor: zoom <= dotRadiusPx / kMinSeedWorld / ppuBase, clamped to [kZoomMin, kZoomMax] (zoom 25 with
// the default 25 px dot at base 100; zoom 2 with the "from a dot" 2 px setting). Callers pass the base
// and the dot in the SAME space - the GUI uses logical px, which makes the ceiling display-scale-
// independent. Degenerate inputs (nothing measured/applied yet) fall back to the static kZoomMax.
double zoomCeiling(double ppuBase, double dotRadiusPx);
// Multiplies the zoom by `factor` while keeping `anchor` (the world point under the mouse) at the same
// screen position. Clamped to [kZoomMin, zoomMax] - the caller passes zoomCeiling(...) as zoomMax
// (kZoomMax when not, e.g. before the first frame has measured the canvas).
bool zoomAt(Tab& t, double factor, ef::Vec2 anchor, double zoomMax = kZoomMax);
// Pans by a screen-space delta in device pixels (right-drag, arrow keys): the world centre moves opposite.
// `pixelsPerUnit` is the tab's current view scale.
bool pan(Tab& t, ef::Vec2 screenDeltaPx, double pixelsPerUnit);
// Back to zoom 1 with the world origin at the canvas centre.
bool resetView(Tab& t);

// v2.6 phase 2 (WR-05): the GUI canvas's zoom-settle arming decision, extracted from the ImGui layer so
// the arm -> viewRecalcTick -> calculate composition is doctested (deleting it used to pass every
// suite). Call once per frame for a tab with the tab's ZOOM-RELATIVE (logical) ppu (100 * zoom). Done
// tabs only (Done and no live session), and never on the first frame (armedViewPpu starts 0): a ppu
// change either arms pendingViewRecalcAt = -2 - the pending recalculation would run in a DIFFERENT
// regime than the last computation (returns true) - or disarms it (-1: the settle would reproduce the
// identical legacy calculation). viewPpu/armedViewPpu are stamped on every call. A settled ppu in
// (kRefPpu, kRefPpuEps] is the legacy regime (WR-01 fp round-trip drift) and snaps the stamp down to
// kRefPpu; adapts() itself stays strict.
bool armViewRecalc(Tab& t, double logicalPpu);

}  // namespace editor

}  // namespace efapp
