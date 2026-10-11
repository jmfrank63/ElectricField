# 01-01/01-02 Summary: Editor depth (v2.3)

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** TITLE-01, COL-05, MENU-01, CALC-04, EDIT-11, EDIT-12, EDIT-13, GRID-01..04

## What was built

- **Title (TITLE-01):** the window title shows the collection: "Electric Field - <Collection> - <Tab>"
  (UTF-8 em dash), "Electric Field - <Collection>" without tabs.
- **Remove configuration (COL-05):** right-click a configuration in the chooser -> confirmation ->
  removed from the current collection file. Open tabs are unaffected.
- **Menus (MENU-01):** File shows "New Configuration..." (Ctrl+N, the chooser = the current collection)
  and "New Collection..."; the bare "New" preset submenu and "New..." are gone (the built-ins live in
  the Original collection; `Workspace::open(index)` stays as internal plumbing for smoke/screenshots).
- **Continuous calculation fixed (CALC-04):** root cause - the GUI re-armed the 0.35 s debounce deadline
  on EVERY frame while the tab was stale, so the deadline slid forever and nothing recomputed. Fix: the
  editor mutators mark `pendingRecalcAt = -2` and the GUI converts the marker to a real deadline exactly
  once; idle frames cannot re-arm. Pinned by a smoke regression: enable continuous calculation, edit
  through the real canvas input path, and the tab recomputes on its own.
- **Esc revert (EDIT-11):** `Tab.computed` snapshots the charges at every (re)computation; Esc on a stale
  tab restores them (the reverted edits stay reachable through Undo).
- **Undo/Redo (EDIT-12):** per-tab history (charges + method, capped at 100) recorded by every mutation;
  Edit > Undo (Ctrl+Z) / Redo (Ctrl+Y); undo/redo mark stale, dirty and re-arm continuous calculation.
- **Grid (GRID-01..04):** per-tab grid settings (active, visible, Cartesian/polar, origin x/y, spacing,
  polar angle step), rendered behind the scene (Cartesian lines; polar circles + rays), snapping applied
  to drag, double-click add and typed positions (no-op when inactive; arrow steps use the grid spacing
  when active). Persisted per tab in the session (`grid = active,visible,polar,ox,oy,spacing,angle`).

## Verification

- `zig build test`: 113 + 4 cases green (new: sign flip, view mutators, dirty tracking, autoRecalcTick,
  session collection/autocalc/grid round-trips, scene-centre rendering, windowTitle with collection).
- `zig build smoke` green: `editor=ok` now also covers wheel zoom and the continuous-calculation
  regression through the real canvas input path.

## Notes

- The v2.2 continuous-calculation bug was the GUI re-arming the deadline on every frame; the fix moves
  arming into a marker set only by real edits (CALC-04 regression test in the smoke).
- A transient `AccessDenied` during verification was the walkthrough instance holding the old exe;
  the failing smoke had also run against that stale binary. Re-verified on the fresh build.
