---
phase: 01-foundation-portable-physics-core
plan: 01
subsystem: infra
tags: [zig-0.17, sdl3, dear-imgui, doctest, github-actions, cpp17]
status: complete

requires: []
provides:
  - "Hash-pinned zig build graph (efcore static lib, efcore-tests, ElectricField GUI exe; steps test/run/smoke; option -Dgui)"
  - "Std-only C++17 efcore chain: math, vec2, rk (11 solvers), method, params, charge_system, presets (9 data rows)"
  - "doctest suite with QA-01 analytic field/potential checks"
  - "SDL3 + Dear ImGui hello window with headless --smoke mode"
  - "Repo hygiene: v145 toolset commit, MSVC outputs untracked, unanchored MSVC + Zig .gitignore"
  - ".github/workflows/ci.yml 3-OS gate (defined, not pushed)"
affects: [01-02, 01-03, 01-04, phase-2-compute, phase-3-gui]

actuals:
  tokens: 8500
  tasks: 3
  commits: 4

tech-stack:
  added: [Zig 0.17.0 build, castholm/SDL 3.4.18 (commit dbcb19aa), Dear ImGui v1.92.9b-docking, doctest v2.5.3, mlugg/setup-zig v2.2.1 (SHA pinned)]
  patterns: [std-only core with explicit ComputeParams, -Werror strict flags on core and tests only, native-only builds (no cross-target flag), CI runs only zig build steps]

key-files:
  created:
    - build.zig
    - build.zig.zon
    - src/core/ef/math.hpp
    - src/core/ef/vec2.hpp
    - src/core/ef/rk.hpp
    - src/core/ef/method.hpp
    - src/core/ef/params.hpp
    - src/core/ef/charge_system.hpp
    - src/core/ef/presets.hpp
    - src/core/ef/presets.cpp
    - tests/test_main.cpp
    - tests/test_field.cpp
    - src/gui/main.cpp
    - .github/workflows/ci.yml
  modified:
    - ElectricField/ElectricField.vcxproj
    - .gitignore

key-decisions:
  - "Copied the validated research-ref skeleton, applying only the enumerated deltas (chargeRadiusWorld, citations, rk comments, extra radius check)"
  - "build.zig core_sources/test_sources hold only the skeleton files; plan 01-03 appends the rest in REF order"
  - "Dormand-Prince dpLow kept as MFC error coefficients for parity; fix/no-fix is a Phase 2 decision"

patterns-established:
  - "Tracer-first: whole vertical slice (deps, build, core, test, GUI) proven before expansion"
  - "Hygiene in two ordered commits: toolset change alone, then untrack + ignore"

requirements-completed: [BUILD-01, BUILD-02, BUILD-03, BUILD-04, BUILD-05, CORE-01, QA-01]

coverage:
  - id: D1
    description: "zig build test -Dgui=false runs the doctest suite (5 cases, 27 assertions) in Debug and ReleaseFast without building SDL3 or ImGui"
    requirement: "CORE-01"
    verification:
      - kind: integration
        ref: "zig build test -Dgui=false --summary all; zig build test -Dgui=false -Doptimize=ReleaseFast --summary all"
        status: pass
  - id: D2
    description: "Analytic point-charge potential/field and dipole superposition at epsilon 1e-12"
    requirement: "QA-01"
    verification:
      - kind: unit
        ref: "tests/test_field.cpp#point charge field and potential match the analytic values; #superposition: dipole on its axis and on the mid-plane"
        status: pass
  - id: D3
    description: "Headless GUI smoke: dummy video driver + software renderer, 2 ImGui frames, presets=9 from the core"
    requirement: "BUILD-02"
    verification:
      - kind: integration
        ref: "zig build smoke --summary all"
        status: pass
  - id: D4
    description: "ElectricField.exe is a GUI-subsystem PE built for x86_64-windows-gnu"
    requirement: "BUILD-01"
    verification:
      - kind: other
        ref: "od PE optional-header Subsystem field == 2; zig env target x86_64-windows-gnu"
        status: pass
  - id: D5
    description: "No MSVC artifact tracked; git status clean after zig build"
    requirement: "BUILD-05"
    verification:
      - kind: other
        ref: "git ls-files gates + git status --porcelain after zig build"
        status: pass
  - id: D6
    description: "ci.yml encodes the 3-OS gate (fail-fast false, SHA-pinned setup-zig, contents: read); job body green locally"
    requirement: "BUILD-04"
    verification:
      - kind: other
        ref: "grep/awk structural check prints 'ci.yml OK'; five CI zig build commands exit 0 on Windows"
        status: pass
  - id: D7
    description: "Interactive window (zig build run) shows ImGui panel and opens no console window"
    verification:
      - kind: manual_procedural
        ref: "zig build run on Windows (human-check, deferred to end-of-phase per human_verify_mode)"
        status: unknown
