# 02-01 Summary: Editor core and explicit calculation (efapp)

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** EDIT-01..05, CALC-01, CALC-02

## What was built

- `Tab.selected` (int, -1 = none) and `Tab.stale` (bool).
- `src/app/editor.{hpp,cpp}` (efapp, std-only, strict flags): `hitCharge` (nearest within radius),
  `moveCharge`, `addCharge` (appends + selects), `removeSelected` (refuses on the last charge), 
  `stepSelected`, `stepValue`. Every mutator refuses while the tab has a live session, keeps
  `scene.charges` index-aligned with `field.charges` (positions and values), clamps positions to
  +/-1e5, and marks `stale`.
- `Workspace::calculate(serial)`: restarts a Done/Failed tab from its current charges and params
  through the same factory, charges-only scene, fresh expected count, Failed-on-error identical to
  `open()`; refuses unknown/still-computing serials.
- `ui_logic`: `worldFromCanvas` / `canvasFromWorld` pure conversions (y-up origin at canvas centre).

## Verification

- `zig build test -Dgui=false`: 102/102 cases (95 + 7 new: hit test, guards while computing, scene sync,
  add/remove invariant, step mutators, calculate cycle on an edited tab incl. method change to RK4,
  conversion round-trip with y flip).

## Notes

- ComputeSession takes the charge system BY VALUE, so edits could never race workers - editing is still
  gated to session-free tabs to keep the stale/calculate story simple.
- One test-side fix along the way: a checked Y coordinate that was actually X (wrong test data, not code).
