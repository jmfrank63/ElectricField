# Phase 1 research reference implementation

Validated scratch implementation produced while researching Phase 1 (2026-10-10). It is **input for the planner and
executor, not shipped code**: copy what is useful into the repo root layout of decision D-01 and delete this folder
once Phase 1 is done.

Validated with Zig 0.17.0 on Windows 11 (x86_64-windows-gnu, Debug/ReleaseSafe/ReleaseFast/ReleaseSmall) and on
Ubuntu 24.04 (WSL2, x86_64-linux-gnu, only Zig installed, no apt packages):

- `zig build test -Dgui=false`  -> 27 doctest cases / 4382 assertions green, zero warnings under `-Werror`
  (`-Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -Wuninitialized -pedantic`)
- `zig build` + `zig build smoke` -> hello window starts headless (SDL `dummy` driver, `software` renderer)
- macOS arm64 was NOT validated (no Mac available): CI is the oracle.

| File | Purpose |
|------|---------|
| `build.zig`, `build.zig.zon` | Build graph (core lib, doctest exe, SDL3+ImGui GUI, `run`/`smoke` steps) and hash-pinned deps |
| `src/core/ef/*.hpp,*.cpp` | Port of Vector2D, RungeKutta (11 methods), Charge/field, seed planning, presets, line tracer |
| `src/gui/main.cpp` | Hello window with `--smoke` |
| `tests/*.cpp` | doctest suite (vector math, analytic field/potential, convergence orders, seeds, presets, MFC cross-check) |
| `ci.yml` | Candidate `.github/workflows/ci.yml` |
| `gitignore.proposed` | Candidate `.gitignore` (verified against `git ls-files` of this repo) |
| `tools/baseline/capture-mfc-baseline.ps1` | QA-05 automation: captures all 9 presets from the MFC build (verified on this machine) |

`tests/test_presets.cpp` reads `ElectricField/ConfigDocs.cpp` from the repo root (build.zig sets the test cwd); the check
is skipped when the file is absent and must be deleted together with `ElectricField/` in Phase 4.
