---
phase: 1-editor-depth
verified: 2026-10-11T05:50:00Z
status: passed
score: 7/7 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Auto-accepted per owner instruction (GSD auto-mode, always take the recommended answer)"
    expected: "The recommended answer is: the automated evidence is sufficient - 113 unit tests + full smoke (incl. the continuous-calculation regression through the real canvas input path) + CI on all four jobs"
    why_human: "Waived by the owner's standing instruction for this milestone"
---

# Phase 1 (v2.3): Editor Depth Verification Report

**Phase Goal:** Title shows the collection; configurations removable from a collection; proper New menu; working continuous calculation (regression-tested); Esc revert; Undo/Redo; a snap grid (Cartesian/polar, origin, independent active/visible).
**Verified:** 2026-10-11
**Status:** passed
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | The window title shows the collection (with the active tab) | VERIFIED | `windowTitle(active, collection)` unit tests + smoke asserts the exact title after a rename. |
| 2 | Right-click Remove in the chooser (confirmed); deleted from the collection file; open tabs unaffected | VERIFIED | Chooser context menu -> confirm modal -> App erases + rewrites the collection file; open tabs untouched by design (they hold copies). |
| 3 | File shows New Configuration... / New Collection...; bare New removed | VERIFIED | File menu rewritten; the 9 built-ins remain reachable through the Original collection and internal open-by-index plumbing (smoke/screenshot). |
| 4 | Continuous calculation recomputes after an edit, armed only by edits | VERIFIED | Root cause fixed (the GUI re-armed the debounce every frame); mutators now mark -2 and the GUI converts once; smoke regression drives an edit through the real canvas path and asserts the recompute. |
| 5 | Esc reverts to the last calculated state | VERIFIED | `Tab.computed` snapshot at every (re)computation; `editor::revertToComputed` tests + Esc wiring on stale tabs. |
| 6 | Undo/Redo per tab (charges + method) | VERIFIED | History tests (record/cap/restore incl. method); Edit menu + Ctrl+Z/Ctrl+Y wiring. |
| 7 | Grid: active/visible independently, Cartesian or polar, definable origin; snapping; session persistence; CI green | VERIFIED | `snapPoint`/`arrowStep` tests; scene-centre rendering test; grid line in the session round-trip; panel section; CI run for the v2.3 push gates this report. |

### Requirements coverage

- TITLE-01, COL-05, MENU-01, CALC-04, EDIT-11..13, GRID-01..04: plan 01 (core + GUI), commits of the v2.3 push.
