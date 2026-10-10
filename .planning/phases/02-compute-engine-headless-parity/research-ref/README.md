# research-ref (Phase 2) - validated reference implementation, DELETE at phase close-out

Scratch input for the planner/executor. Everything here was built and run with Zig 0.17.0 on Windows 11
(x86_64-windows-gnu) and Ubuntu 24.04 (WSL2, x86_64-linux-gnu) on top of the Phase 1 repo state (commit 192baa8).
It is NOT compiled by the repo's build; the executor copies it into the real tree (same relative paths) and then
removes this directory (Phase 1 precedent: commit d17dd2f).

Layout mirrors the repo root. Files marked (new) do not exist yet, (changed) replace the Phase 1 file.

| File | Status | Role |
|------|--------|------|
| `src/core/ef/params.hpp` | changed | `+ numThreads` |
| `src/core/ef/method.hpp` | changed | D-15: `methodFromName`, bounds-checked `info()`, `withSolver` throws on an invalid enum |
| `src/core/ef/rk.hpp` | changed | D-16: `computeStages()` shared by `SolveStep` and `SolveStepPair` (output bit-identical) |
| `src/core/ef/tracer.hpp` | changed | cancel stride, `traceElectricLine` + `traceEquipotential` + `shouldPostEquipotential` (CORE-07 fix), `PotentialLine`; `traceFieldLine` kept as a wrapper for the Phase 1 tests |
| `src/core/ef/bezier.hpp` | new | guarded `adjustForBezier` (D-07) |
| `src/core/ef/finalizer.{hpp,cpp}` | new | de-dup port (D-05) + `finalize()` |
| `src/core/ef/session.{hpp,cpp}` | new | `ComputeSession` (D-01..D-04) |
| `src/core/ef/scene.{hpp,cpp}`, `svg.{hpp,cpp}` | new | `FieldScene`, `Painter`, `renderScene`, `SvgPainter` (D-11) |
| `src/render/imgui_painter.hpp`, `png_export.{hpp,cpp}` | new | ImGui `ImDrawList` painter + headless PNG (needs SDL3 + ImGui) |
| `src/cli/main.cpp` | new | `ef-cli` (D-10, D-12) |
| `build.zig`, `build.zig.zon` | changed | lazy GUI deps (D-14), `-Dsanitize=thread` (D-20), `ef-cli` |
| `tests/test_{tracer,bezier,finalizer,session,scene}.cpp`, `tests/test_params.hpp` | new | QA-02/03, CORE-04..07 regressions |
| `tests/test_presets.cpp` | changed | D-17 |
| `tools/parity/compare-mfc.sh`, `expected-counts.csv` | new | D-13, D-12 |
| `.github/workflows/ci.yml` | changed | parity step + artifacts, TSan job |
| `measure/measure.cpp`, `measure/lines_release_o2_windows.csv` | research only | measurement harness + raw per-job timing table; do not copy into the repo |

Verified state of this code: 54 doctest cases pass in Debug (41 s), ReleaseFast (5 s) and under ThreadSanitizer
(Debug and ReleaseSafe, WSL Ubuntu 24.04, no race reported); `ef-cli --all` output (SVG md5 and PNG pixels) is
identical on Windows and Linux.

Build the reference as a scratch copy: `git archive HEAD | tar -x -C <tmp>` then overlay this directory, then
`zig build test -Dgui=false`, `zig build -Doptimize=ReleaseFast`, `zig-out/bin/ef-cli --all --out-dir out`.