---

# Phase 1 Plan 01: Walking Skeleton Summary

**Zig 0.17 build graph with hash-pinned SDL3/ImGui/doctest, a std-only efcore with analytic doctests, a headless-smokeable SDL3 + ImGui hello window, MSVC artifacts untracked, and a 3-OS CI workflow definition (not pushed).**

## Performance

- **Tasks:** 3 of 3
- **Commits:** 4 (3 task commits, one of which is the toolset commit + untrack commit pair for Task 2)
- **Files:** 14 created, 2 modified, 38 untracked from the index

## Accomplishments

- Task 1 (tracer): copied the validated research-ref skeleton, applied the enumerated deltas. `zig build test -Dgui=false` passes (5 cases / 27 assertions), `zig build` produces `zig-out/bin/ElectricField.exe` (PE subsystem 2 = Windows GUI, target x86_64-windows-gnu), `zig build smoke` prints `video=dummy renderer=software presets=9` and `smoke ok frames=2`. Tracer verified end-to-end before expansion.
- Task 2: v145 toolset change landed alone (4 `PlatformToolset` lines), then 38 index entries (37 under `ElectricField/x64/` plus `ElectricField.vcxproj.user`) were untracked with `.gitignore` rewritten. `git status` is clean after `zig build`; no MFC source touched.
- Task 3: `ci.yml` copied verbatim; structural check prints `ci.yml OK`; the five CI zig build commands all pass locally, including the ReleaseFast core run.

## Task Commits

1. **Task 1: walking skeleton** - `ae1033e` (feat)
2. **Task 2a: v145 toolset** - `e21ef49` (chore)
3. **Task 2b: untrack MSVC outputs + .gitignore** - `d4ba5b6` (chore)
4. **Task 3: 3-OS CI workflow** - `ec46365` (ci)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] ImGui writes imgui.ini into the repo root during smoke/run**
- **Found during:** Task 1 (after `zig build smoke`)
- **Issue:** ImGui persists window state to `imgui.ini` in the working directory; the file showed up as untracked and would break the "git status stays clean" intent (BUILD-05) after a smoke or run.
- **Fix:** Deleted the generated file and appended an `imgui.ini` rule to the otherwise verbatim `.gitignore` in the untrack commit. `src/gui/main.cpp` stays verbatim; the real per-user settings path (SDL pref path) is a Phase 3 concern.
- **Files modified:** `.gitignore`
- **Commit:** `d4ba5b6`

The build.zig, params.hpp, charge_system.hpp, rk.hpp and test_field.cpp edits in Task 1 are the plan's enumerated deltas, not deviations.

## Authentication Gates

None.

## Known Stubs

None. The hello window text is intentional placeholder UI for the skeleton; Phase 3 replaces it.

## Threat Flags

None. No new network endpoints, auth paths or trust-boundary file access beyond the plan's threat model.

## Issues Encountered

- The cold dependency fetch was already cached on this machine from the research phase, so the first full build was fast; a cold-cache timing was not observed here.
- Pending: the `zig build run` interactive window check (human-check in Task 1) is deferred to the end-of-phase human verification (`human_verify_mode: end-of-phase`).
- Nothing was pushed (D-12 consent lives in plan 01-04); `git ls-remote --heads origin feature/cross-platform` lists 0 heads.

## Self-Check: PASSED

- Files present: build.zig, build.zig.zon, src/core/ef/*.hpp, presets.cpp, tests/test_main.cpp, tests/test_field.cpp, src/gui/main.cpp, .github/workflows/ci.yml, .gitignore
- Commits present: ae1033e, e21ef49, d4ba5b6, ec46365
