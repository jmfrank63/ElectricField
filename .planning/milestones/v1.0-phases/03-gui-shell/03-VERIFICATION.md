---
phase: 03-gui-shell
verified: 2026-10-11T00:00:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification:
  - test: "Windows real-window walkthrough (double-click zig-out\\bin\\ElectricField.exe in Explorer)"
    expected: "No console window; icon is the MFC artwork; the 'New' chooser opens on start and Close Chooser shows 'No configuration open'; File > New opens each of the 9 presets as '<Name> 1' and a repeat as '<Name> 2'; busy tab shows spinner marker, 'Computing... n / m lines' overlay and hourglass cursor over the canvas only; closing a computing tab via x, middle-click and Ctrl+W is instant; Ctrl+N chooser Up/Down/Enter/double-click/Esc; Help > About shows the version with 7-char hash and the three links open in a browser (also Enter on a focused link must not close the box); window title 'Electric Field - Dipole 1'; resize keeps origin centred and clips; Ctrl+Q with 3 computing tabs exits at once"
    why_human: "Pointer, real menus, link clicks, cursor shape and OS-level console/icon behaviour cannot be driven by the headless smoke (dummy driver)"
  - test: "HiDPI on a real 125/150/200 % Windows display and when dragging the window between monitors of different scale"
    expected: "1 px lines sharp (no blur or half-width hairlines), canvas picture the same physical size as at 100 %, chrome and text rescale when the window changes monitor, minimum window size follows the scale"
    why_human: "UI-SPEC backstop (verification: backstop). All local displays report content scale 1.0; --scale 2 and the unit tests only prove the arithmetic and that k=2 renders without error"
  - test: "Real windows on Linux (X11 and Wayland) and macOS (Retina)"
    expected: "App opens, tabs/menus/About work, Cmd+N/W/Q chords work on macOS, canvas sharp on Retina (framebuffer scale 2, k = 1.0), About links open the default browser"
    why_human: "CI proves build + scripted UI on dummy driver + software renderer on all three OSes, not a real window, real GPU renderer, or real display scale"
  - test: "Visual side-by-side of a finished Dipole tab in the real window against docs/baseline/mfc/dipole.png"
    expected: "Same lines, same colours, same charge size and position; GUI AA slightly softer"
    why_human: "Final subjective judgement; the headless side-by-side (zig-out/check/phase3-gui-vs-mfc.png) is visually equivalent, 0.8 % of pixels differ at 10 % fuzz"
---

# Phase 3: GUI Shell Verification Report

**Phase Goal:** As a user, I want to open the 9 preset configurations in tabs, so that I can see their field lines and equipotentials on any desktop OS.
**Verified:** 2026-10-11
**Status:** passed (human verification approved 2026-10-11)
**Re-verification:** No, initial verification

## Human Verification Results (2026-10-11)

User verdict: "All good — continue".

