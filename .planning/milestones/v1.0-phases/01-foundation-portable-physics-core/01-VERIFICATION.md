---
phase: 01-foundation-portable-physics-core
verified: 2026-10-10T12:00:00Z
status: human_needed
score: 5/5 roadmap success criteria verified (all plan must-haves verified or accepted); 2 human-only items outstanding
behavior_unverified: 0
overrides_applied: 0
gaps: []
deferred: []
human_verification:
  - test: "Run `zig build run` on Windows and look at the window"
    expected: "A resizable 960x640 'Electric Field' window opens with an ImGui panel reading 'Hello from SDL3 + Dear ImGui'; no console window appears (GUI subsystem); closing the window exits cleanly."
    why_human: "Interactive window on a real display. Automated evidence covers only the headless dummy-driver/software-renderer path (`zig build smoke` prints `video=dummy renderer=software presets=9` / `smoke ok frames=2`) and the PE subsystem flag (value 2 = GUI)."
  - test: "Visually review the 9 baseline PNGs in docs/baseline/mfc/"
    expected: "Each image shows the right charge layout and sign colouring (blue = +, red = -) for its preset, black field lines, grey equipotentials, y-up, centred, 100 px per unit, fully computed (no half-drawn views). Layout and colours match what the MFC app shows at default options."
    why_human: "Judging whether the images are correct MFC renderings is visual. Mechanically verified: 9 files, all 1024x768, 9 distinct SHA-256 hashes, README provenance present. One image (quadrupole.png) was inspected and its charge colours match the preset table (top-left -1 red, top-right +1 blue, bottom-left +1 blue, bottom-right -1 red)."
---

# Phase 1: Foundation & Portable Physics Core Verification Report

**Phase Goal:** As the developer, I want a Zig-built, CI-verified physics core on all three OSes, so that the port rests on trusted numerics.
**Mode:** mvp
**Verified:** 2026-10-10
**Status:** human_needed
**Re-verification:** No, initial verification

All automated checks pass. The phase goal is achieved in the codebase. The two remaining items need a person (interactive window, visual review of reference images), so the status is `human_needed` rather than `passed`. There are no gaps and no blockers.

## Goal Achievement

