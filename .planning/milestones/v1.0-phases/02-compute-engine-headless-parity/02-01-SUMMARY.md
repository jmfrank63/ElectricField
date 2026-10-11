---
phase: 02-compute-engine-headless-parity
plan: 01
subsystem: compute-engine
tags: [cpp17, zig-build, doctest, threads, cancellation, equipotentials, dedup, bezier]
status: complete

requires:
  - phase: 01-foundation-portable-physics-core
    provides: "ChargeSystem, ComputeParams, Method/withSolver, rk.hpp, seeds, presets, zig build test harness"
provides:
  - "ef::ComputeSession: RAII, cancellable, N-thread engine (constructor returns at once; cancel/poll/wait/takeResult)"
  - "ef::traceElectricLine / traceEquipotential with hooks, kCancelStride = 1024, shouldPostEquipotential"
  - "ef::dedupEquipotentials / ef::finalize: verbatim MFC GetDataFromThreads de-dup in world units"
  - "Guarded ef::adjustForBezier (kBezierMinDivisor = 1e-9, MFC t3 formula kept)"
  - "D-15 method lookup: methodFromName, bounds-checked info(), withSolver throws on invalid Method"
  - "Session test suite (10 cases): determinism, invariants, de-dup, poll, cancel/destroy latency incl. 9 presets x 11 methods"
affects: [02-02, 02-03, 02-04, phase-3-gui]

actuals:
  tokens: 9200
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns: [condition-variable worker pool with in-flight termination, drain-and-swap poll of immutable shared lines, JobKey ordering for determinism, compile-time TSan-aware latency budget]

key-files:
  created:
    - src/core/ef/bezier.hpp
    - src/core/ef/finalizer.hpp
    - src/core/ef/finalizer.cpp
    - src/core/ef/session.hpp
    - src/core/ef/session.cpp
    - tests/test_params.hpp
    - tests/test_session.cpp
  modified:
    - src/core/ef/params.hpp
    - src/core/ef/tracer.hpp
    - src/core/ef/method.hpp
    - build.zig

key-decisions:
  - "Engine copied from the compile-tested research-ref overlay; only the plan's deltas applied (kCancelStride comment, interim local lightParams, TSan-aware kLatencyBudgetMs, two new test cases)"
  - "research-ref rk.hpp (computeStages refactor, D-16) deliberately NOT copied: not in this plan's file list, belongs to a later plan"
  - "kCancelStride stays 1024 (D-02 amended 2026-10-10); comment no longer quotes the old stride"
  - "MFC parity quirks (unsorted two-opposite-charge shortcut, unfiltered last group, AdjustForBezier t3, pixel/world mixing in thinning) kept verbatim; plan 02-02 pins them"

patterns-established:
  - "Latency budget is 200 ms in every build except ThreadSanitizer (1000 ms), selected at compile time via __SANITIZE_THREAD__ / __has_feature(thread_sanitizer)"
  - "Tracer slice first: Task 1 proves preset -> N threads -> equipotentials -> de-dup -> Bezier before the contract tests"

requirements-completed: [CORE-04, CORE-05, CORE-06, QA-02, QA-03]

duration: 8min
completed: 2026-10-10
---

# Phase 2 Plan 01: Threaded ComputeSession, Equipotentials and De-dup Summary

**Cancellable N-thread `ef::ComputeSession` (condition-variable pool, JobKey-ordered deterministic results, verbatim MFC equipotential de-dup, guarded Bezier fit) with a 38-case doctest suite proving 1-vs-7-thread equality and cancel/destroy latency of under 8 ms for all 99 preset x method combinations.**

## Performance

- **Duration:** ~8 min
- **Tasks:** 2 (1 tracer, 1 auto)
- **Commits:** 87ce32c (Task 1), f63366d (Task 2)

## Accomplishments

- Task 1 (tracer): preset -> `ComputeSession` on 1 and on 7 workers -> field lines + posted equipotential jobs -> de-dup + Bezier -> `FieldResult`, identical within 1e-12 for dipole and parallel-dipoles. Verified in ReleaseFast and Debug (tracer gate: passed, expansion proceeded).
- Task 2: D-15 (`methodFromName`, bounds-checked `info()`, throwing `withSolver`), full REF session suite, `numThreads 0` boundary case, TSan-aware `kLatencyBudgetMs`, and the every-preset/every-method cancel + destroy sweep.
- Suite grew from 28 to 38 doctest cases; all green in Debug and ReleaseFast.

## Measured latencies (recorded as the plan requires)

| Build | Heaviest job `worst cancel latency ms:` | Sweep line |
|-------|------------------------------------------|------------|
| ReleaseFast | 0.6376 | 99 runs, 98 cancelled mid-run, worst cancel 0.8202 ms (four-charges-same-sign/adaptive-fehlberg), worst destroy 17.9017 ms (two-unequal-charges-same-sign/rk3per8) |
| Debug | 5.8251 | 99 runs, 99 cancelled mid-run, worst cancel 7.1552 ms (two-dipoles-same-direction/adaptive-dormand-prince), worst destroy 22.0325 ms (four-charges-same-sign/adaptive-dormand-prince) |

Both are far inside the 200 ms contract. Destroy time includes the 5 ms head start, so 17.9 / 22.0 ms is about 13 / 17 ms of cancel + join. The ReleaseFast run finished one preset/method before the cancel landed (98 of 99 cancelled mid-run), which the test allows.

## Task Commits

1. **Task 1 (tracer): engine end to end** - `87ce32c` (feat) - build.zig, bezier.hpp, finalizer.{hpp,cpp}, params.hpp, session.{hpp,cpp}, tracer.hpp, tests/test_session.cpp
2. **Task 2: session contract** - `f63366d` (feat) - method.hpp, tests/test_params.hpp, tests/test_session.cpp

## Deviations from Plan

None - plan executed as written.

Notes (not deviations): the research-ref `rk.hpp` differs from the repo copy (D-16 `computeStages` refactor); it is outside this plan's file list and was left alone. The Task 1 interim test file omits the unused `using Clock` alias as well as `msSince` (both come back with the full REF file in Task 2).

## Authentication Gates

None.

## Known Stubs

None. No placeholder data, TODO/FIXME or empty-value stubs in the files created or modified (windows ledger not appended).

## Threat Flags

None. The only new surface is in-process threading, which the plan's threat model (T-02-01..06) already covers. ThreadSanitizer evidence follows in plan 02-02.

## Verification

- `zig build test -Dgui=false -Doptimize=ReleaseFast --summary all`: 38 cases, 211960 assertions, pass
- `zig build test -Dgui=false --summary all` (Debug): 38 cases, 211961 assertions, pass
- std-only include gate, MFC/GUI identifier gate, detach/delete-this/jthread gate: all print nothing
- `git diff --name-only 7281ffb HEAD -- ElectricField/`: empty
- Not run here by design: the TSan build and the `__has_feature` TSan branch of `kLatencyBudgetMs` (plan 02-02)

## Self-Check: PASSED

- FOUND: src/core/ef/session.hpp, session.cpp, finalizer.hpp, finalizer.cpp, bezier.hpp, tests/test_session.cpp, tests/test_params.hpp
- FOUND commits: 87ce32c, f63366d
