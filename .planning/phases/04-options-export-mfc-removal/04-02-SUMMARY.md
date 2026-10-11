# 04-02 Summary: PNG and SVG export

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** EXP-01, EXP-02

## What was built

- File ▸ Export ▸ PNG… / SVG… (File menu, where MFC had printing). The submenu is enabled only while the
  active tab's session is Done — only then does the tab hold a complete scene that can "match what is shown".
- The destination comes from SDL3's native save dialog (PNG/SVG filters). The callback may run on any
  thread (portal/zenity on Linux), so the chosen path crosses into the UI thread behind a mutex and is
  exported at the next frame boundary — renderPng's ImGui-context rule (phase-2 D-17) is kept by design.
  A dialog failure (e.g. no zenity) surfaces as an error message instead of silence.
- PNG goes through `efrender::renderPng`, SVG through `ef::renderSvg` written with `SDL_IOFromFile` —
  both handle non-ASCII paths on Windows natively (D-07, closes the export part of phase-2 IN-06).
  The exported image is the tab's cached scene + the current user draw style + exactly the on-screen
  canvas view.
- New headless mode `ElectricField --export <dir>`: computes the Dipole, exports `dipole.png` + `dipole.svg`
  with the oracle's fixed inputs (default options, 1024x768 @ 100 px/unit). A CI step runs it after the
  parity step and `cmp`s both files against ef-cli's output — byte equality proven on all three OSes
  (verified locally on Windows: SHA256 equal for both formats).

## Verification

- `zig build test`: 86 core/app cases + 4 render-context cases pass (renderPng context rule, already
  exercised by the phase-3 suites — the Painter contract of IN-05 is covered there and by the SVG cases
  of test_scene.cpp).
- `zig build smoke`: unchanged, all green.
- Local export run: counts match ef-cli (2 charges, 16 field lines, 75 equipotentials), PNG and SVG
  byte-identical to the oracle.

## Notes / carry-over

- IN-04 (CI concurrency) turned out to be already implemented in phase 3 (cancel-in-progress +
  branch filter); nothing to do.
- The save dialog itself is not exercised headlessly (native UI); the export code path is, via --export.
