---
phase: 03-gui-shell
plan: 02
subsystem: ui
tags: [cpp17, zig-build, imgui, sdl3, tabs, hidpi, win32-resource, git-hash]
status: complete

requires:
  - phase: 03-gui-shell
    provides: efapp Workspace/FramePacer, efrender renderSurface with the ImGui context rule (plan 03-01)
  - phase: 02-compute-engine
    provides: ef::ComputeSession, ef::renderScene, ef-cli parity oracle
provides:
  - real GUI exe ElectricField (SDL3 + Dear ImGui): preset menu and chooser, tabs, busy overlay, failed state, About box, HiDPI scaling
  - GUI-subsystem Windows exe with the MFC icon (assets/ElectricField.rc, byte-identical .ico)
  - zig build smoke running the full UI-SPEC sequence on the real App code (also on Linux)
  - generated ef_git_hash.h refreshed at make time (About box version line, printed by --smoke as "about: ...")
affects: [03-03 CI, Phase 4 MFC removal]

actuals:
  tokens: 14300
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Prototype copy-not-redesign: 11 files byte-identical to research-ref, hand edits only in build.zig and app.cpp"
    - "Make-time generated header: Run step with has_side_effects captures git output into a WriteFiles directory that is an include path"
    - "No-git fallback at configure time (runFallible probe) writes an empty define, so the build never depends on git"

key-files:
  created:
    - src/gui/app.hpp
    - src/gui/app.cpp
    - src/gui/ui.hpp
    - src/gui/ui.cpp
    - src/gui/icon.hpp
    - src/gui/icon.cpp
    - assets/ElectricField.ico
    - assets/ElectricField.rc
    - assets/ElectricField-256.png
    - assets/ElectricField-96.png
  modified:
    - src/gui/main.cpp
    - build.zig
    - build.zig.zon

key-decisions:
  - "Git hash variant: the primary one (git log --format with a #define line into ef_git_hash.h); the githash.txt/#embed contingency was not needed because Windows argv quoting passed the format argument intact"
  - "Accepted deviation (RESEARCH Open Question 3): the chooser opens with keyboard focus on the preselected row instead of the Open Configuration button, so Up/Down work at once and Enter still opens"
  - "Developer flags --screenshot, --size, --scale, --modal stay undocumented and are listed only in the main.cpp header comment (Open Question 4)"

requirements-completed: [VIEW-01, VIEW-02, VIEW-03, VIEW-04, VIEW-05]

coverage:
  - id: D1
    description: "GUI-subsystem exe, no console, preset chooser and menus, tab per configuration, clean quit"
    requirement: VIEW-01
    verification:
      - kind: other
        ref: "zig build smoke -Doptimize=ReleaseFast: 'smoke ok frames=172 tabs_opened=2 lines=16 closed=2' (Windows and Linux)"
        status: pass
      - kind: other
        ref: "PE optional header Subsystem of zig-out/bin/ElectricField.exe = 2"
        status: pass
  - id: D2
    description: "Tabs close instantly also while computing; quit with 3 busy sessions is fast and leaks no tab"
    requirement: VIEW-02
    verification:
      - kind: other
        ref: "smoke extras: keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=2 (Windows), quit_ms=1 (Linux)"
        status: pass
  - id: D3
    description: "Canvas is the shared renderScene output, white, y-up, centred; resize clips and never recomputes"
    requirement: VIEW-03
    verification:
      - kind: other
        ref: "smoke: 3 canvas checks vs efrender::renderSurface, chrome_bleed_px=0, blue_px=1913 red_px=1916"
        status: pass
      - kind: other
        ref: "--screenshot dipole 1024x820: 'canvas=(0,52)-(1024,820) k=1.00 ok=1'; magick AE vs ef-cli dipole.png = 3.67 (informational)"
        status: pass
  - id: D4
    description: "Busy overlay and failed-tab state; idle loop does not spin"
    requirement: VIEW-04
    verification:
      - kind: other
        ref: "smoke extras overlay=ok failed_tab=ok; 5 idle frames in 1.3 s"
        status: pass
      - kind: other
        ref: "real Windows window: title=Electric Field idle_cpu_pct=0.31"
        status: pass
  - id: D5
    description: "About box content and fixed URLs; startup failure path; fresh git hash"
    requirement: VIEW-05
    verification:
      - kind: other
        ref: "SDL_VIDEODRIVER=nosuch: exit 1, stderr 'Electric Field could not start.' / 'SDL initialisation: nosuch not available' (Windows and Linux)"
        status: pass
      - kind: other
        ref: "after commit 6af277c a plain zig build smoke printed 'about: Version 0.1.0 (6af277c)' without clearing .zig-cache; Linux git archive tree printed '(unknown)'"
        status: pass
      - kind: other
        ref: "greps: 3 https literals in ui.cpp, one SDL_OpenURL call, one SDL_SetClipboardText call, 0 direct draw calls in src/gui"
        status: pass

