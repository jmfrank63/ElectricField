---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 3
current_phase_name: GUI Shell
status: planning
stopped_at: Completed 02-04-PLAN.md
last_updated: "2026-10-10T14:33:01.222Z"
last_activity: 2026-10-10
last_activity_desc: Phase 2 complete, transitioned to Phase 3
progress:
  total_phases: 2
  completed_phases: 2
  total_plans: 8
  completed_plans: 8
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-10-10 after Phase 2)

**Core value:** The physics and visual output must stay correct and recognisable: the same charge configurations produce the same field lines and equipotentials, now buildable and runnable on Windows, Linux and macOS without Visual Studio.
**Current focus:** Phase 03 — GUI Shell (Dear ImGui + SDL3)

## Current Position

Phase: 3 — GUI Shell
Plan: Not started
Status: Ready to plan
Last activity: 2026-10-10 — Phase 2 complete, transitioned to Phase 3

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 4
- Average duration: - min
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 2 | 4 | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 25min | 3 tasks | 16 files |
| Phase 01 P02 | 25min | 2 tasks | 11 files |
| Phase 01 P03 | 12min | 3 tasks | 8 files |
| Phase 01 P04 | 60min | 3 tasks | 24 files |
| Phase 02 P01 | 8min | 2 tasks | 11 files |
| Phase 02 P02 | 25min | 2 tasks | 8 files |
| Phase 02 P03 | 20min | 2 tasks | 13 files |
| Phase 02 P04 | 40min | 2 tasks | 3 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Research]: Stack = Zig 0.17.0 + Dear ImGui 1.92.9b (docking) + SDL3 via castholm/SDL + sdlrenderer3 + doctest 2.5.3; Windows target x86_64-windows-gnu
- [Research]: Fallback if the Phase 1 CI gate cannot go green on all 3 OSes = Qt 6.12 LTS Widgets + CMake (core stays build-system-neutral)
- [Roadmap]: Coarse granularity - 8 researched phases merged into 4; Phases 1-2 are GUI-independent
- [Roadmap]: MFC sources stay in the tree until Phase 4 so the QA-05 baseline (Phase 1) comes from the real app
- [Roadmap]: Printing is replaced by PNG/SVG export; progressive display is a v2 item (not existing behaviour)
- [Phase ?]: 01-01: Added imgui.ini to .gitignore (ImGui writes it to cwd on smoke/run); main.cpp left verbatim
- [Phase ?]: [Phase 01 P02]: MFC baseline recorded with MSVC 14.51.36231 / v145, 96 DPI; compare against docs/baseline/mfc with fuzz 10%, never byte equality
- [Phase ?]: [01-03] DP error weights: keep vs fix is an open Phase 2 decision; MFC behaviour kept and pinned by characterisation test
- [Phase ?]: [Phase 01 P04] D-12 gate GO: Zig 0.17 + SDL3 + ImGui green on windows/linux/macos-arm64 on first CI run (run 38040977802, head d17dd2f); Qt 6 + CMake fallback not triggered
- [Phase ?]: [Phase 02 P01] Engine from research-ref overlay; kCancelStride=1024; latency budget 200 ms (1000 ms only under TSan); research-ref rk.hpp (D-16) left for a later plan
- [Phase ?]: [Phase 02 P02] Core test gates run -Dgui=false with lazy SDL3/ImGui; TSan verified on WSL Ubuntu for the whole 52-case suite; Dormand-Prince low weights and other MFC quirks stay pinned
- [Phase ?]: [Phase 02 P03] ef-cli PNG = ImGui ImDrawList on SDL software renderer (offscreen surface, AA texture off, tessellation 0.3); ink recall 0.988-0.993 vs MFC; Windows and Linux counts identical; renderPng must run on the UI thread between frames
- [Phase ?]: [Phase 02 P04] CI gate green on windows/linux/macos-arm64/tsan-linux (runs 38054538292, 38055102426); macOS arm64 parity counts identical to Windows/Linux, so A1 holds and the tolerance is unchanged

- [Phase 2]: Verified (human-approved MFC side-by-side); code review 3 iterations, 11/11 warnings fixed; final CI run 38058493194 green incl. tsan-linux

### Pending Todos

None yet.

### Blockers/Concerns

- [Phase 1] RESOLVED 2026-10-10: Linux and macOS execution of the Zig + SDL3 + ImGui stack verified by CI run 38040977802 (head d17dd2f, windows/linux/macos-arm64 all green, smoke presets=9); D-12 gate = GO
- [Phase 1] RESOLVED: QA-05 MFC baseline captured (docs/baseline/mfc, 9 PNGs)
- [Phase 1]: Zig 0.17 build API churn (C/C++ calls live on Module); start from `zig init` of the installed version
- [Phase 2] RESOLVED: `FieldLineJob job(job)` reproduced under MSVC (binds outer parameter); fixed as `equipJob` with a fail-first regression test
- [Phase 3]: ImGui thin-line AA slightly softer/fainter than Direct2D (ink recall 0.988-0.993, user-approved); plutovg behind the Painter remains an option if export quality needs it
- [Phase 3]: Phase 1 review WR-02 (GUI busy loop) and Phase 2 IN-03 must be fixed in the real GUI loop (vsync / SDL_WaitEventTimeout); renderPng must run on the UI thread between frames
- [Phase 4]: Phase 2 review IN-02/IN-04/IN-05/IN-06 open (compare-mfc.sh robustness, CI double runs/cancel-in-progress on master, Painter contract asserts, UTF-8 paths on Windows)

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-10-10T13:25:20.560Z
Stopped at: Phase 2 complete, ready to plan Phase 3
Resume file: None

## Deferred Verification

| Phase | State | Resume |
|-------|-------|--------|
| 01 | verification_deferred_human | /gsd-verify-work 01 |
