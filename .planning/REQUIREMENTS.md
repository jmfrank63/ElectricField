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
- [x] **BUILD-06**: All MFC / Win32 / Direct2D / GDI code, the `.vcxproj`/`.sln`, and `.rc` resources are removed from the source tree
- [x] **BUILD-07**: README documents the new build, run and test instructions for all three OSes

### Physics Core

- [x] **CORE-01**: Physics core (vectors, charges, field/potential, Runge-Kutta, line tracing, presets) is a GUI-free C++ library depending only on the standard library and has no global/app state
- [x] **CORE-02**: All 11 integration methods (Euler, Midpoint, Ralston, Heun, RK4, RK 3/8, adaptive Heun-Euler, Bogacki-Shampine, Cash-Karp, Fehlberg, Dormand-Prince) are available in the original order and produce field lines
- [x] **CORE-03**: Field lines are seeded around each charge proportionally to |q| × lines-per-unit-charge, preserving the original `sign(0)==+1` source-selection behaviour for net-zero configurations
- [x] **CORE-04**: Equipotential lines are computed at the configured potential interval, including the de-duplication pass (moved from the MFC document into the core, in world units)
- [x] **CORE-05**: Computation runs on a configurable number of worker threads without blocking the UI
- [x] **CORE-06**: A running computation can be cancelled (tab close, app quit) within ~200 ms without crashes, leaks or detached threads
- [x] **CORE-07**: Known portability defects are fixed: unqualified `abs(double)`, `M_PI` redefinition, `FieldLineJob job(job)` self-copy, unguarded `AdjustForBezier` divide-by-zero; core compiles warning-clean with Clang (via Zig) using `-ffp-contract=off`, no fast-math

### Configurations & Display

- [x] **VIEW-01**: User can open any of the 9 preset charge configurations (Dipole, Two equal charges, Two unequal charges same sign, Two unequal charges different sign, Quadrupole, Four charges same sign, Two dipoles same direction, Two dipoles different direction, Parallel dipoles) from a menu, each in its own tab, with charge positions/magnitudes identical to the MFC version
- [x] **VIEW-02**: User can have several configurations open at once and close any tab, including while it is still computing
- [x] **VIEW-03**: User sees charges drawn as filled circles colored by sign and field/equipotential lines drawn anti-aliased with Bezier smoothing, y-up, centered, at the original scale (100 px per unit), correct on HiDPI displays
- [x] **VIEW-04**: User sees a busy indicator while a configuration is computing, and the completed field lines and equipotentials once computation finishes (parity behaviour)
- [x] **VIEW-05**: App runs as a GUI application (no console window on Windows) with an About box including the project/blog links

### Options

- [x] **OPT-01**: User can open an Options dialog with a Computation section: lines per unit charge (4–128, default 16), calculate equipotentials (default on), potential interval (0.01–0.50, default 0.10), threads (1–128, default 4), integration method (default Euler)
- [x] **OPT-02**: User can set Draw options: positive/negative charge colors, field/equipotential line colors and thicknesses (1–64), with the original defaults (blue, red, black, grey 192); all thickness fields are validated
- [x] **OPT-03**: Changing Draw options repaints all open tabs immediately; Computation options apply to newly opened configurations
- [x] **OPT-04**: Options persist between runs in a portable config file in the platform user config directory (colors as `#RRGGBB`, method by name); missing or invalid file falls back to defaults

### Export

- [x] **EXP-01**: User can export the current configuration view to PNG (replaces printing)
- [x] **EXP-02**: User can export the current configuration view to SVG

### Quality

- [x] **QA-01**: Unit tests cover vector math, charge field/potential against analytic values, and convergence order of the integrators
- [x] **QA-02**: Tests cover line generation invariants per preset (seed counts, termination, de-dup) using tolerances rather than bit-exact geometry, and `sign(0)==1`
- [x] **QA-03**: Tests cover compute-session behaviour: 1-vs-N thread result equivalence, cancellation latency, no data races (TSan run on Linux CI)
- [x] **QA-04**: A headless CLI renders any preset to SVG/PNG without a display, used in CI on all three OSes as a parity check
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
| BUILD-06 | Phase 4 | Complete |
| BUILD-07 | Phase 4 | Complete |
| CORE-01 | Phase 1 | Complete |
| CORE-02 | Phase 1 | Complete |
| CORE-03 | Phase 1 | Complete |
| CORE-04 | Phase 2 | Complete |
| CORE-05 | Phase 2 | Complete |
| CORE-06 | Phase 2 | Complete |
| CORE-07 | Phase 2 | Complete |
| VIEW-01 | Phase 3 | Complete |
| VIEW-02 | Phase 3 | Complete |
| VIEW-03 | Phase 3 | Complete |
| VIEW-04 | Phase 3 | Complete |
| VIEW-05 | Phase 3 | Complete |
| OPT-01 | Phase 4 | Complete |
| OPT-02 | Phase 4 | Complete |
| OPT-03 | Phase 4 | Complete |
| OPT-04 | Phase 4 | Complete |
| EXP-01 | Phase 4 | Complete |
| EXP-02 | Phase 4 | Complete |
| QA-01 | Phase 1 | Complete |
| QA-02 | Phase 2 | Complete |
| QA-03 | Phase 2 | Complete |
| QA-04 | Phase 2 | Complete |
| QA-05 | Phase 1 | Complete |