duration: 60min
completed: 2026-10-11
---

# Phase 3 Plan 02: Dear ImGui + SDL3 GUI shell Summary

**The prototype's SDL3 + Dear ImGui shell (menus, preset chooser, reorderable tabs, busy overlay, About box, HiDPI scaling) is now the real ElectricField exe with the MFC icon and GUI subsystem, a make-time git hash in the About box, and a full UI smoke that passes on Windows and Linux.**

## Performance

- **Duration:** about 60 min (most of it builds; the Linux pre-flight was cold)
- **Completed:** 2026-10-11
- **Tasks:** 2 (both committed)
- **Files:** 13 in Task 1 (10 new, 3 modified), 2 in Task 2

## Accomplishments

- `src/gui/{main,app,ui,icon}` and `assets/*` are byte-identical copies of research-ref (`cmp` clean); `assets/ElectricField.ico` is identical to `ElectricField/res/ElectricField.ico`.
- `build.zig` equals the prototype's minus the dev-only probe helper and its registration loop (0 added lines, 25 removed lines). `.paths` gained `assets` and `tests_gui`; no dependency hash changed.
- Task 2 replaced the configure-time git helper by `gitHashHeader`, so the About box follows HEAD without clearing `.zig-cache`; `--smoke` prints `about: Version <semver> (<hash>)`.
- Linux (WSL, no display, no git repository): `test-render`, the full smoke, `(unknown)` hash and the fatal start all pass.

## Task Commits

1. **Task 1 (tracer): the real GUI end to end** - `9084a3d` (feat)
2. **Task 2: fresh git hash at make time, Linux pre-flight** - `6af277c` (feat)

## Verification Evidence

| Gate | Result |
|------|--------|
| `zig build test --summary all` (Debug, after Task 1 and after Task 2) | exit 0 |
| `zig build -Doptimize=ReleaseFast` | 16/16 steps |
| `zig build smoke -Doptimize=ReleaseFast` (Windows) | `video=dummy renderer=software presets=9`; 3 canvas checks ok (diff_px 50 / 28 / 0, chrome_bleed_px=0, blue_px=1913, red_px=1916); `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=2`; `smoke ok frames=172 tabs_opened=2 lines=16 closed=2`; no imgui.ini |
| PE Subsystem | 2 (GUI) |
| `ef-cli --all --expect tools/parity/expected-counts.csv` | exit 0 (Phase 2 parity unchanged) |
| `--screenshot dipole zig-out/check/gui-dipole.png --size 1024x820` | `screenshot dipole 1024x820 canvas=(0,52)-(1024,820) k=1.00 ok=1` |
| Informational: `magick compare -metric AE -fuzz 10%` GUI canvas vs ef-cli dipole.png | 3.67 differing pixels of 786,432 (research measured 0 to 32) |
| Fatal start `SDL_VIDEODRIVER=nosuch` (Windows) | exit 1, no message box; `Electric Field could not start.` / `SDL initialisation: nosuch not available` |
| Real window (Windows, PowerShell, no input) | `title=Electric Field idle_cpu_pct=0.31` (limit 2); no imgui.ini afterwards |
| Freshness proof | after `6af277c`, `zig build smoke` printed `about: Version 0.1.0 (6af277c)` equal to `git rev-parse --short=7 HEAD`, build.zig untouched, cache kept |
| Linux pre-flight on `git archive HEAD` (WSL Ubuntu, no DISPLAY, no repository) | `test-render` success (13/13 steps); smoke `about: Version 0.1.0 (unknown)`; canvas checks diff_px 50 / 28 / 0, chrome_bleed_px=0; `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=1`; `smoke ok frames=172 tabs_opened=2 lines=16 closed=2`; fatal start exit 1 with `Electric Field could not start.` |
| `git diff --name-only 8f8935a -- src/core tools/parity docs/baseline ElectricField` | empty |

