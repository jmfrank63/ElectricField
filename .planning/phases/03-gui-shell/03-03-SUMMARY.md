---
phase: 03-gui-shell
plan: 03
subsystem: ci
tags: [github-actions, ci, gui-smoke, screenshots, sdl3, imgui, tsan]
status: complete

requires:
  - phase: 03-gui-shell
    provides: efapp/efrender (03-01), GUI shell, zig build smoke and test-render steps, --screenshot developer flags (03-02)
  - phase: 02-compute-engine
    provides: ef-cli parity oracle and the Phase 2 CI workflow
provides:
  - CI that builds and runs the real GUI code on windows, linux and macos-arm64 (render-context tests, full UI smoke, headless screenshots)
  - Windows PE-subsystem gate (VIEW-05, no console window) in CI
  - gui-<os> and parity-<os> artifacts for every OS, 14 days
  - research-ref prototype removed from the tree (history a4e42b8)
affects: [Phase 4 MFC removal, /gsd-verify-work 03]

actuals:
  tokens: 6000
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "CI evidence for a GUI app without a display: SDL dummy video driver + software renderer, scripted UI session, screenshots of the real UI as artifacts"
    - "Gate extensions only add steps; core gates, parity, tsan-linux, matrix and permissions are untouched"

key-files:
  created: []
  modified:
    - .github/workflows/ci.yml
    - .gitignore
  deleted:
    - .planning/phases/03-gui-shell/research-ref/ (70 files)

key-decisions:
  - "No source fix was needed: Linux and macOS passed the keyboard, canvas, screenshot and TSan checks on the first push, so the RESEARCH A3 (macOS Ctrl/Super) and window-size-sync risks did not materialise in CI"
  - "test-render runs once in build-test (-Doptimize=ReleaseFast) instead of re-running the whole core suite a third time"

requirements-completed: [VIEW-01, VIEW-02, VIEW-03, VIEW-04, VIEW-05]

coverage:
  - id: D1
    description: "Real GUI path (tabs, chooser, keyboard, quit) runs on all three OSes in CI"
    requirement: VIEW-01
    verification:
      - kind: other
        ref: "run 38074759060: windows-x64, linux-x64, macos-arm64 print 'smoke ok frames=172|173 tabs_opened=2 lines=16 closed=2' and 'smoke extras: keys=ok middle_click=ok failed_tab=ok overlay=ok'"
        status: pass
  - id: D2
    description: "Close while computing and quit are fast; tab model race-free"
    requirement: VIEW-02
    verification:
      - kind: other
        ref: "smoke extras quit_ms=0..1 on all OSes; tsan-linux success (efapp Workspace tests, real and scripted sessions, with the core suite)"
        status: pass
  - id: D3
    description: "Canvas is the shared renderScene output on every OS"
    requirement: VIEW-03
    verification:
      - kind: other
        ref: "3 'canvas check ... ok' lines per OS with identical diff_px 50/28/0, chrome_bleed_px=0, blue_px=1913, red_px=1916; screenshots.txt 3 lines ok=1 on each OS, 3 PNGs each"
        status: pass
  - id: D4
    description: "Idle loop does not spin; busy overlay and failed tab"
    requirement: VIEW-04
    verification:
      - kind: other
        ref: "smoke extras idle_frames=5/1.3s overlay=ok failed_tab=ok on all OSes"
        status: pass
  - id: D5
    description: "GUI-subsystem exe and About box with fresh git hash"
    requirement: VIEW-05
    verification:
      - kind: other
        ref: "windows-x64 job: 'PE subsystem=2'; 'about: Version 0.1.0 (<pushed short hash>)' on all three OSes"
        status: pass

duration: 45min
completed: 2026-10-11
---

# Phase 3 Plan 03: CI for the GUI Summary

**CI now builds the real Dear ImGui + SDL3 GUI on Windows, Linux and macOS arm64, runs the render-context tests and the full scripted UI smoke there (identical canvas, keyboard and quit results on all three), proves the Windows GUI subsystem, uploads headless screenshots of the real UI, and kept the Phase 2 parity and TSan gates green; the research prototype is removed.**

## Performance

- **Duration:** about 45 min (mostly CI wall time; each run took 8 to 14 min end to end)
- **Completed:** 2026-10-11
- **Tasks:** 2 (3 commits)

## Accomplishments