**Coverage:**

- v1 requirements: 30 total
- Mapped to phases: 30 (Phase 1: 10, Phase 2: 7, Phase 3: 5, Phase 4: 8)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-10-10*
*Last updated: 2026-10-10 after roadmap creation (traceability filled)*

---

# Milestone v2.0 Requirements (added 2026-10-11)

## User Configuration Files

- [x] **CFG-01**: Named charge configurations (value, x, y per charge) persist in a human-readable configurations.ini in the platform user config directory; invalid entries are skipped safely and never affect the 9 built-in presets
- [x] **CFG-02**: The preset chooser lists user configurations below the built-ins; File > New > "Empty Configuration" opens an editable empty tab
- [x] **CFG-03**: Save As (Ctrl+S / File > Save As) stores the current tab's charges under a chosen name; overwriting an existing name asks for confirmation; built-in presets cannot be overwritten

## Charge Editor

- [x] **EDIT-01**: Clicking on or near a charge selects it; dragging with the mouse moves it, with the charge glyph following in realtime
- [x] **EDIT-02**: The arrow keys nudge the selected charge (Shift = larger step)
- [x] **EDIT-03**: The editor panel shows the selected charge's value, x and y as input fields with up/down buttons; every change updates the canvas immediately without starting a computation
- [x] **EDIT-04**: Double-clicking empty canvas adds a charge; Delete removes the selected one; at least one charge must remain
- [x] **EDIT-05**: A tab edited after its computation is marked stale (drawn lines belong to the pre-edit charges) until it is recalculated

## Explicit Calculation

- [x] **CALC-01**: A Calculate button (F5) recomputes the tab from the current charges, reusing the busy/close/cancel behaviour of the existing sessions
- [x] **CALC-02**: The editor panel offers a calculation method selector defaulting to the app option; it applies at the next Calculate

## Traceability (v2)

| Requirement | Phase | Status |
|-------------|-------|--------|
| CFG-01 | v2 Phase 1 | Complete |
| CFG-02 | v2 Phase 1 | Complete |
| CFG-03 | v2 Phase 1 | Complete |
| EDIT-01..05 | v2 Phase 2 | Complete |
| CALC-01..02 | v2 Phase 2 | Complete |

---

# Milestone v2.1 Requirements (added 2026-10-11)

## Editor & Session Polish

- [ ] **EDIT-06**: The editor panel has an Add-charge section (value, x, y inputs and an Add button) that appends and selects a charge without needing the canvas
- [ ] **EDIT-07**: The six stepper buttons render as arrow glyphs (left/right for X, up/down for Y and Value)
- [ ] **TAB-01**: The active tab can be renamed from the editor panel; the window title follows; an empty name is refused

## Session Persistence

- [ ] **SES-01**: Open tabs (title, source, charges) persist in a session.ini in the user config directory and restore on startup; restored titles never collide with later opens
- [ ] **SES-02**: Each tab's calculation method is stored in the session file and restored with the tab
- [ ] **SES-03**: Help > Open Configuration Folder reveals the folder holding options.ini / configurations.ini / session.ini

## Traceability (v2.1)

| Requirement | Status |
|-------------|--------|
| EDIT-06 | Pending |
| EDIT-07 | Pending |
| TAB-01 | Pending |
| SES-01 | Pending |
| SES-02 | Pending |
| SES-03 | Pending |

---

# Milestone v2.2 Requirements (added 2026-10-11)

## Collections

- [ ] **COL-01**: Collections live as one human-readable file per collection under Collections\ in the user config directory; on first run the Original collection is seeded with the 9 built-in presets plus any legacy user configurations from configurations.ini (left untouched)
- [ ] **COL-02**: The menu bar shows the current collection with a switcher; File > New Collection creates and switches to an empty collection; switching saves the current session and closes its tabs
- [ ] **COL-03**: Calculate All closes the open tabs and opens every configuration of the current collection in tabs, each computing as usual
- [ ] **COL-04**: The session file records the current collection and restores it (with its tabs) on startup; Save As stores into the current collection file

