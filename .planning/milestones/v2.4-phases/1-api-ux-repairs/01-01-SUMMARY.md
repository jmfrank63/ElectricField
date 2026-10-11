# 01-01 Summary: Command API, script runner, grid fix (efapp + core)

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** API-01, API-02, API-04, BUG-01, BUG-02 (core half)

## What was built

- `src/core/ef/grid.hpp`: `ef::GridSettings` moved into the core (scene.hpp re-exports it) so the
  renderer can draw it.
- `renderScene(painter, scene, style, view, const GridSettings*)`: the grid is now a core pass -
  background fill -> grid -> paths -> charges in ONE call, so the canvas background can never
  overwrite the grid again (BUG-01). ef-cli/SVG export pass nullptr and stay gridless. Line/circle
  counts are bounded (2000) so a pathological spacing cannot stall a frame.
- `App::command(line)` (src/gui/app.cpp): the single dispatcher - app.*/collection.*/config.*/tab.*/
  charge.*/editor.*/view.*/grid.set/options.set commands, "ok" or "error: <why>" per call; quoted
  tokens for names with spaces; `app.state` dumps collection + collections + configs + every tab
  (state, method, autocalc, stale, dirty, selected, lines, zoom, centre, grid, charges);
  `app.expect <text>` asserts against the dump; `app.quit` refuses with dirty tabs unless `force`.
- Collection actions behind the commands: saveCollection (merge matching edited tabs via baseTitle,
  mark them clean), saveCollectionAs (tabs stay open), closeCollection (save + switch to Original),
  removeConfiguration, saveConfigurationAs.
- Per-collection sessions: `Sessions/<Collection>.ini` holds the tabs (incl. grid settings now);
  `session.ini` is reduced to the pointer (`collection = <name>`); legacy content migrates once at
  startup; switching saves the outgoing and restores the incoming tabs.
- `--user-dir <dir>` redirects ALL user-file paths; scripts run in a temp sandbox by default
  (API-04). Restored tabs establish the active tab (the CI-caught fix, fe96c4f).

## Verification

- `zig build test`: 114 + 4 cases green (incl. the grid-pass regression: grid colour in strokes,
  absent for nullptr and when visible=false).
- Script suite (5 scenarios, via --script with reliable exit codes): ALL PASSED.