### Observable Truths (ROADMAP success criteria, the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `zig build` and `zig build test` succeed with only Zig 0.17.0; Dear ImGui, SDL3 and doctest come from a hash-pinned `build.zig.zon`; an SDL3+ImGui hello-window target builds and starts on each OS | VERIFIED (interactive Windows window goes to human item 1) | Ran locally on Windows with Zig 0.17.0: `zig build --summary all` (11/11 steps) and `zig build test` (28/28 test cases) pass. `build.zig.zon` pins sdl3 (git commit `dbcb19a...` plus hash), imgui v1.92.9b-docking (hash) and doctest v2.5.3 (hash), `minimum_zig_version = "0.17.0"`. `zig build smoke` printed `video=dummy renderer=software presets=9` and `smoke ok frames=2`. CI run 38040977802 (empty Zig cache): `zig build`, `zig build test` and `zig build smoke` succeeded on windows, linux and macos-arm64; the log shows `presets=9` on all three. `ci.yml` has no package-manager install step. |
| 2 | A GitHub Actions workflow builds and tests on windows, ubuntu and macos-arm64 for every push to the branch and is green on all three | VERIFIED | `.github/workflows/ci.yml`: matrix windows-latest, ubuntu-latest, macos-latest; `on.push.branches: [feature/cross-platform]` plus pull_request, no path filters; `fail-fast: false`; `permissions: contents: read`; setup-zig pinned to a commit SHA; no `continue-on-error`. `gh run view 38040977802` (headSha `d17dd2f`, the last code-changing commit): conclusion success, 3/3 jobs success, every step success. The follow-up run 38044238937 (headSha `3e672b2`) is also success on 3/3 jobs. The only commit after that, `f857878` (docs: code review report), changes no code and is not pushed. `git diff --stat d17dd2f HEAD` touches only .planning files. |
| 3 | `git ls-files` lists no `ElectricField/x64/`, `.vs/` or `*.vcxproj.user`; `git status` stays clean after an MSVC build and a Zig build because `.gitignore` covers both | VERIFIED | `git ls-files \| grep -E "x64/\|\.vs/\|vcxproj.user"` is empty. `git check-ignore -v` confirms `ElectricField/x64`, `.vs`, `zig-out` and `.zig-cache` are covered. `git status --short` is empty after my local `zig build`, `zig build test` and `zig build smoke` runs, while `--ignored` shows the on-disk MSVC outputs (`x64/`, `ElectricField/x64/`, `.vs/`, `*.vcxproj.user`) as ignored. MSBuild cleanliness was not re-run by me. 01-04-SUMMARY records it, and the ignored on-disk MSVC outputs are consistent with that claim. |
| 4 | The GUI-free core (std only, no global state) passes its tests: field and potential match analytic values; all 11 integrators exist in the original order with their expected convergence order; every preset yields seeds proportional to \|q\| x lines-per-unit-charge; net-zero keeps `sign(0)==+1` | VERIFIED | See sub-rows below. Core suite passes in Debug+UBSan and in ReleaseFast locally (28 test cases, 4385 assertions) and on all three OSes in CI. |
| 5 | Reference images of all 9 presets from the existing MFC build with default options are recorded in the repo before any MFC file is removed | VERIFIED (visual correctness goes to human item 2) | `docs/baseline/mfc/` holds 9 PNGs, all 1024x768, 9 distinct hashes, named by preset slug, plus a README with provenance (date, MSVC 14.51 / v145, DPI 96, build and capture commands, defaults, reproducibility compare). `tools/baseline/capture-mfc-baseline.ps1` is committed with registry backup and restore (`-BackupDir` defaults to `%TEMP%`, `Stop-Process` only on the process it started). The MFC tree is intact: `git diff fea046f HEAD -- ElectricField` shows only the v143 to v145 toolset lines in the vcxproj plus untracked build outputs and the `.user` file. |

#### Truth 4 in detail

| Aspect | Evidence |
|--------|----------|
| Std-only, no GUI identifiers | Includes in `src/core/ef/*` are only `<array> <cmath> <cstddef> <cstdlib> <cfloat> <string_view> <vector>` plus `ef/` headers. A grep for windows.h, afx, MFC, d2d, SDL and imgui in `src/core` finds nothing in code. |
| No global or app state | Grep for `g_`, `thread_local` and non-const statics is empty. The only `static` is the const preset table. |
| Charge field and potential vs analytic | `tests/test_field.cpp` checks Potential `q/r` and E `q r/\|r\|^3` at epsilon 1e-12 for a 3-4-5 triangle, plus dipole on-axis and mid-plane superposition. Passes. |
| 11 integrators in original order | `kMethods` has 11 entries; the test checks `enum value == index` and the stable names; `withSolver` dispatches all 11 with consistent `IsAdaptive()`. |
| Convergence order | `test_rk_order.cpp` measures observed order (log2 of error ratio) for the 6 fixed-step methods and the stepping solution of the 5 adaptive pairs, on a linear rotation ODE (within 0.3 of nominal) and a non-linear time-dependent ODE (at least nominal - 0.5). It also checks the embedded lower order and the adaptive step-control behaviour (shrink, grow, `max_step` clamp). The Dormand-Prince error-weight defect (low weights are `b - b*`) is kept deliberately for MFC parity and pinned by a characterisation test. Phase 2 owns that decision. |
| Every method produces a field line | `test_seeds.cpp` traces a dipole line with each of the 11 methods through `withSolver`. Each ends within 0.35 of the negative charge with all points finite. |
| Seeds proportional to \|q\| x L | The test covers all 9 presets, L in {4, 16, 32} and equipotentials on and off. Sources get exactly `\|q\|*L` seeds and sinks get 1 (or 0 with equipotentials off). Every seed lies at radius 0.25. |
| `sign(0)==+1` | `ef::sign(0) == +1` is tested. Net-zero presets (dipole, quadrupole, two-dipoles, parallel) have their positive charges as sources. A negative total flips the roles. |
| Presets equal the MFC source | `test_presets.cpp` parses `ElectricField/ConfigDocs.cpp`. I ran it alone: 1 case, 94 assertions, so the cross-check really executes and is not silently skipped (see warning WR-05). |

