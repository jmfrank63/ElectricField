---
phase: 03-gui-shell
reviewed: 2026-10-11T00:00:00Z
depth: standard
files_reviewed: 22
files_reviewed_list:
  - .github/workflows/ci.yml
  - .gitignore
  - assets/ElectricField.rc
  - build.zig
  - build.zig.zon
  - src/app/ui_logic.cpp
  - src/app/ui_logic.hpp
  - src/app/workspace.cpp
  - src/app/workspace.hpp
  - src/gui/app.cpp
  - src/gui/app.hpp
  - src/gui/icon.cpp
  - src/gui/icon.hpp
  - src/gui/main.cpp
  - src/gui/ui.cpp
  - src/gui/ui.hpp
  - src/imgui_config/ef_imconfig.h
  - src/render/png_export.cpp
  - src/render/png_export.hpp
  - tests/test_app.cpp
  - tests_gui/test_main.cpp
  - tests_gui/test_render_ctx.cpp
findings:
  critical: 0
  warning: 0
  info: 3
  total: 3
status: clean
---

# Phase 3: Code Review Report (iteration 2, re-review after fixes)

**Reviewed:** 2026-10-11 · **Depth:** standard · **Files Reviewed:** 22 · **Status:** clean (no critical or warning findings remain; 3 minor info items)

## Summary

All iteration-1 findings (WR-01..05, IN-01..05) are resolved. The fixes in `git diff b9e4775..HEAD` (10 files) were traced against the focus areas (assert-hook thread-safety and re-entrancy, message box from inside an assert during rendering, argument-parsing edge cases, minimum-size update ordering) and no regression of warning or critical severity was found. Three small info items are noted; none block.

## Resolution table (iteration 1)

| ID | Status | Verification |
|----|--------|--------------|
| WR-01 | Resolved | `src/gui/ui.cpp:147-172`: `linkActivated` is set inside the `TextLink` branch, `enterClose = !linkActivated && (Enter \|\| KeypadEnter)`. The URL-open failure note is now set in a frame that does not clear it. Button and Escape still close. |
| WR-02 | Resolved | `src/gui/app.cpp:115-130`: `refreshScale` re-applies `SDL_SetWindowMinimumSize(minimumWindowSize(k))` on every scale refresh. The creation-time clamp (`min(minSize, windowSize)`) applies only to the `initial` call, which now runs at the end of `init` once the window exists (the earlier call is removed; nothing between window creation and that point depends on the minimum). |
| WR-03 | Resolved (acceptable, see below) | `build.zig:205-212`: probe and make-time step run the identical argv with cwd = build root. |
| WR-04 | Resolved | `src/gui/main.cpp:53-93`: unknown, incomplete or invalid options exit 2 with usage on stderr. The `--size` check is `sscanf == 2 && w > 0 && h > 0`. `--scale` uses `strtod` with end-pointer and `!(k > 0)` (also rejects NaN). `--modal` is validated. `-psn_*` is tolerated. The message box is suppressed whenever `--smoke` or `--screenshot` appears anywhere on the command line, so headless/CI invocations cannot block. All three CI screenshot invocations (`ci.yml:80-82`) and `build.zig:249` still parse. |
| WR-05 | Resolved | `src/imgui_config/ef_imconfig.h`: stderr line, then a one-shot hook (cleared before the call, so a recursive assert from the hook cannot loop), then `_Exit(3)`. The hook is installed only in interactive mode (`main.cpp:95-96`); headless, ef-cli and unit tests keep the silent exit 3. |
| IN-01 | Resolved | Stale wording removed in `ui.hpp` and `ui_logic.hpp`. |
| IN-02 | Resolved | `checkCanvas` fails with an explicit report if pixel density != 1. |
| IN-03 | Resolved | `App::lastSerial()` returns 0 for no tab. `pumpUntilDone(0, ...)` returns false because `find(0)` is null (serials start above 0). `closeSerial = 0` means "none" (`ui.cpp:356`), so the guard is consistent. `runScreenshot` now reports a timeout or failure. The remaining `tabs().back()` uses in `smokeExtras` are short-circuited behind `size() == 1`. |
| IN-04 | Resolved | `defaultWindowSize` comment now matches the code. |
| IN-05 | Resolved | `GuiLikeContext` is null-safe with `ok`/`backend` flags and `REQUIRE(gui.ok)` at the call sites. Unused `Workspace ws` removed. |