| Human test | Result |
|---|---|
| Windows real-window walkthrough | pass - user approved |
| HiDPI on a real 125-200 % display | skipped - no multi-monitor HiDPI rig available; scale arithmetic and k=2 rendering covered by the automated checks |
| Real windows on Linux (X11/Wayland) and macOS (Retina) | skipped - no local Linux/macOS desktop; CI covers the same binary and event loop on all three OSes |
| Visual side-by-side vs MFC dipole baseline | pass - zig-out/check/phase3-gui-vs-mfc.png approved in-thread |

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | App launches with no console window on Windows; any of the 9 presets opens from a menu, each in its own tab, charges identical to MFC | VERIFIED | PE optional-header Subsystem of `zig-out/bin/ElectricField.exe` = 2 (read by me with `od`; CI windows job also prints `PE subsystem=2`). `ui.cpp:302-324` File > New builds 9 entries from `ef::presets()`; `Workspace::open` (`workspace.cpp:26`) creates one tab per call from `ef::makeSystem(presets()[i])`. Preset table in `src/core/ef/presets.cpp` compared to `ElectricField/ConfigDocs.cpp`: dipole, quadrupole, parallel, opposed dipoles etc. match positions and signs; `git diff 8f8935a HEAD -- src/core tools/parity docs/baseline ElectricField` is empty. `ef-cli --all --expect tools/parity/expected-counts.csv` exit 0 (9 presets, counts match). |
| 2 | Several configs open at once; close any tab including a computing one, no hang/crash; quit during computation exits cleanly | VERIFIED | `Workspace::close` cancels then destroys (join via RAII); `closeAll` cancels every session first. Unit tests (`tests/test_app.cpp`: "closing a computing tab and closeAll() return promptly...", scripted-fake cancel-before-destroy) in the 78/78 doctest run, which I re-ran: `78 passed, 212997 assertions`. Smoke (my run) `quit_ms=2` with 3 busy sessions, `tabs_opened=2 closed=2`, `middle_click=ok`. tsan-linux CI job success. `App::shutdown` closes sessions before ImGui/SDL teardown (`app.cpp:98`). |
| 3 | Busy indicator on a computing tab; completed field lines and equipotentials appear when done | VERIFIED | `tabLabel` busy glyph + `overlayText` (`ui_logic.cpp`), `drawBusyOverlay` + wait cursor in `ui.cpp:76-78`; `Workspace::poll` builds the scene once on done (`workspace.cpp:74-77`). Smoke `overlay=ok`, Dipole driven to done, `lines=16`; failed tab path `failed_tab=ok`. Finished-tab screenshot (below) shows all lines and equipotentials. |
| 4 | Charges as filled circles by sign; AA Bezier lines; y-up, centred, 100 px/unit; sharp on HiDPI; visually matches the MFC reference | VERIFIED (HiDPI hardware part routed to human) | Single draw path: 0 direct `AddBezierCubic/AddCircleFilled/PathBezierCubicCurveTo` in `src/gui/*.cpp`; `ef::renderScene(painter, ...)` once in `ui.cpp:71`; `ImGuiPainter` does `PathStroke(... Closed)` and `AddCircleFilled`. `canvasView` = `100*k` px/unit, `scaledStyle(k)` scales thickness/radius, `SDL_WINDOW_HIGH_PIXEL_DENSITY`, k from display content scale. Smoke canvas checks vs `efrender::renderSurface` at 640x480 / 1024x820 / 1280x1024: `diff_px=50/28/0`, `chrome_bleed_px=0`, `blue_px=1913 red_px=1916`. Side-by-side with `docs/baseline/mfc/dipole.png` saved to `zig-out/check/phase3-gui-vs-mfc.png`: blue (+) left at x=-2, red (-) right, same line pattern; `magick compare -metric AE -fuzz 10%` GUI canvas vs MFC = 6380 / 786,432 px (0.81 %) dipole, 1.04 % quadrupole, 1.14 % two-unequal-different-signs, consistent with AA/Bezier tessellation differences. Real sharpness on a 125-200 % display needs a human (backstop). |
| 5 | About box showing project and blog links | VERIFIED | `aboutModal` (`ui.cpp:101-177`): icon, "Electric Field", `Version 0.1.0 (<hash>)`, credits, licence, 3 `TextLink`s -> blog `https://compphys.go.ro/electric-field-lines/`, `https://github.com/jmfrank63/ElectricField`, `https://github.com/aromanro/ElectricField` via `SDL_OpenURL`, clipboard fallback. Smoke prints `about: Version 0.1.0 (776aa9d)` = HEAD; `--screenshot ... --modal about` exits ok on all CI OSes. Actual link click needs a human (listed). |