### Plan must-haves (spot-verified beyond the roadmap contract)

| Must-have | Status | Evidence |
|-----------|--------|----------|
| `zig build test -Dgui=false` runs doctest without building SDL3/ImGui | VERIFIED | Build graph shows only the efcore lib and tests compiled. 5/5 steps. (Dependencies are still fetched, see WR-01.) |
| `zig build smoke` headless, 2 frames, `presets=9`, exit 0 | VERIFIED | Ran locally, and in CI on all three OSes. |
| Core flags `-Werror`, `-ffp-contract=off`, strict warnings; GUI and third-party excluded | VERIFIED | `build.zig` `core_flags` contains `-Werror`, `-ffp-contract=off`, `-Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -pedantic`. `gui_flags` has none of them. Core builds clean under Zig 0.17 clang. |
| Windows exe is a GUI-subsystem binary using `SDL_main.h` | VERIFIED | `zig-out/bin/ElectricField.exe`: PE subsystem field = 2 (WINDOWS_GUI). `main.cpp` includes `<SDL3/SDL_main.h>` and uses `int main(int, char**)`. |
| 9 presets as a data table with .rc names, charges in MFC order | VERIFIED | `presets.cpp` table plus the name test plus the ConfigDocs cross-check. |
| Empty ChargeSystem handled | VERIFIED | `totalCharge` returns 0 for empty. Tested in `test_field.cpp` and `test_seeds.cpp`. |
| Same suite passes in Debug (UBSan) and ReleaseFast | VERIFIED | Ran both locally (28/28, 4385/4385) and in CI. |
| ci.yml core gate then GUI gate on the 3 OSes with pinned action | VERIFIED | Steps: core Debug, core ReleaseFast, build, test, smoke. |
| Zon pins: 3 distinct hashes, url+hash, SDL pinned to a commit | VERIFIED | `build.zig.zon` read directly. |
| MFC sources untouched (only vcxproj toolset line plus untracking) | VERIFIED | `git diff fea046f HEAD -- ElectricField` shows only the toolset line change, the removed `.user` and the removed `x64/` outputs. |
| `research-ref/` removed, nothing outside `.planning` references it | VERIFIED | Directory absent. A grep for `research-ref` outside `.planning` finds nothing. |
| D-12: push only after explicit user consent (prohibition, test tier) | VERIFIED by record (not independently re-provable) | 01-04-SUMMARY records the user answering "push" at the blocking checkpoint. `origin/feature/cross-platform` equals `3e672b2` and the run history starts at `d17dd2f`. Nothing contradicts the record. |
| CI gate not weakened (prohibition, test tier) | VERIFIED | No `continue-on-error`, three matrix OSes with `fail-fast: false`, `-Werror` and `-ffp-contract=off` present, and tolerances and steps match the plan. |
| MFC baseline prohibitions (registry key restored, backup outside repo, kill only own process) | VERIFIED by code read | The script backs the key up to `$BackupDir` (default `%TEMP%`), restores it in a finally path, and `Stop-Process -Id $p.Id` targets only its own process. |

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `build.zig.zon` | VERIFIED | Pins sdl3, imgui and doctest. |
| `build.zig` | VERIFIED | efcore lib, efcore-tests, ElectricField exe, steps `test`, `run`, `smoke`, option `gui`. |
| `src/core/ef/{math,vec2,rk,method,params,charge_system,presets,seeds,tracer}` | VERIFIED | Substantive (1124 lines across core and tests), wired through the tests and `src/gui/main.cpp` (`ef::presets()`). |
| `tests/test_*.cpp` (6 files, 28 test cases) | VERIFIED | Run and pass. Assertions are real numerical checks, not placeholders. |
| `.github/workflows/ci.yml` | VERIFIED | |
| `docs/baseline/mfc/*.png` (9) plus README | VERIFIED | |
| `tools/baseline/capture-mfc-baseline.ps1` | VERIFIED | |
| `.gitignore` | VERIFIED | Covers `.zig-cache/`, `zig-out/`, `zig-pkg/`, `.vs/`, `x64/`, `*.user`, and other MSVC artifacts. |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| `build.zig` tests | `ElectricField/ConfigDocs.cpp` | `setCwd(b.path("."))`, parsed by `test_presets.cpp` | WIRED (94 assertions executed) |
| `test_seeds.cpp` | `method.hpp` `withSolver` over `kMethods` | Drives `traceFieldLine` for all 11 methods | WIRED |
| `tracer.hpp` | `charge_system.hpp` | `ENormalized` direction and `HitCharge` stop | WIRED |
| `src/gui/main.cpp` | `efcore` | `ef::presets().size()` printed in the smoke line | WIRED |
| push to branch | `ci.yml` | `on.push.branches: [feature/cross-platform]` | WIRED (runs exist for the pushed heads) |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Core tests, Debug+UBSan | `zig build test -Dgui=false --summary all` | 28/28 cases, 4385 assertions | PASS |
| Core tests, ReleaseFast | `zig build test -Dgui=false -Doptimize=ReleaseFast` | 28/28 cases, 4385 assertions | PASS |
| Full build | `zig build --summary all` | 11/11 steps | PASS |
| Default test step | `zig build test` | 28/28 | PASS |
| Headless GUI start | `zig build smoke` | `video=dummy renderer=software presets=9`, `smoke ok frames=2` | PASS |
| Preset cross-check actually runs | `efcore-tests -tc="preset charges*"` | 94 assertions passed | PASS |
| GUI subsystem flag | read PE header of `ElectricField.exe` | subsystem = 2 | PASS |
| Tree stays clean after the Zig build | `git status --short` | empty | PASS |
| CI verdict | `gh run view 38040977802` and `38044238937` | success, 3/3 jobs each, headSha matches | PASS |

