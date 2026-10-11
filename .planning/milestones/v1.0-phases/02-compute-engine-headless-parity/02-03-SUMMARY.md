---
phase: 02-compute-engine-headless-parity
plan: 03
subsystem: headless-output
tags: [ef-cli, svg, png, imgui-drawlist, sdl-software-renderer, parity-oracle, zig-build]
status: complete

requires:
  - phase: 02-compute-engine-headless-parity
    provides: "ComputeSession, methodFromName, ComputeParams (plan 02-01); lazy GUI deps, -Dgui=false test gate, 52-case suite (plan 02-02)"
provides:
  - "ef::scene layer (Color, DrawStyle MFC defaults, View, FieldScene, Painter, buildScene, renderScene): the single toolkit-free draw-order/transform/cull/closed-figure decision"
  - "ef::SvgPainter / renderSvg and efrender::ImGuiPainter (the only ImDrawList mapping, reused by Phase 3)"
  - "efrender::renderPng: ImGui on SDL software renderer over an offscreen surface, no window, no video subsystem, SDL_SavePNG"
  - "ef-cli console exe: --list, --preset, --svg, --png, --threads 1..128, --method, --width/--height 16..8192, --all --out-dir, --expect"
  - "tools/parity/expected-counts.csv: proven, boundary-checked per-preset count oracle"
  - "tools/parity/compare-mfc.sh: informational MFC image metrics report (parity-out/mfc-compare.csv), never in CI"
  - "build.zig identical to research-ref (addCli; SVG-only with -Dgui=false)"
affects: [02-04, 03-gui]

actuals:
  tokens: 60000
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns: ["Painter interface with 4 calls; SVG and ImGui back ends only implement it", "renderPng creates and restores its own ImGui context", "fail-first proofs on copies of the expectations file, never on the committed CSV"]

key-files:
  created:
    - src/core/ef/scene.hpp
    - src/core/ef/scene.cpp
    - src/core/ef/svg.hpp
    - src/core/ef/svg.cpp
    - src/render/imgui_painter.hpp
    - src/render/png_export.hpp
    - src/render/png_export.cpp
    - src/cli/main.cpp
    - tests/test_scene.cpp
    - tools/parity/expected-counts.csv
    - tools/parity/compare-mfc.sh
  modified:
    - build.zig
    - .gitignore

key-decisions:
  - "All sources copied verbatim from the research-ref overlay; the only delta is compare-mfc.sh writing parity-out/mfc-compare.csv through tee (D-13)"
  - "PNG uses AntiAliasedLinesUseTex=false and CurveTessellationTol=0.3 on the software renderer; ink recall 0.988-0.993 against the MFC baselines"
  - "Phase 3/4 must call efrender::renderPng between frames on the UI thread (its ImGui context swap is not thread safe, Pitfall 9)"

requirements-completed: [QA-04, CORE-04, CORE-05]

duration: 20min
completed: 2026-10-10
---

# Phase 2 Plan 03: Headless SVG/PNG ef-cli and parity oracle Summary

