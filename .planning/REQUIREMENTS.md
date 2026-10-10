# Requirements: ElectricField — Cross-Platform Port

**Defined:** 2026-10-10
**Core Value:** The physics and visual output must stay correct and recognisable: the same charge configurations produce the same field lines and equipotentials, now buildable and runnable on Windows, Linux and macOS without Visual Studio.

## v1 Requirements

Requirements for the cross-platform release. Each maps to roadmap phases.

### Build & Repository

- [x] **BUILD-01**: Developer can build the app with `zig build` (Zig 0.17.0) on Windows, Linux and macOS from the command line, without Visual Studio or MFC installed
- [x] **BUILD-02**: Developer can run the core test suite with `zig build test` on all three OSes
- [x] **BUILD-03**: All third-party dependencies (Dear ImGui, SDL3, doctest) are fetched via hash-pinned `build.zig.zon`; no manual dependency installation beyond Zig (and platform system libs on Linux)
- [x] **BUILD-04**: CI (GitHub Actions) builds and runs tests on windows, ubuntu and macos-arm64 for every push to the branch
- [x] **BUILD-05**: Build artifacts (`ElectricField/x64/`, `.vs/`, `*.vcxproj.user`) are untracked, and `.gitignore` covers MSVC and Zig outputs (`.zig-cache/`, `zig-out/`)
- [ ] **BUILD-06**: All MFC / Win32 / Direct2D / GDI code, the `.vcxproj`/`.sln`, and `.rc` resources are removed from the source tree
- [ ] **BUILD-07**: README documents the new build, run and test instructions for all three OSes

### Physics Core

- [x] **CORE-01**: Physics core (vectors, charges, field/potential, Runge-Kutta, line tracing, presets) is a GUI-free C++ library depending only on the standard library and has no global/app state
- [x] **CORE-02**: All 11 integration methods (Euler, Midpoint, Ralston, Heun, RK4, RK 3/8, adaptive Heun-Euler, Bogacki-Shampine, Cash-Karp, Fehlberg, Dormand-Prince) are available in the original order and produce field lines
- [x] **CORE-03**: Field lines are seeded around each charge proportionally to |q| × lines-per-unit-charge, preserving the original `sign(0)==+1` source-selection behaviour for net-zero configurations
- [x] **CORE-04**: Equipotential lines are computed at the configured potential interval, including the de-duplication pass (moved from the MFC document into the core, in world units)
- [x] **CORE-05**: Computation runs on a configurable number of worker threads without blocking the UI
- [x] **CORE-06**: A running computation can be cancelled (tab close, app quit) within ~200 ms without crashes, leaks or detached threads
- [x] **CORE-07**: Known portability defects are fixed: unqualified `abs(double)`, `M_PI` redefinition, `FieldLineJob job(job)` self-copy, unguarded `AdjustForBezier` divide-by-zero; core compiles warning-clean with Clang (via Zig) using `-ffp-contract=off`, no fast-math

### Configurations & Display

- [ ] **VIEW-01**: User can open any of the 9 preset charge configurations (Dipole, Two equal charges, Two unequal charges same sign, Two unequal charges different sign, Quadrupole, Four charges same sign, Two dipoles same direction, Two dipoles different direction, Parallel dipoles) from a menu, each in its own tab, with charge positions/magnitudes identical to the MFC version
- [ ] **VIEW-02**: User can have several configurations open at once and close any tab, including while it is still computing
- [ ] **VIEW-03**: User sees charges drawn as filled circles colored by sign and field/equipotential lines drawn anti-aliased with Bezier smoothing, y-up, centered, at the original scale (100 px per unit), correct on HiDPI displays
- [ ] **VIEW-04**: User sees a busy indicator while a configuration is computing, and the completed field lines and equipotentials once computation finishes (parity behaviour)
- [ ] **VIEW-05**: App runs as a GUI application (no console window on Windows) with an About box including the project/blog links

### Options

- [ ] **OPT-01**: User can open an Options dialog with a Computation section: lines per unit charge (4–128, default 16), calculate equipotentials (default on), potential interval (0.01–0.50, default 0.10), threads (1–128, default 4), integration method (default Euler)
- [ ] **OPT-02**: User can set Draw options: positive/negative charge colors, field/equipotential line colors and thicknesses (1–64), with the original defaults (blue, red, black, grey 192); all thickness fields are validated
- [ ] **OPT-03**: Changing Draw options repaints all open tabs immediately; Computation options apply to newly opened configurations
- [ ] **OPT-04**: Options persist between runs in a portable config file in the platform user config directory (colors as `#RRGGBB`, method by name); missing or invalid file falls back to defaults