**Score:** 5/5 truths verified (0 present-but-behavior-unverified). The state-transition/cancellation invariants (SC2, SC3) have behavioural tests that pass, not just symbol presence.

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `src/app/workspace.{hpp,cpp}`, `ui_logic.{hpp,cpp}` | VERIFIED | Substantive (112 / 86 lines), wired into `gui/app.cpp` (`ws_.pollAll()`, `ws_.closeAll()`), covered by `tests/test_app.cpp` |
| `src/gui/{main,app,ui,icon}.*` | VERIFIED | 1222 lines total in the .cpp files, built into `ElectricField.exe`, exercised by smoke |
| `src/render/png_export.cpp` (context rule), `tests_gui/test_render_ctx.cpp` | VERIFIED | `zig build test-render`: 4/4 cases, 37 assertions, exit 0 |
| `src/imgui_config/ef_imconfig.h` | VERIFIED | Non-interactive IM_ASSERT, wired via `-DIMGUI_USER_CONFIG` |
| `assets/ElectricField.{ico,rc}`, `-256.png`, `-96.png` | VERIFIED | `cmp assets/ElectricField.ico ElectricField/res/ElectricField.ico` identical; `.rc` wired by `addWin32ResourceFile` (`build.zig:237`), `exe.subsystem = .windows` (`build.zig:239`) |
| `.github/workflows/ci.yml` | VERIFIED | 4 jobs, 6 `uses:` all SHA-pinned, test-render + PE check + smoke + screenshots + artifact uploads present |
| `research-ref/` removed | VERIFIED | `git ls-files | grep -c research-ref` = 0 |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| `ui.cpp` | `ef::renderScene` | `ImGuiPainter` on window draw list | WIRED (`ui.cpp:70-71`) |
| `app.cpp` | `Workspace` | `ws_.pollAll()` each frame, `closeAll()` in `shutdown()` | WIRED |
| `app.cpp` | `efrender::renderSurface` | smoke `checkCanvas` | WIRED (3 canvas checks pass) |
| `build.zig` | `assets/ElectricField.rc` | `addWin32ResourceFile` | WIRED |
| `app.cpp` | `ef_git_hash.h` | make-time header; smoke prints `about: ... (776aa9d)` equal to HEAD | WIRED |
| ci.yml | build.zig steps | `zig build test-render`, `zig build smoke`, `--screenshot` flags | WIRED (jobs green) |

### Data-Flow Trace (Level 4)

| Artifact | Data | Source | Real data | Status |
|----------|------|--------|-----------|--------|
| `drawCanvas` | `t.scene` | `buildScene(t.field, FieldResult)` from `ComputeSession::takeResult` (charges-only scene while computing) | Yes: smoke `lines=16` for Dipole; screenshots show full lines; pixel comparison against `renderSurface` passes | FLOWING |
| `drawBusyOverlay` | `t.linesReceived`, `t.expectedLines` | `poll()` batch count, `ef::expectedLineCount` | Yes | FLOWING |
| `aboutModal` | `env.gitHash` | generated `ef_git_hash.h` | Yes (`776aa9d`) | FLOWING |

### Behavioral Spot-Checks (run by me on 776aa9d)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Core + app + render tests | `zig build test --summary all` | exit 0; doctest 78/78 (212997 assertions) + render-ctx 4/4 | PASS |
| Render-context tests | `zig build test-render` | exit 0, 4 cases | PASS |
| Full UI smoke | `zig build smoke -Doptimize=ReleaseFast` | `smoke ok frames=172 tabs_opened=2 lines=16 closed=2`, 3 canvas checks ok, `keys=ok middle_click=ok failed_tab=ok overlay=ok idle_frames=5/1.3s quit_ms=2`; no `imgui.ini` | PASS |
| Parity oracle | `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` | exit 0, 9 presets | PASS |
| GUI subsystem | PE Subsystem read | 2 | PASS |
| Fatal start | `SDL_VIDEODRIVER=nosuch ElectricField.exe` | exit 1, "Electric Field could not start." / "SDL initialisation: nosuch not available", no dialog | PASS |
| Bad flags | `ElectricField.exe --bogus` | usage on stderr, no window | PASS |
| Screenshots (3 presets) | `--screenshot <slug> ... --size 1024x820` | `ok=1` each | PASS |
| Real window start | PowerShell Start-Process x4 | title `Electric Field`; idle CPU 0 / 0 / 0 % in three runs and 3.12 % in the first run (one-sample outlier at 15.6 ms timer granularity while the CI watcher and builds were running; limit 2 %); no `imgui.ini` | PASS (noted) |

### CI for the pushed head (776aa9d == origin/feature/cross-platform)

