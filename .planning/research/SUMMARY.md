# Project Research Summary

**Project:** ElectricField cross-platform port (MFC → portable GUI, command-line build)
**Domain:** C++ scientific-visualization desktop app (2D field lines/equipotentials, Runge-Kutta, worker threads)
**Researched:** 2026-10-10
**Confidence:** MEDIUM-HIGH (code-level & Windows spike HIGH; Linux/macOS runtime unexecuted)

## Executive Summary

ElectricField is a ~4.3k-LOC MFC/Direct2D MDI app whose physics is already nearly portable, but entangled with MFC via the `theApp.options` global, `Draw(...)` methods on core types, and a hidden equipotential de-dup living in `CElectricFieldDoc`. Expert approach: extract a GUI-free core (injected immutable params, RAII compute session with cooperative cancellation, pure finalizer/Bezier-path functions), add a scene-as-data layer with a 4-call `Painter`, keep the GUI shell thin, and use an SVG/CLI oracle for CI.

Recommended stack: **zig build 0.17.0 + Dear ImGui 1.92.9b (docking) + SDL3 via `castholm/SDL` + `sdlrenderer3` backend + doctest 2.5.3**, 3-OS GitHub Actions native matrix. Spiked on Windows: full GUI stack builds, `std::thread` works under Zig libc++, headless PNG render works, Linux cross-compile links. Printing becomes PNG/SVG export (allowed by PROJECT.md). Fallback: Qt 6.12 LTS Widgets + CMake — core stays build-system-neutral so the switch is cheap.

Main risks: silent numeric drift leaving MSVC; logic lost with Document class / `.rc` string tables; the threading rewrite; unverified Linux/macOS execution of the Zig+SDL stack; Zig 0.17 build API churn.

## Conflicts Resolved

1. **Stack vs early spike** — Adopt STACK recommendation; make a 3-OS CI hello-window + `zig build test` spike a Phase 1 gate. Irrecoverable failure → Qt 6 + CMake fallback.
2. **Progressive display** — Current app does NOT do it (all-at-once after all threads finish). Parity = wait indicator then full draw. Progressive drawing = cheap differentiator (drain-and-swap makes it nearly free); equipotentials only on completion (de-dup needs the full set).
3. **README** — FEATURES.md is wrong that none exists; `README.md` exists (says VS2015). Requirement = "update README".

## Key Findings

### Recommended Stack

Zig 0.17.0 single toolchain; Windows target `x86_64-windows-gnu` (not `-msvc`); all deps vendored via hash-pinned `build.zig.zon`, `.minimum_zig_version = "0.17.0"`.

**Core technologies:**
- **Zig 0.17.0** — build system + compiler + package manager
- **Dear ImGui v1.92.9b-docking** — compiled directly from source; viewports off
- **SDL3 3.4.18 (`castholm/SDL`, `release-3.4.x`)** — window/input/file dialogs/`SDL_GetPrefPath`/`SDL_SavePNG`/HiDPI. Do NOT use `allyourcodebase/SDL3` (fails on Windows without symlink privilege)
- **`imgui_impl_sdl3` + `imgui_impl_sdlrenderer3`** — per-OS GPU backend; `software` driver for headless CI renders
- **doctest 2.5.3** — via `zig build test`
- **CI:** `mlugg/setup-zig` pinned 0.17.0 on ubuntu / windows / macos-arm64 (macOS cannot be cross-built)
- **Fallback:** Qt 6.12 LTS + CMake/Ninja (LGPLv3, needs deploy tooling)

**Zig 0.17 API note:** C/C++ calls live on `Module`, not `Compile`: `b.createModule(.{ .link_libcpp = true, ... })`, `module.addCSourceFiles`/`addIncludePath`/`linkLibrary`, `.root_module = ...` on executables; `addWin32ResourceFile` on Module and deprecated; `subsystem`/`win32_manifest` stay on Compile; `sanitize_c` is an enum (off/trap/full); `@cImport` removed. Start from `zig init` of the installed version, not web examples. Windows `main` must be `int main(int, char**)` + `#include <SDL3/SDL_main.h>`. Don't depend on ZLS for build.zig.

