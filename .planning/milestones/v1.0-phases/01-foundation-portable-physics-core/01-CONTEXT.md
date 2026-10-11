# Phase 1: Foundation & Portable Physics Core - Context

**Gathered:** 2026-10-10
**Status:** Ready for planning
**Mode:** auto (all gray areas auto-resolved with recommended options; see 01-DISCUSSION-LOG.md)

<domain>
## Phase Boundary

Delivers: (1) git hygiene (untrack build artifacts, correct `.gitignore`), (2) a `zig build` (Zig 0.17.0) skeleton with hash-pinned deps (Dear ImGui, SDL3 via castholm/SDL, doctest) plus a minimal SDL3+ImGui hello-window target, (3) a GitHub Actions 3-OS CI gate (windows / ubuntu / macos-arm64), (4) a GUI-free, standard-library-only physics core library (vectors, charges, field/potential, all 11 Runge-Kutta methods, seed planning, 9 presets as data) with doctest tests, and (5) reference images of all 9 presets rendered by the existing MFC build.

Not in this phase: threaded compute session/cancellation, equipotential de-dup/finalizer, SVG/CLI rendering (Phase 2); real GUI (Phase 3); options dialog/persistence/export/MFC deletion (Phase 4).

</domain>

<decisions>
## Implementation Decisions

