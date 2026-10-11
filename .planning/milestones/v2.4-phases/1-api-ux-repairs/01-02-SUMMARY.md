# 01-02 Summary: GUI repairs, collection-centric menu, script suite

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** API-03, BUG-02 (UI half), TAB-02..04, COL-06..09 (GUI half)

## What was built

- **BUG-02 (the "New Collection crash")**: after the leave prompt's answer was consumed, the modal
  stayed open as a ghost (CloseCurrentPopup was never called) - the reported lockup/crash. A
  `closePopups` flag is now set by the App on every collection/quit decision and consumed by every
  modal exactly one frame later; removed by the frame after drawUi. Smoke regression: drive the
  leave prompt through iterate(), answer "Quit anyway", assert it closed.
- **File menu (COL-06..08)**: New Configuration... / New Collection... / Close Tab / Close
  Collection / Save Collection (Ctrl+S, works with no tabs too) / Save Collection As... /
  Save Configuration As... (tab-level) / Calculate All / Export / Exit. Ctrl+S is now
  Save Collection.
- **Tab context menu (TAB-04)**: right-click a tab -> Close Tab / Rename... (modal, TAB-03) /
  Save Configuration As... / Calculate.
- **Script suite (API-03)**: tests/scripts/{10-editor,20-grid,30-collections,40-session,
  41-session-restore}.efscript; the CI step "Command-API script tests" runs them on all three OSes
  (41 reuses 40's user dir to prove the session restore).
- docs/api.md: the full command reference + menu parity table; README gained the "Command API"
  section.

## Verification

- `zig build test`: 114 + 4 green; `zig build smoke` green (incl. the BUG-02 block).
- Script suite ALL PASSED locally (Start-Process -PassThru exit codes).
- First CI run caught `tab.close -> error: no active tab` after a session restore (the restore path
  never set the active tab) - fixed in fe96c4f; the re-run gates this milestone.

## Notes / carry-over

- Output-mode matrix verified by hand: normal (window opens and closes itself), windowless (--out),
  and a fullscreen script command all exit 0 with all expectations met.
