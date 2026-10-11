# Milestones

## v2.0 v2.0 (Shipped: 2026-10-11)

**Phases completed:** 2 phases, 4 plans

**Key accomplishments:**

- User charge configurations: named charge sets in a human-readable `configurations.ini` (skip-and-count parser, case-insensitive dedupe, preset names reserved), Save As with overwrite confirmation, chooser integration and an "Empty Configuration" starting point.
- The charge editor: click-select with ring, mouse drag, arrow-key nudging (Shift = larger), double-click to add, Delete with the one-charge invariant, and an editor panel with value/x/y fields and press-and-hold -/+ buttons that move charges in realtime without recomputing.
- Explicit calculation: Calculate (F5) and a per-tab integration-method selector, with a stale marker on edited tabs until recomputation; `Workspace::calculate` reuses the session factory and Failed-tab paths.
- Verified: 102 doctest cases + full headless smoke incl. the editor loop through the injected input queue (`editor=ok`), CI green on windows-x64, linux-x64, macos-arm64 and tsan-linux; human-approved walkthroughs for both phases.

---

## v1.0 v1.0 (Shipped: 2026-10-11)

**Phases completed:** 4 phases, 14 plans, 26 tasks

**Key accomplishments:**

- Zig 0.17 build graph with hash-pinned SDL3/ImGui/doctest, a std-only efcore with analytic doctests, a headless-smokeable SDL3 + ImGui hello window, MSVC artifacts untracked, and a 3-OS CI workflow definition (not pushed).
- Nine 1024x768 reference PNGs of the unmodified MFC app (Release|x64, v145, default options) plus a re-runnable PowerShell capture script that drives the app via Win32 messages and PrintWindow, with HKCU options backup/restore.
- Seed planning and single-line tracing ported from the MFC source so all 11 Runge-Kutta methods produce a dipole field line, plus tolerance-based oracles for vector math, convergence order, adaptive step control and a regex cross-check of the 9 presets against ElectricField/ConfigDocs.cpp.
- Phase 1 go/no-go gate passed: the pushed head d17dd2f built, tested (Debug+UBSan and ReleaseFast core suites) and ran the headless SDL3+ImGui smoke start-up green on windows-x64, linux-x64 and macos-arm64 on the first CI run, with Zig 0.17.0 installed by mlugg/setup-zig on an empty cache.
- Cancellable N-thread `ef::ComputeSession` (condition-variable pool, JobKey-ordered deterministic results, verbatim MFC equipotential de-dup, guarded Bezier fit) with a 38-case doctest suite proving 1-vs-7-thread equality and cancel/destroy latency of under 8 ms for all 99 preset x method combinations.
- ThreadSanitizer build option with lazy SDL3/ImGui dependencies, a fail-not-skip MFC preset oracle, and regression tests (self-copy proven fail-first) pinning every CORE-07 defect and the de-dup boundary, with the whole 52-case suite race-free on Linux.
- ef-cli renders all 9 presets to SVG and to PNG (ImGui ImDrawList over SDL's offscreen software renderer, no display) through one shared scene/Painter layer, with committed expected counts proven by a tolerance-boundary fail-first test and identical on Windows and Linux.
- CI renders all 9 presets headlessly on windows, ubuntu and macos-arm64 against the committed count oracle, runs the engine under ThreadSanitizer on Linux, and is green on all four jobs for both pushed heads.
- GUI-free tab model (Workspace over an ISession seam on ef::ComputeSession), pure UI logic with FramePacer, and the ImGui-context-safe offscreen render path copied from the working prototype, wired through build.zig and covered by 78 core/app doctests plus 4 headless render tests, TSan-clean on Linux.
- The prototype's SDL3 + Dear ImGui shell (menus, preset chooser, reorderable tabs, busy overlay, About box, HiDPI scaling) is now the real ElectricField exe with the MFC icon and GUI subsystem, a make-time git hash in the About box, and a full UI smoke that passes on Windows and Linux.
- CI now builds the real Dear ImGui + SDL3 GUI on Windows, Linux and macOS arm64, runs the render-context tests and the full scripted UI smoke there (identical canvas, keyboard and quit results on all three), proves the Windows GUI subsystem, uploads headless screenshots of the real UI, and kept the Phase 2 parity and TSan gates green; the research prototype is removed.
- Complete
- Complete
- Complete

---
