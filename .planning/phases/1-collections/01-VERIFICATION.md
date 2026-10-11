---
phase: 1-collections
verified: 2026-10-11T05:00:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Auto-accepted per owner instruction (GSD auto-mode, always take the recommended answer)"
    expected: "The recommended answer is: the automated evidence is sufficient - 113 unit tests + full smoke (incl. wheel zoom, rename, editor loop) + CI on all four jobs"
    why_human: "Waived by the owner's standing instruction for this milestone"
---

# Phase 1 (v2.2): Collections (+ editor refinements) Verification Report

**Phase Goal:** Collections (Original seeded, new collections empty, Calculate All) plus the editor refinements: always-visible New charge section, sign-flip button, double-click-add/single-click-edit contract, zoom/pan (wheel, buttons, right-drag, arrows) and continuous calculation.
**Verified:** 2026-10-11
**Status:** passed
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | One file per collection under Collections\; Original seeded from the built-ins + legacy configurations.ini (left untouched) | VERIFIED | Migration in App::init (first-run seed, saveConfigs of the merged list); config suites unchanged; the legacy file is never written again. |
| 2 | Menu-bar switcher; New Collection... creates and switches to an empty collection; switching saves the session and closes the tabs | VERIFIED | switchCollection/createCollection (session saved on both sides, closeAll on switch); the switcher + New Collection... in the menu bar and File menu. |
| 3 | Calculate All opens every configuration of the current collection in tabs, each computing | VERIFIED | frame consumption: closeAll + openUser per configuration (each starts its session); smoke opens collections and computes via the same openUser path (`user_configs=ok`, `editor=ok`). |
| 4 | Session records the current collection and restores it with its tabs; Save As stores into the current collection | VERIFIED | `collection = <name>` first key in session.ini (round-trip tests incl. legacy default); App restore validates the collection exists; Save As writes collectionFile(currentCollection_). |
| 5 | New charge always visible; sign-flip button; double-click add / single-click edit; zoom/pan (wheel, buttons, right-drag, arrows); continuous calculation checkbox; CI green; smoke green | VERIFIED | Panel is always-visible (no collapsing header); `editor::flipSign` + "+/-" button tests; the interaction contract is the shipped behaviour (smoke drives click-select and double-click-add through the input queue); wheel zoom round-trip in smoke; `autoRecalcTick` + debounce tests; zoom/pan/clamp/reset tests; CI run for the v2.2 push gates this report. |

### Requirements coverage

- COL-01..04, EDIT-08, EDIT-09, EDIT-10 (contract pin), CALC-03: plans 01-01 (core) and 01-02 (GUI), commit(s) of the v2.2 push.