Run [38078969099](https://github.com/jmfrank63/ElectricField/actions/runs/38078969099), conclusion success:

| Job | Conclusion | Duration |
|-----|-----------|----------|
| windows-x64 | success | 10m16s |
| linux-x64 | success | 6m34s |
| macos-arm64 | success | 7m33s |
| tsan-linux | success | 2m04s |

Artifacts: exactly `gui-linux-x64, gui-macos-arm64, gui-windows-x64, parity-linux-x64, parity-macos-arm64, parity-windows-x64`. Job logs on all three OSes: `about: Version 0.1.0 (776aa9d)`, identical three canvas lines (`diff_px=50/28/0`, `chrome_bleed_px=0`), `smoke extras: keys=ok middle_click=ok failed_tab=ok overlay=ok`, `smoke ok frames=171..173 tabs_opened=2 lines=16 closed=2`, three `screenshot ... ok=1` lines; windows: `PE subsystem=2`. Remote tags: 0.

### Probe Execution

SKIPPED: no `scripts/*/tests/probe-*.sh` declared by this phase (checks are `zig build smoke` / `test-render`, run above).

### Requirements Coverage

| Requirement | Source plans | Description | Status | Evidence |
|-------------|--------------|-------------|--------|----------|
| VIEW-01 | 03-01, 03-02, 03-03 | Open any of 9 presets from a menu, each in own tab, identical charges | SATISFIED | File > New / chooser, `Workspace::open`, preset table == `ConfigDocs.cpp`, parity oracle |
| VIEW-02 | 03-01..03 | Several configs open, close any tab incl. computing | SATISFIED | cancel-then-join unit tests, smoke quit/middle-click, TSan green |
| VIEW-03 | 03-01..03 | Filled circles by sign, AA Bezier, y-up, centred, 100 px/unit, HiDPI-correct | SATISFIED (HiDPI hardware: human item) | single `renderScene` path, canvas checks, MFC side-by-side |
| VIEW-04 | 03-01..03 | Busy indicator, then finished lines | SATISFIED | overlay/glyph, smoke `overlay=ok`, Done transition tests |
| VIEW-05 | 03-02, 03-03 | GUI app (no console on Windows), About box with links | SATISFIED | PE subsystem 2, About modal, 3 fixed URLs |

REQUIREMENTS.md maps exactly VIEW-01..05 to Phase 3 (5 of 30); all five are claimed by plan frontmatter. No orphaned requirements.

### Prohibitions (ADR-550 D3/D4)

| Prohibition | Tier | Result |
|-------------|------|--------|
| No change to `src/core`, `tools/parity`, `docs/baseline`, `ElectricField/` since 8f8935a | test | VERIFIED by me: `git diff --name-only 8f8935a HEAD -- ...` empty; parity oracle passes |
| No computation left running after tab close / quit | test | VERIFIED: unit tests wired in `zig build test`, TSan job, smoke `closed==opened` |
| About opens only its 3 constant URLs | test | VERIFIED by inspection/grep (3 `https://` literals, 1 `SDL_OpenURL(`, 1 `SDL_SetClipboardText(`). Info: enforced by plan-time greps, not an automated test in CI |
| GUI writes no file or setting (only `--screenshot` path) | test | VERIFIED: `IniFilename = nullptr` (1 hit), no `imgui.ini` after smoke and real-window runs. Same info note |
| No second scene-drawing path | test | VERIFIED: 0 direct draw calls in `src/gui/*.cpp`, 1 `renderScene(painter`. Same info note |
| CI gate not weakened; no force-push/tag | test | VERIFIED: no `continue-on-error`, matrix intact, 6 pinned `uses:`, 0 remote tags |

None of the prohibitions are unresolved; the three grep-based ones have no continuous CI guard (see Info).

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (phase files) | - | `TBD/FIXME/XXX/TODO/HACK` | none found | no debt markers |
| `src/gui/ui.cpp` | 138-139 | none | - | review items WR-01..05 / IN-01..05 all fixed (03-REVIEW-FIX.md); re-review clean |

Info: the grep-based prohibition checks (URLs, no-file-writes, single draw path) are not part of CI; a future refactor could regress them silently. Not a gap for this phase.

### Human Verification Required

See the `human_verification` frontmatter list: Windows real-window walkthrough, HiDPI on a real 125-200 % display (UI-SPEC backstop), real windows on Linux and macOS, and a final visual confirmation against the MFC baseline.

### Gaps Summary

No gaps. Every ROADMAP success criterion is backed by code I read and commands I ran at head 776aa9d, and the head's CI run is green on all four jobs with the expected artifacts. Status is `human_needed` solely because HiDPI sharpness, real-display behaviour on each OS, link clicks and the cursor are not observable headlessly.

Side-by-side evidence for the user: `C:\Users\jmfrank\source\repos\jmfrank63\ElectricField\zig-out\check\phase3-gui-vs-mfc.png` (MFC dipole.png left, GUI canvas crop right); also `zig-out/check/phase3-gui-vs-mfc-quadrupole.png`.

---

_Verified: 2026-10-11_
_Verifier: Claude (gsd-verifier)_
