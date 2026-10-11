# 04-03 Summary: MFC removal, README and tree guard

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** BUILD-06, BUILD-07

## What was done

- Deleted the MFC tree: `ElectricField/` (all sources, res/, .rc), `ElectricField.sln`,
  `tools/baseline/capture-mfc-baseline.ps1` and `tools/parity/compare-mfc.sh` (their subject is gone;
  `docs/baseline/mfc/*.png` and `tools/parity/expected-counts.csv` stay as the visual reference and the
  parity oracle).
- `tests/test_presets.cpp` no longer parses `ElectricField/ConfigDocs.cpp`: the 9 charge tables are a
  committed snapshot taken from the MFC source at deletion time, and the preset suite passes unchanged
  (86/86). The `setCwd(repo root)` in build.zig now only serves general test robustness.
- README rewritten for the port: features, the 9-preset table, Zig-0.17-only build/run/test instructions,
  headless CLI modes, options file locations, project layout, CI description, credits, GPL-3. A fresh GUI
  screenshot (`docs/images/gui-dipole.png`) rendered by the new app replaces the MFC-era screenshots.
- `.gitignore` loses the MSVC/Visual Studio section (nothing builds those artifacts any more).
- New CI step "MFC-free tree guard (BUILD-06)": `git grep` for MFC/Direct2D/GDI/MSVC-project markers
  (afx, CWinApp, CWnd, CDocument, CMDIF, d2d1, dwrite.h, gdi32, GetProfileInt, .vcxproj, .sln) across
  src, tests, tools, build files and README. Prose comments naming Direct2D as replaced history are
  allowed; API-level markers are not.

## Deviations, deliberate

- `assets/ElectricField.rc` (37 bytes, icon reference) stays: the phase-3 approved way to give the
  Windows exe its icon (D-14). The roadmap's "no .rc" targets MFC menu/dialog/string resources.
- `src/gui/main.cpp` includes `windows.h` under `_WIN32` for the headless crash-dialog suppression
  (SetErrorMode / abort behaviour) — no UI dependency, required for unattended CI runs.

## Verification

- `zig build test`: 86 + 4 cases pass; `zig build smoke` green; `ef-cli --all --expect` exit 0.
- Guard grep clean; no MFC/Win32/D2D/GDI/MSVC-project files in the tree.
- CI gates: push run must stay green on windows-x64, linux-x64, macos-arm64, tsan-linux.
