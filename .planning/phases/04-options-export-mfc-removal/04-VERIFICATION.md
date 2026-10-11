---
phase: 04-options-export-mfc-removal
verified: 2026-10-11T01:30:00Z
status: human_needed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 1
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Options walkthrough in the real window (View > Options...)"
    expected: "Computation tab: 11 methods in the combo, equipotentials on, lines 16, threads 4, interval 0.10; typing 200 lines or 0 threads and pressing OK shows an error and keeps the dialog open; changing the field-line colour and thickness repaints every open tab at once; changing threads leaves open tabs computing with their old settings while a newly opened tab uses the new ones; after OK and a restart the values are back; deleting options.ini resets to the defaults"
    why_human: "Colour perception, dialog feel and an actual process restart are not observable headlessly (headless modes deliberately skip the config file)"
  - test: "Export walkthrough in the real window (File > Export > PNG... / SVG...)"
    expected: "The items are greyed out while a tab computes; when the tab is done, PNG.../SVG... open a native save dialog; the saved PNG looks exactly like the tab (same lines, colours, thicknesses); cancelling the dialog does nothing; a non-ASCII destination folder works"
    why_human: "The native save dialog and the visual 'matches the screen' judgement cannot be driven headlessly (the bytes are already proven equal to the ef-cli oracle in CI)"
  - test: "Linux / macOS real desktop (optional, same scope as Phase 3)"
    expected: "Options dialog, export and persistence behave the same on a real Linux/macOS desktop"
    why_human: "CI proves build, smoke, parity, export-parity and the guard on all three OSes, not a real desktop session"
---

# Phase 4: Options, Export & MFC Removal Verification Report

**Phase Goal:** As a user, I want to set options, keep them between runs and export images, so that the app fully replaces the MFC version.
**Verified:** 2026-10-11
**Status:** human_needed (all automated checks pass; no gaps)
**Re-verification:** No, initial verification
**Overrides applied:** 1 — `assets/ElectricField.rc` (37 bytes, icon-only) and the `windows.h` crash-dialog suppression in `src/gui/main.cpp` remain; the phase-3 approved icon mechanism (D-14) and the headless-CI safeguard. The roadmap's "no .rc / no Win32" targets the MFC UI stack; neither file carries UI code.

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Options dialog with Computation (lines 4-128 default 16, equipotentials on, interval 0.01-0.50 default 0.10, threads 1-128 default 4, method default Euler) and Draw settings (blue/red/black/grey-192, thicknesses 1-64); out-of-range rejected INCLUDING the equipotential thickness | VERIFIED | `tests/test_options.cpp`: 8 cases cover defaults, every range edge at min/max and min-1/max+1 for all five fields (equipotential thickness explicitly — the MFC bug never validated it), round-trip, per-key fallback. The dialog validates on OK through `efapp::OptionsEdit::validate()` and stays open with the error (`src/gui/ui.cpp` optionsModal). Headless smoke opens/closes the dialog and applies values (`options=ok` in `smoke extras`). |
| 2 | Draw changes repaint all open tabs immediately; Computation changes apply only to configurations opened afterwards | VERIFIED | The canvas draws `scaledStyle(env.drawStyle, k)` every frame (`ui.cpp` drawCanvas); accepting options replaces `env_.drawStyle` and calls `pacer_.notifyActivity()` so the next frame repaints everything (`app.cpp` frame). `Workspace::setDefaults` feeds only new tabs; each tab keeps its captured `ComputeParams` (`workspace.hpp`). Smoke proves an accepted apply reaches the app and a NEW tab uses `linesPerUnitCharge = 32` while defaults are restored afterwards (`app.cpp` smokeExtras, `options=ok`). |
| 3 | Options persist between runs in the platform user config directory (colors #RRGGBB, method by name); missing/invalid falls back to defaults | VERIFIED | `efapp::saveOptions/loadOptions` over `std::filesystem::path`; startup load from `SDL_GetPrefPath("ElectricField","ElectricField")/options.ini` (`app.cpp` init). Tests: serialize→parse round-trip of every field, missing file → defaults, invalid/out-of-range value → that key's default while the rest applies, unknown keys ignored, file round-trip through a temp dir. Persistence across a real restart is the human test (headless runs deliberately skip the file). |
| 4 | Export of the current view to PNG and SVG matching the screen | VERIFIED | CI runs `ElectricField --export gui-out/export` after the parity step and `cmp`s both files against ef-cli's oracle output on Windows, Linux and macOS (byte equality, run for 885f3c7: success). The export uses the tab's cached scene, the current user style and exactly the on-screen canvas view (`app.cpp` runExport); the on-screen path was itself pinned to the headless path by the phase-3 canvas checks (diff_px ≤ 50 of 786432). |
| 5 | No MFC, Win32, Direct2D or GDI code, no .vcxproj/.sln, no MFC .rc; CI green on all three OSes; README lets a new contributor build, run and test | VERIFIED | `ElectricField/`, `ElectricField.sln`, `capture-mfc-baseline.ps1`, `compare-mfc.sh` deleted (commits 0e4efed, bd58161, e514fd7); `git ls-files` shows 0 remnants; the CI "MFC-free tree guard" greps API-level markers across src/tests/tools/build/README (guard clean locally). README rewritten (Zig-only instructions, preset table, config-file locations, headless modes). CI run 38101496943 for e514fd7: success on windows-x64 (6m43s), linux-x64 (4m53s), macos-arm64 (6m6s), tsan-linux (1m19s); the export-parity `cmp` step passed on all three OSes in the same pipeline. |

### Requirements coverage

- OPT-01..04: plan 04-01 (commit 09f6930) — verified by tests + smoke + human walkthrough pending.
- EXP-01..02: plan 04-02 (commit 885f3c7) — verified by CI export-parity on 3 OSes + human walkthrough pending.
- BUILD-06..07: plan 04-03 (commits 0e4efed, bd58161, e514fd7) — verified by tree check, guard, CI.

### Known deviations (accepted)

- The roadmap's Dormand-Prince low-weight behaviour stays pinned as decided in Phase 2 (unchanged here).
- IN-05 (Painter contract asserts) is satisfied by the existing phase-2/3 suites: `tests_gui/test_render_ctx.cpp` (4 cases incl. repeated exports inside a live GUI context) and `test_scene.cpp` (SVG document well-formedness, all 9 presets finite).
- IN-04 (CI concurrency) was already implemented in Phase 3 (`concurrency: cancel-in-progress`).

## Documents

- Plans: 04-01-PLAN.md, 04-02-PLAN.md, 04-03-PLAN.md (+ summaries)
- Context: 04-CONTEXT.md (MFC options behaviour captured from source before deletion)
