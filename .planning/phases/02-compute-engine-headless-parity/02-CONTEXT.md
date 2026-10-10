# Phase 2: Compute Engine & Headless Parity - Context

**Gathered:** 2026-10-10
**Status:** Ready for planning
**Mode:** autonomous smart discuss (all four areas: user accepted recommended answers)

<domain>
## Phase Boundary

Delivers the GUI-independent compute and rendering pipeline on top of the Phase 1 core: a cancellable multi-threaded `ComputeSession` that traces all field lines and equipotentials of a configuration, the equipotential de-duplication and Bezier smoothing ported from the MFC document/FieldLine code into the core, a toolkit-free scene + painter layer with SVG and PNG output, and a headless `ef-cli` that renders any of the 9 presets — run in CI on windows, ubuntu and macos-arm64 as a parity check, plus a Linux TSan job. Also folds in the Phase 1 code-review hardening items.

Not in this phase: the interactive GUI (Phase 3), options dialog / config persistence / GUI export / MFC deletion (Phase 4), progressive drawing in the GUI (v2).

</domain>

<decisions>
## Implementation Decisions

### Compute engine & threading
- **D-01:** RAII `ComputeSession` per configuration: owns its worker pool (thread count from `ComputeParams`), a condition-variable job queue, and joins all workers in its destructor. No detached threads, no `delete this`, no hand-off of calculators to an app object (replaces MFC `FieldLinesCalculator` lifetime hacks). `std::thread` + `std::mutex`/`std::condition_variable` + `std::atomic` (no `std::jthread`).
- **D-02:** Cooperative cancellation: an atomic stop flag checked inside the integration loop every ~4096 steps (and between jobs); `cancel()` + destructor must return within ~200 ms for every preset/method.
- **D-03:** Result hand-off via `poll()` drain-and-swap of completed lines into a caller-owned snapshot (UI thread never touches worker-owned containers). Parity display = everything once the session reports done; `poll()` makes v2 progressive drawing nearly free.
- **D-04:** Deterministic output: completed lines are ordered by job sequence number, so 1-thread and N-thread runs produce equivalent results (tolerance-based comparison).

### Equipotentials & numerics parity
- **D-05:** Port the MFC equipotential de-duplication (`CElectricFieldDoc::GetDataFromThreads`, `ElectricField/ElectricFieldDoc.cpp:155+`) verbatim into a core `Finalizer` in world units: same sort-by-potential, same `interval/3` grouping rule, same reference point (-7, 5.5), same "two charges of different sign → no duplicates" shortcut. Tests on Quadrupole and Parallel dipoles.
- **D-06:** Keep the Dormand-Prince "low" weight quirk for parity (Phase 1 characterisation test stays). Record "correct DP error weights + re-baseline" as a v2 item.
- **D-07:** Port `FieldLine::AdjustForBezier` into the core as a pure function, guarded against divide-by-zero (coincident points), keeping MFC's `t3` formula for parity (note it in code).
- **D-08:** Keep the pixel/world unit mixing in point thinning (fixed `pixelsPerUnit = 100`) for parity.
- **D-09:** The `FieldLineJob job(job)` self-copy and other CORE-07 defects get regression tests in this phase (the `abs`/`M_PI` fixes from Phase 1 get explicit regression tests if not already present).

### Headless CLI & CI parity
- **D-10:** `ef-cli` executable: `ef-cli --preset <slug> [--svg out.svg] [--png out.png] [--threads N] [--method <name>] [--width W --height H]` (default 1024×768, default options = MFC defaults); `--list` prints preset slugs; prints a one-line summary (charges, field lines, equipotentials, ms).
- **D-11:** One toolkit-free scene/painter layer: `FieldScene` (data) + a small `Painter` interface (circle fill, cubic Bezier path stroke, polyline, clear) + one shared `renderScene()`. SVG painter writes the file directly; PNG painter uses the same ImGui `ImDrawList` code that the Phase 3 GUI will use, rendered on SDL's `software` renderer with the `dummy` video driver and saved via `SDL_SavePNG` — so CLI and GUI output are the same code path. y-up, centered, 100 px/unit, MFC default colours/thickness.
- **D-12:** CI parity check: on all three OSes, run `ef-cli` for all 9 presets; assert charge count, field-line count and equipotential count per preset within tolerance (and agreement across OSes via committed expected counts with tolerance); upload SVG/PNG as artifacts. No cross-OS pixel equality.
- **D-13:** MFC comparison: a script (`tools/parity/`) renders all 9 presets and compares against `docs/baseline/mfc/*.png`, writing a report (e.g. edge-overlap / fuzzed AE metric). Informational, not a failing CI gate (Direct2D vs ImGui anti-aliasing differs).

