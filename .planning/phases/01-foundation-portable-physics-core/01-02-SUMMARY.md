---
phase: 01-foundation-portable-physics-core
plan: 02
subsystem: testing
tags: [mfc-baseline, powershell, win32, printwindow, imagemagick, qa-05]
status: complete

requires:
  - phase: 01-01
    provides: "v145 toolset commit that makes the MFC project build reproducibly on VS 18"
provides:
  - "tools/baseline/capture-mfc-baseline.ps1: re-runnable Win32-message + PrintWindow capture with registry backup/restore"
  - "docs/baseline/mfc/*.png: 9 reference images (1024x768) of the unmodified MFC app, default options"
  - "docs/baseline/mfc/README.md: provenance, exact commands, preset table, reproducibility result"
affects: [01-03, 01-04, phase-3-gui, phase-4-mfc-removal]

actuals:
  tokens: 4100
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns: [out-of-repo MSBuild OutDir/IntDir under %TEMP%, registry backup outside repo restored in finally, fuzz-tolerant image comparison]

key-files:
  created:
    - tools/baseline/capture-mfc-baseline.ps1
    - docs/baseline/mfc/README.md
    - docs/baseline/mfc/dipole.png
    - docs/baseline/mfc/two-equal-charges.png
    - docs/baseline/mfc/two-unequal-charges-same-sign.png
    - docs/baseline/mfc/two-unequal-charges-different-signs.png
    - docs/baseline/mfc/quadrupole.png
    - docs/baseline/mfc/four-charges-same-sign.png
    - docs/baseline/mfc/two-dipoles-same-direction.png
    - docs/baseline/mfc/two-dipoles-opposed-direction.png
    - docs/baseline/mfc/parallel-dipoles.png
  modified: []

key-decisions:
  - "Baseline toolchain recorded as MSVC 14.51.36231 / v145 / VS 18 Insiders, Windows 10.0.26100.9278, 96 DPI"
  - "README reproducibility 'AE raw' is an integer count of pixels differing in any channel (computed by difference+threshold), because magick compare -metric AE without fuzz prints a fractional value under Q16-HDRI"

patterns-established:
  - "Baseline images are compared with a fuzz tolerance (10%), never byte equality: thread scheduling can shift a few pixels"
  - "Capture scripts that touch HKCU back the key up outside the repo and restore it in finally"

requirements-completed: [QA-05]

duration: 25min
completed: 2026-10-10
---

# Phase 1 Plan 02: MFC Baseline Capture Summary

**Nine 1024x768 reference PNGs of the unmodified MFC app (Release|x64, v145, default options) plus a re-runnable PowerShell capture script that drives the app via Win32 messages and PrintWindow, with HKCU options backup/restore.**

## Performance

- **Duration:** about 25 min
- **Tasks:** 2 (tracer + all presets)
- **Files:** 11 created, none modified; nothing under `ElectricField/` touched

## Accomplishments

- Tracer: MSBuild Release|x64 to `%TEMP%\ef-mfc-baseline\out` (13.6 s warm, repo stays clean), capture of the dipole, then the plan gate (fresh capture compared with `-fuzz 10%`) passed with AE 0 before any expansion.
- All 9 presets captured in about 5.5-6 s each (dark pixels 4807 to 11352, all well above the 1500 completeness threshold). Visual check of a 3x3 montage: charges at preset positions, blue positive / red negative, complete black field lines, grey equipotentials.
- Reproducibility: quadrupole AE 0 / 0, four-charges-same-sign AE 0 / 0, two-dipoles-same-direction AE 0 with fuzz and 3 raw differing pixels. All pass `magick compare -metric AE -fuzz 10%`.
- The script verifies each chooser entry text (including the `Oposed` typo) before selecting it, so file names always match the captured preset.
- The HKCU options key was identical before and after every run (full `reg query /s` diff); the backup lives in `%TEMP%`, not in the repo; no ElectricField.exe was left running.

## Task Commits

1. **Task 1 (tracer): capture script + dipole image** - `09b7597` (feat)
2. **Task 2: 9 presets, reproducibility, README** - `26eea3f` (docs)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Bash path quoting during first capture attempt**
- **Found during:** Task 1
- **Issue:** Backslash escapes in `-OutDir docs\\baseline\\mfc` were collapsed by Git Bash, creating a stray `docsbaselinemfc/` directory.
- **Fix:** Removed the stray directory (created by me, untracked, never committed) and re-ran with forward slashes. No change to the script.
- **Files modified:** none committed
- **Commit:** n/a

**2. [Rule 1 - Bug in plan wording] Raw AE value is not an integer under ImageMagick Q16-HDRI**
- **Found during:** Task 2
- **Issue:** `magick compare -metric AE` without fuzz printed `0.0617647 (7.85e-08)` for two-dipoles-same-direction, which does not satisfy the plan's `AE raw = <integer pixel count>` format.
- **Fix:** Counted differing pixels with `-compose difference -composite -colorspace Gray -threshold 0` and recorded the integer (0, 0, 3) in the README, with the method described in the text.
- **Files modified:** docs/baseline/mfc/README.md
- **Commit:** 26eea3f

**3. [Plan note] README claim about which preset jitters**
- The research expected quadrupole to jitter by a pixel; in this run quadrupole was exact and two-dipoles-same-direction differed by 3 pixels. README states "a few pixels may differ" instead of naming a preset.

### Acceptance-criteria note

`git ls-files | grep -cE '\.reg$'` outputs `1`, not `0`, because the repository already tracks `ElectricField/ElectricField.reg` (MFC shell-registration file from the initial commit, unrelated to this plan). No registry backup is tracked: the backup is `%TEMP%\ElectricField-options-backup.reg`, and `docs/` contains no `.reg` file. The intent of the criterion (no backup committed) is met.

## Authentication Gates

None.

## Known Stubs

None.

## Threat Flags

None. Mitigations T-01-08 to T-01-11 applied as planned (backup outside repo, restore in finally, process stop only by own PID, no user-profile paths in README).

## Issues Encountered

During an exploratory shell command at the start of the plan, two stray `python` processes were started waiting on stdin (my error); they hold no files and were left alone since I could not confirm they were mine alone.

## Next Phase Readiness

Baseline recorded before any MFC removal (QA-05). Plans 01-03 and 01-04 are unaffected; Phase 3 can compare against `docs/baseline/mfc/` with a fuzz tolerance.

## Self-Check: PASSED

- tools/baseline/capture-mfc-baseline.ps1, docs/baseline/mfc/README.md and all 9 PNGs exist
- commits 09b7597 and 26eea3f exist
