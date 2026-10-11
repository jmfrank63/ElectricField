---
phase: 03-gui-shell
plan: 01
subsystem: ui
tags: [cpp17, zig-build, doctest, imgui, sdl3, tab-model, threadsanitizer]
status: complete

requires:
  - phase: 02-compute-engine
    provides: ef::ComputeSession, ef::buildScene, ef::presets(), efrender PNG export, ef-cli parity oracle
provides:
  - efapp static library (std-only): Workspace tab model over ISession/ComputeSession, pure UI decisions, FramePacer
  - efrender static library with renderSurface and the ImGui context rule (renderPng delegates to it)
  - non-interactive IM_ASSERT (ef_imconfig.h, exit 3) for every imgui.h translation unit
  - test-render step (4 headless context regression tests) wired into zig build test
  - scripted-session and FramePacer doctests (RESEARCH Wave 0 gaps closed, mutation-proven)
affects: [03-02 GUI window, 03-03 CI]

actuals:
  tokens: 12835
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "ISession seam: Workspace talks to a minimal session interface so failed/cancelled/no-result paths are testable with scripted fakes"
    - "cancel-all-then-join: closeAll() requests cancel on every session before the first destroy"
    - "ImGui context rule: renderSurface creates its own context and explicitly restores the caller's"
    - "Harness declared before Workspace in tests so fake-session destructors can log into it"

key-files:
  created:
    - src/app/workspace.hpp
    - src/app/workspace.cpp
    - src/app/ui_logic.hpp
    - src/app/ui_logic.cpp
    - src/imgui_config/ef_imconfig.h
    - tests/test_app.cpp
    - tests_gui/test_main.cpp
    - tests_gui/test_render_ctx.cpp
  modified:
    - src/render/png_export.hpp
    - src/render/png_export.cpp
    - build.zig

key-decisions:
  - "Copied the working prototype byte-for-byte (10 files verified with cmp); only build.zig was hand-merged"
  - "Left the hello-window addGui() in place (plus one include path line); the full GUI lands in plan 03-02"
  - "Dev-only probe-<name> build helper and probes/ shots2/ directories not brought into the product tree"

requirements-completed: [VIEW-01, VIEW-02, VIEW-03, VIEW-04]

coverage:
  - id: D1
    description: "Workspace opens each preset as an independent tab with never-reused per-preset numbers and serials, starting a session at once"
    requirement: VIEW-01
    verification:
      - kind: unit
        ref: "tests/test_app.cpp#tab titles use a per-preset counter that is never reused"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#workspace: two tabs of one preset stay independent and numbers keep rising after closes"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#workspace: an empty workspace is inert"
        status: pass
  - id: D2
    description: "Closing a computing tab or all tabs cancels before destroying, returns promptly and leaves no session"
    requirement: VIEW-02
    verification:
      - kind: unit
        ref: "tests/test_app.cpp#closing a computing tab and closeAll() return promptly and leave no session"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#workspace: close cancels before destroying and closeAll cancels every session first"
        status: pass
      - kind: other
        ref: "WSL Ubuntu: zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe (78/78, no race)"
        status: pass
  - id: D3
    description: "Offscreen render path is safe inside a live ImGui context (no assert, caller state untouched, same pixels)"
    requirement: VIEW-03
    verification:
      - kind: unit
        ref: "zig build test-render (tests_gui/test_render_ctx.cpp, 4 cases)"
        status: pass
      - kind: other
        ref: "mutant: SetCurrentContext line removed -> test-render exits 1 with 'Already initialized a renderer backend'"
        status: pass
  - id: D4
    description: "Busy and failed states: busy glyph, tab label, overlay text, failure text, and every Workspace failure branch"
    requirement: VIEW-04
    verification:
      - kind: unit
        ref: "tests/test_app.cpp#workspace: a failed session keeps its error, an empty one reads Unknown error"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#workspace: cancelled, no result and done end in the states the UI-SPEC fixes"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#workspace: a factory that throws or returns null gives a Failed tab, never an exception"
        status: pass
  - id: D5
    description: "FramePacer blocks only when idle with no settle frames; modal arms 12 frames; smaller requests never lower the pending count"
    requirement: VIEW-04
    verification:
      - kind: unit
        ref: "tests/test_app.cpp#frame pacer: blocks only when idle with no settle frames left"
        status: pass
      - kind: unit
        ref: "tests/test_app.cpp#frame pacer: a modal arms 12 frames and a smaller request never lowers a pending larger one"
        status: pass

duration: 40min
completed: 2026-10-11
---

# Phase 3 Plan 01: efapp tab model and render-context rule Summary

**GUI-free tab model (Workspace over an ISession seam on ef::ComputeSession), pure UI logic with FramePacer, and the ImGui-context-safe offscreen render path copied from the working prototype, wired through build.zig and covered by 78 core/app doctests plus 4 headless render tests, TSan-clean on Linux.**

## Performance

- **Duration:** about 40 min
- **Completed:** 2026-10-11
- **Tasks:** 2 (both committed)
- **Files:** 11 (8 created, 3 modified)

