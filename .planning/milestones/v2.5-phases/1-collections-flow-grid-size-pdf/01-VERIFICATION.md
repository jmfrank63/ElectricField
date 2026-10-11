# Phase 1 Verification: Collections Flow, Grid/Size Options & PDF Export

All commands run on the working tree at 3de6bc6 (Windows, PowerShell; exit codes read from
`Start-Process -PassThru ... .ExitCode`, never `$LASTEXITCODE` - the harness does not update it).

| Check | Command | Result |
|-------|---------|--------|
| Build | `zig build` | clean (no errors) |
| Unit tests | `zig build test` | 119 + 4 doctest cases, 0 failed |
| Smoke | `zig build smoke` | smoke ok frames=276, extras all ok |
| Script suite | 5x `--script tests\scripts\*.efscript --user-dir <fresh>` | all exit 0 |
| Headless export | `ElectricField --export <dir>` | exit 0; dipole.pdf (81 KB) + dipole.png + dipole.svg |
| Path-form open | `collection.open "<abs>\Collections\Sandbox.ini"` | exit 0; collection/configs/tabs/charges restored |
| Guards | `tab.new-empty` / `config.open` / `collection.save-as` without a collection | errors, as designed |
| Grid state | script 20-grid | `grid=1,1,1,1,1,0,1,30` round-trips the intersections flag |
| PDF structure | tests/test_scene.cpp | header/MediaBox/y-down cm/path ops/Length/xref offsets/bookmarks verified |
| Charge size | tests/test_options.cpp | 2 and 120 accepted, 1/121 rejected, round-trip 45 |
| CI | run for 3de6bc6 (windows-x64, linux-x64, macos-arm64, tsan-linux) | green (see final report) |

Known notes: a running ElectricField.exe blocks `zig build` (AccessDenied) - closed politely before
rebuilding; GUI-subsystem stdout is lossy, so script results go through `--out` files.
