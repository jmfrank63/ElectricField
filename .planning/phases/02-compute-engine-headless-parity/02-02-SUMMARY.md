---
phase: 02-compute-engine-headless-parity
plan: 02
subsystem: compute-engine
tags: [threadsanitizer, zig-build, lazy-dependencies, regression-tests, doctest, rk-refactor]
status: complete

requires:
  - phase: 02-compute-engine-headless-parity
    provides: "ComputeSession, tracer/bezier/finalizer, session test suite (plan 02-01)"
provides:
  - "build.zig: -Dsanitize=none|thread, Ctx/coreModule(), lazy GUI deps via b.lazyDependency, TSAN_OPTIONS on the test step"
  - "build.zig.zon: sdl3 and imgui marked .lazy = true (hashes unchanged); -Dgui=false fetches only doctest"
  - "D-17: MFC preset oracle fails (not skips) when ConfigDocs.cpp is unreadable; parser state reset per document"
  - "CORE-07 regression oracles: job self-copy (fail-first proven), abs(double), libc pi macro, FMA canary, Bezier divide-by-zero"
  - "D-05 / CORE-04: de-dup shortcut, grouping, determinism and strict 2*chargeRadius boundary pinned"
  - "D-16: AdaptiveRungeKutta::computeStages shared by SolveStep and SolveStepPair"
  - "Whole suite (52 cases) TSan-clean on Linux (WSL Ubuntu 24.04)"
affects: [02-03, 02-04]

actuals:
  tokens: 6200
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns: [lazy Zig dependencies with "orelse return null", sanitize_thread on every core-link module, fail-first proof by temporary mutation]

key-files:
  created:
    - tests/test_tracer.cpp
    - tests/test_bezier.cpp
    - tests/test_finalizer.cpp
  modified:
    - build.zig
    - build.zig.zon
    - tests/test_presets.cpp
    - tests/test_rk_order.cpp
    - src/core/ef/rk.hpp

key-decisions:
  - "build.zig copied from the research-ref overlay with the CLI, scene/svg and test_scene pieces removed (restored by plan 02-03); Task 2 adds the three new test files to test_sources"
  - "Dormand-Prince low weights, AdjustForBezier t3 formula, pixel/world mixing, unsorted two-opposite-charge shortcut and unfiltered last group all stay as in MFC and stay pinned (D-05..D-08)"

patterns-established:
  - "TSan runs happen on the committed HEAD via git archive into the WSL home directory, one wsl.exe call"
  - "Fail-first proofs: mutate the code under test, run the suite, expect the named test to fail, git checkout the file, re-run green"

requirements-completed: [CORE-07, QA-03, CORE-04, QA-02]

duration: 25min
completed: 2026-10-10
---

# Phase 2 Plan 02: Race-checked engine and CORE-07 regression oracles Summary

**ThreadSanitizer build option with lazy SDL3/ImGui dependencies, a fail-not-skip MFC preset oracle, and regression tests (self-copy proven fail-first) pinning every CORE-07 defect and the de-dup boundary, with the whole 52-case suite race-free on Linux.**

## Performance

- **Duration:** about 25 min (most of it cold fetches and the TSan builds)
- **Started:** 2026-10-10T12:30Z
- **Completed:** 2026-10-10T12:54Z
- **Tasks:** 2
- **Files modified:** 8 (3 new)

## Accomplishments

- `-Dsanitize=thread` instruments the core library and the test binary (`tsan_symbols=44`), with `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`. The 60-session random-cancel stress test and the every-preset/every-method cancel sweep ran race-free on WSL Ubuntu 24.04, after Task 1 (38 cases) and again on the final HEAD (52 cases).
- `-Dgui=false` never fetches the GUI stack: a fresh clone built with an empty global cache has exactly one entry in `zig-pkg` (the doctest package `N-V-__8AAB9aPgADa6XPcoVfyGpb-2fhHYgVMX9InfAngj-p`). `zig build` and `zig build smoke` still configure and pass with the GUI (`presets=9`, `smoke ok frames=2`).
- D-17: the preset cross-check now fails when `ElectricField/ConfigDocs.cpp` is unreadable. Proven fail-first by running the test binary from `zig-out` (no `ElectricField/` there): exit 1, `ElectricField/ConfigDocs.cpp not readable (cwd must be the repository root)`.
- 14 new test cases (6 tracer, 3 bezier, 5 finalizer): equipotential posting, job self-copy, abs(double), libc pi macro, FMA canary, Bezier divide-by-zero, de-dup shortcut/grouping/determinism/finalize, and the strict `2 * chargeRadius` boundary (exactly 0.5 world units apart merges, 0.5000001 apart is kept).
- D-16: `computeStages()` is the single stage implementation behind `SolveStep` and `SolveStepPair`; all Phase 1 order and Dormand-Prince characterisation tests pass unchanged, `dpLow` untouched.