## Judgement on WR-03 (no make-time empty-hash fallback)

Acceptable. Probe and make-time step now use byte-identical argv and the same cwd (`b.root.root_dir.handle` vs `b.path(".")`) and inherit the same environment, so a probe success implies the make-time `git log` succeeds. The cases that motivated the finding (an unrelated repo in the process cwd, `safe.directory` refusing the build root, no `.git`) now make the probe fail and select the static empty-hash header. Not adding `-c safe.directory=*` is the correct call. The only residual is a cached configure result going stale after `.git` is removed between builds, which is a negligible edge case.

## Focus-area analysis (no findings above Info)

- **Assert hook thread-safety.** `ef_imgui_assert_hook` is a plain, non-atomic inline pointer. `IM_ASSERT` can only fire on the thread that drives ImGui (UI thread; session workers never touch ImGui and `renderSurface` runs on the caller's thread), and the process terminates immediately afterwards, so no data race is realistically reachable. It is set once in `run()` before `App::init`, never concurrently.
- **Re-entrancy and a message box during rendering.** `SDL_ShowSimpleMessageBox` runs a nested loop on the UI thread. The app registers no `SDL_AddEventWatch` or render callback, and `SDL_PumpEvents` is not called by the box. The nested loop can therefore not re-enter `frame()`, `NewFrame` or `Render` while ImGui state is mid-frame. The hook is cleared before the box opens, `_Exit(3)` follows unconditionally, and the `[[noreturn]]` contract holds even if the hook returns. The box works before `SDL_Init` and after `SDL_Quit` too (SDL returns false instead of hanging), so asserts during init or shutdown still exit with code 3.
- **Argument parsing.** The headless pre-scan means even a malformed headless invocation exits 2 without a dialog. `i + 2 >= argc` for `--screenshot` is the exact complement of the old `i + 2 < argc` guard. A value that is itself an option name (e.g. `--modal --smoke`) is consumed as the value and rejected, which is the right outcome.
- **Minimum-size ordering.** `refreshScale(true)` now runs after the renderer and ImGui exist; the final minimum is applied before the first frame. For non-initial refreshes the new minimum is applied after `k_` and the style are updated, so the layout and the window constraint agree in the same frame. The smoke "minimum window size" step uses `minimumWindowSize(k_)`, which equals the clamped initial minimum under the dummy driver, so the test is not affected.

## Info

### IN-06: After the first display event the creation-size clamp of the minimum is lost
**File:** `src/gui/app.cpp:124-129`
**Issue:** Only the `initial` call clamps the minimum to the creation size. Any later `SDL_EVENT_WINDOW_DISPLAY_CHANGED` / `..._SCALE_CHANGED` (also possible spuriously right after start-up on some platforms) applies the full `minimumWindowSize(k)`. With an interactive `--size` smaller than the minimum, or a display whose usable area is smaller than the minimum (e.g. a very high content scale on a small panel), SDL then grows the window to the minimum. Unreachable in the headless modes and in normal configurations (default size is always >= minimum unless the display is tiny), so this is cosmetic.
**Fix:** Optionally keep the clamp as a member (`minClampW_/minClampH_` captured at creation) and apply `std::min(minSize, clamp * k / k0)`, or accept it as documented behaviour.

### IN-07: `--size` accepts trailing garbage and is not range-checked
**File:** `src/gui/main.cpp:67-71`
**Issue:** `sscanf("%dx%d")` returns 2 for `1024x820abc`, and an out-of-range number (`99999999999x1`) is undefined behaviour in `sscanf`. Harmless in practice (developer/CI option; `SDL_CreateWindow` failure is reported), but the stricter validation added for `--scale` was not applied here.
**Fix:** Parse with `strtol` and an end-pointer check, or use `"%dx%d%n"` and require the consumed length to equal `strlen`; cap each dimension (e.g. <= 16384).

### IN-08: `--help` / `-h` is reported as an error
**File:** `src/gui/main.cpp:88-90`
**Issue:** Because every unknown option is rejected, `--help` prints usage but exits 2 and, in interactive form, opens an error-styled message box.
**Fix:** Add a `--help`/`-h` branch that prints `kUsage` to stdout and returns 0 (no message box).

---

_Reviewed: 2026-10-11_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