### Phase 1 code-review carry-over (see `.planning/phases/01-foundation-portable-physics-core/01-REVIEW.md`)
- **D-14:** WR-01: make `sdl3` and `imgui` lazy dependencies (`.lazy = true` + `b.lazyDependency`) so `-Dgui=false` core gates never fetch the GUI stack (ef-cli PNG path needs them, so it is part of the GUI-enabled build or its own option).
- **D-15:** WR-03: `methodFromName` / name lookup returning `std::optional`, bounds-checked `info()`, no silent fallback to Euler in `withSolver`.
- **D-16:** WR-06: `SolveStep` and `SolveStepPair` share one stage computation (parity re-verified by existing tests).
- **D-17:** WR-05: preset cross-check test fails (not skips) when `ElectricField/ConfigDocs.cpp` is unreadable (until Phase 4 deletes MFC; then the test switches to a committed snapshot); parser state reset per document; emptiness guard.
- **D-18:** WR-04: capture script checks `PrintWindow` result, counts dark pixels only at alpha 255, validates `-Exe` path.
- **D-19:** WR-02 (GUI busy loop) is deferred to Phase 3 where the real loop is written. IN-* info items at Claude's discretion.
- **D-20:** Add a Linux CI job running the core tests and a session/cancel stress test with `-Dsanitize=thread` (TSan); must report no data races.

### Claude's Discretion
- Exact class/file names inside `src/core/ef/` and the new scene/painter/CLI modules; whether the PNG painter lives in a separate `efrender` library; expected-count tolerance values; MFC comparison metric details; how the `-Dsanitize=thread` option is wired in build.zig.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/core/ef/`: `ChargeSystem`, `ComputeParams`, `rk.hpp` (11 methods + `withSolver`), `seeds.hpp` (`planSeeds`, `expectedLineCount`), `tracer.hpp` (`traceFieldLine<Solver>` single-line tracer, `FieldLine`), `presets.hpp` (9 presets, `makeSystem`).
- `src/gui/main.cpp`: working SDL3 + ImGui + sdlrenderer3 setup incl. headless `dummy`/`software` path (`--smoke`).
- `build.zig`: `efcore` static lib, doctest test exe, GUI exe, `smoke` step, `-Dgui` option.
- `.github/workflows/ci.yml`: 3-OS matrix running core gates, full build, tests and smoke.
- `docs/baseline/mfc/*.png`: MFC reference images (1024×768, default options).

### Established Patterns
- MFC numerics are the source of truth; parity quirks are kept and pinned by characterisation tests; tests are tolerance/invariant-based.
- Core is std-only, no globals, strict warnings as errors, `-ffp-contract=off`.

### Integration Points
- MFC references: `ElectricField/FieldLinesCalculator.cpp` (job queue, `CalcThread`, equipotential job posting, `FieldLineJob job(job)`), `ElectricField/ElectricFieldDoc.cpp` (`GetData`, `GetDataFromThreads` de-dup, `AdjustForBezier` threads), `ElectricField/FieldLine.cpp` (`AdjustForBezier`, Bezier drawing), `ElectricField/ElectricFieldView.cpp` (D2D transform, colours).
- Phase 3 GUI will reuse `ComputeSession`, `FieldScene`, `renderScene()` and the ImDrawList painter.

</code_context>

<specifics>
## Specific Ideas

- Parity first: everything visible must match the MFC baselines in `docs/baseline/mfc/` as closely as the different anti-aliasing allows.

</specifics>

<deferred>
## Deferred Ideas

- Correct Dormand-Prince error weights (+ re-baseline) — v2.
- Progressive drawing in the GUI (VIEW2-02) — v2 (engine supports it via `poll()`).
- GUI busy-loop fix (WR-02) — Phase 3.

</deferred>