### Probe Execution

Step 7c: SKIPPED. The phase declares no `probe-*.sh` scripts (the `smoke` build step is covered above).

### Requirements Coverage

All 10 phase requirement IDs appear in plan frontmatter. REQUIREMENTS.md maps exactly these 10 to Phase 1, so there are no orphans. They are marked `[x]` and Complete there.

| Requirement | Source Plan(s) | Status | Evidence |
|-------------|----------------|--------|----------|
| BUILD-01 | 01-01, 01-04 | SATISFIED | `zig build` works locally on Windows. Green on all three OSes in CI with only Zig installed. Interactive window is human item 1. |
| BUILD-02 | 01-01, 01-03, 01-04 | SATISFIED | `zig build test` passes locally and in CI on all three. |
| BUILD-03 | 01-01, 01-04 | SATISFIED | Three url+hash pins in `build.zig.zon`. CI fetched them on an empty cache. |
| BUILD-04 | 01-01, 01-04 | SATISFIED | `ci.yml` plus green runs 38040977802 and 38044238937. |
| BUILD-05 | 01-01, 01-04 | SATISFIED | Nothing tracked under `x64/`, `.vs/` or `*.vcxproj.user`. `.gitignore` covers MSVC and Zig outputs. |
| CORE-01 | 01-01, 01-03 | SATISFIED | std-only core, no global state, warning-clean under `-Werror`. |
| CORE-02 | 01-03 | SATISFIED | 11 methods in the original order. Each traces a field line. Convergence orders tested. |
| CORE-03 | 01-03 | SATISFIED | Seed count test over 9 presets x 3 values of L x equipotentials on/off, `sign(0)==+1` tests. |
| QA-01 | 01-01, 01-03 | SATISFIED | Vector math, analytic field/potential and convergence-order tests. |
| QA-05 | 01-02 | SATISFIED (visual review is human item 2) | 9 baseline PNGs recorded before any MFC removal, with provenance. |