## Editor refinement

- [ ] **EDIT-08**: The New-charge entry (value, x, y and Add) is always visible in the editor panel, not hidden behind a collapsed header
- [ ] **EDIT-09**: A switch-sign button flips the selected charge's value between positive and negative (and marks the tab stale)

## Traceability (v2.2)

| Requirement | Status |
|-------------|--------|
| COL-01..04 | Complete |
| EDIT-08 | Complete |
| EDIT-09 | Complete |
| TITLE-01, COL-05, MENU-01, CALC-04, EDIT-11..13, GRID-01..04 (v2.3) | Complete |

---

# Milestone v2.3 Requirements (added 2026-10-11)

## Title & Menus

- [ ] **TITLE-01**: The window title shows the collection (with the active tab when one is open)
- [ ] **MENU-01**: The File menu shows New Configuration... (opens the chooser) and New Collection...; the bare New / New... entries are removed

## Collections

- [ ] **COL-05**: Right-clicking a configuration in the chooser offers Remove (with confirmation); it is deleted from the collection file; open tabs are unaffected

## Calculation

- [ ] **CALC-04**: Continuous calculation actually recomputes after an edit (the debounce deadline is armed only by edits, never re-armed idle); pinned by a smoke regression test

## Editor

- [ ] **EDIT-11**: Esc on a stale tab reverts the charges to the last calculated state
- [ ] **EDIT-12**: Edit > Undo (Ctrl+Z) / Redo (Ctrl+Y) steps through charge and method changes per tab
- [ ] **EDIT-13**: Double-click empty canvas adds a charge in one go; single click selects it for editing (interaction contract, pinned)

## Grid

- [ ] **GRID-01**: A grid can be set active (snapping) independently of visibility
- [ ] **GRID-02**: A grid can be shown or hidden independently
- [ ] **GRID-03**: The grid is Cartesian or polar
- [ ] **GRID-04**: The grid origin is definable for both grid types; snapping applies to drag, double-click add and typed positions; grid settings persist in the session

---

# Milestone v2.4 Requirements (added 2026-10-11)

## Automation API

- [ ] **API-01**: A command API exposes every menu action and every editor/collection operation (app/tab/config/charge/view/grid/options commands) with a text result per call
- [ ] **API-02**: A headless script runner executes command files (ElectricField --script <file> [--user-dir <dir>] [--out <file>]); app.state dumps the full UI state as a test oracle
- [ ] **API-03**: Script tests in CI cover the editor scenarios (open/edit/undo/redo/esc/grid/save/remove/collection flows) on all three OSes
- [ ] **API-04**: --user-dir redirects all user-file paths (options/configurations/Collections/sessions) so scripts never touch real user data

## Bug fixes

- [ ] **BUG-01**: Grids are visible (BUG: the background fill overwrote them)
- [ ] **BUG-02**: New Collection works (BUG: the leave-confirm modal stayed open after its answer and locked the UI)

## Tab & Collection UX

- [ ] **TAB-02**: Tabs closable via x button, middle click and Close Tab (verified; tab right-click menu added)
- [ ] **TAB-03**: Tab rename works (panel name field + tab right-click menu)
- [ ] **TAB-04**: The tab right-click menu offers Close / Rename / Save Configuration As / Calculate
- [ ] **COL-06**: Save Collection (Ctrl+S) stores the collection file, merging matching edited tabs
- [ ] **COL-07**: Save Collection As... saves under a new name and switches, keeping the tabs open
- [ ] **COL-08**: Close Collection saves and switches to Original
- [ ] **COL-09**: Each collection's open tabs persist per collection (Sessions/<Name>.ini) and are restored on switch and on start

---

# Milestone v2.5 Requirements (added 2026-10-12)

## Collections flow

- [ ] **OPEN-01**: File > Open Collection... (Ctrl+O) opens a collection chooser; starting with no restored tabs shows the collection chooser first, and after opening an empty collection the configuration chooser follows; configurations are never auto-opened

## Grid style

- [ ] **GRID-05**: Grid lines are light green
- [ ] **GRID-06**: An intersections-only mode shows just the lattice points (Cartesian and polar) in a distinctly different colour; panel + API + session persistence

## Charge size

- [ ] **SIZE-01**: Charge size is a draw option (2-120 px, dot to very large, default 25) in View > Options and via options.set charge-size; persisted in options.ini; physics hit radius unchanged

## Traceability (v2.5)

| Requirement | Status |
|-------------|--------|
| OPEN-01 | Pending |
| GRID-05..07 | Pending |
| SIZE-01 | Pending |