## Task Commits

1. **Task 1 (tracer): lazy GUI deps, -Dsanitize=thread, fail-not-skip preset oracle** - `fe235f8` (build)
2. **Task 2: CORE-07 regression oracles and shared RK stage computation** - `bdba4a8` (test)

## Verification Evidence

| Gate | Result |
|------|--------|
| Debug core suite (after Task 1) | 38/38 passed, 211990 assertions, run 32 s |
| ReleaseFast core suite (after Task 1) | 38/38 passed, 7 s |
| Debug core suite (after Task 2) | 52/52 passed |
| ReleaseFast core suite (after Task 2) | 52/52 passed, 212621 assertions |
| `zig build --summary all` / `zig build smoke` | 12/12 and 11/11 steps; `presets=9`, `smoke ok frames=2` |
| D-17 fail-first (run from `zig-out`) | exit 1, output contains `not readable` |
| D-14 fresh-clone laziness (empty `ZIG_GLOBAL_CACHE_DIR`, `-Dgui=false`) | exit 0, `zig-pkg` has only the doctest entry; took 3 min cold |
| TSan, Task 1 HEAD (`fe235f8`) | exit 0, 38/38 passed, `tsan_symbols=44`, 32.5 s |
| TSan, final HEAD (`bdba4a8`) | exit 0, 52/52 passed, 211990+ assertions, `tsan_symbols=44`, 33.0 s |
| `git diff --name-only 7281ffb HEAD -- ElectricField/` | empty |

Cancel latency under TSan (budget 1000 ms):

- Task 1 HEAD: heaviest job `worst cancel latency ms: 1.17864`; sweep `99 runs, 99 cancelled mid-run, worst cancel 2.16638 ms (parallel-dipoles/adaptive-cash-karp), worst destroy 8.05058 ms (parallel-dipoles/adaptive-dormand-prince)`.
- Final HEAD: heaviest job `1.3092 ms`; sweep `99 runs, 99 cancelled mid-run, worst cancel 1.77786 ms (two-dipoles-opposed-direction/adaptive-cash-karp), worst destroy 8.31837 ms (four-charges-same-sign/adaptive-cash-karp)`.

Fail-first proof of the self-copy regression (Task 2 step 6): `tracer.hpp` line 139 was temporarily changed to `hooks.postEquipotential(Vec2(), oldPotential)`. With that mutation `zig build test -Dgui=false -Doptimize=ReleaseFast` exited 1 and the test `FieldLineJob job(job) regression (CORE-07): a posted job is a copy of the PARENT's current state, never uninitialised data` failed repeatedly on `tests\test_tracer.cpp:67: CHECK( std::abs(dipole.Potential(c.start) - c.label) <= p.potentialInterval + 1e-9 )`. One session test (`test_session.cpp:121`, `r.potential.size() == raw`) failed under the mutation too. After `git checkout -- src/core/ef/tracer.hpp` the file diff was clean and the suite exited 0 with 52/52 passed.

## Deviations from Plan

None. The plan executed as written. Notes that are not deviations:

- The ReleaseFast cancel sweep logged `98 cancelled mid-run` once (one job finished before the cancel landed); the assertion allows this and it passed. The TSan runs reported 99 of 99.
- The REF files are LF and the working tree is CRLF (`core.autocrlf=true`); git only warned about line-ending normalisation.

## Known Stubs

None.

## Threat Flags

None. No new network endpoints, auth paths or trust-boundary file access. `build.zig.zon` changed only by adding `.lazy = true`; the three url+hash pairs are unchanged.

## Issues Encountered

- A stray `python` probe hung in the shell for the tool's timeout and was abandoned; it had no effect on the repo.

## Next Phase Readiness

- Plan 02-03 can restore `ef/scene.cpp`, `ef/svg.cpp`, `test_scene.cpp`, `cli_flags`, `cli_flags_png` and `addCli` on top of the Ctx/coreModule structure, after which build.zig equals the REF version.
- Plan 02-04 adds the CI TSan job; the local Linux TSan recipe is validated here. The research-ref directory is still present (removal belongs to phase close-out).

## Self-Check: PASSED

- Created files found: `tests/test_tracer.cpp`, `tests/test_bezier.cpp`, `tests/test_finalizer.cpp`
- Commits found: `fe235f8`, `bdba4a8`