### Anti-Patterns Found

No `TBD`, `FIXME`, `XXX`, `TODO` or `HACK` in `src/`, `tests/`, `build.zig`, `build.zig.zon`, `.github/` or `tools/`. No stub implementations. Core functions do real computation and are exercised by tests.

Advisory items from `01-REVIEW.md` (0 critical, 6 warning, 5 info). I re-checked the ones that bear on the phase goal. None fails a must-have, and none blocks the phase:

| ID | Finding | Verifier assessment |
|----|---------|---------------------|
| WR-01 | `sdl3` and `imgui` are not `.lazy`, so `-Dgui=false` and the CI "core gate first" still fetch them. The CI comment claims independence from the GUI stack. | Confirmed (`grep lazy build.zig.zon` is empty). This does not fail any success criterion (the dependencies are hash-pinned and fetched, and nothing is built for `-Dgui=false`). Warning only. Worth fixing early in Phase 2. |
| WR-02 | The GUI loop busy-spins (no vsync, no wait). Confirmed (`SDL_SetRenderVSync` absent). | Out of scope for a hello-window skeleton. Carry into Phase 3. |
| WR-03 | `info()` and `withSolver` do not validate out-of-range `Method` values. | Not reachable until Phase 4 loads user config. Carry forward. |
| WR-04 | The baseline capture can write a blank PNG if `PrintWindow` fails. | The 9 committed images are distinct and the quadrupole image is visibly correct. Covered by human item 2. |
| WR-05 | The preset cross-check would silently pass if `ConfigDocs.cpp` could not be opened. | It does run (94 assertions). The risk is only when someone runs from another cwd. Harden in Phase 2. |
| WR-06 | `SolveStepPair` duplicates the `SolveStep` stage loop, so the low-order tests exercise a copy. | Does not affect the verified truths (the high-order stepping path is tested through `SolveStepFixed` and `SolveStep`). Maintainability, carry forward. |
| IN-01 to IN-05 | `expectedLineCount` vs `planSeeds` when equipotentials are off; degenerate params; GUI error paths; CI branch triggers and ignore anchors; missing `<algorithm>`. | Informational. |

### Human Verification Required

#### 1. Interactive window on Windows

**Test:** Run `zig build run` on Windows.
**Expected:** A resizable "Electric Field" window opens with the ImGui hello panel and no console window. Closing it exits cleanly.
**Why human:** Needs a real display and a person. Headless smoke, the GUI subsystem flag and CI are all green.

#### 2. Visual review of the 9 baseline PNGs

**Test:** Open each file in `docs/baseline/mfc/` and compare it with the preset table in `docs/baseline/mfc/README.md`.
**Expected:** Correct charge layout and colours, black field lines, grey equipotentials, complete drawing, y-up and centred, same look as the MFC app at defaults.
**Why human:** "Looks like the MFC app" is a visual judgement. Mechanical checks (9 files, 1024x768, distinct hashes, README provenance) pass, and the one image I viewed was consistent with its preset.

### Gaps Summary

None. Every roadmap success criterion and every plan must-have is verified by code, by locally executed commands, or by the green CI runs for the pushed heads. The phase contains no behavior-dependent truths of the cancellation or state-transition kind (threading starts in Phase 2). The status is `human_needed` solely because of the two human-only checks above.

---

_Verified: 2026-10-10_
_Verifier: Claude (gsd-verifier)_