### Source layout & coexistence with MFC
- **D-01:** New code lives at the repo root, separate from the MFC tree: `build.zig`, `build.zig.zon`, `src/core/` (physics library, namespace `ef`), `src/gui/` (hello-window now, real shell in Phase 3), `tests/` (doctest). The `ElectricField/` MFC directory, `.sln` and `.vcxproj` are left **untouched** and must still build with MSBuild until Phase 4 (needed for the QA-05 baseline and side-by-side comparison).
- **D-02:** The core is a **port (copy + refactor)** of the MFC-era classes, not an in-place edit: MFC source files are not modified in this phase.
- **D-03:** Core is header-mostly C++17 (C++20 allowed if Zig's clang handles it cleanly; no C++ modules). No `stdafx.h`, no MFC/Win32 types, no globals; parameters passed explicitly (`ComputeParams` value type incl. fixed constants `pixelsPerUnit = 100`, `chargeRadiusWorld = 0.25`).

### Numerics policy
- **D-04:** Research numerics traps are hard rules: `std::abs` only (no unqualified `abs` on doubles; rename the `abs(Vector2D)` helper), `kPi` constant instead of `M_PI`, `inline constexpr int sign(double x){ return x >= 0 ? 1 : -1; }` preserving `sign(0) == +1`, core compiled with `-ffp-contract=off`, never fast-math, strict warnings (`-Wall -Wextra -Wshadow -Wabsolute-value` …) as errors for core code.
- **D-05:** Tests use tolerances/invariants, never bit-exact golden geometry. Integrator convergence-order tests use an ODE with a closed-form solution (e.g. rotation field / harmonic oscillator): observed order within ±0.3 of nominal; adaptive methods tested for error ≤ tolerance and actual step adaptation.
- **D-06:** All 11 integrators exposed in the **original enum order** (Euler, Midpoint, Ralston, Heun, RK4, RK3/8, adaptive Heun-Euler, Bogacki-Shampine, Cash-Karp, Fehlberg, Dormand-Prince) with stable string names (used later for config persistence).

### Presets
- **D-07:** The 9 presets become a data table (`PresetRegistry`) with display names from the `.rc` string table and charge positions/magnitudes copied exactly from `ElectricField/ConfigDocs.cpp`. A test asserts each preset's charges match the MFC values.

### Build & dependencies
- **D-08:** Zig 0.17.0 exactly (`.minimum_zig_version = "0.17.0"`); deps hash-pinned in `build.zig.zon`: Dear ImGui `v1.92.9b-docking`, SDL3 via `castholm/SDL` `release-3.4.x` (pin a commit), doctest `2.5.3`. Do not use `allyourcodebase/SDL3`. Use the Zig 0.17 `Module` API (`b.createModule`, `module.addCSourceFiles`, `.root_module`); start from `zig init` output of the installed version, not web examples. — **Reversibility:** costly — switching to the Qt/CMake fallback rewrites the build and GUI layer (core unaffected).
- **D-09:** Build steps: `zig build` (core lib + hello-window exe), `zig build test` (doctest suite), `zig build run` (hello window). Windows target `x86_64-windows-gnu`; Windows GUI exe uses `#include <SDL3/SDL_main.h>` + `int main(int, char**)`.
- **D-10:** The hello-window supports a `--smoke` flag: create window, render at least one complete ImGui frame (two are allowed so the backend's font-atlas texture upload on the first frame is exercised before the second frame draws), exit 0; runnable under SDL's `dummy` video driver so CI can start it on all 3 OSes without a display. *(Amended 2026-10-10 during plan-check iteration 2 by the auto-mode orchestrator — original wording "render one ImGui frame" was an auto-selected default; the intent, a headless start-and-render smoke test, is unchanged.)*

### CI gate
- **D-11:** GitHub Actions workflow `.github/workflows/ci.yml`, matrix `windows-latest`, `ubuntu-latest`, `macos-latest` (arm64); Zig via `mlugg/setup-zig` pinned to 0.17.0; runs `zig build`, `zig build test`, and the `--smoke` run. Triggers: push to `feature/cross-platform` and pull requests. Cache the Zig global cache.
- **D-12:** CI only goes green once the branch is pushed to `origin` (github.com/jmfrank63/ElectricField). Pushing is outward-facing → the executor must **ask the user before the first push**. If the gate fails irrecoverably on Linux or macOS, stop and record the Qt 6 + CMake fallback decision rather than hacking around it. — **Reversibility:** one-way — a pushed branch and CI runs are published on GitHub.

### Git hygiene
- **D-13:** In its own commit: `git rm -r --cached ElectricField/x64 ElectricField/ElectricField.vcxproj.user` (and anything under `.vs/` if tracked); rewrite `.gitignore` with unanchored patterns (`x64/`, `Debug/`, `Release/`, `.vs/`, `*.user`, `*.obj`, `*.pdb`, `*.ilk`, `*.pch`, `*.tlog`, `*.idb`) plus Zig outputs (`.zig-cache/`, `zig-out/`, `zig-pkg/`). The pending `v143 → v145` toolset change in `ElectricField/ElectricField.vcxproj` is committed separately so the MFC baseline build is reproducible on VS 18.

### MFC baseline (QA-05)
- **D-14:** Build the MFC app with MSBuild (Release|x64, toolset v145; MSVC 14.51 has MFC) into an **untracked** output dir; open each of the 9 presets with default options (reset the app's registry options to defaults first) at a fixed client size (target 1024×768), wait for computation to finish, capture the view to PNG. Store as `docs/baseline/mfc/<preset-slug>.png` with a `docs/baseline/mfc/README.md` noting MSVC version, options, window size and date. Automation preferred (PowerShell + Win32 `PrintWindow`, or computer-use); if impractical, make it a human-assisted checkpoint.

### Claude's Discretion
- Exact file split inside `src/core/`, class names (guided by research ARCHITECTURE.md: `Vec2`, `ChargeSystem`, `ComputeParams`, `SeedPlanner`, `PresetRegistry`), test file organisation, CI caching details, whether `Vector2D` keeps its template parameter.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project scope
- `.planning/PROJECT.md` — context, constraints, key decisions
- `.planning/REQUIREMENTS.md` — BUILD-01..05, CORE-01..03, QA-01, QA-05 for this phase
- `.planning/ROADMAP.md` §Phase 1 — goal and success criteria

### Research
- `.planning/research/SUMMARY.md` — consolidated decisions and pitfalls
- `.planning/research/STACK.md` — verified Zig 0.17 + ImGui + SDL3 setup, build.zig skeleton, Module-vs-Compile API notes, spike results
- `.planning/research/ARCHITECTURE.md` — target core components and boundaries
- `.planning/research/PITFALLS.md` — numerics traps, Zig API churn, git hygiene

### Existing code (source of truth for numerics and presets)
- `ElectricField/Vector2D.h`, `ElectricField/Vector2D.cpp` (textually included — not a compile unit)
- `ElectricField/RungeKutta.h`, `ElectricField/RungeKutta.inl` (textually included)
- `ElectricField/Charge.h`, `ElectricField/Charge.cpp`, `ElectricField/TheElectricField.h`, `ElectricField/TheElectricField.cpp`, `ElectricField/FieldLine.h`, `ElectricField/FieldLine.cpp`
- `ElectricField/FieldLinesCalculator.h`, `ElectricField/FieldLinesCalculator.cpp` — seeding logic, `sign()` macro, job structure
- `ElectricField/ConfigDocs.cpp` — the 9 presets' charges
- `ElectricField/ElectricField.rc` — preset display names
- `ElectricField/Options.h`, `ElectricField/Options.cpp` — defaults and enum order

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Vector2D<T>`, `RungeKutta`/`AdaptiveRungeKutta` templates: numerically proven; portable after removing `stdafx.h` includes and fixing `abs`/`M_PI`.
- `Charge::E/Potential`, `TheElectricField::E/Potential/ENormalized/GetTotalCharge`: pure math, only coupled via `theApp.options` in `HitCharge`.
- `ConfigDocs.cpp`: preset charge tables.

### Established Patterns
- Threading already `std::thread`/`std::mutex`/`std::atomic` (relevant in Phase 2).
- Coordinates in world units; pixels = world × `distanceUnitLength` (100).

### Integration Points
- MFC `Draw(CHwndRenderTarget*)` / `Draw(CDC*)` methods are dropped from the core (rendering moves to a painter in Phase 2).
- `theApp.options` reads in `Charge::HitCharge` and `FieldLine::AddPoint` → replaced by explicit `ComputeParams`.

</code_context>
