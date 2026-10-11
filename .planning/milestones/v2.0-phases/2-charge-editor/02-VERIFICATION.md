---
phase: 2-charge-editor
verified: 2026-10-11T02:50:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Editor walkthrough in the real window"
    expected: "Click a charge selects it (blue ring); dragging moves the glyph in realtime; arrow keys nudge (Shift = bigger); double-click empty canvas adds a charge; Delete removes the selected one but never the last; the panel's -/+ buttons move the charge while held; typing exact numbers into value/x/y works; the stale hint appears after edits and Calculate (F5) replaces the lines; the Method combo applies at the next Calculate; Save As stores the edited charges"
    why_human: "Pointer feel, realtime glyph tracking and the drag experience are not observable headlessly"
---

# Phase 2 (v2): Charge Editor & Explicit Calculation Verification Report

**Phase Goal:** As a user, I want to place and fine-tune charges in the canvas - by mouse, cursor keys and numeric fields with up/down buttons - see the charges move in realtime, and recompute explicitly with a Calculate button and a per-calculation method.
**Verified:** 2026-10-11
**Status:** passed (human verification approved 2026-10-11)
**Re-verification:** No, initial verification

## Human Verification Results (2026-10-11)

User verdict: "All good — continue".

| Human test | Result |
|---|---|
| Editor walkthrough in the real window (select/drag/keys/double-click/Delete, panel fields with -/+ buttons, stale hint, Calculate F5, method combo, Save As of edited charges) | pass - user approved |

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Click selects (nearest within 0.30 wu), drag moves in realtime, arrow keys nudge (Shift = 0.5), double-click adds, Delete removes with the >= 1 invariant | VERIFIED (logic) + human (feel) | `tests/test_editor.cpp`: hit test nearest/none, mutator guards, add+select, remove invariant; smoke `editor=ok` drives click-select, drag (+60 px -> +0.6 wu), double-click add and Delete through the real input queue headlessly. |
| 2 | Panel fields with -/+ buttons update the charge immediately, without starting a computation | VERIFIED (logic) + human (feel) | All field mutations route through `efapp::editor` mutators which never touch the session; `stepValue`/`moveCharge` tests prove field+scene sync and stale marking; the smoke asserts no recompute happens implicitly (stale stays set until F5). |
| 3 | Calculate (F5) recomputes from the current charges; edited tabs are marked stale until then | VERIFIED | `Workspace::calculate` tests: refusal while computing/unknown, recompute from moved charges with fresh lines and stale cleared; smoke asserts stale after edits and its absence after F5-completion. |
| 4 | The method selector defaults to the app option and applies at the next Calculate | VERIFIED | The panel edits the tab's captured `params.method` (which came from the app option at open); the editor test changes Euler->RK4 and proves the recomputed tab carries it; no implicit recompute (prohibited + tested). |
| 5 | CI green on all three OSes + TSan; smoke covers add/select/move/calculate; README documents the editor | VERIFIED | README feature paragraphs added; smoke `editor=ok`; CI run 38106137162 (push 247fce4): success on windows-x64 (6m46s), linux-x64 (4m57s), macos-arm64 (6m31s), tsan-linux (2m56s). |

### Requirements coverage

- EDIT-01..05, CALC-01..02: plans 02-01 (commit f771208, core) and 02-02 (commit f771208, GUI + smoke + README).

### Known deviations (accepted)

- None in this phase. (Carried from v1: the icon-only .rc and the windows.h crash-dialog suppression.)
