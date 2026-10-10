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
- ✓ Multi-threaded background computation with a job queue, configurable thread count, progressive display while computing, safe cancellation when a document is closed mid-computation (`FieldLinesCalculator`, already std::thread/std::mutex/std::atomic) — existing
- ✓ Anti-aliased rendering of charges (colored by sign) and field/equipotential lines with Bezier smoothing, y-up coordinate system centered in the view — existing
- ✓ Options dialog with Computation page (lines per unit charge, equipotentials on/off, potential interval, threads, method) and Draw page (positive/negative charge colors, field/equipotential line colors and thicknesses) — existing
- ✓ Options persisted between runs (currently Windows registry) — existing
- ✓ Printing / print preview (GDI path) — existing

### Active

- [ ] Remove every MFC / Win32 / Direct2D / GDI dependency from the codebase
- [ ] Separate the physics core (Vector2D, Charge, FieldLine/PotentialLine, TheElectricField, RungeKutta, FieldLinesCalculator, ComputationThread, Options data) into a GUI-independent library with no dependency on `theApp` or any UI type
- [ ] Replace the GUI with a cross-platform framework (choice made by research) preserving all Validated capabilities
- [ ] Options persisted in a portable way (config file in the platform's user config dir)
- [ ] Replace printing with a portable equivalent (e.g. export the view to PNG/SVG/PDF) if native printing is impractical in the chosen framework
- [ ] Command-line build on Windows, Linux, macOS without the Visual Studio IDE (build system chosen by research: zig build 0.17 or CMake)
- [ ] Automated tests for the physics core (field/potential values, integrator accuracy, line generation sanity)
- [ ] CI that builds (and runs core tests) on all three OSes
- [ ] Remove tracked build artifacts (`ElectricField/x64/`, `.vs/`) from git and fix `.gitignore`
- [ ] Update README with new build instructions

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
| Drop MFC entirely (no dual build) | MFC blocks portability and Zig builds; master retains MFC version | — Pending |
| GUI framework chosen by research | User delegated choice (candidates: Qt 6, wxWidgets, Dear ImGui + SDL3/GLFW, others) | — Pending |
| Build system chosen by research | User delegated choice between zig build 0.17 and CMake, depending on GUI stack fit | — Pending |
| Extract GUI-independent core library first | Enables testing and decouples port risk from physics | — Pending |

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
*Last updated: 2026-10-10 after initialization*
