---
phase: 01-foundation-portable-physics-core
reviewed: 2026-10-10T00:00:00Z
depth: standard
files_reviewed: 24
files_reviewed_list:
  - .github/workflows/ci.yml
  - .gitignore
  - ElectricField/ElectricField.vcxproj
  - build.zig
  - build.zig.zon
  - src/core/ef/charge_system.hpp
  - src/core/ef/math.hpp
  - src/core/ef/method.hpp
  - src/core/ef/params.hpp
  - src/core/ef/presets.cpp
  - src/core/ef/presets.hpp
  - src/core/ef/rk.hpp
  - src/core/ef/seeds.cpp
  - src/core/ef/seeds.hpp
  - src/core/ef/tracer.hpp
  - src/core/ef/vec2.hpp
  - src/gui/main.cpp
  - tests/test_field.cpp
  - tests/test_main.cpp
  - tests/test_presets.cpp
  - tests/test_rk_order.cpp
  - tests/test_seeds.cpp
  - tests/test_vec2.cpp
  - tools/baseline/capture-mfc-baseline.ps1
findings:
  critical: 0
  warning: 6
  info: 5
  total: 11
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-10-10
**Depth:** standard
**Files Reviewed:** 24
**Status:** issues_found

## Summary

Numerical port is faithful to the MFC source (verified line-by-line against `RungeKutta.h/.inl`, `FieldLinesCalculator.*`, `FieldLine.h`, `Charge.h`, `TheElectricField.*`, `Options.*`, `Vector2D.*`, `ConfigDocs.cpp`: tableaus, error control, seed planning, thinning thresholds, hit radius, `GetTotalCharge`, preset values, default options and `.rc` display names all match; documented quirks preserved; the only deviations are the intentional empty-system guard and the `std::abs` fix in `expectedLineCount`). Findings are robustness/process issues: the CI "core gate first" isn't actually independent of GUI deps, the GUI loop spins the CPU, several unvalidated inputs (method enum, params) become reachable once Phase 2 loads user config, the baseline-capture tool can write a blank PNG, and two tests can pass silently.

## Structural Findings (fallow)

None provided.

## Warnings

### WR-01: `-Dgui=false` / "core gate first" CI step still fetches SDL3 and ImGui

**File:** `build.zig.zon:9-16`, `build.zig:41-43,72-74`, `.github/workflows/ci.yml:40-45`
**Issue:** `sdl3` and `imgui` are non-lazy dependencies, so Zig's package manager fetches them before `build()` runs regardless of `b.dependency()` calls. `zig build test -Dgui=false` therefore still downloads SDL3/ImGui; a fetch/hash failure in the GUI stack fails the "Core tests" steps — exactly the masking the CI comment says it prevents — and makes the `gui` option useless for offline/core-only builds.
**Fix:** Mark both `.lazy = true` in `build.zig.zon`; use `b.lazyDependency("sdl3", …) orelse return;` and `b.lazyDependency("imgui", …) orelse return;` inside `addGui`.

### WR-02: GUI main loop busy-spins (no vsync, no wait)

**File:** `src/gui/main.cpp:32,46-65`
**Issue:** SDL3 renderer defaults to vsync off and the loop uses `SDL_PollEvent` with no frame cap — 100% of a core for a hello window; Phase 3 would inherit it.
**Fix:** `SDL_SetRenderVSync(renderer, 1);` after creating the renderer; optionally `SDL_WaitEventTimeout(&e, 16)` when idle.

### WR-03: `info()` / `withSolver` have no validation of out-of-range `Method` values

**File:** `src/core/ef/method.hpp:49,68-70`
**Issue:** MFC cast an unvalidated registry int to the enum; once Phase 4 reads user config an out-of-range value is a real input. `info(m)` indexes `kMethods[...]` unchecked (UB); `withSolver` silently falls back to Euler, masking the corruption.
**Fix:** Add `std::optional<Method> methodFromName(std::string_view)`, bounds-checked `info()` (`.at()` or `nullptr`), and abort/assert on the unreachable path in `withSolver`.

### WR-04: Baseline capture can emit a blank/black PNG that passes completion detection