- `.github/workflows/ci.yml` (build-test, in order after "Build everything"): "Render-context regression tests (headless)" (`zig build test-render -Doptimize=ReleaseFast`), "Windows GUI subsystem (VIEW-05 - no console window)" (`if: runner.os == 'Windows'`, reads e_lfanew and the Subsystem word with `od`, fails unless 2), the unchanged smoke step (now the full UI sequence), "Headless GUI screenshots" (dipole 1024x820, About modal, chooser at `--scale 2`, each `tee -a gui-out/screenshots.txt`) and "Upload GUI screenshots" (`gui-<matrix.name>`, `if: always()`, 14 days, same pinned upload-artifact SHA). Triggers, permissions, concurrency, timeouts, matrix, core gates, parity step and tsan-linux job are unchanged.
- `.gitignore` ignores `/gui-out/`.
- `research-ref/` (70 tracked files) removed; no non-`.planning` file refers to it; `probes/` and `shots2/` never entered the product tree.

## Task Commits

1. **Task 1 (tracer): CI end to end for the GUI** - `ef38dde` (ci). Pushed, run 38073970097 green on the first attempt.
2. **Task 2: phase close-out, research-ref removed** - `b42f062` (chore). Pushed, run 38074759060 green on the first attempt.
3. Docs commit for this SUMMARY, STATE.md, ROADMAP.md (final).

## CI Verdicts

