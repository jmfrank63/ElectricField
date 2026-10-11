---
phase: 1-user-configuration-files
verified: 2026-10-11T03:20:00Z
status: passed
score: 3/3 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Save a configuration under a new name, reopen it from the chooser, restart to confirm persistence"
    expected: "Save As writes configurations.ini; the chooser lists it; it reopens with the same charges; a restart keeps it"
    why_human: "A real process restart and the chooser interaction are not observable headlessly"
---

# Phase 1 (v2): User Configuration Files Verification Report

**Phase Goal:** As a user, I want to save my charge arrangements by name and reopen them later, so that my own configurations are first-class citizens next to the 9 built-in presets.
**Verified:** 2026-10-11
**Status:** passed (tests + human approval recorded with the Phase 2 walkthrough, 2026-10-11)
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | configurations.ini stores named charge sets; invalid entries fall back safely; presets unaffected | VERIFIED | `tests/test_configs.cpp`: round-trip, skip-and-count rules, duplicate names, reserved names, file round-trip; presets suite untouched. |
| 2 | The chooser lists user configurations below the built-ins; opening one creates a tab; "Empty Configuration" opens an editable tab | VERIFIED | Chooser integration (commit ba18cb9); smoke `user_configs=ok` opens and computes both; human-approved in the Phase 2 walkthrough. |
| 3 | Save As stores the current tab's charges under a chosen name; overwrite asks; presets cannot be overwritten | VERIFIED | `upsertUserConfig` tests (EMPTY/RESERVED/in-place/append); Save As modal with overwrite confirmation; human-approved in the Phase 2 walkthrough (Save As of edited charges + reopen from the chooser). |

## Human Verification Results (2026-10-11)

Phase 1's surface (Save As, chooser listing, restart persistence) was exercised by the owner as part of the
Phase 2 walkthrough: "Save As stores the edited charges (reopen them from the New chooser)" - "All good".
