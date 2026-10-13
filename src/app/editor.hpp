#pragma once
// The charge editor core (v2 Phase 2, EDIT-01..05): pure, std-only mutations of a tab's charges with
// realtime-draw semantics. Every mutator keeps scene.charges index-aligned with field.charges (buildScene
// order), marks the tab stale (the drawn lines belong to the pre-edit charges) and REFUSES while the tab
// has a live session (computing): editing is a Done/Failed activity. Recomputation happens exclusively
// through Workspace::calculate.
#include <cstddef>

#include "app/workspace.hpp"
#include "ef/vec2.hpp"

namespace efapp {

namespace editor {

// Selecting: the nearest charge within `radius` world units; -1 when none (or the field is empty).
int hitCharge(const ef::ChargeSystem& field, ef::Vec2 world, double radius);

// All mutators return false when the tab is computing or the request is invalid (unknown index, last
// charge, nothing selected). Positions are clamped to [-1e5, 1e5] world units.

bool moveCharge(Tab& t, std::size_t index, ef::Vec2 world);
// Appends a charge at `world`, selects it, marks stale.
bool addCharge(Tab& t, ef::Vec2 world, int value);
// Removes the selected charge; refuses when it is the last one (EDIT-04: at least one charge remains).
bool removeSelected(Tab& t);
// Nudges the selected charge by `delta` (arrow keys; the caller owns the step size incl. Shift).
bool stepSelected(Tab& t, ef::Vec2 delta);
// Value +/- 1 on the selected charge (editor panel buttons).
bool stepValue(Tab& t, int delta);
// Flips the sign of the selected charge's value (+10 -> -10); 0 stays 0 (v2.2 EDIT-09).
bool flipSign(Tab& t);

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
// the canvas" rule is superseded - the seed/hit radii now adapt to the view). The drawn dot (25 px by
// default) must still cover the floored seed circle (kMinSeedWorld = 0.01 world units): view scale
// <= 25 / 0.01 = 2500 device px per world unit, or the floored seed circle pokes out as a white ring.
inline constexpr double kMaxPpu = 2500.0;
// The largest legal zoom for a view whose base scale is `pixelsPerUnitBase` px per world unit WITHOUT
// the zoom factor (canvasView's 100 * HiDPI scale): zoom <= kMaxPpu / pixelsPerUnitBase, clamped to
// [kZoomMin, kZoomMax] (zoom 25 at k = 1; zoom 12.5 at HiDPI k = 2 - unchanged on-screen feel).
// `minCanvasPx` no longer bounds the formula but keeps its parameter slot; degenerate inputs (no canvas
// measured yet) still fall back to the static kZoomMax.
double zoomCeiling(double minCanvasPx, double pixelsPerUnitBase);
// Multiplies the zoom by `factor` while keeping `anchor` (the world point under the mouse) at the same
// screen position. Clamped to [kZoomMin, zoomMax] - the caller passes zoomCeiling(...) as zoomMax
// (kZoomMax when not, e.g. before the first frame has measured the canvas).
bool zoomAt(Tab& t, double factor, ef::Vec2 anchor, double zoomMax = kZoomMax);
// Pans by a screen-space delta in device pixels (right-drag, arrow keys): the world centre moves opposite.
// `pixelsPerUnit` is the tab's current view scale.
bool pan(Tab& t, ef::Vec2 screenDeltaPx, double pixelsPerUnit);
// Back to zoom 1 with the world origin at the canvas centre.
bool resetView(Tab& t);

}  // namespace editor

}  // namespace efapp