| Run | Head | Conclusion | Jobs |
|-----|------|-----------|------|
| [38073970097](https://github.com/jmfrank63/ElectricField/actions/runs/38073970097) (Task 1) | `ef38dde88041713d3045a427581679ee265c620d` | success | tsan-linux 3m34s, windows-x64 5m41s, linux-x64 6m42s, macos-arm64 7m49s |
| [38074759060](https://github.com/jmfrank63/ElectricField/actions/runs/38074759060) (final code head `b42f062`) | `b42f062585397f3a7c45b6a6a43ce871783cd0ab` | success | macos-arm64 6m50s, tsan-linux 2m08s, linux-x64 6m31s, windows-x64 13m26s |

Both runs: artifacts exactly `gui-linux-x64, gui-macos-arm64, gui-windows-x64, parity-linux-x64, parity-macos-arm64, parity-windows-x64`. The Task 1 gui artifacts were downloaded: each holds 3 PNGs and a `screenshots.txt` with 3 lines ending `ok=1` (`screenshot dipole 1024x820 canvas=(0,52)-(1024,820) k=1.00 ok=1` twice, `screenshot dipole 2048x1648 canvas=(0,104)-(2048,1648) k=2.00 ok=1`), identical on all three OSes. The `parity-*` re-download into the existing `zig-out/ci` tree hit "file exists" from earlier phases; the artifacts are present per the API and the parity step passed in every job, so this was not pursued.

### Per-OS smoke lines (Open Question 5 outcome)

Run 38073970097 (head `ef38dde`), the same three canvas lines on all OSes:

```
canvas check 640x480: ok (canvas 640x428 at (0,52) window 640x480 diff_px=50 of 273920 chrome_bleed_px=0 blue_px=1913 red_px=1916)
canvas check 1024x820: ok (canvas 1024x768 at (0,52) window 1024x820 diff_px=28 of 786432 chrome_bleed_px=0 blue_px=1913 red_px=1916)
canvas check 1280x1024: ok (canvas 1280x972 at (0,52) window 1280x1024 diff_px=0 of 1244160 chrome_bleed_px=0 blue_px=1913 red_px=1916)
```

| OS | about | smoke extras | smoke ok |
|----|-------|--------------|----------|
| windows-x64 | `Version 0.1.0 (ef38dde)`, `PE subsystem=2` | `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=1` | `frames=172 tabs_opened=2 lines=16 closed=2` (run 1) |
| linux-x64 | `Version 0.1.0 (ef38dde)` | `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=1` | `frames=173 tabs_opened=2 lines=16 closed=2` |
| macos-arm64 | `Version 0.1.0 (ef38dde)` | `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=0` | `frames=174 tabs_opened=2 lines=16 closed=2` |

Final run 38074759060 (head `b42f062`): `about: Version 0.1.0 (b42f062)` on all three; windows `PE subsystem=2`, `quit_ms=1`, `smoke ok frames=173 ...`; linux `quit_ms=0`, `smoke ok frames=172 ...`; macos `quit_ms=0`, `smoke ok frames=172 ...`; `smoke extras: keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s` everywhere; every `smoke ok` ends `tabs_opened=2 lines=16 closed=2`. The frame count differs by one or two between runs and OSes (timing of the scripted session); every checked count (`tabs_opened`, `lines`, `closed`) is identical.

## Verification Evidence

| Gate | Result |
|------|--------|
| Local dry run of the whole job body (Windows) | exit 0: core tests Debug and ReleaseFast, full build, test-render, `PE subsystem=2`, smoke, 3 screenshots, parity |
| Same dry run after removing research-ref (final local gate) | exit 0; `git status --porcelain` empty outside `.planning`; `git diff --name-only 8f8935a HEAD -- src/core tools/parity docs/baseline ElectricField` empty; no `probes`/`shots2` |
| ci.yml greps | 6 `uses:` all 40-hex pinned (2 each of checkout, setup-zig, upload-artifact); 3 `zig build test -D`; 1 test-render; 1 smoke; 3 `--screenshot dipole`; 1 Windows-only `if`; 2 `if: always()`; 1 gui and 1 parity artifact name; 1 `fail-fast: false`; 3 matrix rows; 1 `contents: read`; 0 `continue-on-error`, `secrets.`, `pull_request_target`, `-Dtarget`; no tabs |
| build.zig | `"-Werror"` once, `"-ffp-contract=off"` once (unchanged) |
| Pre-push scan | no credential patterns in the pushed range (the only text hit was the plan's own prose naming the patterns); no binaries outside `assets/` and research-ref |
| Remote | `git ls-remote --tags origin | wc -l` = 0 before and after; HEAD (after the push of `b42f062`) equals `origin/feature/cross-platform`; normal fast-forward pushes only |

## Deviations from Plan

None - plan executed as written. The 3-iteration fix budget was not used: both CI runs were green on their first attempt, so no `fix(03-03)` commit and no source change in `src/gui`, `src/render` or `src/app`. The one adjustment worth noting is operational: the plan's "Windows GUI subsystem" check and screenshot commands were copied into the workflow unchanged from the local dry run.

## Phase-level notes carried into verification

- **Accepted UI-SPEC deviation (RESEARCH Open Question 3):** the preset chooser opens with keyboard focus on the preselected row rather than the Open Configuration button, so Up/Down work at once and Enter still opens.
- **Git hash variant used:** the primary one (`git -c core.abbrev=7 log -1 --format` into a generated `ef_git_hash.h` at make time). It resolved to the pushed short hash on all three CI OSes (`ef38dde`, then `b42f062`), so it is proven fresh in CI as well as locally.
- **Informational GUI-vs-ef-cli AE:** `magick compare -metric AE -fuzz 10%` of the GUI canvas against ef-cli `dipole.png` = 3.67 differing pixels of 786,432 (plan 03-02; research measured 0 to 32).
- **Parity and TSan:** ef-cli parity counts unchanged on all three OSes; tsan-linux (efapp Workspace tests, real and scripted sessions, plus the core suite) reported no race.

## Human checks still open (end of phase, for /gsd-verify-work 03)

1. **Windows real window (from plan 03-02):** menus, link clicks, wait cursor, quit with three busy tabs, visual comparison with `docs/baseline/mfc/dipole.png`, and HiDPI at 125 to 200 % (local displays are 100 %, only `--scale 2` arithmetic is covered automatically).
2. **macOS (Apple silicon, Retina):** build with `zig build -Doptimize=ReleaseFast` on a Mac and run `zig-out/bin/ElectricField`; open several presets, close one while computing, use Cmd+N / Cmd+W / Cmd+Q, open About and click a link; look at the `gui-macos-arm64` screenshots. Expected: shortcut labels read Cmd+N/Cmd+W/Cmd+Q, sharp Retina rendering with the canvas at the same physical size as on a 1x display, links open the browser, quit while computing is immediate. CI only runs it headless.
3. **Linux desktop (X11 and, if available, Wayland):** the same walkthrough as on Windows from a real session; look at the `gui-linux-x64` screenshots. Expected: same behaviour as Windows, X11/Wayland chosen at runtime without extra packages. CI and the WSL pre-flight use the dummy video driver.

## Known Stubs

None. This plan changed only the workflow, `.gitignore` and removed the prototype tree.

## Threat Flags

None. The new surface is covered by the plan's threat model: permissions stay `contents: read` with no secrets or privileged triggers (T-03-14), no new action and all 6 `uses:` pinned (T-03-15), only `gui-out` (3 PNG + screenshots.txt) and `parity-out` uploaded for 14 days (T-03-16), the gate was not weakened (T-03-17), only normal pushes of `feature/cross-platform` and 0 tags (T-03-18), scan clean (T-03-19).

## Self-Check: PASSED

- Files: `.github/workflows/ci.yml` and `.gitignore` committed in `ef38dde`; `.planning/phases/03-gui-shell/research-ref` absent and untracked (0 files).
- Commits: `ef38dde` and `b42f062` present in `git log` and on origin.
- Runs: 38073970097 and 38074759060 both `success` with 4/4 jobs and the 6 expected artifacts.
- Prohibitions: `src/core`, `tools/parity`, `docs/baseline`, `ElectricField` unchanged since `8f8935a`; no force-push, no tags.