## Accomplishments

- `src/app` (efapp) is std-only and built with the strict core flags (-Werror). `zig build test -Dgui=false` runs core plus app tests without fetching SDL3 or Dear ImGui.
- `renderSurface` creates its own ImGui context and restores the caller's with the explicit `ImGui::SetCurrentContext(ctx);` line. `renderPng` delegates to it. The 4 `test-render` cases pass, and the regression test fails when that line is removed.
- `IM_ASSERT` is non-interactive for every imgui.h user: `-DIMGUI_USER_CONFIG="ef_imconfig.h"` is in `gui_flags` and the `src/imgui_config` include path is on imgui, efrender, tests_gui and the GUI exe.
- ef-cli parity is unchanged: all 9 presets match `tools/parity/expected-counts.csv`. The hello smoke still prints `smoke ok frames=2`.
- RESEARCH Wave 0 gaps are closed with 9 new doctests (2 `frame pacer:`, 7 `workspace:`) driven by a scripted fake session.

## Task Commits

1. **Task 1 (tracer): tab model and render-context rule, wired through build.zig** - `ba2e7f7` (feat)
2. **Task 2: FramePacer and scripted-session Workspace tests** - `c0c02d5` (test)

## Verification Evidence

| Gate | Result |
|------|--------|
| `zig build test -Dgui=false` (Debug) | 69 cases (60 core + 9 copied app) pass |
| `zig build test -Dgui=false -Doptimize=ReleaseFast` | pass |
| `zig build test-render` | 4 cases, 43 assertions pass |
| `zig build test` (full) | 69 + 4 pass, 17/17 steps |
| `zig build -Doptimize=ReleaseFast` | 16/16 steps |
| `zig build smoke -Doptimize=ReleaseFast` | `smoke ok frames=2` |
| `ef-cli --all --expect tools/parity/expected-counts.csv` | exit 0, 9/9 presets match |
| After Task 2: `zig build test -Dgui=false` (Debug and ReleaseFast) | 78 cases pass |
| WSL Ubuntu TSan: `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` on committed HEAD | 78/78 pass, no race report |
| `git diff --name-only 8f8935a -- src/core tools/parity docs/baseline ElectricField` | empty |

### Mutation proofs (the new guards are real)

- **Context rule (Task 1):** with `ImGui::SetCurrentContext(ctx);` removed from `src/render/png_export.cpp`, `zig build test-render` exited 1 and `zig-out/check/ctx-mutant.log` contained `Already initialized a renderer backend`. The file was restored from the prototype (`cmp` identical) and `test-render` passed again.
- **Wave 0 (a):** `FramePacer::shouldBlock` made to ignore the settle counter (`return !anyComputing;`) failed the case `frame pacer: blocks only when idle with no settle frames left`.
- **Wave 0 (b):** the `cancelAll();` call removed from the start of `Workspace::closeAll` failed the case `workspace: close cancels before destroying and closeAll cancels every session first` (`REQUIRE( h.log.size() == 6 )`).
- Both Wave 0 mutations were restored with `git checkout -- src/app`, and `git diff --quiet -- src/app` succeeded afterwards.

## Decisions Made

- Copy, not redesign: the 10 prototype source and test files match research-ref byte for byte. Only build.zig was merged by hand, taking the library and test parts of the prototype build.zig.
- `addGui` keeps its hello-window signature and body plus one added line, `mod.addIncludePath(b.path("src/imgui_config"));`, so the D-16 smoke keeps working until plan 03-02 replaces it.
- The prototype `probe-<name>` helper, `probes/` and `shots2/` stay out of the tree. The only `addProbe` text left in build.zig is zero (verified).

## Deviations from Plan

None - plan executed exactly as written. The new test file section contains 9 cases (plan minimum 8) because the ordering/adjacency edge from the VIEW-01 truths got its own case.

## Issues Encountered

- An early scripting-tool probe (`python -` reading stdin) hung and was left running as a background task. I identified the two orphan `python.exe` processes by their command line and creation time, stopped only those, and confirmed none remained. It had no effect on the repository.

## Known Stubs

None. No placeholder data or unwired components were added; all files are copies of the working prototype plus tests.

## Threat Flags

None. No new network endpoints, auth paths or file access were introduced; the new surface (ImGui context handling, IM_ASSERT config, tab lifetime) is covered by T-03-01 to T-03-04 in the plan.

## Next Phase Readiness

Plan 03-02 can link `efapp` and `efrender` into the real GUI (`src/gui/app,ui,icon`, build-info header, Windows resource) and replace the hello-window `addGui`. The `ISession` seam and `setFactory` are ready for the smoke test's failed-tab check.

## Self-Check: PASSED

- Files: all 11 listed key files exist (`git status` clean for them after the commits).
- Commits: `ba2e7f7` and `c0c02d5` present in `git log`.
- Prohibition: `src/core`, `tools/parity`, `docs/baseline` and `ElectricField` unchanged since `8f8935a`.
