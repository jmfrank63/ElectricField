# ElectricField — Cross-Platform Port

## What This Is

ElectricField is a desktop simulator that computes and draws electric field lines and equipotential lines for configurations of point charges, using a family of Runge-Kutta integrators (fixed-step and adaptive). It is currently a Windows-only MFC MDI application (Direct2D rendering). This project removes MFC entirely and replaces the GUI with a cross-platform solution, so the app builds from the command line and runs on Windows, Linux, and macOS.

## Core Value

The physics and visual output must stay correct and recognisable: the same charge configurations produce the same field lines and equipotentials, now buildable and runnable on all three desktop OSes without Visual Studio.

## Business Context

Personal / educational computational-physics project (companion to the blog post https://compphys.go.ro/electric-field-lines/). Motivation: today it only builds with Visual Studio + MFC (MFC is only installed for one MSVC toolset on the dev machine; `zig build` cannot compile MFC because Zig's libc++ conflicts with the MSVC runtime). Removing MFC unlocks portable, command-line builds (zig build or CMake).

## Requirements

### Validated

Existing capabilities in the MFC app (must be preserved by the port):

- ✓ 9 preset charge configurations, each openable as a separate document/window: Dipole, Two equal charges, Two unequal charges same sign, Two unequal charges different sign, Quadrupole, Four charges same sign, Two dipoles same direction, Two dipoles different direction, Parallel dipoles (`ConfigDocs.cpp`) — existing
- ✓ Electric field line computation seeded around each charge (count proportional to charge, `numLinesOnUnitCharge`) — existing
- ✓ Optional equipotential line computation at a configurable potential interval — existing
- ✓ 11 integration methods: Euler, Midpoint, Ralston, Heun, RK4, RK 3/8, adaptive Heun-Euler, Bogacki-Shampine, Cash-Karp, Fehlberg, Dormand-Prince (`RungeKutta.h/.inl`) — existing
- ✓ Multi-threaded background computation with a job queue, configurable thread count, wait cursor while computing and results drawn once all threads finish (no progressive display), documents closable mid-computation via detached threads handed to the app object (`FieldLinesCalculator`, already std::thread/std::mutex/std::atomic) — existing
- ✓ Anti-aliased rendering of charges (colored by sign) and field/equipotential lines with Bezier smoothing, y-up coordinate system centered in the view — existing
- ✓ Options dialog with Computation page (lines per unit charge, equipotentials on/off, potential interval, threads, method) and Draw page (positive/negative charge colors, field/equipotential line colors and thicknesses) — existing
- ✓ Options persisted between runs (currently Windows registry) — existing
- ✓ Printing / print preview (GDI path) — existing
- ✓ Physics core separated into a GUI-independent, std-only library (`src/core/ef/`: vectors, charges, 11 RK methods, seeds, tracer, presets, threaded cancellable `ComputeSession`, equipotential de-dup `Finalizer`, guarded Bezier, scene/SVG) with no app state — Phases 1–2
- ✓ Command-line build on Windows, Linux, macOS without Visual Studio (`zig build`, Zig 0.17, hash-pinned deps) — Phase 1 (CI-verified)
- ✓ Automated tests for the physics core (60 doctest cases: analytic field/potential, convergence order, seeds, presets vs MFC source, sessions, cancel latency, de-dup, regression oracles; ThreadSanitizer-clean) — Phases 1–2
- ✓ CI that builds, runs core tests, headless parity (9 presets, 3 OSes) and a Linux TSan job — Phases 1–2
- ✓ Tracked build artifacts removed from git and `.gitignore` fixed — Phase 1
- ✓ Headless rendering of any preset to SVG/PNG (`ef-cli`), visually matching the MFC baselines (human-approved side-by-side, 2026-10-10) — Phase 2

### Active

- [ ] Remove every MFC / Win32 / Direct2D / GDI dependency from the codebase (MFC tree still present until Phase 4)
- [ ] Replace the GUI with a cross-platform framework (Dear ImGui + SDL3) preserving all Validated capabilities — Phase 3
- [ ] Options persisted in a portable way (config file in the platform's user config dir) — Phase 4
- [ ] Replace printing with PNG/SVG export from the GUI (painter/exporters exist since Phase 2) — Phase 4
- [ ] Update README with new build instructions — Phase 4

### Out of Scope

- Interactive charge placement / editing charges with the mouse — not in the current app; possible future milestone
- Saving/loading user-defined configurations to files — `Serialize` is an empty stub today; not part of the port
- New physics (3D fields, dielectrics, continuous charge distributions) — port only
- Keeping the MFC build alive in parallel — the branch replaces it; master keeps the MFC version
- Mobile / web targets — desktop only for this milestone

## Context

- Codebase: ~4.3k lines of C++ in `ElectricField/`. MFC App Wizard MDI app (`CElectricFieldApp`, `CMainFrame`, `CChildFrame`, `CElectricFieldDoc` + 9 derived docs in namespace `ChargeConfiguration`, `CElectricFieldView`).
- MFC coupling in the "core": `Charge`, `FieldLine`, `TheElectricField` have `Draw(CHwndRenderTarget*, CRect&)` and `Draw(CDC*, float)` methods; `Charge::HitCharge` and `FieldLine::AddPoint` read `theApp.options`; `Options` uses `COLORREF` and the registry. Threading is already standard C++.
- The view polls the document on a timer for newly computed lines; documents closed mid-computation hand their calculator to the app until threads finish.
- Current toolchain: VS 18 Insiders, toolset v145, MFC only present for MSVC 14.51. Zig 0.17.0 installed (scoop). Command-line MSBuild of the MFC app works (verified 2026-10-10).
- Prior finding: `zig c++` compiles 17/18 MFC sources but fails on `std::thread` and at link (libc++ vs MSVC runtime). Once MFC is gone, Zig becomes viable.
- Work happens on branch `feature/cross-platform`.

## Constraints

- **Language**: Keep C++ for the physics core (C++17 or newer) — reuse existing, proven numerics.
- **Platforms**: Windows, Linux, macOS desktop; command-line build only, no IDE required.
- **Toolchain**: Must not require MSVC/MFC. Zig 0.17 is available and preferred if the chosen GUI stack builds cleanly with it; otherwise CMake.
- **Licensing**: Repo is open source (see LICENSE); GUI dependency license must be compatible.
- **Behaviour parity**: Same default options and same configurations should give visually equivalent output.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Drop MFC entirely (no dual build) | MFC blocks portability and Zig builds; master retains MFC version | — Pending (MFC kept as reference until Phase 4) |
| GUI framework chosen by research | User delegated choice (candidates: Qt 6, wxWidgets, Dear ImGui + SDL3/GLFW, others) | ✓ Dear ImGui 1.92.9b (docking) + SDL3 (castholm/SDL) + sdlrenderer3; Qt 6 + CMake documented fallback (not needed: 3-OS CI green) |
| Build system chosen by research | User delegated choice between zig build 0.17 and CMake, depending on GUI stack fit | ✓ zig build 0.17.0; CI green on windows-x64, linux-x64, macos-arm64 |
| Extract GUI-independent core library first | Enables testing and decouples port risk from physics | ✓ Done (Phases 1–2); GUI will consume `ComputeSession`, `FieldScene`, `renderScene()` and `ImGuiPainter` |
| Keep MFC numerical quirks for parity (Dormand-Prince "low" weights, AdjustForBezier t3, pixel/world thinning, de-dup heuristic) | Visual parity with the MFC baselines is the core value | ✓ Kept and pinned by characterisation tests; DP weight fix deferred to v2 |
| One shared drawing path (Painter → SVG / ImDrawList) | CLI, CI and GUI render identically | ✓ Phase 2; PNG via surface-backed SDL software renderer (display-free) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-10-10 after Phase 2*
