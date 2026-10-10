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
  warning: 5
  info: 5
  total: 10
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-10-11 · **Depth:** standard · **Files Reviewed:** 22 · **Status:** issues_found

## Summary

No blockers in the focus areas. Session lifecycle is sound (`Workspace::close`/`closeAll` cancel before destroy; `Tab::session` is the last member so it is destroyed first; `drawUi` defers closes via `toClose`). Tab ImGui IDs use never-reused `###ef<serial>`. `App::shutdown` is idempotent and correctly ordered (sessions → backends → context → texture → renderer → window → `SDL_Quit`); `fail()` copies `SDL_GetError()` before shutdown. The offscreen render context rule in `renderSurface` is correct (create → guard → explicit `SetCurrentContext`; `BackendGuard` before `CtxGuard`; caller context restored on every path). Only three constant URLs reach `SDL_OpenURL`. Findings are interaction edge cases, build/CI robustness and the cost of making `IM_ASSERT` fatal.

## Warnings

### WR-01: Enter on a focused About link opens the URL and also closes the About modal
**File:** `src/gui/ui.cpp:147-170`
**Issue:** `ImGui::TextLink` activates on Enter/Space; the same frame's close condition tests `IsKeyPressed(Enter/KeypadEnter)` unconditionally, so keyboard activation of a link also closes the dialog, and on `SDL_OpenURL` failure the clipboard-fallback note is set and cleared in the same frame (never shown).
**Fix:** Track `linkActivated` in the link loop and only treat Enter as "close" when no link was activated this frame.

### WR-02: Minimum window size is set once and never follows later scale changes
**File:** `src/gui/app.cpp:62-63, 117-126`
**Issue:** `SDL_SetWindowMinimumSize(minimumWindowSize(k0))` is never refreshed in `refreshScale()`; after moving to a 2.0 display the window can shrink to half the intended logical minimum, crushing the layout/overlay.
**Fix:** In `refreshScale()` set `SDL_SetWindowMinimumSize(window_, minimumWindowSize(k))` (keep the initial `std::min` clamp only at creation).

### WR-03: The git-hash probe and the git-hash Run step use different working directories
**File:** `build.zig:201-212`
**Issue:** `b.runFallible` runs `git` in the process cwd while the make-time step uses the build root; the probe can succeed in an unrelated repo (wrong hash) or fail outside one ("(unknown)" despite a checkout); a probe success followed by a make-time git failure (e.g. `safe.directory`) fails the whole GUI build instead of falling back.
**Fix:** Pin both to the build root (`git -C <build_root> ...`, probe with `rev-parse --git-dir` at the build root) and make the make-time step fall back to an empty hash rather than failing the build.

### WR-04: Malformed headless flags fall through to the interactive GUI
**File:** `src/gui/main.cpp:34-48`
**Issue:** Incomplete `--screenshot`, typos like `--smok`, or unparsable `--size` silently start the interactive app (hangs scripts/CI on the chooser; no console on Windows to show diagnostics).
**Fix:** Unknown/incomplete options → usage on stderr, exit 2 (message box only in interactive mode); check `sscanf == 2` for `--size`.

### WR-05: `IM_ASSERT` is fatal and silent in every build, including the interactive release app
**File:** `src/imgui_config/ef_imconfig.h:9-15`
**Issue:** `IM_ASSERT` → `fprintf` + `_Exit(3)` in all modes; asserts normally compiled out in release now kill the user's app with no message (GUI subsystem: stderr invisible) and without joining session threads.
**Fix:** Mode-dependent handler: headless modes keep `_Exit(3)`; interactive mode shows `SDL_ShowSimpleMessageBox` (or logs under the pref path) before exiting — or at minimum document the behaviour.

## Info

### IN-01: Stale "Phase 3 research prototype" wording in `src/gui/ui.hpp:2`, `src/app/ui_logic.hpp:2`.
### IN-02: `checkCanvas` (`src/gui/app.cpp:245-297`) mixes logical canvas coordinates with a pixel-sized surface; exact only at pixel density 1 (true in CI). Scale by `SDL_GetWindowPixelDensity` or assert density == 1.
### IN-03: Unguarded `tabs().back()` in developer modes (`src/gui/app.cpp:371,380,389,416,421,478`); `runScreenshot` returns 1 on timeout without a message.
### IN-04: `defaultWindowSize` comment promises a lower bound the code does not implement (`src/app/ui_logic.hpp:34`, `ui_logic.cpp:62-67`).
### IN-05: `GuiLikeContext` constructor uses `REQUIRE` (leaks + cascades on failure) in `tests_gui/test_render_ctx.cpp:38-53`; unused `Workspace ws` in `tests/test_app.cpp:123`.

---

_Reviewed: 2026-10-11_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
