---
phase: 01-foundation-portable-physics-core
plan: 03
subsystem: testing
tags: [cpp17, zig-build, doctest, runge-kutta, field-lines, seeds, presets, qa-01]
status: complete

requires:
  - phase: 01-01
    provides: "walking-skeleton core (ChargeSystem, ComputeParams, Method/withSolver, rk.hpp, presets) and the zig build test harness"
provides:
  - "ef::planSeeds / ef::expectedLineCount: MFC seeding rules incl. sign(0) == +1, sinks get one seed, std::abs fix"
  - "ef::traceFieldLine<Solver>: pure single-line tracer with MFC step budget, thinning and HitCharge stop"
  - "Test oracles: vector math, 11-method table, convergence order, adaptive step control, DP characterisation, seed invariants, preset parity vs ConfigDocs.cpp"
affects: [01-04, phase-2-threads, phase-3-gui, phase-4-mfc-removal]

actuals:
  tokens: 5600
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: [tolerance-based numeric oracles (never bit-exact geometry), fail-first mutation proof of a parity oracle, tracer-first slice then oracles]

key-files:
  created:
    - src/core/ef/seeds.hpp
    - src/core/ef/seeds.cpp
    - src/core/ef/tracer.hpp
    - tests/test_seeds.cpp
    - tests/test_vec2.cpp
    - tests/test_rk_order.cpp
    - tests/test_presets.cpp
  modified:
    - build.zig

key-decisions:
  - "Dormand-Prince low-order weights stay as in MFC (error coefficients b - b*); pinned by a characterisation test. PHASE 2 DECISION REQUIRED: DP error weights: keep vs fix (a fix needs a recorded decision and a re-baseline against the MFC images)."
  - "build.zig source lists equal research-ref order: core = ef/seeds.cpp, ef/presets.cpp; tests = test_main, test_vec2, test_field, test_rk_order, test_seeds, test_presets (inserted, not appended, per orchestrator note)"
  - "Core stays std-only; MFC tree under ElectricField/ untouched (D-02)"

patterns-established:
  - "Seed is an explicit named-field struct (fixes MFC defect 10, swapped angle/angle_start aggregate init)"
  - "Order tests: rotation ODE strict |p - nominal| <= 0.3, non-linear time-dependent ODE one-sided p >= nominal - 0.5"

requirements-completed: [CORE-01, CORE-02, CORE-03, QA-01, BUILD-02]

duration: 12min
completed: 2026-10-10
---

# Phase 1 Plan 03: Seeds, Tracer and Integrator/Preset Oracles Summary

**Seed planning and single-line tracing ported from the MFC source so all 11 Runge-Kutta methods produce a dipole field line, plus tolerance-based oracles for vector math, convergence order, adaptive step control and a regex cross-check of the 9 presets against ElectricField/ConfigDocs.cpp.**

## Performance

- **Duration:** ~12 min
- **Tasks:** 3 (1 tracer, 2 auto)
- **Files modified:** 8 (7 created, build.zig modified)

## Accomplishments

- `planSeeds` reproduces the MFC loop exactly (angle step 2*pi/(|q|*L), source start -pi - step/2, sink shifted by pi + step, sink break rule, zero charges skipped); `expectedLineCount` uses `std::abs` so all-negative systems no longer wrap (MFC defect 11).
- `traceFieldLine` runs the same code path for every method through `withSolver`; every method traces a dipole line from the + charge that ends within 0.35 of the - charge with all points finite. Sink seeds record no points.
- Convergence order proven for the 6 fixed-step methods and the stepping solution of the 5 adaptive pairs; adaptive step control (tolerance scaling, shrink/grow, max_step clamp) is tested.
- Preset table parity: the regex parse of `ElectricField/ConfigDocs.cpp` matches all 9 presets, and the oracle is proven fail-first.

## Task Commits

1. **Task 1 (tracer): seed planning + single-line tracer** - `92ae444` (feat)
2. **Task 2: integrator convergence-order and vector-math oracles** - `e820172` (test)
3. **Task 3: preset table cross-checked against the MFC source** - `7041336` (test)

## Verification Results

- `zig build test -Dgui=false` (Debug, UBSan traps): 28 test cases, 4385 assertions, all pass.
- `zig build test -Dgui=false -Doptimize=ReleaseFast`: 28 test cases, 4385 assertions, all pass.
- Tracer gate (auto mode active): the tracer verify passed end-to-end before expansion ("Tracer verified end-to-end - expanding").
- **Actual test-case count: 28** (5 test_field + 4 test_vec2 + 8 test_rk_order + 9 test_seeds + 2 test_presets). The plan text mentioned both 27 and 28; 28 is correct (27 = research-ref without the new empty-system case).
- **Fail-first proof (D-07):** changed the dipole's second charge in `src/core/ef/presets.cpp` from -1 to -2. `zig build test` exited 1; output included `tests\test_presets.cpp:51: ERROR: CHECK( docs[i][k].charge == presets()[i].charges[k].charge )` with `logged: presets()[i].slug := dipole` (plus the expected knock-on failures in test_field and test_seeds). After `git checkout -- src/core/ef/presets.cpp` the file was byte-identical to HEAD and the suite was green again (28/28).
- Purity gates (MFC/GUI identifiers, non-std includes, `M_PI`, non-const `static`, unqualified math calls) all print nothing. `PostCalculateEquipotential` occurs 0 times in tracer.hpp. `git log -- src/core/ef/rk.hpp` has exactly 1 commit (DP weights untouched). `git log -- ElectricField/ | grep -c 01-03` is 0.

## Decisions Made

- **Phase 2 decision (open): "DP error weights: keep vs fix".** The Dormand-Prince "low" weights in MFC (`RungeKutta.inl:191`) are the error coefficients b - b*, not b*. Kept for parity and pinned by the test `KNOWN DEFECT (kept for parity)`. Changing it needs a recorded decision and a re-baseline against `docs/baseline/mfc/`.
- Source/test order in `build.zig` follows research-ref (insert, not append), resolving the plan-checker wording inconsistency.

## Deviations from Plan

None - plan executed exactly as written (research-ref files copied with only the listed deltas: corrected MFC citations in seeds.hpp/tracer.hpp, the extra empty-system test case, build.zig source lists).

## Issues Encountered

- research-ref `params.hpp`, `rk.hpp` and `charge_system.hpp` differ from the repo versions (comments and `chargeRadiusWorld()` added during plan 01-01). The repo versions were kept; the seeds/tracer sources only use the shared API and compiled unchanged.

## Known Stubs

None.

## Threat Flags

None - no new network, auth or trust-boundary surface. `test_presets.cpp` reads one tracked repo file relative to the repo root (T-01-16, accepted).

## Next Phase Readiness

Plan 01-04 (CI + human push checkpoint) can build on a green 28-case suite in Debug and ReleaseFast. Phase 2 inherits the open DP decision above and must add cancellation/threading around `traceFieldLine` (T-01-13, accepted for Phase 1).

## Self-Check: PASSED

All 7 created files and the 3 commits (`92ae444`, `e820172`, `7041336`) verified on disk / in git log.