**ef-cli renders all 9 presets to SVG and to PNG (ImGui ImDrawList over SDL's offscreen software renderer, no display) through one shared scene/Painter layer, with committed expected counts proven by a tolerance-boundary fail-first test and identical on Windows and Linux.**

## Performance

- Duration: about 20 min (dominated by the cold WSL Linux build)
- Tasks: 2 of 2
- Commits: cc2a4a9 (Task 1), 8caf46b (Task 2)

## Task Commits

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 (tracer) | ef-cli renders presets to SVG and PNG headlessly | cc2a4a9 | build.zig, src/cli/main.cpp, src/core/ef/scene.{hpp,cpp}, src/core/ef/svg.{hpp,cpp}, src/render/imgui_painter.hpp, src/render/png_export.{hpp,cpp}, tests/test_scene.cpp |
| 2 | Parity expectations and MFC comparison script | 8caf46b | tools/parity/expected-counts.csv, tools/parity/compare-mfc.sh (100755), .gitignore |

Tracer gate: the Task 1 end-to-end slice (dipole to SVG and PNG, expected counts) passed before Task 2 expanded on it.

## Verification Results

### Task 1
- `build.zig` equals research-ref (`diff --strip-trailing-cr` empty).
- `ef-cli --preset dipole --svg --png`: `preset=dipole charges=2 field_lines=16 equipotentials=75 ms=60`, exit 0.
- PNG is 1024 x 768 with PNG magic bytes; SVG has `viewBox="0 0 1024 768"`, 2 circles, black strokes.
- `addCli` has no `subsystem` (console exe); std-only gate on src/core empty.
- `zig build test -Dgui=false` green in Debug and ReleaseFast (test_scene included, 5 cases).

### Task 2: tolerance boundary (committed CSV untouched, copies under zig-out/check)

| Variation | Result |
|-----------|--------|
| dipole equipotentials 77 (diff 2 = max(2, 3%)) | exit 0 |
| dipole equipotentials 78 | exit 1, `PARITY FAIL dipole: ... expected 2/16/78 (equipotential tol 2)` |
| dipole field_lines 17 | exit 1, `PARITY FAIL dipole` |
| file without parallel-dipoles row | exit 1, `PARITY FAIL: expected file covers 8 of 9 presets` |

### Task 2: CLI exit codes

| Invocation | Exit |
|------------|------|
| no arguments | 2 |
| `--preset nope` | 2 |
| `--threads 0` / `--threads 129` | 2 / 2 |
| `--width 15` / `--height 8193` | 2 / 2 |
| `--method nope` | 1 |
| `--threads 1` / `--threads 128` | 0 / 0, both 2/16/75 like the default run |
| `--list` | 9 slugs, dipole first, parallel-dipoles last, equals CSV row order |

### SVG-only build (-Dgui=false)
`--svg` exit 0; `--png` exit 1 with `ef-cli: this ef-cli was built without PNG support (-Dgui=false)`; no PNG written.

### Windows vs Linux (WSL Ubuntu 24.04, DISPLAY, WAYLAND_DISPLAY and XDG_RUNTIME_DIR unset)
`--all --expect` exit 0 on both, `files=18` on Linux, and the 9 summary lines with ms removed are identical (all counts agree, matching the RESEARCH measurements):

| preset | charges | field_lines | equipotentials |
|--------|---------|-------------|----------------|
| dipole | 2 | 16 | 75 |
| two-equal-charges | 2 | 32 | 74 |
| two-unequal-charges-same-sign | 2 | 48 | 113 |
| two-unequal-charges-different-signs | 2 | 32 | 113 |
| quadrupole | 4 | 32 | 142 |
| four-charges-same-sign | 4 | 64 | 128 |
| two-dipoles-same-direction | 4 | 32 | 138 |
| two-dipoles-opposed-direction | 4 | 32 | 130 |
| parallel-dipoles | 4 | 32 | 136 |

### MFC comparison (informational, D-13; RESEARCH reference recall 0.988-0.993, precision 0.995-0.997)

| preset | ae_fuzz10 | ink_recall | ink_precision | darkness_ratio |
|--------|-----------|------------|---------------|----------------|
| dipole | 6375.07 | 0.988 | 0.995 | 0.997 |
| two-equal-charges | 8055.65 | 0.990 | 0.995 | 1.009 |
| two-unequal-charges-same-sign | 11308.4 | 0.991 | 0.995 | 0.996 |
| two-unequal-charges-different-signs | 8984.22 | 0.991 | 0.996 | 1.005 |
| quadrupole | 8183.69 | 0.992 | 0.996 | 0.998 |
| four-charges-same-sign | 14749.4 | 0.992 | 0.997 | 0.999 |
| two-dipoles-same-direction | 7041.98 | 0.993 | 0.996 | 1.002 |
| two-dipoles-opposed-direction | 8824.83 | 0.992 | 0.996 | 0.998 |
| parallel-dipoles | 9908.31 | 0.992 | 0.996 | 1.004 |

All recalls are at least 0.98 (texture AA would give about 0.95). A side-by-side look at parallel-dipoles (MFC baseline vs port PNG) shows the same geometry, colours (black lines, grey equipotentials, blue and red charges) and scale; the port's thin lines are slightly more aliased than Direct2D's. The formal human side-by-side remains deferred to end-of-phase verification per config.

## Deviations from Plan

None - plan executed exactly as written. (The one scripted delta to compare-mfc.sh, the tee-to-report change, was prescribed by the plan; a first sed attempt missed the printf row and was completed with a direct edit before the commit.)

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. New surface (CLI file output, expected-counts parse, ImGui context swap in renderPng) is covered by T-02-07 to T-02-12; `grep -cE 'system\(|popen\(|exec[lv]p?\(' src/cli/main.cpp` outputs 0; `grep -rn compare-mfc .github/` prints nothing.

## Notes for Later Plans

- Plan 02-04 can wire CI around `ef-cli --all --expect tools/parity/expected-counts.csv`. macOS arm64 counts are still unverified until the first CI run (RESEARCH A1); if outside tolerance, adjust the tolerance or re-baseline from the artifact, never skip.
- The working copy on Windows converts to CRLF (core.autocrlf=true); the committed blobs are LF, so `git archive` on Linux yields a runnable compare-mfc.sh.
- Phase 3/4 must call `efrender::renderPng` between frames on the UI thread (it swaps the global ImGui context).

## Self-Check: PASSED

- Files present: scene.hpp/cpp, svg.hpp/cpp, imgui_painter.hpp, png_export.hpp/cpp, cli/main.cpp, test_scene.cpp, expected-counts.csv, compare-mfc.sh (checked via git show of both commits).
- Commits cc2a4a9 and 8caf46b exist on feature/cross-platform.
