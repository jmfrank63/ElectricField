# Roadmap: ElectricField — Cross-Platform Port

## Overview

The MFC/Direct2D Windows app becomes a portable Zig-built application with Dear ImGui + SDL3. The journey starts by proving the toolchain and extracting a trustworthy physics core, then adds the threaded compute engine and a display-free parity oracle (headless CLI, SVG/PNG), then puts the GUI on top, and finally finishes the user-facing parity work (options, persistence, export) and deletes MFC. Phases 1-2 are GUI-independent, so the core stays correct whichever toolkit wins; the Phase 1 CI gate is the go/no-go for the Zig + SDL3 + ImGui stack (fallback if it cannot be made green on all three OSes: Qt 6 + CMake, with the core reused unchanged). MFC code is kept in the tree until Phase 4 so the Phase 1 visual baseline can be taken from the real application.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation & Portable Physics Core** - Clean repo, hash-pinned Zig build, 3-OS CI gate, tested GUI-free physics core, MFC visual baseline captured
- [x] **Phase 2: Compute Engine & Headless Parity** - Cancellable threaded compute, equipotential de-dup, toolkit-free scene with SVG/PNG, headless CLI parity check in CI (completed 2026-10-10)
- [x] **Phase 3: GUI Shell** - ImGui + SDL3 app with preset tabs, anti-aliased rendering, busy indicator, About box, GUI subsystem (completed 2026-10-11)
- [ ] **Phase 4: Options, Export & MFC Removal** - Options dialog, portable persistence, PNG/SVG export, MFC deleted, README updated

## Phase Details

### Phase 1: Foundation & Portable Physics Core

**Goal:** As the developer, I want a Zig-built, CI-verified physics core on all three OSes, so that the port rests on trusted numerics.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: BUILD-01, BUILD-02, BUILD-03, BUILD-04, BUILD-05, CORE-01, CORE-02, CORE-03, QA-01, QA-05
**Success Criteria** (what must be TRUE):

  1. On a fresh clone, `zig build` and `zig build test` succeed from the command line on Windows, Linux and macOS (arm64) with only Zig 0.17.0 installed (plus system libs on Linux); Dear ImGui, SDL3 and doctest are fetched through a hash-pinned `build.zig.zon`, and a minimal SDL3 + ImGui hello-window target builds and starts on each OS
  2. A GitHub Actions workflow builds and runs the tests on windows, ubuntu and macos-arm64 for every push to the branch and is green on all three (this is the go/no-go gate for the Zig stack before later phases depend on it)
  3. `git ls-files` lists no `ElectricField/x64/`, `.vs/` or `*.vcxproj.user` files, and `git status` stays clean after both an MSVC build and a Zig build because `.gitignore` covers MSVC and Zig outputs (`.zig-cache/`, `zig-out/`)
  4. The GUI-free physics core (standard library only, no global/app state) passes its tests on all three OSes: charge field and potential match analytic values, all 11 integrators are available in the original order and show their expected convergence order, and every preset yields field-line seeds proportional to |q| x lines-per-unit-charge (net-zero configurations keep the `sign(0)==+1` behaviour)
  5. Reference images of all 9 presets rendered by the existing MFC build with default options are recorded in the repo before any MFC file is removed

**Plans:** 4/4 plans executed

Plans:
**Wave 1**

- [x] 01-01-PLAN.md — Walking skeleton: hash-pinned zig build, std-only efcore, doctest, SDL3+ImGui hello window (--smoke), repo hygiene, ci.yml (wave 1)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md — QA-05 MFC baseline: capture script, 9 reference PNGs at 1024x768, provenance README (wave 2)
- [x] 01-03-PLAN.md — Core expansion: seeds + tracer for all 11 methods, convergence-order and preset-parity oracles (wave 2)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-04-PLAN.md — Phase gate: close-out checks, D-12 push consent (blocking-human), 3-OS CI verdict or Qt fallback record (wave 3)

### Phase 2: Compute Engine & Headless Parity

**Goal:** As the developer, I want a cancellable threaded compute engine with headless SVG/PNG output, so that CI can verify every preset on all OSes without a display.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: CORE-04, CORE-05, CORE-06, CORE-07, QA-02, QA-03, QA-04
**Success Criteria** (what must be TRUE):

  1. Any preset computes on the configured number of worker threads without blocking the caller; results with 1 thread and with N threads are equivalent within tolerance, field lines terminate, and equipotentials appear at the configured potential interval with the de-duplication pass removing duplicates (checked on Quadrupole and Parallel dipoles)
  2. Cancelling a running computation (as on tab close or app quit) returns within about 200 ms with no crash, leak or detached thread, and the Linux TSan run reports no data races
  3. The known portability defects (unqualified `abs(double)`, `M_PI` redefinition, `FieldLineJob job(job)` self-copy, unguarded `AdjustForBezier` divide-by-zero) no longer reproduce, each with a regression test, and the core compiles warning-clean with Clang using `-ffp-contract=off` and no fast-math
  4. A headless CLI renders any of the 9 presets to SVG and PNG without a display, and CI runs it on windows, ubuntu and macos-arm64 as a parity check whose outputs agree across OSes in charge and line counts within tolerance

