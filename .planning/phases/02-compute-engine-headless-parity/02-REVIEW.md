---
phase: 02-compute-engine-headless-parity
reviewed: 2026-10-10T00:00:00Z
depth: standard
iteration: 3
files_reviewed: 5
files_reviewed_list:
  - src/core/ef/session.cpp
  - src/core/ef/session.hpp
  - tests/test_finalizer.cpp
  - tests/test_session.cpp
  - tools/baseline/capture-mfc-baseline.ps1
findings:
  critical: 0
  warning: 0
  info: 6
  total: 6
status: clean
---

# Phase 2: Code Review Report (iteration 3, final re-review)

**Reviewed:** 2026-10-10
**Depth:** standard
**Files Reviewed:** 5 in iteration 3 (diff 234b2be..HEAD, plus finalizer.cpp for context); iteration 1 covered all 33 phase files, iteration 2 the 10 files touched by fixes.
**Status:** clean — no Critical or Warning findings remain; remaining Info items are out of scope.

## Summary

All eleven warnings (WR-01..WR-11) are resolved; IN-01, IN-07 and IN-08 are addressed. No new Critical or Warning issues. IN-02..IN-06 (iteration 1) remain open and out of scope; one new minor Info note (IN-09).

## Resolution status

| ID | Status | Verification |
|----|--------|--------------|
| WR-01 | RESOLVED | `finish()` guards move-out, sort, copies and `finalize` with try/catch → `fail()`; `done_` always published and `doneCv_` notified; lines moved, not copied |
| WR-02 | RESOLVED | `stop_` honoured in `finish()` and inside `finalize(..., stop)`; a stop publishes as cancelled with no result |
| WR-03 | RESOLVED | `appendNum` handles non-finite values and `snprintf` return value |
| WR-04 | RESOLVED | ef-cli reports SVG open and flush failures |
| WR-05 | RESOLVED | ef-cli `main()` catches all exceptions → exit 1 |
| WR-06 | RESOLVED | strict integer parsing with range checks; contradictory flags exit 2 |
| WR-07 | RESOLVED | strict `--expect`: 4 fields per row, duplicates/unknown slugs fail, rendered vs expected preset sets compared |
| WR-08 | RESOLVED | PNG export uses RAII guards; destruction order painter → BackendGuard → CtxGuard (restores caller context) → renderer → surface |
| WR-09 | RESOLVED | cancel-latency asserted on best of up to 3 attempts (retry only after a miss); deterministic assertions run every attempt |
| WR-10 | RESOLVED | existing options backup is never deleted/overwritten; interrupted-run guard aborts |
| WR-11 | RESOLVED | publication condition `haveR && !stop_.load()` evaluated under `m_`; header contract "a cancel() that has returned before publication wins" is precisely true (seq_cst store/load); status and `takeResult()` always agree; no lock nesting, no lost wakeup, destructor join completes |
| IN-01 | RESOLVED | `<algorithm>` included in ef-cli |
| IN-07 | ADDRESSED | deterministic tests: finalizer honours a set stop flag; cancel after done is a no-op; cancel once all jobs complete yields cancelled XOR result. `finish()` catch path untested (needs an allocation-failure seam) — accepted |
| IN-08 | RESOLVED | partial `.reg` export removed before throwing; cannot delete a user backup (guard aborts earlier) |
| IN-02..IN-06 | OPEN (out of scope) | see iteration-1 findings below |

## Info (open, out of scope)

- **IN-02:** `tools/parity/compare-mfc.sh` — no temp-dir trap, junk CSV on magick errors, 0.000 on blank baseline.
- **IN-03:** `src/gui/main.cpp` — unchecked backend Init, early-return leaks, busy loop (Phase 3 replaces this file; WR-02 of Phase 1 deferred there).
- **IN-04:** `.github/workflows/ci.yml` — double runs on push+PR; `cancel-in-progress` also cancels master builds.
- **IN-05:** `Painter::strokePath` contract (`n >= 1 + 3*bezierSegments`) not defended in SVG/ImGui backends.
- **IN-06:** ef-cli paths are ANSI code page on Windows while `SDL_SavePNG` expects UTF-8.
- **IN-09 (new):** the "cancel after all jobs completed" test is interleaving-safe but cannot force the WR-11 publication branch; coverage is statistical (24/24 observed). Optional test hook later.

---

_Reviewed: 2026-10-10_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