### Expected Features

**Must have (parity):**
- 9 presets as a data table (exact copy of `ConfigDocs.cpp`), one tab each
- Field-line seeding proportional to |q|; equipotentials with de-dup moved into core in world units; all 11 integrators (same enum order)
- Configurable threads, non-blocking UI, safe cancel on tab close / quit
- AA Bezier rendering at fixed 100 px/unit, y-up, HiDPI-correct; colors blue(+)/red(−)/black lines/grey(192) equipotentials
- Options dialog with original ranges/defaults (lines/unit 4–128 def 16; threads 1–128 def 4; thickness 1–64 def 1; interval 0.01–0.50 def 0.10; Euler default; equipotentials on) — fix bug where `potentialFieldLineThickness` is never validated
- Draw changes repaint all tabs; computation changes apply to new tabs
- Portable config file (`#RRGGBB` colors, methods by name)
- PNG/SVG export replaces print; About box with both links; shortcuts

**Should have (differentiators):** pan/zoom (pure view transform), Recompute, Reset-to-defaults, headless CLI render, progressive drawing, status bar with stats.

**Defer:** direction arrows, charge labels, dark canvas, clipboard copy, PDF export.

**Anti-features:** charge editing, save/load, native print, MFC themes/docking, registry migration, new physics.

### Architecture Approach

Three layers, strictly downward dependencies: `efcore` (pure C++17, stdlib only) → `efapp` (toolkit-free settings/scene/painter/Document) → `ef-gui` / `ef-cli`.

**Key patterns:**
1. `ComputeParams` passed by value, including fixed constants `pixelsPerUnit=100`, `chargeRadiusWorld=0.25` (not view settings)
2. RAII `ComputeSession`: condition-variable pool with in-flight termination, cooperative cancel every ~4096 steps, drain-and-swap `poll()`, deterministic job-sequence ordering; `std::thread` + `std::atomic` (not `jthread`)
3. `FieldScene` + 4-call `Painter` + one shared `renderScene()` serving screen, SVG, PNG and tests
4. `PresetRegistry` replaces the 9 document classes

### Critical Pitfalls

1. **Unqualified `abs(double)` truncates on GCC/Clang** — equipotentials disappear, adaptive stepping breaks. Use `std::abs` + `<cmath>`/`<cfloat>`, rename `abs(Vector2D)` helper, strict warnings incl. `-Wabsolute-value`.
2. **`sign(0)==+1` macro & `M_PI`** — replace with `inline constexpr int sign(double x){ return x>=0?1:-1; }`, never `copysign`/signum (breaks net-zero presets); use `kPi`. Test `sign(0)==1`.
3. **`FieldLineJob job(job)` self-copy UB** — rename (`equipJob`), `-Wshadow -Wuninitialized`, regression test.
4. **Core entangled with MFC** — inject params, move finalize into core, extract preset names from `.rc`, fold `.cpp/.inl` into headers, explicit source lists, delete `PrecisionTimer`.
5. **Shutdown/threading** — detached threads, `delete this`, coarse cancel, `theApp.options` race → RAII session, in-loop cancel checks, params snapshot; TSan + cancel-latency (<200 ms) tests.
6. **Numeric drift across compilers** — core built with `-ffp-contract=off`, never fast-math; tolerance/invariant tests, never bit-exact golden geometry; arm64 CI.
7. **Other traps:** `AdjustForBezier` divide-by-zero guard; empty-charge-set guard; COLORREF BGR/RGB swap; keep zoom/export out of core numerics; git hygiene in its own commit (untrack `ElectricField/x64/`, anchor `.gitignore` correctly, add `.zig-cache/`, `zig-out/`, `zig-pkg/`); macOS native runner + ad-hoc signing only.

## Implications for Roadmap

### Phase 1: Hygiene + Zig skeleton + 3-OS CI spike (gate)
**Delivers:** artifacts untracked / `.gitignore` fixed (separate commit), pinned `build.zig`/`.zon`, hello-window + `zig build test` green on windows/ubuntu/macos-arm64.
**Avoids:** pitfalls 10, 11, 12, 15.

