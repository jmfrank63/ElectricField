---
phase: 03-gui-shell
fixed_at: 2026-10-11T00:00:00Z
review_path: .planning/phases/03-gui-shell/03-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 3: Code Review Fix Report

**Fixed at:** 2026-10-11
**Source review:** .planning/phases/03-gui-shell/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope (critical + warning): 5
- Fixed: 5
- Skipped: 0
- Info items also fixed (requested by the orchestrator): IN-01, IN-02, IN-03, IN-04, IN-05

Worktree isolation: `workflow.use_worktrees` is `false` in `.planning/config.json`, so all edits and commits were made in the main checkout on `feature/cross-platform` (no worktree, temp branch or recovery sentinel). Nothing was pushed.

## Fixed Issues

### WR-01: Enter on a focused About link opens the URL and also closes the About modal

**Files modified:** `src/gui/ui.cpp`
**Commit:** ed03917
**Applied fix:** `linkActivated` is tracked in the link loop (the open-URL and clipboard-fallback logic now sit inside the activation branch); the Enter/KeypadEnter close condition is `!linkActivated && (Enter || KeypadEnter)`. Button click and Escape still close. The fallback note now survives the frame in which it is set. Logic fix: requires human verification (keyboard-activate a link with Enter in the interactive app).

### WR-02: Minimum window size is set once and never follows later scale changes

**Files modified:** `src/gui/app.cpp`, `src/gui/app.hpp`
**Commit:** f5b434e
**Applied fix:** `refreshScale(bool initial = false)` now calls `SDL_SetWindowMinimumSize(minimumWindowSize(k))` on every scale refresh. Only the first call (from `init`) clamps the minimum to the creation size, so a small `--size` or a small display does not force the window to grow. The creation-time call in `init` was removed (the final `refreshScale(true)` replaces it). Logic fix: requires human verification (drag the window between displays of different scale).

### WR-03: The git-hash probe and the git-hash Run step use different working directories

**Files modified:** `build.zig`
**Commit:** a3fb27f
**Applied fix:** the configure-time `b.runFallible` probe now runs with `.cwd = .{ .dir = b.root.root_dir.handle }` (the build root), the same directory as the make-time `run.setCwd(b.path("."))`, and with the identical argv. A probe success therefore implies the make-time command succeeds (an unrelated repo in the process cwd, or git refusing the build root, e.g. `safe.directory`, makes the probe fail and select the empty hash). `-c safe.directory=*` was deliberately NOT added (it would disable a git security check). The reviewer's second suggestion (make-time fallback to an empty hash) is not expressible with a Zig `Run` step (no "ignore exit code" option) without a custom Step; the identical-command-and-cwd probe removes the mismatch instead. Verified: `zig build --build-file ElectricField/build.zig` from the parent directory produced the correct hash; the Linux build from a `git archive` copy (no `.git`) shows `Version 0.1.0 (unknown)`.

### WR-04: Malformed headless flags fall through to the interactive GUI

**Files modified:** `src/gui/main.cpp`
**Commit:** 097899a
**Applied fix:** the argument parser now rejects unknown options, incomplete `--screenshot`/`--size`/`--modal`/`--scale`, `--size` that is not `WxH` with both > 0 (`sscanf == 2`), `--modal` other than `about|chooser`, and a non-numeric or non-positive `--scale`. Usage goes to stderr and the exit code is 2. The message box is shown only when no headless mode (`--smoke`/`--screenshot`) appears anywhere on the command line, so headless invocations can never block. macOS `-psn_*` Finder arguments are ignored. Verified: `--smoke --bogus`, `--screenshot dipole`, `--size 10x`, `--scale abc`, `--modal foo` all exit 2 without a dialog; the valid CI invocations still work; interactive `--smok` showed the message box (process killed afterwards).

### WR-05: `IM_ASSERT` is fatal and silent in every build, including the interactive release app

**Files modified:** `src/imgui_config/ef_imconfig.h`, `src/gui/main.cpp`
**Commit:** b50e77a
**Applied fix:** `ef_imconfig.h` gains an inline function pointer `ef_imgui_assert_hook` (null by default). `ef_imgui_assert_fail` still writes to stderr, then calls the hook once (cleared first so it cannot recurse) and ends with `_Exit(3)`. `main.cpp` installs a hook that calls `SDL_ShowSimpleMessageBox` only in interactive mode; the headless modes, ef-cli and the unit tests leave it null, so they remain non-interactive with exit code 3 (the render-context regression test relies on that). Verified with a temporary probe (reverted, not committed): interactive run stayed alive on the message box, headless run exited 3 with the stderr line.

## Info items (fixed at the orchestrator's request)

- **IN-01** (11d2418): "research prototype" wording removed from `src/gui/ui.hpp` and `src/app/ui_logic.hpp`.
- **IN-02** (bde2539): `checkCanvas` now fails with an explicit report if `SDL_GetWindowPixelDensity != 1` instead of silently mixing logical and pixel coordinates (the smoke always runs with the dummy driver, density 1).
- **IN-03** (24debb8): new `App::lastSerial()` (0 if no tab) replaces every unguarded `tabs().back()->serial`; `runScreenshot` prints a message when the session does not finish.
- **IN-04** (cad7838): the `defaultWindowSize` comment now states what the code does (no lower bound; the minimum is `minimumWindowSize`).
- **IN-05** (5e97bb8): `GuiLikeContext` no longer uses `REQUIRE` in the constructor (`ok`/`backend` flags, null-safe destructor, tests `REQUIRE(gui.ok)`); the unused `Workspace ws` in `tests/test_app.cpp` was removed.

## Verification

Where it ran: the Windows gates ran in the MAIN CHECKOUT (`C:\Users\jmfrank\source\repos\jmfrank63\ElectricField`, no worktree); the Linux smoke ran in WSL (Ubuntu) from a `git archive HEAD` copy of the final commit using Zig 0.17.0 at `~/zig17`.

| Gate | Result |
|------|--------|
| `zig build test -Dgui=false` | PASS (78 test cases, 212997 assertions) |
| `zig build test` | PASS (GUI render-context tests: 4 cases, 37 assertions) |
| `zig build test-render` | PASS (4 cases, 37 assertions) |
| `zig build smoke -Doptimize=ReleaseFast` | PASS (`smoke ok frames=172 tabs_opened=2 lines=16 closed=2`) |
| `zig build -Doptimize=ReleaseFast` + `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` | PASS (exit 0, all 9 presets match the expected counts) |
| WSL Linux smoke from `git archive HEAD` | PASS (`Version 0.1.0 (unknown)` without `.git`; `smoke ok`) |

Real GUI processes were started only by this run and all were terminated (final `tasklist` shows no ElectricField.exe). Working tree is clean; no files under `src/core`, `tools/parity`, `docs/baseline` or `ElectricField/` were touched. Not pushed.

---

_Fixed: 2026-10-11_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
