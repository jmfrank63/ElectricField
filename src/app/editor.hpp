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

}  // namespace editor

}  // namespace efapp