### Phase 2: Portable numerics (Vec2, RungeKutta)
**Delivers:** header-only, no stdafx/macros, `std::abs`, `sign` fn, `kPi`, strict warnings, `-ffp-contract=off`, convergence-order tests for all 11 methods.
**Avoids:** pitfalls 1, 2, 7, 13.

### Phase 3: Physics model + presets
**Delivers:** `ChargeSystem`, `ComputeParams`, plain-data lines, `LineRecorder`, `SeedPlanner`, `PresetRegistry`; analytic E/V and seed-count tests.
**Avoids:** pitfalls 4, 14.

### Phase 4: Compute engine + Finalizer
**Delivers:** `LineTracer`, `ComputeSession`, `Finalizer` (de-dup ported), `job(job)` UB fixed. Tests: 1-vs-N thread determinism, cancel latency, termination invariants, de-dup on Quadrupole/Parallel dipoles, TSan/ASan on Linux.
**Avoids:** pitfalls 3, 4, 5, C.

### Phase 5: Settings + scene + painter + SVG + Document + ef-cli
**Delivers:** `SettingsStore` (default-parity test), guarded `PathBuilder`, `renderScene`, `SvgPainter`, `Document`, CLI rendering all 9 presets in CI; one-time comparison vs MFC build.
**Avoids:** pitfalls 8, 9, 17.

### Phase 6: ImGui + SDL3 shell
**Delivers:** tabs, File▸New from registry, `ImDrawList` painter, HiDPI, per-frame poll + idle gating, status text, close-tab cancel, GUI-subsystem exe.

### Phase 7: Options + persistence wiring + export + About
**Delivers:** Options dialog, PNG/SVG export, About, shortcuts, removal of MFC project/print code, README update.

### Phase 8: Differentiators (optional)
**Delivers:** pan/zoom, Recompute, progressive drawing, status stats, golden-image test on one OS, release notes.

### Phase Ordering Rationale

- Phases 1–5 are GUI-independent: the core stays correct whichever toolkit wins.
- The CLI/SVG oracle gives 3-OS parity checks with no display.
- Risky threading work is sanitizer-tested before the UI depends on it.
- (Granularity is "coarse": the roadmapper should merge these into 3–5 phases.)

### Research Flags

Needs research: Phase 1 (macOS/Linux spike, setup-zig 0.17), Phase 4 (threading/TSan/`job(job)` repro), Phase 6 (Bezier AA/tessellation, HiDPI, Linux file dialogs), Phase 7 (offscreen PNG export).
Standard patterns: Phases 2, 3, 5, 8.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM-HIGH | Windows spiked; Linux/macOS execution unverified |
| Features | HIGH (parity) / MEDIUM (differentiators) | Parity audited from source |
| Architecture | HIGH | Read from source |
| Pitfalls | HIGH (code) / MEDIUM (toolchain/ABI) | Cross-OS behavior reasoned |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- macOS/Linux Zig+SDL execution → Phase 1 spike; Qt/CMake fallback
- `job(job)` MSVC behavior → reproduce in Phase 4
- Baseline of MFC output for the 9 presets → capture before deleting MFC
- Debug `sanitize_c` default and `-ffp-contract` behavior under Zig 0.17
- ImGui thin-line AA quality for export → swap in plutovg behind `Painter` if inadequate
- Preset names/icons only in `.rc` → extract in Phase 3
- Qt LGPL vs repo LICENSE (fallback only)
- PROJECT.md corrections: progressive display is not existing behavior (corrected); README exists

## Sources

### Primary (HIGH confidence)
- Repository source audit; installed Zig 0.17.0 `lib/std/Build`; local Windows spike; castholm/SDL; Dear ImGui / SDL3 / Zig release docs

### Secondary (MEDIUM confidence)
- Ziggit threads on libc++ vs libstdc++; Zig `rc` package; LLVM discourse on `-ffp-contract`; Qt / wxWidgets release pages

### Tertiary (LOW confidence)
- PhET guide; old Qt deploy forum threads; macOS SDK cross-compile notes

---
*Research completed: 2026-10-10*
*Ready for roadmap: yes*
