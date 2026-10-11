# 01-01 Summary: Collections core and session collection tag (efapp)

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** COL-01, COL-04, SES extension, EDIT-09, CALC-03, ZOOM/PAN core

## What was built

- `src/core/ef/scene.hpp/.cpp`: `View` gains `centre` (the world point at the canvas centre; default
  origin keeps every existing render byte-identical) - the rendering transform honours it in drawPath
  and the charge loop; this is what makes pan/zoom possible for every backend at once.
- `src/app/ini_text.hpp`: the shared trim/toLower/int/double/Csv helpers factored out of configs.cpp
  (behaviour unchanged; config suites stay green).
- `src/app/session_file.{hpp,cpp}`: `collection = <name>` written as the first key and parsed back
  (older files without it parse as Original); `SavedTab.autoRecalc` persisted as `autocalc = true`;
  `saveSession` overloads (with/without collection) and `loadSessionFull` (collection + tabs).
- `Tab` gains the view and continuous-calculation state: `zoom` (clamped 0.05..20), `centre`,
  `autoRecalc`, `pendingRecalcAt`; `Workspace::calculate` disarms the deadline;
  `Workspace::autoRecalcTick(now)` recomputes due tabs; `Workspace::rename/openRestored(autoRecalc)`;
  `Workspace::anyDirty()`; `Tab.dirty` set by every charge mutator.
- `editor::flipSign` (EDIT-09), `editor::zoomAt` (anchor-preserving, clamped), `editor::pan`,
  `editor::resetView` (view-only: never stale, never dirty).

## Verification

- `zig build test -Dgui=false`: 113/113 cases (incl. scene-centre rendering, flipSign, view mutators,
  dirty tracking, autoRecalcTick flow, session collection/autocalc round-trips).