**File:** `tools/baseline/capture-mfc-baseline.ps1:50-55,91-96,157-163`
**Issue:** `PrintWindow`'s return value is unchecked; on failure the bitmap stays transparent black, `W.Dark` ignores alpha, so ~786k "dark" pixels exceed the 1500 threshold, the image is stable for 2 s, and a meaningless PNG is saved as an oracle. `$Exe` is not validated before `Start-Process`.
**Fix:** Throw/return `$null` from `Grab` when `PrintWindow` fails; count dark only when alpha == 255; `if (-not (Test-Path -LiteralPath $Exe)) { throw ... }`.

### WR-05: Preset cross-check test can silently pass; fragile parser state

**File:** `tests/test_presets.cpp:25-29,35-44`
**Issue:** If `ElectricField/ConfigDocs.cpp` can't be opened (wrong cwd), the test emits `MESSAGE` and returns → pass; the D-07 oracle vanishes unnoticed. `cur` isn't reset per `IMPLEMENT_DYNCREATE` (a doc relying on the default charge would inherit the previous value). `docs.back()` is called without an emptiness check (UB).
**Fix:** `FAIL`/`REQUIRE(in.good())` until Phase 4 deletes the MFC tree (gate any skip on an explicit env var); reset `cur = PresetCharge{1, 0, 0};` per document; `REQUIRE(!docs.empty())` before `docs.back()`.

### WR-06: `SolveStepPair` duplicates `SolveStep`'s stage loop — low-order tests exercise a copy

**File:** `src/core/ef/rk.hpp:70-82` vs `84-120`
**Issue:** The "embedded lower order" and "KNOWN DEFECT" tests run `SolveStepPair`, not production `SolveStep`; the loops can drift apart while tests stay green.
**Fix:** Have `SolveStep` compute `(high, low)` via `SolveStepPair` (or a shared private `computeStages`) and re-run parity checks (`-ffp-contract=off` keeps arithmetic order identical).

## Info

### IN-01: `expectedLineCount` disagrees with `planSeeds` when equipotentials are off

**File:** `src/core/ef/seeds.cpp:46-58` vs `26-28`
**Issue:** `planSeeds` emits no sink seed when `calculateEquipotentials` is false; `expectedLineCount` always counts one per sink (MFC parity), so a progress denominator overshoots. Test covers only the default. `int` product can overflow for large inputs.
**Fix:** Document as parity or account for the flag (+ test); compute in `std::int64_t` or clamp.

### IN-02: `ComputeParams` / seed planner accept degenerate values silently

**File:** `src/core/ef/params.hpp:16,20`, `src/core/ef/seeds.cpp:20`
**Issue:** `pixelsPerUnit == 0` → inf radii; `linesPerUnitCharge == 0` → infinite `angle_step`, no seeds. Unreachable today; reachable once config is loaded.
**Fix:** `ComputeParams::sanitized()` or validation in the config loader (minimum 1).

### IN-03: GUI error paths and `imgui.ini`

**File:** `src/gui/main.cpp:28-36,41-42`
**Issue:** `SDL_CreateWindow`/`SDL_CreateRenderer` failure paths return without cleanup/`SDL_Quit`; ImGui backend init results ignored; `--smoke` writes `imgui.ini` into cwd.
**Fix:** Check init results; in smoke mode set `ImGui::GetIO().IniFilename = nullptr;`.

### IN-04: CI, vcxproj and `.gitignore` housekeeping

**File:** `.github/workflows/ci.yml:6,30`, `ElectricField/ElectricField.vcxproj`, `.gitignore:7-9`
**Issue:** CI triggers only on `feature/cross-platform` pushes and PRs (nothing builds `master` after merge); `actions/checkout@v7` tag-pinned vs SHA-pinned `setup-zig`; v145 toolset requires VS 18 for the legacy MFC build (acceptable since Phase 4 deletes it); unanchored `Debug/`/`Release/` ignore rules match any such directory.
**Fix:** Add `master` to `push.branches`; SHA-pin checkout; note toolset in README; anchor ignore patterns.

### IN-05: Missing `<algorithm>` include in order tests

**File:** `tests/test_rk_order.cpp:132`
**Issue:** `std::min` used without `#include <algorithm>` (works via transitive libc++ includes).
**Fix:** Add `#include <algorithm>`.

---

_Reviewed: 2026-10-10_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