**Plans:** 4/4 plans complete

Plans:
**Wave 1**

- [x] 02-01-PLAN.md — Threaded engine: ComputeSession (N workers, cancel, poll, job-key order), equipotentials + MFC de-dup, guarded Bezier, D-15 method lookup; session contract incl. cancel/destroy latency for every preset and method (wave 1)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02-PLAN.md — Oracles and build hardening: -Dsanitize=thread + lazy GUI deps with a local Linux TSan run, D-17 fail-not-skip preset oracle, CORE-07 regression oracles (self-copy fail-first), de-dup boundary, shared RK stages (wave 2)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-03-PLAN.md — Headless output: FieldScene + Painter, SVG and ImGui/SDL-software PNG, ef-cli, committed expected counts with a proven tolerance gate, Windows/Linux agreement, MFC comparison report (wave 3)

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 02-04-PLAN.md — CI gate: 3-OS headless parity + artifacts and tsan-linux job green for the pushed head; D-18 capture-script hardening, IN-03, research-ref removal, final green verdict (wave 4)

### Phase 3: GUI Shell

**Goal:** As a user, I want to open the 9 preset configurations in tabs, so that I can see their field lines and equipotentials on any desktop OS.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: VIEW-01, VIEW-02, VIEW-03, VIEW-04, VIEW-05
**Success Criteria** (what must be TRUE):

  1. User launches the app (no console window on Windows) and can open any of the 9 preset configurations from a menu, each in its own tab, with charge positions and magnitudes identical to the MFC version
  2. User can keep several configurations open at once and close any tab, including one that is still computing, without a hang or crash; quitting during a computation exits cleanly
  3. User sees a busy indicator on a tab while it computes, then the completed field lines and equipotentials once computation finishes
  4. User sees charges as filled circles colored by sign and field and equipotential lines anti-aliased with Bezier smoothing, y-up and centered at 100 px per unit, sharp and correctly scaled on a HiDPI display, visually matching the Phase 1 MFC reference images
  5. User can open an About box showing the project and blog links

**Plans:** 3/3 plans complete

Plans:
**Wave 1**

- [x] 03-01-PLAN.md — Tab model (efapp) over real ComputeSessions + render-context rule (renderSurface) from the prototype, wired into build.zig with doctests and test-render; Wave 0 FramePacer/scripted-session tests, Linux TSan (wave 1)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 03-02-PLAN.md — The real GUI: src/gui + assets from the prototype, GUI subsystem exe with icon, full UI smoke, real-window idle check; make-time git hash for About; Linux pre-flight in WSL (wave 2)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 03-03-PLAN.md — CI gate: test-render, Windows subsystem check, GUI screenshots on 3 OSes, pushed and green incl. tsan-linux; research-ref removed, final green verdict (wave 3)

**UI hint**: yes

### Phase 4: Options, Export & MFC Removal

**Goal:** As a user, I want to set options, keep them between runs and export images, so that the app fully replaces the MFC version.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: OPT-01, OPT-02, OPT-03, OPT-04, EXP-01, EXP-02, BUILD-06, BUILD-07
**Success Criteria** (what must be TRUE):

  1. User can open an Options dialog with Computation settings (lines per unit charge 4-128 default 16, equipotentials on by default, potential interval 0.01-0.50 default 0.10, threads 1-128 default 4, integration method default Euler) and Draw settings (positive/negative charge colors, field and equipotential line colors and thicknesses 1-64, original defaults), and out-of-range values including the equipotential thickness are rejected
  2. Changing Draw options repaints all open tabs immediately, while changed Computation options apply to configurations opened afterwards
  3. Options persist between runs in a config file in the platform user config directory (colors as `#RRGGBB`, method by name); a missing or invalid file falls back to the defaults
  4. User can export the current configuration view to PNG and to SVG, and the exported files match what is shown on screen
  5. The source tree contains no MFC, Win32, Direct2D or GDI code, no `.vcxproj`/`.sln` and no `.rc` resources; CI stays green on all three OSes, and the README lets a new contributor build, run and test on Windows, Linux and macOS

**Plans**: TBD
**UI hint**: yes

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation & Portable Physics Core | 4/4 | In Progress|  |
| 2. Compute Engine & Headless Parity | 4/4 | Complete    | 2026-10-10 |
| 3. GUI Shell | 3/3 | Complete    | 2026-10-11 |
| 4. Options, Export & MFC Removal | 0/TBD | Not started | - |