### Export

- [ ] **EXP-01**: User can export the current configuration view to PNG (replaces printing)
- [ ] **EXP-02**: User can export the current configuration view to SVG

### Quality

- [x] **QA-01**: Unit tests cover vector math, charge field/potential against analytic values, and convergence order of the integrators
- [x] **QA-02**: Tests cover line generation invariants per preset (seed counts, termination, de-dup) using tolerances rather than bit-exact geometry, and `sign(0)==1`
- [x] **QA-03**: Tests cover compute-session behaviour: 1-vs-N thread result equivalence, cancellation latency, no data races (TSan run on Linux CI)
- [ ] **QA-04**: A headless CLI renders any preset to SVG/PNG without a display, used in CI on all three OSes as a parity check
- [x] **QA-05**: A one-time visual comparison of all 9 presets against the MFC build (default options) is recorded before MFC code is deleted

## v2 Requirements

Deferred. Tracked but not in the current roadmap.

### Viewing

- **VIEW2-01**: User can pan and zoom the view (pure view transform; core numerics unchanged)
- **VIEW2-02**: User sees field lines appear progressively while computing
- **VIEW2-03**: Status bar with line counts / computation time
- **VIEW2-04**: Direction arrows on field lines; charge labels; dark canvas theme

### Workflow

- **WF-01**: User can recompute an open configuration after changing computation options
- **WF-02**: User can reset options to defaults
- **WF-03**: Copy view to clipboard; PDF export

## Out of Scope

| Feature | Reason |
|---------|--------|
| Interactive charge placement / editing | Not in the current app; future milestone |
| Saving/loading configuration files | `Serialize` is an empty stub today; not part of the port |
| Native printing / print preview | Replaced by PNG/SVG export; impractical in ImGui/SDL |
| MFC "application look" themes, docking, toolbar customization, window manager | MFC boilerplate with no user value |
| Migration of old Windows registry settings | Low value; defaults are restored instead |
| New physics (3D, dielectrics, continuous distributions) | Port only |
| Keeping the MFC build alive on this branch | master retains the MFC version |
| Intel macOS (x86_64-macos) | Zig 0.17 marks it obsolescent; no CI runners |
| Mobile / web targets | Desktop only |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Complete |
| BUILD-02 | Phase 1 | Complete |
| BUILD-03 | Phase 1 | Complete |
| BUILD-04 | Phase 1 | Complete |
| BUILD-05 | Phase 1 | Complete |
| BUILD-06 | Phase 4 | Pending |
| BUILD-07 | Phase 4 | Pending |
| CORE-01 | Phase 1 | Complete |
| CORE-02 | Phase 1 | Complete |
| CORE-03 | Phase 1 | Complete |
| CORE-04 | Phase 2 | Complete |
| CORE-05 | Phase 2 | Complete |
| CORE-06 | Phase 2 | Complete |
| CORE-07 | Phase 2 | Complete |
| VIEW-01 | Phase 3 | Pending |
| VIEW-02 | Phase 3 | Pending |
| VIEW-03 | Phase 3 | Pending |
| VIEW-04 | Phase 3 | Pending |
| VIEW-05 | Phase 3 | Pending |
| OPT-01 | Phase 4 | Pending |
| OPT-02 | Phase 4 | Pending |
| OPT-03 | Phase 4 | Pending |
| OPT-04 | Phase 4 | Pending |
| EXP-01 | Phase 4 | Pending |
| EXP-02 | Phase 4 | Pending |
| QA-01 | Phase 1 | Complete |
| QA-02 | Phase 2 | Complete |
| QA-03 | Phase 2 | Complete |
| QA-04 | Phase 2 | Pending |
| QA-05 | Phase 1 | Complete |

**Coverage:**

- v1 requirements: 30 total
- Mapped to phases: 30 (Phase 1: 10, Phase 2: 7, Phase 3: 5, Phase 4: 8)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-10-10*
*Last updated: 2026-10-10 after roadmap creation (traceability filled)*
