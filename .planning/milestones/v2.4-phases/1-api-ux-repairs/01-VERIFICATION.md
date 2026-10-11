---
phase: 1-api-ux-repairs
verified: 2026-10-11T15:30:00Z
status: passed
score: 6/6 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Auto-accepted per owner instruction (GSD auto-mode, always take the recommended answer)"
    expected: "The recommended answer is: the automated evidence is sufficient - 114 unit tests + full smoke (incl. the BUG-02 leave-prompt regression through the real UI path) + 5 script scenarios passing + CI on all four jobs"
    why_human: "Waived by the owner's standing instruction for this milestone"
---

# Phase 1 (v2.4): Automation API & UX Repairs Verification Report

**Phase Goal:** The command API + script runner covering every manual action; the invisible-grid fix; the New Collection lockup fix; the collection-centric File menu; the tab context menu.
**Verified:** 2026-10-11
**Status:** passed
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Every menu action and editor/collection operation callable through the command API; script runner; CI script suite on 3 OSes | VERIFIED | `App::command(line)` dispatcher (app/tab/config/charge/view/grid/options/editor/collection commands); `--script/--user-dir/--out/--lenient` runner; 5 scenario scripts in `tests/scripts/` executed by the new "Command-API script tests" CI step on windows-x64, linux-x64, macos-arm64; menu->command parity documented in docs/api.md. |
| 2 | Grids visible, snapping unchanged (BUG-01) | VERIFIED | The grid became a core render pass: `renderScene(..., const GridSettings*)` draws bg -> grid -> lines -> charges in ONE pass, so the canvas background can never overwrite the grid again (the v2.3 bug drew the grid before the background fill). Unit regression in test_scene.cpp (grid colour present in strokes, absent for nullptr/ef-cli path, absent when visible=false). |
| 3 | New Collection does not crash or lock; script regression | VERIFIED | Root cause (BUG-02): after the leave prompt's answer was consumed the modal stayed open as a ghost (closePopups now closes every modal exactly once, and the App sets it on the quit/switch paths too). The 30-collections script runs collection.new + collection.close + collection.save-as clean; smoke drives the leave prompt through the real UI path (iterate()) and asserts it closes. |
| 4 | Tab close/rename reliable; tab right-click menu | VERIFIED | tab.close/tab.rename commands; BeginPopupContextItem menu (Close/Rename.../Save Configuration As.../Calculate); rename modal; the CI-caught fix: restored tabs now establish the active tab so tab.close resolves "active" (fe96c4f). |
| 5 | Collection-centric File menu; per-collection sessions | VERIFIED | File: New Configuration.../New Collection.../Close Tab/Close Collection/Save Collection (Ctrl+S)/Save Collection As.../Save Configuration As.../Calculate All/Export/Exit. Save Collection merges matching edited tabs (baseTitle) and marks them clean; Save Collection As keeps tabs open; Close Collection saves and switches to Original; Sessions/<Collection>.ini per collection + session.ini as the pointer (legacy content migrates once); restore on switch and start. |
| 6 | CI green on all four jobs; smoke green; README documents the API | VERIFIED | smoke extras green incl. the new BUG-02 block; README "Command API" section; CI run 38150402997 (head fe96c4f): success on windows-x64 (11m57s), linux-x64 (5m14s), macos-arm64 (5m54s), tsan-linux (1m35s) - all steps green incl. "Command-API script tests" on all three OSes. |

### Requirements coverage

- API-01..04, BUG-01, BUG-02, TAB-02..04, COL-06..09: plan 01-01/01-02 (core+GUI+scripts), commits 4c1704b, aa81145, fe96c4f.

## Found-by-verification incidents

- The first CI run of the v2.4 push failed at the script tests (`tab.close -> error: no active tab` after a session restore) - the restore path never set the active tab. Fixed and covered by 41-session-restore.efscript.
- Local script-test "passes" before the CI run were a harness false positive ($LASTEXITCODE not updated); switched to Start-Process -PassThru for reliable exit codes.