Single drawing path and fixed URLs: 0 `AddBezierCubic|AddCircleFilled|PathBezierCubicCurveTo` in `src/gui/*.cpp` (comments stripped), 1 `ef::renderScene(painter`, 3 `https://` in ui.cpp, 1 `SDL_OpenURL(`, 1 `SDL_SetClipboardText(`, `IniFilename = nullptr` once, `KeyRepeatDelay = 1e9f` once.

## Decisions Made

- Git hash variant used: the primary one. `git -c core.abbrev=7 log -1 "--format=#define EF_GIT_HASH \"%h\""` run as a Run step with `has_side_effects = true`, its stdout copied into a `WriteFiles` directory used as an include path. The generated header compiled on Windows, so the `githash.txt` / `#embed` contingency was not needed.
- The chooser opening with focus on the preselected row (RESEARCH Open Question 3, resolved) and the undocumented developer flags (Open Question 4) are accepted as the plan states.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Linux pre-flight script never installed the exe**
- **Found during:** Task 2 step 6
- **Issue:** The plan's WSL one-liner runs `zig build test-render` and `zig build smoke` and then `./zig-out/bin/ElectricField` for the fatal start. Neither step installs the exe (smoke runs the cached artifact), so the script stopped under `set -e` with exit 127 right after the smoke, before the fatal-start check. The Windows flow did not hit this because Task 1 builds the install step first.
- **Fix:** No source change was needed. I ran `zig build -Doptimize=ReleaseFast` in the same extracted tree and then the fatal start: exit 1, `Electric Field could not start.`, no imgui.ini. Everything before that point of the script (test-render, smoke lines, `(unknown)`) had passed.
- **Files modified:** none
- **Commit:** none. Plan 03-03 or any CI job that repeats the fatal start must run `zig build` (install) first.

None of the other steps deviated: the code is the prototype and the only hand edits are the planned ones.

## Issues Encountered

- The WSL command was run in the background and polled; its trailing `echo done rc=$?` reported 0 for the redirect, not for the script, which hid the exit-127 stop until I read the log. The remaining checks were then run by hand as described above.

## Known Stubs

None. All UI surfaces are wired to the Workspace and the shared renderer; the only empty value is the intentional `EF_GIT_HASH ""` fallback when git is unavailable, which the About box shows as `(unknown)`.

## Threat Flags

None. The new surface is already in the plan's threat model: T-03-06/07 (fixed URLs, one open call, one clipboard call, greps pass), T-03-09 (the .rc names only the byte-identical icon), T-03-10 (fatal start returned exit 1 in 30 s with no message box in the headless run), T-03-11 (no imgui.ini after smoke and after the real window), T-03-12 (fixed git argv, no shell, empty fallback).

## Human checks still open (end of phase)

- Windows real-window walkthrough (menus, link clicks, wait cursor, quit with three busy tabs, visual comparison with `docs/baseline/mfc/dipole.png`) and the HiDPI check at 125 to 200 %. The local displays are all 100 %, so only `--screenshot --scale 2` arithmetic is covered automatically.

## Next Phase Readiness

Plan 03-03 (CI) can run `zig build smoke` on windows, linux and macos-arm64 and grep the `about:` line and the final smoke line. macOS key injection (the Ctrl/Super swap in `App::chord`) is first exercised there. Any CI step that runs the installed exe (fatal start) needs `zig build` before it.

## Self-Check: PASSED

- Files: all 13 listed key files exist and are committed (`git status` clean).
- Commits: `9084a3d` and `6af277c` present in `git log`.
- Prohibition: `src/core`, `tools/parity`, `docs/baseline` and `ElectricField` unchanged since `8f8935a`.
