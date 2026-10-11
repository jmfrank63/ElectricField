# Phase 2: Compute Engine & Headless Parity - Research

**Researched:** 2026-10-10
**Domain:** C++17 threaded numerics port (MFC job queue -> RAII worker pool), toolkit-free scene/painter layer, SVG + headless PNG output, Zig 0.17 build wiring, 3-OS CI parity, ThreadSanitizer
**Confidence:** HIGH (everything below was read from the MFC/Phase 1 sources or built and run this session; the only unverified item is macOS arm64 execution, see Assumptions Log)

> **Validated reference implementation:** `.planning/phases/02-compute-engine-headless-parity/research-ref/` (see its README).
> Everything tagged `[VERIFIED: prototype]` was compiled and run from that code with Zig 0.17.0 on Windows 11 (x86_64-windows-gnu) and
> Ubuntu 24.04 (WSL2, x86_64-linux-gnu). The planner may tell executors to copy it (same relative paths) instead of re-deriving it,
> and to delete the directory at phase close-out (Phase 1 precedent, commit d17dd2f).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Compute engine & threading**
- **D-01:** RAII `ComputeSession` per configuration: owns its worker pool (thread count from `ComputeParams`), a condition-variable job queue, and joins all workers in its destructor. No detached threads, no `delete this`, no hand-off of calculators to an app object (replaces MFC `FieldLinesCalculator` lifetime hacks). `std::thread` + `std::mutex`/`std::condition_variable` + `std::atomic` (no `std::jthread`).
- **D-02:** Cooperative cancellation: an atomic stop flag checked inside the integration loop every ~4096 steps (and between jobs); `cancel()` + destructor must return within ~200 ms for every preset/method.
- **D-03:** Result hand-off via `poll()` drain-and-swap of completed lines into a caller-owned snapshot (UI thread never touches worker-owned containers). Parity display = everything once the session reports done; `poll()` makes v2 progressive drawing nearly free.
- **D-04:** Deterministic output: completed lines are ordered by job sequence number, so 1-thread and N-thread runs produce equivalent results (tolerance-based comparison).

**Equipotentials & numerics parity**
- **D-05:** Port the MFC equipotential de-duplication (`CElectricFieldDoc::GetDataFromThreads`, `ElectricField/ElectricFieldDoc.cpp:155+`) verbatim into a core `Finalizer` in world units: same sort-by-potential, same `interval/3` grouping rule, same reference point (-7, 5.5), same "two charges of different sign -> no duplicates" shortcut. Tests on Quadrupole and Parallel dipoles.
- **D-06:** Keep the Dormand-Prince "low" weight quirk for parity (Phase 1 characterisation test stays). Record "correct DP error weights + re-baseline" as a v2 item.
- **D-07:** Port `FieldLine::AdjustForBezier` into the core as a pure function, guarded against divide-by-zero (coincident points), keeping MFC's `t3` formula for parity (note it in code).
- **D-08:** Keep the pixel/world unit mixing in point thinning (fixed `pixelsPerUnit = 100`) for parity.
- **D-09:** The `FieldLineJob job(job)` self-copy and other CORE-07 defects get regression tests in this phase (the `abs`/`M_PI` fixes from Phase 1 get explicit regression tests if not already present).

**Headless CLI & CI parity**
- **D-10:** `ef-cli` executable: `ef-cli --preset <slug> [--svg out.svg] [--png out.png] [--threads N] [--method <name>] [--width W --height H]` (default 1024x768, default options = MFC defaults); `--list` prints preset slugs; prints a one-line summary (charges, field lines, equipotentials, ms).
- **D-11:** One toolkit-free scene/painter layer: `FieldScene` (data) + a small `Painter` interface (circle fill, cubic Bezier path stroke, polyline, clear) + one shared `renderScene()`. SVG painter writes the file directly; PNG painter uses the same ImGui `ImDrawList` code that the Phase 3 GUI will use, rendered on SDL's `software` renderer with the `dummy` video driver and saved via `SDL_SavePNG` - so CLI and GUI output are the same code path. y-up, centered, 100 px/unit, MFC default colours/thickness.
- **D-12:** CI parity check: on all three OSes, run `ef-cli` for all 9 presets; assert charge count, field-line count and equipotential count per preset within tolerance (and agreement across OSes via committed expected counts with tolerance); upload SVG/PNG as artifacts. No cross-OS pixel equality.
- **D-13:** MFC comparison: a script (`tools/parity/`) renders all 9 presets and compares against `docs/baseline/mfc/*.png`, writing a report (e.g. edge-overlap / fuzzed AE metric). Informational, not a failing CI gate (Direct2D vs ImGui anti-aliasing differs).

**Phase 1 code-review carry-over** (see `.planning/phases/01-foundation-portable-physics-core/01-REVIEW.md`)
- **D-14:** WR-01: make `sdl3` and `imgui` lazy dependencies (`.lazy = true` + `b.lazyDependency`) so `-Dgui=false` core gates never fetch the GUI stack (ef-cli PNG path needs them, so it is part of the GUI-enabled build or its own option).
- **D-15:** WR-03: `methodFromName` / name lookup returning `std::optional`, bounds-checked `info()`, no silent fallback to Euler in `withSolver`.
- **D-16:** WR-06: `SolveStep` and `SolveStepPair` share one stage computation (parity re-verified by existing tests).
- **D-17:** WR-05: preset cross-check test fails (not skips) when `ElectricField/ConfigDocs.cpp` is unreadable (until Phase 4 deletes MFC; then the test switches to a committed snapshot); parser state reset per document; emptiness guard.
- **D-18:** WR-04: capture script checks `PrintWindow` result, counts dark pixels only at alpha 255, validates `-Exe` path.
- **D-19:** WR-02 (GUI busy loop) is deferred to Phase 3 where the real loop is written. IN-* info items at Claude's discretion.
- **D-20:** Add a Linux CI job running the core tests and a session/cancel stress test with `-Dsanitize=thread` (TSan); must report no data races.

### Claude's Discretion
- Exact class/file names inside `src/core/ef/` and the new scene/painter/CLI modules; whether the PNG painter lives in a separate `efrender` library; expected-count tolerance values; MFC comparison metric details; how the `-Dsanitize=thread` option is wired in build.zig.

### Deferred Ideas (OUT OF SCOPE)
- Correct Dormand-Prince error weights (+ re-baseline) - v2.
- Progressive drawing in the GUI (VIEW2-02) - v2 (engine supports it via `poll()`).
- GUI busy-loop fix (WR-02) - Phase 3.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CORE-04 | Equipotentials at the configured interval incl. de-dup, in the core, world units | "MFC behaviour to reproduce" 2-4; `finalizer.{hpp,cpp}`, `tracer.hpp::traceEquipotential/shouldPostEquipotential`; measured counts table |
| CORE-05 | Configurable worker threads, non-blocking | `ComputeSession` design (condvar pool, in-flight termination, `poll()`); 1-vs-N bit-identical result measured |
| CORE-06 | Cancel within ~200 ms, no crash/leak/detached thread | Cancellation design + measured per-stride latency (Release 4 ms, Debug 45 ms, TSan Debug 73 ms) |
| CORE-07 | Portability defects fixed; warning-clean; `-ffp-contract=off`, no fast-math | MSVC `job(job)` finding, `shouldPostEquipotential`, guarded `adjustForBezier`, regression tests incl. an FMA-contraction canary |
| QA-02 | Line-generation invariants per preset, tolerances not bit-exact, `sign(0)==1` | `test_session.cpp` invariants, `test_finalizer.cpp`, existing `test_seeds.cpp` |
| QA-03 | Session behaviour: 1-vs-N equivalence, cancel latency, TSan on Linux CI | `test_session.cpp`, `-Dsanitize=thread`, `tsan-linux` job (verified in WSL) |
| QA-04 | Headless CLI renders any preset to SVG/PNG, used in CI on 3 OSes | `ef-cli`, `expected-counts.csv`, CI parity step, artifact upload |
</phase_requirements>

## Project Constraints (from .claude/CLAUDE.md)

- C++17 or newer for the physics core, reuse the proven numerics; **Zig 0.17.0**, command-line build only, no MSVC/MFC, no IDE; Windows target `x86_64-windows-gnu` (never `-msvc`); macOS must be built natively (Apple SDK licensing), never pass `-Dtarget` on the macOS runner.
- GUI stack is fixed: Dear ImGui 1.92.9b-docking + SDL3 via `castholm/SDL` (release-3.4.x) + `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3`; **do not use** `allyourcodebase/SDL3`, `imgui_impl_opengl3/win32/dx11`, ImGui multi-viewport, ImPlot. Draw with `ImDrawList`; `style.CurveTessellationTol` lowered to ~0.3 for smooth Beziers; keep `thickness >= 1.0f`.
- Tests: doctest 2.5.3; headless golden-image idea uses SDL software renderer; compare with tolerance, never bit-exact (AA differs).
- Behaviour parity: same default options and configurations must give visually equivalent output; MFC numerics are the source of truth, quirks kept and pinned by characterisation tests; core is std-only, no globals, strict warnings as errors, `-ffp-contract=off`, no fast-math.
- CI: GitHub Actions matrix `ubuntu-latest`, `windows-latest`, `macos-latest` (arm64), `mlugg/setup-zig` pinned.
- GSD workflow: file changes go through a GSD command (this research writes only planning artifacts).

## Summary

Phase 2 is a faithful port plus three new layers. I read the complete MFC compute path and ported it into a validated reference implementation (`research-ref/`): a RAII `ComputeSession` (condition-variable pool, in-flight termination, cooperative cancel, `poll()` drain-and-swap, job-key ordering), the equipotential tracer and the `Finalizer` de-dup port, a guarded `adjustForBezier`, a `FieldScene` + 4-call `Painter` + `renderScene()` with an SVG backend and an ImGui `ImDrawList` backend, and `ef-cli`. All 54 doctest cases pass in Debug, ReleaseFast and under ThreadSanitizer; the 9 presets render to SVG and PNG in 4.1 s total (ReleaseFast, 4 threads).

Three findings change the plan relative to CONTEXT.md/STACK.md assumptions. (1) **`FieldLineJob job(job)` is not UB under MSVC**: the MSVC legacy lookup binds the initializer to the *outer parameter*, so MSVC silently copies the parent job (correct), while Clang/GCC and `cl /permissive-` read an uninitialised object (garbage jobs, `-Wuninitialized`). The intent is `FieldLineJob equipJob(job)`; the regression test must therefore assert "posted job == parent state", which I verified. (2) **ImGui's default texture-based AA lines render jagged on SDL's software renderer** (nearest-neighbour texture sampling); setting `ImGuiStyle::AntiAliasedLinesUseTex = false` (vertex-alpha fringe AA) makes the port match the MFC baselines: ink recall 0.988-0.993, precision 0.995-0.997, total ink within 1% for all 9 presets. Use that setting in the shared painter path. (3) **`-Dgui=false` genuinely avoids the GUI fetch** once the deps are `.lazy = true` (verified with a fresh global cache: only doctest is fetched), and ThreadSanitizer works with Zig 0.17 + Zig's libc++ on Linux (clean run of all tests; a deliberate race is reported with exit code 66).

Output is deterministic and portable: 1-thread and 7-thread runs produce identical points, and the SVGs and PNGs rendered on Windows and on Linux are byte/pixel-identical (`-ffp-contract=off` + Zig toolchain). macOS arm64 is the only unverified platform, so the CI expectations use exact charge/field-line counts and a +-max(2, 3%) equipotential tolerance.

**Primary recommendation:** Copy `research-ref/` into the tree in four waves (core hardening + tracer/bezier; finalizer + session; scene/SVG + CLI + PNG; CI/parity/docs), keep the poll stride at 1024 steps (measured, within the "~4096" intent), use `ImGuiStyle::AntiAliasedLinesUseTex = false` for every ImDrawList consumer, and gate CI on exact counts (not pixels).

## Architectural Responsibility Map

This is a desktop app with no network tiers; "tiers" are the library layers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Field/potential evaluation, RK integration, line tracing, seeds | `efcore` (std-only) | - | pure numerics, no I/O; already Phase 1 |
| Job queue, worker pool, cancel, result ordering | `efcore` (`ComputeSession`) | - | threads via `std::`; GUI/CLI only call `poll()/wait()/cancel()` |
| Equipotential de-dup, Bezier control-point fit | `efcore` (`Finalizer`, `bezier.hpp`) | - | pure functions in world units (moved out of the MFC document) |
| Draw order, y-flip/centre, 100 px/unit, culling, closed figures | `efcore` (`scene.{hpp,cpp}`, `renderScene`) | - | decided once; toolkit-free (std-only) so SVG and tests need no GUI libs |
| SVG output | `efcore` (`svg.{hpp,cpp}`) | CLI writes the file | string building only |
| ImDrawList painter + PNG encode | `efrender` (ImGui + SDL3) | GUI (Phase 3) reuses `imgui_painter.hpp` | needs ImGui/SDL; only built when `-Dgui=true` |
| Argument parsing, summary line, parity assertion | `ef-cli` | CI | headless oracle |
| Cross-OS regression gate | GitHub Actions | `expected-counts.csv` | counts, not pixels |
| MFC visual comparison | `tools/parity` script (ImageMagick) | - | informational, local only |

## Standard Stack

No new third-party dependency is introduced by this phase. Everything is already pinned in `build.zig.zon` (hash-pinned) from Phase 1.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Zig (`zig build`, `zig c++`) | 0.17.0 [VERIFIED: `zig version` = 0.17.0, Windows and WSL] | Build, C++17 compile (Clang 22, libc++), TSan runtime | project constraint (CLAUDE.md) |
| doctest | 2.5.3 [VERIFIED: `build.zig.zon` doctest URL `.../v2.5.3.tar.gz`] | unit tests | already in use |
| Dear ImGui | v1.92.9b-docking [VERIFIED: `build.zig.zon`] | `ImDrawList` painter shared by CLI PNG and Phase 3 GUI | project constraint |
| SDL3 (castholm port) | 0.6.0 + SDL 3.4.18 [VERIFIED: `build.zig.zon` hash string `sdl-0.6.0+3.4.18-SDL--p-gqAGWpnfTLlhs0dgjDtc4P4_CHeNLGY_SX_Fv`] | `SDL_CreateSoftwareRenderer`, `SDL_RenderReadPixels`, `SDL_SavePNG` | project constraint |

### Supporting (tools, not packages)
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| ImageMagick | 7.1.2-32 [VERIFIED: `magick -version`] | `tools/parity/compare-mfc.sh` only | local informational comparison; NOT used in CI |
| WSL Ubuntu | 24.04.4 LTS, kernel 6.18 [VERIFIED] with Zig 0.17.0 in `~/zig17` | TSan / Linux checks on a Windows dev box | developer verification |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| ImGui painter for PNG | plutovg software rasteriser | STACK.md fallback only if ImGui AA were inadequate; with `AntiAliasedLinesUseTex=false` it is adequate (metrics below) - do not add it |
| `SDL_CreateSoftwareRenderer(surface)` | `dummy` video driver + window + software renderer | the surface renderer needs no `SDL_Init`, no window and no env/hints; verified to work with `DISPLAY` unset on Linux. CONTEXT D-11 wording ("dummy video driver") is satisfied in effect |
| `std::thread` pool | `std::jthread`, TBB | locked by D-01 |

**Installation:** nothing to install. `zig build` fetches (hash-pinned, lazily for SDL3/ImGui).

**Version verification:** versions are read from `build.zig.zon` this session, not from memory.

## Package Legitimacy Audit

No new external packages are added in this phase (doctest, ImGui and SDL3 were audited in Phase 1 and are hash-pinned in `build.zig.zon`). The only new third-party references are GitHub Actions: `actions/upload-artifact` v7.0.2 [VERIFIED: `gh api repos/actions/upload-artifact/releases/latest` -> `v7.0.2`, tag commit `cf430e030ddbb5b0abf93d22962f4752f3646cd9`], pinned by commit SHA in the reference `ci.yml`.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none new) | - | - | - | - | - | - |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## MFC compute behaviour to reproduce (source of truth)

All line numbers are in `ElectricField/` and were read this session. Phase 1 already ported seeding (`planSeeds`), the single-line tracer, point thinning and the 11 solvers; the items below are what Phase 2 adds.

### 1. Job model and queue (FieldLinesCalculator.h:19-35, .cpp:153-176, 179-205)
- `FieldLineJob` = `{Charge charge; int total_charge; bool has_different_signs; double angle_start; double angle; Vector2D<double> point; bool isEquipotential=false; double old_potential=0;}` (`FieldLinesCalculator.h:19-31`). `StartCalculating` pushes one E-line job per seed into a `std::deque` under `m_jobsSection` (`.cpp:250`) and starts `numThreads` `CalcThread`s (`.cpp:257-258`).
- `CalcThread::Calculate` (`.cpp:153-176`): loop `for (;!Terminate;)`: lock, `if (Terminate || m_jobs.empty()) break;`, pop front, unlock, `ProcessJob`. On exit: `delete this; ++calc->finishedThreads`. **A thread exits as soon as the queue is empty even if other threads are mid-job and may still post equipotential jobs.**
- `ProcessJob` (`.cpp:179-205`): equipotential job -> `CalculateEquipotential`, then `if (Terminate) return;`, push a `PotentialLine{potential = job.old_potential, weightCenter, points}` under `m_potentialLinesSection`. E-line job -> `functorE.charge_sign = sign(job.charge.charge)`, `CalculateElectricFieldLine`, `if (Terminate) return;`, push a `FieldLine` (**also when empty**: sink seeds record no points) under `m_electricLinesSection`.
- Restart hack (`.cpp:41-47`): after posting, `if (finishedThreads > 0) { --finishedThreads; StartComputingThread(&field); }`. `CheckStatus()` = `finishedThreads >= startedThreads` (`.cpp:277-282`). Replaced by the in-flight counter (below).
- `Terminate` is only tested between jobs and after a job; inside a line it is never tested. One E-line job can run 40000 / 1000000 / 5000000 steps and one equipotential up to 800000 / 1500000 steps, hence the detached-thread machinery in `ElectricFieldDoc::OnCloseDocument` (`ElectricFieldDoc.cpp:237-251`).

### 2. When and how equipotential jobs are posted (`PostCalculateEquipotential`, .cpp:23-49)
Called once per step of an E-line, after the `HitCharge` break and after `t += step` (`.cpp:97-106`). Verbatim logic:
```cpp
if (job.angle != job.angle_start || !calculateEquipotentials) return;   // only the FIRST seed of each charge
const double potential = functorV.theField->Potential(startPoint);        // startPoint = job.point (current point)
if (sign(job.charge.charge)*sign(potential) > 0 && abs(job.old_potential - potential) >= potentialInterval)
{
    if (job.old_potential == 0) job.old_potential = floor(potential / potentialInterval) * potentialInterval;
    else job.old_potential += sign(potential - job.old_potential) * potentialInterval;
    ...lock; FieldLineJob job(job); job.isEquipotential = true; m_jobs.push_back(job); ...
```
[VERIFIED: ElectricField/FieldLinesCalculator.cpp:25-39]. So: only the first line of every charge (sources **and** sinks: a sink's first line is traced without recording points, only to spawn equipotentials) posts a job each time the potential has moved by one interval; the job starts at the *current point of that line*; its label is `old_potential` (a multiple of the interval, up to float noise), not the potential at the start point. Posting stops for a net-zero source once the potential changes sign (`sign(charge)*sign(potential) > 0`). Port: `shouldPostEquipotential()` in `tracer.hpp` returns whether to post and updates the label; `ComputeSession` pushes the child job.

### 3. What an equipotential job traces (`CalculateEquipotential`, .cpp:113-150)
Start point = `job.point`; direction functor `FunctorForV` = unit vector perpendicular to `E` (`FieldLinesCalculator.h:63-77`: `v = E(pos); (v.X, v.Y) = (-v.Y, v.X); return v.Normalize();`). Verbatim parameters: `num_steps = (m_Solver->IsAdaptive() ? 800000 : 1500000)`, `step = (m_Solver->IsAdaptive() ? 0.001 : 0.0001)`, `SolveStep(functorV, point, t, step, next_step, 1E-3, 0.01)`, thinning through the same `AddPoint`, `fieldLine.weightCenter += point * step`, and the closing rule
```cpp
dist = (startPoint - point).Length();
if (dist * distanceUnitLength < 6. && t > 2.*dist) { fieldLine.points.push_back(startPoint); fieldLine.weightCenter /= t; break; }
```
[VERIFIED: ElectricField/FieldLinesCalculator.cpp:113-150]. Quirk kept: if the loop ends by step budget the line is not closed and `weightCenter` stays the **undivided** sum (the de-dup then uses that value). No cancel check exists in the original; the port adds one.

### 4. E-line termination (`CalculateElectricFieldLine`, .cpp:51-110) - already in Phase 1
Budget `steps = 40000`, `1000000` if `has_different_signs`, `5000000` if `has_different_signs && total_charge == 0`; break when `HitCharge(point)` (hit radius `chargeRadius / 1.5 / distanceUnitLength` = 0.1667 world units, `Charge.h:25`); `needs_precision = len*100 < 3000`; adaptive `precision = max_step = needs ? 0.001 : 0.1`, fixed `step = needs ? 0.001 : 0.5`. **There is no distance cut-off**: a line that escapes to infinity runs its full budget (this is the long single job: 1M steps, 80-394 ms in Release, up to 5.6 s Debug for `two-unequal-charges-different-signs`).

### 5. `FieldLineJob job(job)` (.cpp:37) - what MSVC does and what is correct
Reproduced with `cl` 14.51.36231 (the baseline toolset) on a struct with the same members and `Charge`'s user-declared ctor/dtor [VERIFIED: prototype `ef-msvc/selfcopy.cpp`, parent = `{charge 7, point (1.5,2.5), old_potential 3.25, angle 0.5}`]:

| Compiler / mode | Result of `FieldLineJob job(job)` |
|-----------------|-----------------------------------|
| `cl /O2`, `cl /Od` (default = legacy lookup, as in `ElectricField.vcxproj`, which sets no `ConformanceMode`/`/permissive-`) | `charge=7 point=(1.5,2.5) oldpot=3.25 angle=0.5 isEq=1` = **correct copy of the parameter** |
| `cl /O2 /W4 /permissive-` | warning C4457 + C4700 "uninitialized local variable 'job' used"; copies garbage |
| `zig c++` (Clang 22) `-O0` / `-O2` | `-Wshadow` + `-Wuninitialized` warnings; copies garbage (e.g. `charge=1113585600`, `point=(-3.7e-103, 5.3e-312)`) |

So the MFC output is *not* affected (baseline images are correct) and the **intent is `FieldLineJob equipJob(job);`** (copy of the parent, including the just-updated `old_potential` and the current `point`). Regression test (D-09, `tests/test_tracer.cpp`): every posted child has a finite start point on the parent line, label within one interval of `Potential(start)`, `sign(label) == sign(charge)`; plus labels advance by exactly one interval per post. The compile-time guard is `-Wshadow -Wuninitialized -Werror` (already in `core_flags`).

### 6. De-duplication (`GetDataFromThreads`, ElectricFieldDoc.cpp:155-228) - exact port spec
Runs only once all threads are done (`GetData`, `.cpp:139-152`), then `AdjustForBezier` on both line sets on two threads.
1. `electricFieldLines` swapped in as is (including empty sink lines).
2. If no equipotentials: done. If `charges.size() == 2 && has_different_signs`: equipotentials taken **raw, unsorted** (`.cpp:165-169`).
3. Else `sort` by `potential` ascending (MFC: unstable `std::sort`; port: **`std::stable_sort` over lines pre-ordered by job key**, which is what makes D-04 hold).
4. Group scan with `potential = 0`, `equipotentials = {}`, `ref = (-7., 5.5)`: for each line `if (abs(potential - line.potential) < interval/3. || 0 == potential)` -> `potential = line.potential; group.push_back(line)`; **else** flush: sort the group by `(ref - weightCenter).Length()` ascending; for each `equiline`: if `out` non-empty push iff `(out.back().weightCenter - equiline.weightCenter).Length() * distanceUnitLength > 2. * chargeRadius || abs(potential - out.back().potential) > interval * 2. / 3.`, else push unconditionally; clear group; then start the next group with `line`.
5. After the loop the **last group is appended without any de-dup** (`.cpp:225`). Keep this quirk.
6. In world units, the reads of `theApp.options.distanceUnitLength / chargeRadius` become `p.pixelsPerUnit` and `p.chargeRadiusPx` (`ComputeParams`, 100 and 25.0).

Measured effect with default options, Euler [VERIFIED: prototype, identical on Windows and Linux]: raw -> final equipotentials: dipole 75 -> 75 (shortcut), two-equal-charges 84 -> 74, two-unequal-same-sign 126 -> 113, two-unequal-different-signs 113 -> 113 (shortcut), **quadrupole 142 -> 142 (nothing to remove: the + regions are disconnected)**, four-same-sign 201 -> 128, two-dipoles-same 138 -> 138, two-dipoles-opposed 143 -> 130, **parallel-dipoles 150 -> 136**. So the "check on Quadrupole and Parallel dipoles" of D-05 means: Quadrupole must lose nothing, Parallel dipoles must lose duplicates (`test_session.cpp`).

### 7. `AdjustForBezier` (FieldLine.cpp:183-236)
Groups `p0,[p1,p2,p3],[p4,p5,p6],...` share endpoints: loop reads `pt0 = *it`, advances three times to `pt3`, rewrites `pt1`/`pt2`, then continues **from pt3** (it is not incremented past it); incomplete trailing groups (< 4 points left) are untouched. Maths: `t1 = |p1-p0|; t2 = t1 + |p2-p1|; t3 = t1 + t2 + |p3-p2|` (quirk: `t2` already contains `t1`), `t1 /= t3; t2 /= t3; divi = 3 t1 t2 (1-t1)(1-t2)(t2-t1)`; then the two 2x2 solves for X and Y. `ASSERT(abs(divi) > DBL_MIN)` (`.cpp:222`) is an MFC debug-only macro; Release divides by ~0. Port (`bezier.hpp`): returns `false` and leaves the points untouched when `!(|divi| > 1e-9)` (also catches NaN when `t3 == 0`) or any output is non-finite. Property used by the test: the fitted cubic passes through `pt1` at `t1` and `pt2` at `t2` (1e-9). Measured over 1.9 M groups (9 presets x Euler, RK4, Heun-Euler, Cash-Karp, Dormand-Prince) [VERIFIED: prototype `measure bezier`]: normal groups have `|divi|` >= 4e-3 (equal spacing gives t1 = 1/4, t2 = 1/2, divi ~ 0.049); exactly **one pathological far-field group per run** exists in `parallel-dipoles` (chord ~4400 px = 44 units from the origin, i.e. off-screen) with `|divi|` = 2.8e-6 (Euler), 2.1e-6 (Heun-Euler..Cash-Karp), 6.4e-5 (Midpoint..RK3/8) and 1.3e-9 (Dormand-Prince, `t1 = 0.0006, t2 = 0.0012`); MFC draws those with control points 1e3-1e4 px away. The reference guard (`1e-9`) triggers 0 times on this data; any threshold in [1e-9, 1e-6] gives identical *visible* output (the planner may pick 1e-6 to also suppress that off-screen spike; one constant, no test impact).

### 8. Drawing (for the scene layer)
- Draw order: electric lines, potential lines, charges (`TheElectricField.cpp:21-31`); background `Clear(White)` (`ElectricFieldView.cpp:129`); transform `Scale(1., -1.)*Translation(w/2, h/2)` (`ElectricFieldView.cpp:134`); AA `D2D1_ANTIALIAS_MODE_PER_PRIMITIVE` (`:127`).
- Charge: filled ellipse radius `chargeRadius` = 25 px (`Charge.cpp:29-32`), colour `charge > 0 ? positiveChargeColor : negativeChargeColor` (`Charge.cpp:27`).
- Defaults [VERIFIED: ElectricField/Options.cpp:8-23]: `distanceUnitLength(100)`, `chargeRadius(25.0f)`, `positiveChargeColor(RGB(0, 0, 255))`, `negativeChargeColor(RGB(255, 0, 0))`, `electricFieldLineColor(RGB(0, 0, 0))`, `potentialFieldLineColor(RGB(192, 192, 192))`, `electricFieldLineThickness(1)`, `potentialFieldLineThickness(1)`, `numLinesOnUnitCharge(16)`, `calculateEquipotentials(true)`, `potentialInterval(0.1)`, `numThreads(4)`, `calculationMethod(CalculationMethod::EulerMethod)`.
- Path construction (`FieldLine.cpp:13-105`): a figure begins at the first in-clip `p0`, each loop iteration consumes the next 3 points as one cubic `(p1,p2,p3)`; only the group's first point is clip-tested, with `irect` = view rect inflated by 10. When a tested point is outside the clip the figure is ended (`broken = true`) and the next in-clip point starts a new figure at an **arbitrary index**, which misaligns the p0/c1/c2/p3 triples (the quirk ARCHITECTURE.md item 3 flagged). The clip rectangle only shifts `left/top` (`ElectricFieldView.cpp:136-137`), so it over-includes the right/bottom half. Trailing 1-2 points after the last full group are drawn as straight lines (`morePoints`/`pt2Valid`, `FieldLine.cpp:83-88`). A potential line is closed iff `isPotential && points.front() == points.back() && !broken` (`:92`).
- Port decision (implemented in `scene.cpp`): cull per *cubic segment* (conservative control-polygon bounding box vs. view +-10 px), keep runs of visible segments as one stroke so the p0/c1/c2/p3 alignment is never broken, draw trailing points as lines, close only when nothing was culled. Visible output is identical inside the view.

## Architecture Patterns

### System Architecture Diagram

```
 ef-cli / tests / (Phase 3 GUI)
        |  ComputeSession(ChargeSystem, ComputeParams)          cancel() / ~ComputeSession()
        v                                                              |
 +---------------------- ComputeSession (efcore) -------------------+  |
 | planSeeds -> queue_ (deque<Job>, key {seedIdx, child})            |<-+  stop_ (atomic<bool>)
 |   mutex m_ + cv_  <---- postEquipotential(child job) ----+        |
 |                                                           |        |
 |  worker 1..N (std::thread, 1 solver each via withSolver)  |        |
 |   pop job, ++inFlight_                                    |        |
 |   E-line:  traceElectricLine --polls stop_ every 1024 steps--> hooks.postEquipotential  (first seed only)
 |   equip:   traceEquipotential --polls stop_ every 1024 steps
 |   push shared_ptr<const Keyed*Line> -> allE_/allP_ + pendingE_/pendingP_, --inFlight_
 |   exit when queue empty && inFlight_==0  (or stop_)                |
 |  last worker: sort by JobKey -> finalize() -> result_, done_       |
 +--------------+--------------------------------+-------------------+
                | poll(PollBatch&)  drain-and-swap |  wait() / takeResult(FieldResult&)
                v                                  v
        raw lines (progressive v2)        FieldResult{electric, potential}   (de-duped, Bezier-fitted)
                                                   |
                                          buildScene(field, result) -> FieldScene (world units, pure data)
                                                   |
                          renderScene(Painter&, scene, DrawStyle, View{w,h,100 px/unit})
                          (order, y-up/centre, cull, closed figures; device pixels out)
                          /                                   \
                 SvgPainter (std::string)              ImGuiPainter (ImDrawList*, origin)
                          |                                   |  Phase 3: window draw list
                    ef-cli --svg                      PNG: SDL_CreateSoftwareRenderer(surface)
                                                      + ImGui_ImplSDLRenderer3 -> SDL_RenderReadPixels -> SDL_SavePNG
```

### Recommended Project Structure (reference implementation already has it)
```
src/core/ef/   params.hpp method.hpp rk.hpp tracer.hpp bezier.hpp finalizer.{hpp,cpp} session.{hpp,cpp}
               scene.{hpp,cpp} svg.{hpp,cpp}   (all std-only, in lib efcore)
src/render/    imgui_painter.hpp png_export.{hpp,cpp}   (lib efrender, only with -Dgui=true; shared with Phase 3)
src/cli/       main.cpp                                   (exe ef-cli)
tests/         test_{tracer,bezier,finalizer,session,scene}.cpp test_params.hpp (+ changed test_presets.cpp)
tools/parity/  compare-mfc.sh expected-counts.csv
```

### Pattern 1: Condition-variable pool with in-flight termination (replaces `finishedThreads`)
**What:** workers wait on `stop_ || !queue_.empty() || inFlight_ == 0`; a job increments `inFlight_` when popped and decrements after its result *and any posted children* are published; a worker returns when it wakes with `queue_.empty()` (that implies `inFlight_ == 0`) or `stop_`. The last worker to exit (`activeWorkers_` hits 0) finalizes and sets `done_`.
**Why this is the contract:** a posted child makes `queue_` non-empty *before* the parent decrements `inFlight_`, so idle workers can never conclude "done" while a parent is still posting.
**Lost wake-up rule:** `cancel()` must store `stop_`, then lock and unlock `m_` (pairing with the predicate check under the lock), then `notify_all` - otherwise a worker that just evaluated the predicate can sleep forever.
**Example:** `research-ref/src/core/ef/session.cpp` [VERIFIED: prototype, TSan-clean].

### Pattern 2: Deterministic ordering by job key (D-04)
`JobKey{parent = seed index, child = 0 for the seed's own line, 1..k for the equipotentials spawned while tracing it}`. The child ordinal is assigned in the *sequentially traced* parent line, so it does not depend on scheduling. `finish()` sorts both raw sets by key before `finalize()`. Measured: 1-thread and 7-thread results are identical (test tolerance 1e-12 passes; counts identical).

### Pattern 3: Drain-and-swap `poll()` with immutable shared lines (D-03)
Workers publish `shared_ptr<const KeyedElectricLine>`/`KeyedPotentialLine` into both `allE_` (for the finalizer) and `pendingE_`; `poll(PollBatch&)` swaps `pendingE_` out under the lock (O(1) when the caller's vector is empty) and returns the status `{done, cancelled, failed, error, jobsCompleted, jobsKnown}`. Lines are never mutated after publication, so the UI thread cannot race a worker, and the finalizer reuses the same objects without copies under the lock.

### Pattern 4: One painter path for CLI and GUI (D-11)
`ef::Painter` has `begin/strokePath/fillCircle/end` in **device pixels**; `strokePath(pts, n, bezierSegments, closed, color, width)` takes a chain of `bezierSegments` cubics followed by line-to points, which covers both "Bezier path stroke" and "polyline" of D-11 in one call. `efrender::ImGuiPainter` maps that to `PathLineTo`, `PathBezierCubicCurveTo(c1,c2,p3, 0)`, `PathStroke(col, closed ? ImDrawFlags_Closed : 0, width)`, `AddCircleFilled(c, r, col, 0)`. PNG export creates its own ImGui context (restoring the previous one afterwards), sets `io.DisplaySize` to the export size (independent of any window), runs two frames (first builds the font atlas), draws into `ImGui::GetBackgroundDrawList()`, then `SDL_RenderReadPixels` + `SDL_SavePNG`.

### Anti-Patterns to Avoid
- **Texture-based AA lines on the software renderer:** jagged 1 px lines (nearest sampling). Set `ImGuiStyle::AntiAliasedLinesUseTex = false` wherever the shared painter is used (CLI export now, GUI in Phase 3) so both paths look the same.
- **Detaching or sharing solvers:** `RungeKutta::K[]` is per-instance state; one solver per worker (`withSolver` inside the worker).
- **Finalizing on the UI thread:** `finalize()` is 0.2-3.7 ms (ReleaseFast) for the 9 presets, but run it in the last worker anyway so a Debug build cannot stall the UI.
- **Bit-exact golden files across OSes:** not needed here (Windows == Linux byte-for-byte), but macOS libm may differ; assert counts.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| PNG encoding | a PNG writer / stb | `SDL_SavePNG(surface, path)` [VERIFIED: `SDL_surface.h:733`] | already in SDL 3.4 |
| Offscreen raster | custom rasteriser | `SDL_CreateSoftwareRenderer(SDL_Surface*)` + `SDL_RenderReadPixels` [VERIFIED: `SDL_render.h:421`, `:2572`] | needs no window/display |
| Curve tessellation / AA | own Bezier flattening | `ImDrawList::PathBezierCubicCurveTo` (+ `CurveTessellationTol = 0.3`) | same code path as the GUI |
| Thread pool, futures | task framework | `std::thread` + `std::condition_variable` | D-01, 150 lines |
| Cancellation primitive | `std::stop_token`/`jthread` | `std::atomic<bool>` | D-01 (C++17) |
| Image diff in CI | custom diff | none: CI asserts counts; ImageMagick only in the local script | pixels differ per AA |
| Float formatting in SVG | `std::to_chars` (floating) | `snprintf("%.2f")` + zero trim | locale is "C" (the core never calls `setlocale`); portable on libc++ 22 |

**Key insight:** the numerically delicate parts (tableaus, thinning, seeds) are done; everything left is plumbing around them, and all plumbing already exists in std/SDL/ImGui.

## Cancellation design and measurements (CORE-06, D-02)

**Design:** `tracer` loops poll `hooks.cancelled()` (relaxed atomic load) when `i % kCancelStride == 0` (including `i == 0`); the pool loop re-checks `stop_` under the lock before taking each job; a cancelled trace returns without publishing a line. `~ComputeSession` = `cancel()` + `join()` of all workers; the last worker's `finish()` checks `stop_` before finalizing (a cancel that arrives during `finalize()` waits for it: 0.2-3.7 ms Release).

**Measured cost per poll interval** [VERIFIED: prototype `measure/measure.cpp lines`, single thread, all 11 methods x 9 presets, full table in `research-ref/measure/lines_release_o2_windows.csv`]. "Block" = time between two consecutive `cancelled()` calls = 4096 steps:

| Build | Worst block (4096 steps) | Worst single job | Notes |
|-------|--------------------------|------------------|-------|
| ReleaseFast/-O2 | **4.0 ms** (Dormand-Prince, parallel-dipoles) | **394 ms** (DP, two-unequal-charges-different-signs, 1M-step line) | Euler worst block 0.7 ms |
| Debug + UBSan (`zig c++` default) | **~45 ms** (DP; two Euler outliers of 331/594 ms coincided with a concurrent Debug test run saturating the machine, so treat Debug numbers as +-10x under load) | 5.6 s (same job) | unoptimised, UBSan traps |

Measured end-to-end `cancel()` -> `wait()` latency (worst of 3 cancel delays 0/5/30 ms, heaviest preset + Dormand-Prince, 4 threads, stride 4096): Windows Debug **21-45 ms**; Linux Debug+TSan **73 ms**; Linux ReleaseSafe+TSan **2.6 ms** [VERIFIED: test output `worst cancel latency ms`]. All inside 200 ms, but the Debug margin was only 4-5x.

**Recommendation (minor refinement of D-02's "~4096"):** `kCancelStride = 1024`. It cuts the worst Debug block to ~11 ms at no measurable cost, so the 200 ms contract holds with a 10x margin on slow CI machines. The reference implementation uses 1024 (tests pass with it); change one constant if the user insists on 4096.

**Test bounds:** assert `< 200 ms` for cancel/destroy; under TSan (`__SANITIZE_THREAD__` / `__has_feature(thread_sanitizer)`) widen to 1000 ms to avoid CI flakes (TSan slows code 5-15x).

**Not cancellable:** `SolveStep`'s inner retry loop (`loop > 1000`, `rk.hpp`) is bounded and was never observed to dominate; `finalize()` is short. Do not add checks inside the RK stages.

## ThreadSanitizer via Zig 0.17 (D-20, QA-03)

[VERIFIED: prototype, WSL Ubuntu 24.04 kernel 6.18, Zig 0.17.0, clean `~/ef-p2`]
- `Module.sanitize_thread: ?bool` exists in Zig 0.17 (`lib/std/Build/Module.zig:34,222`). The reference `build.zig` adds `const Sanitize = enum { none, thread }` and `-Dsanitize=thread`; it sets `.sanitize_thread = true` on **every module of the link** (core lib, test exe, ef-cli) via one `coreModule()` helper, and sets `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1` on the test run step.
- `zig build test -Dgui=false -Dsanitize=thread` (Debug): all pass (53 cases at the time of that run), ~2 min on a 36-core machine; `-Doptimize=ReleaseSafe` on the final code: **54/54 pass in 8 s**, no race reported. The test binary contains the TSan runtime and instrumentation (`nm`: `__tsan_read8`, `__tsan_func_entry`, 171 `__tsan_*` symbols). `ef-cli --preset parallel-dipoles --threads 8` under TSan: exit 0.
- **Negative control** (a deliberate unsynchronised `++g` from two `std::thread`s built with `zig c++ -fsanitize=thread`): `WARNING: ThreadSanitizer: data race`, exit code 66. So a race fails the CI job.
- TSan is Linux-only in this design (Zig does not support it for the Windows target; macOS not verified). CI job `tsan-linux` uses `-Doptimize=ReleaseSafe` and a precautionary `sudo sysctl -w vm.mmap_rnd_bits=28 || true` (old TSan runtimes abort with "unexpected memory mapping" on kernels with 32 ASLR bits; Zig 0.17 bundles a current runtime, so it is expected to be a no-op - keep until a run proves it unnecessary).
- Tests that matter for TSan: `1 thread and N threads...`, `poll() drains...`, `cancel()...`, `destroying a running session... all methods`, `stress: many short-lived sessions with random cancel points` (60 sessions, random preset/method/cancel point, `wait()` after `cancel()`).

## Scene, Painter, SVG, PNG (D-11)

### Data model and API (implemented in `scene.hpp`)
`FieldScene{vector<ScenePath> electric, potential; vector<SceneCharge> charges}` in **world units**, `ScenePath{vector<Vec2> points; bool closed}` where `points` keeps the post-`adjustForBezier` layout. `View{widthPx=1024, heightPx=768, pixelsPerUnit=100}` is a pure output transform (never feeds physics). `DrawStyle` defaults = the MFC values quoted above plus `background {255,255,255}`. `renderScene` computes device coordinates `x = w/2 + X*ppu`, `y = h/2 - Y*ppu` in `double`, casts to `float`, culls, and calls the painter.

### SVG writer
One `<path fill="none" stroke="#rrggbb" stroke-width="1" d="M x y C x y x y x y ... L x y [Z]"/>` per visible run (`C` once, then implicit repeats; `L` for trailing line points), `<circle cx cy r fill>` per charge, `<rect width="100%" height="100%" fill="#ffffff"/>` background, `viewBox="0 0 W H"`. Device coordinates are written directly (no `scale(1,-1)` group, so stroke widths stay 1 px). Numbers `%.2f` with trailing zeros trimmed. Measured sizes: 80-161 KB per preset (1024x768), generated in 5-9 ms.

### PNG path
`SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32)` -> `SDL_CreateSoftwareRenderer(surface)` -> `ImGui_ImplSDLRenderer3_Init` -> 2 frames into `GetBackgroundDrawList()` -> `SDL_RenderReadPixels(renderer, nullptr)` -> `SDL_SavePNG(pixels, path)` [signatures VERIFIED in `zig-pkg/sdl-0.6.0+3.4.18-.../include/SDL3/SDL_render.h:421,2572` and `SDL_surface.h:173,733`]. No `SDL_Init`, no window, no hints, no environment: works with `DISPLAY`/`WAYLAND_DISPLAY`/`XDG_RUNTIME_DIR` unset [VERIFIED: WSL `env -u ...`]. `-DImDrawIdx=unsigned int` (already in `gui_flags`) is required because the meshes exceed 64k vertices; 4096x3072 renders fine (357 KB PNG).

### Anti-aliasing quality and MFC parity [VERIFIED: `tools/parity/compare-mfc.sh` against `docs/baseline/mfc/*.png`, ImageMagick 7.1.2-32]
| Setting | Result |
|---------|--------|
| default (`AntiAliasedLinesUseTex = true`) | visibly stair-stepped black lines on the software renderer; ink recall 0.947-0.957 |
| **`AntiAliasedLinesUseTex = false`** (reference code) | smooth, like Direct2D; **ink recall 0.988-0.993, ink precision 0.995-0.997, total-ink ratio 0.996-1.009**, `AE -fuzz 10%` 6.4k-14.7k px (0.8-1.9% of 786k) |

"ink" = darker than 97% grey; recall = share of MFC ink with port ink within 1 px; precision = reverse. Equipotential duplicates overlay exactly, so the images cannot validate the de-dup; **counts are the oracle** (CI), the images validate geometry, thickness, colours and layout. `CurveTessellationTol = 0.3f` as STACK.md recommends.

### Sharing with Phase 3
Phase 3 adds `ImGuiPainter(ImGui::GetWindowDrawList(), cursorScreenPos)` and calls the same `renderScene`; it must also set `AntiAliasedLinesUseTex = false` (Phase 3 note) and never call `efrender::renderPng` while its own context is mid-frame (the function restores the previously current context, but ImGui contexts are not thread-safe).

## ef-cli and build wiring (D-10, D-14)

- Options: the reference `build.zig` has `-Dgui=bool` (default true) and `-Dsanitize=thread`. With `-Dgui=false`: core + tests + an **SVG-only** `ef-cli` (`--png` exits 1 with "built without PNG support"), and **no GUI dependency is fetched**. With `-Dgui=true`: `efrender` lib (png_export.cpp, gui flags without `-Werror`) + `ef-cli` with `-DEF_CLI_PNG=1` + the GUI exe. `zig build` installs `zig-out/bin/ef-cli[.exe]` and `ElectricField[.exe]`.
- Lazy dependencies [VERIFIED]: `build.zig.zon` adds `.lazy = true` to `sdl3` and `imgui`; `build.zig` uses `b.lazyDependency("sdl3", ...) orelse return null` inside `addGuiDeps`, which returns `?GuiDeps` (Zig 0.17: `lazyDependency` returns null on the first pass, fetches, and re-runs `build()`). The remaining code must tolerate `null` (`addCli(..., gui: ?GuiDeps)`, `if (gui) |g| addGui(...)`). Proof: copy the repo with an empty `ZIG_GLOBAL_CACHE_DIR`, `zig build -Dgui=false --fetch=needed` -> `zig-pkg/` contains only doctest. **Caveat:** with a warm cache an old configure result may repopulate `zig-pkg/` with SDL/ImGui; judge laziness only from a fresh cache.
- A fresh `zig build` (gui=true) on a clean checkout still works (first pass returns null, Zig fetches SDL3 + ImGui + `sdl_linux_deps`, second pass builds). Linux needs no apt packages (Phase 1 CI run 38040977802 proved this on `ubuntu-latest`; the lazy `sdl_linux_deps` package is fetched as before). `ef-cli` stays a **console** executable (do not copy `exe.subsystem = .windows` from the GUI target) so stdout/stderr and exit codes reach CI.
- Argument rules in the reference CLI: `--threads 1..128`, `--width/--height 16..8192`, unknown `--method` -> exit 1, unknown/missing `--preset` -> exit 2, `--all --out-dir D [--expect FILE]` renders all presets to `D/<slug>.svg|png` (PNG only when built with it) and exits 1 on parity failure. Summary line: `preset=<slug> charges=N field_lines=N equipotentials=N ms=N`. **On Windows stdout is CRLF**: any script parsing `--list` must strip `\r` (`tr -d '\015'`).

## CI design (D-12, D-20)

Reference `ci.yml` (YAML-parses; local equivalents of every command were run, the workflow itself was not executed on GitHub):
- `build-test` (matrix windows/ubuntu/macos-arm64, `defaults.run.shell: bash` so the same script text runs on all three): existing core gates, then `zig build -Doptimize=ReleaseFast` (also yields `ef-cli`), `zig build test`, `zig build smoke`, then **parity**: `mkdir -p parity-out; ./zig-out/bin/ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv | tee parity-out/summary.txt` (GitHub's bash runs with `-eo pipefail`, so a non-zero ef-cli exit fails the step) and `actions/upload-artifact@cf430e0... # v7.0.2` with `if: always()`, name `parity-${{ matrix.name }}`, path `parity-out`, 14 days.
- `tsan-linux` (ubuntu-latest): the sysctl precaution, `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe`, then a TSan build of `ef-cli` and `--preset parallel-dipoles --threads 8`.
- IN-04 folded in: `push.branches: [feature/cross-platform, master]`.
- Redundancy note: "Tests (default configuration)" re-runs the Debug suite that the first step already ran (41 s here, probably 2-3x on a 4-vCPU runner). Either drop it or keep it as the proof that the default (GUI) build graph configures; the planner decides.

### Expected counts and tolerance [VERIFIED: prototype; Windows == Linux for every row; Euler, 4 threads, MFC defaults]
| slug | charges | field lines | equipotentials | seeds | ef-cli ms (ReleaseFast Win) |
|------|---------|-------------|----------------|-------|-----------------------------|
| dipole | 2 | 16 | 75 | 17 | 72 |
| two-equal-charges | 2 | 32 | 74 | 32 | 242 |
| two-unequal-charges-same-sign | 2 | 48 | 113 | 48 | 336 |
| two-unequal-charges-different-signs | 2 | 32 | 113 | 33 | 396 |
| quadrupole | 4 | 32 | 142 | 34 | 122 |
| four-charges-same-sign | 4 | 64 | 128 | 64 | 952 |
| two-dipoles-same-direction | 4 | 32 | 138 | 34 | 120 |
| two-dipoles-opposed-direction | 4 | 32 | 130 | 34 | 171 |
| parallel-dipoles | 4 | 32 | 136 | 34 | 228 |

Field lines = sources only (sinks record nothing; the `finalize()` drops the empty sink lines), so the count is exact and deterministic. Equipotentials depend on libm only through `sin/cos` (seeds; Euler) and `pow` (adaptive methods): other solvers vary by up to 4 (e.g. four-same-sign 128 Euler/RK4, 125 Heun-Euler, 124 Dormand-Prince), so the committed file is for the default **Euler**. Tolerance implemented: charges and field lines exact, equipotentials `+-max(2, 3%)` (the reference CLI prints the diff and exits 1). SVG md5 and PNG pixels were bit-identical Windows vs Linux; macOS arm64 is unverified, so the first macOS CI run is the confirmation (see Open Questions).

## MFC comparison script (D-13)
`tools/parity/compare-mfc.sh <ef-cli> [out-dir] [baseline-dir]` (bash + ImageMagick 7; informational, never a gate) renders all 9 presets then prints `preset,ae_fuzz10,ink_recall,ink_precision,darkness_ratio`. Metrics: `magick compare -metric AE -fuzz 10%` (raw differing pixels), ink recall/precision with a 1 px dilation (threshold 97% grey, `-alpha off` first: the port's RGBA PNGs otherwise skew the product), and total-ink ratio (port/MFC). Observed (this session):

| preset | ae_fuzz10 | recall | precision | ink ratio |
|--------|-----------|--------|-----------|-----------|
| dipole | 6375 | 0.988 | 0.995 | 0.997 |
| two-equal-charges | 8056 | 0.990 | 0.995 | 1.009 |
| two-unequal-charges-same-sign | 11308 | 0.991 | 0.995 | 0.996 |
| two-unequal-charges-different-signs | 8984 | 0.991 | 0.996 | 1.005 |
| quadrupole | 8184 | 0.992 | 0.996 | 0.998 |
| four-charges-same-sign | 14749 | 0.992 | 0.997 | 0.999 |
| two-dipoles-same-direction | 7042 | 0.993 | 0.996 | 1.002 |
| two-dipoles-opposed-direction | 8825 | 0.992 | 0.996 | 0.998 |
| parallel-dipoles | 9908 | 0.992 | 0.996 | 1.004 |

## Phase 1 review carry-over (D-14..D-19): concrete changes
| Item | Change | Status in `research-ref` |
|------|--------|--------------------------|
| D-14 WR-01 | `.lazy = true` on `sdl3`,`imgui` in `build.zig.zon`; `lazyDependency` in `addGuiDeps` (see wiring). CI's `-Dgui=false` gates unchanged | done, verified (fresh-cache fetch list) |
| D-15 WR-03 | `info()` uses `kMethods.at(...)` (throws `std::out_of_range`); `constexpr std::optional<Method> methodFromName(std::string_view)`; `withSolver` ends in `throw std::invalid_argument(...)` (no Euler fallback); `ComputeSession` catches it in the worker -> `failed` + `error` | done; `test_session.cpp` ("invalid Method fails cleanly") |
| D-16 WR-06 | `AdaptiveRungeKutta::computeStages(f, cur, t, h, sumHigh, sumLow)` used by `SolveStep` and `SolveStepPair`; fixed-step `SolveStepFixed` untouched | done; an adaptive run (DP, parallel-dipoles, 1 thread) produced a byte-identical SVG before/after, and all Phase 1 order/DP tests pass |
| D-17 WR-05 | `REQUIRE_MESSAGE(in.good(), ...)` instead of skip; `cur = PresetCharge{1,0,0}` on each `IMPLEMENT_DYNCREATE`; `REQUIRE(!docs.empty())` before `docs.back()` | done in `tests/test_presets.cpp` |
| D-18 WR-04 | in `tools/baseline/capture-mfc-baseline.ps1`: (a) `Grab`: `$ok = [W]::PrintWindow($h, $hdc, 3)` then `if (-not $ok) { $bmp.Dispose(); return $null }`, and the polling loop must `continue` (and the final capture `throw`) on `$null`; (b) `W.Dark`: count a pixel only when `buf[i+3] == 255` (BGRA alpha); (c) before `Start-Process` (line ~128): `if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Exe not found: $Exe" }`. Cannot be exercised without rebuilding the MFC exe, so the executor verifies by PowerShell parse (`[System.Management.Automation.Language.Parser]::ParseFile`) and a `-Exe` bad-path run | text only (not applied; the real script needs the MFC exe to run) |
| D-19 | WR-02 deferred to Phase 3 | - |
| IN-01 | document `expectedLineCount` as MFC parity (counts one per sink even when equipotentials are off) and add a test, or make it flag-aware; compute in `std::int64_t` | discretion |
| IN-02 | `ComputeParams::sanitized()`: `pixelsPerUnit >= 1`, `linesPerUnitCharge >= 1`, `numThreads` in [1,128], `potentialInterval` in [0.01, 0.5]; `ComputeSession` already uses `max(1, numThreads)`; `ef-cli` rejects bad input | CLI done, `sanitized()` is the Phase 4 loader's job |
| IN-03/IN-05 | `ImGui::GetIO().IniFilename = nullptr` in smoke mode; `#include <algorithm>` in `tests/test_rk_order.cpp` | discretion |
| IN-04 | master in `push.branches`; SHA-pin `actions/checkout` | `ci.yml` has the branch change |

## Common Pitfalls

### Pitfall 1: Debug test time explodes
**What goes wrong:** the first draft of the session tests took 4 min 55 s in Debug (full MFC defaults over many presets).
**Why:** equipotential jobs dominate (they scale with `1/potentialInterval`, not with lines per charge): Debug+UBSan single-thread Euler sums to ~123 s for the 9 presets, 99 s of it equipotentials (four-charges-same-sign alone 35 s of equipotentials).
**How to avoid:** `tests/test_params.hpp::lightParams()` (4 lines, interval 0.5, 4 threads) for sweeps; full defaults only through `ef-cli` in ReleaseFast. Result: 41 s Debug, 5 s ReleaseFast. **Warning sign:** `zig build test` > 2 min locally.

### Pitfall 2: CORE-07 regression test that cannot fail
`job(job)` does not misbehave under MSVC, so a test that only checks "equipotentials exist" is green on the old code too. Assert on the *posted child's state* (finite, on the parent line, label tracks `Potential(start)`); that fails with garbage copies (verified by the garbage values above).

### Pitfall 3: Lost wake-up on cancel
Setting the atomic and calling `notify_all` without taking the mutex can leave a worker asleep after it evaluated the predicate. `cancel()` locks/unlocks `m_` between the store and the notify.

### Pitfall 4: Equipotential duplicates are invisible in images
Duplicates draw on top of each other, so MFC-vs-port image metrics cannot detect a missing de-dup (total-ink ratio moves by < 1%). Keep the count assertions (`expected-counts.csv`, `test_session.cpp` quadrupole/parallel-dipoles) as the oracle.

### Pitfall 5: ImGui texture AA on SDL software renderer
See above. Also: do not forget `-DImDrawIdx=unsigned int` for `efrender` (it is in `gui_flags`).

### Pitfall 6: `lazyDependency` returns null on the first pass
`orelse @panic` or unconditional use of the result breaks the first `zig build`. Return `null` up the chain and skip the GUI targets. Also: judging laziness from a warm cache is misleading.

### Pitfall 7: CRLF and `.exe` on Windows
`ef-cli --list` prints CRLF on Windows (text-mode stdout); strip `\r` in scripts. `./zig-out/bin/ef-cli` resolves `.exe` under Git Bash. Use `shell: bash` on all matrix legs; `PATH` munging across WSL/Git Bash needs a script file (`MSYS_NO_PATHCONV=1`), not inline `export PATH=...`.

### Pitfall 8: TSan environment
Only Linux; old runtimes + high-entropy ASLR abort (precautionary sysctl); under TSan widen latency bounds; do not build GUI deps with TSan (not needed: `-Dgui=false`).

### Pitfall 9: ImGui global context
`renderPng` creates/destroys a context. It restores the previous one, but a GUI calling it from another thread or mid-frame would race the global current-context pointer. Phase 4 should call it on the UI thread between frames.

## Code Examples

Verified, compile-tested sources are in `research-ref/`; the load-bearing ones:

### Posting rule (CORE-07 fix) - `research-ref/src/core/ef/tracer.hpp`
```cpp
inline bool shouldPostEquipotential(const ChargeSystem& field, const ComputeParams& p, const Seed& seed, const Vec2& point,
                                    double& oldPotential) {
  if (!seed.first || !p.calculateEquipotentials) return false;
  const double potential = field.Potential(point);
  if (sign(seed.charge) * sign(potential) > 0 && std::abs(oldPotential - potential) >= p.potentialInterval) {
    if (oldPotential == 0) oldPotential = std::floor(potential / p.potentialInterval) * p.potentialInterval;
    else oldPotential += sign(potential - oldPotential) * p.potentialInterval;
    return true;
  }
  return false;
}
```

### Worker loop - `research-ref/src/core/ef/session.cpp`
```cpp
cv_.wait(lk, [this] { return stop_.load(std::memory_order_relaxed) || !queue_.empty() || inFlight_ == 0; });
if (stop_.load(std::memory_order_relaxed) || queue_.empty()) return;  // nothing can appear any more
job = std::move(queue_.front()); queue_.pop_front(); ++inFlight_;
```

### FMA-contraction canary - `research-ref/tests/test_tracer.cpp`
```cpp
volatile double x = 1.0 + 0x1p-30, y = 1.0 - 0x1p-30;
const double r = x * y - 1.0;   // fused (contract on) = -2^-60 ; unfused (-ffp-contract=off) = 0
CHECK(r == 0.0);
```
[VERIFIED: with `-mfma -ffp-contract=on` or default `-mcpu=native` the value is -8.67e-19; with `-ffp-contract=off` it is 0]. Clang's default is contraction **on**, so the flag in `core_flags` is load-bearing on x86-64 with FMA and on aarch64.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Detached threads + `delete this` + app-level calculator graveyard | RAII session, cooperative cancel, join | this phase | tab close / quit = destructor |
| `finishedThreads` restart hack | in-flight counter + condvar | this phase | correct "done" without races |
| Dedup in the MFC document, on the UI thread | pure `finalize()` on the last worker | this phase | testable, off the UI thread |
| D2D/GDI draw code x2 | one `renderScene` + Painter | this phase | same geometry for SVG/PNG/GUI |
| Zig non-lazy deps | `.lazy = true` + `lazyDependency` (Zig 0.17) | this phase | core gates never fetch SDL/ImGui |

**Deprecated/outdated:** the CONTEXT wording "dummy video driver + software renderer" for PNG: `SDL_CreateSoftwareRenderer(surface)` needs neither (the `--smoke` path still uses them for the GUI exe).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | macOS arm64 produces the same field-line counts and equipotential counts within +-max(2, 3%) as Windows/Linux (Windows == Linux was verified byte-for-byte; Apple libm for `sin/cos/pow` is unverified) | CI design | first macOS parity step fails; fix = widen tolerance or re-baseline the expected file once from the macOS artifact |
| A2 | `ubuntu-latest` TSan runs without the `vm.mmap_rnd_bits` sysctl (Zig bundles LLVM 22 runtime) | CI design | job aborts with "unexpected memory mapping"; the sysctl step in the reference already covers it |
| A3 | A 4-vCPU CI runner runs the Debug suite in 2-3x the 41 s measured on this box (36 cores) and the TSan ReleaseSafe suite in < 1 min | CI design | `timeout-minutes: 30` is generous; trim tests via `lightParams()` if not |
| A4 | The software-renderer fringe AA path (vertex alpha only) renders identically on macOS (no platform code involved) | Scene/PNG | macOS PNGs differ slightly; CI does not compare pixels, so no failure |
| A5 | `kCancelStride = 1024` is acceptable as the reading of "every ~4096 steps" in D-02 | Cancellation | user wants exactly 4096; change one constant, measured latency stays < 200 ms (21-73 ms) |
| A6 | Degenerate-group threshold `1e-9` for `|divi|` is fine (0 fallbacks across the 9 presets x 5 methods tried; the one pathological off-screen group per run has divi >= 1.3e-9) | Bezier | a different value changes only pathological, off-screen input |
| A7 | GitHub macOS runners have the same ASLR/TSan behaviour irrelevant here (TSan is Linux-only in this phase) | CI | none |

## Open Questions (RESOLVED)

Resolved by the autonomous orchestrator (auto mode, recommended options) on 2026-10-10; CONTEXT D-02 amended accordingly.

1. **Poll stride 1024 vs 4096 (A5).** What we know: measured latencies are fine either way; 1024 gives 10x margin in Debug. **RESOLVED:** ship `kCancelStride = 1024`; D-02 amended from "~4096" to "~1024".
2. **Where macOS counts land (A1).** **RESOLVED:** land the CI parity change with the committed Euler expectations and tolerance; if the first macos-arm64 run lands outside tolerance, adjust the tolerance or re-baseline from that run's `parity-macos-arm64/summary.txt` artifact in a follow-up commit within this phase. Never silently skip the check.
3. **Keep the redundant CI step "Tests (default configuration)"?** **RESOLVED:** drop the duplicate; keep one Debug core gate (`-Dgui=false`), one ReleaseFast core run, the GUI-configured build + tests + smoke, the ef-cli parity run, and the Linux TSan job.
4. **Bezier guard threshold (planner's pick per summary).** **RESOLVED:** keep `1e-9` (closest to MFC parity); the single off-screen far-field Bezier spike in parallel-dipoles is accepted and documented.
5. **`ComputeSession` wake callback for Phase 3.** **RESOLVED:** polling only in Phase 2 (per D-03); an `onWake` callback is a Phase 3 decision.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Zig | build/test (all) | yes | 0.17.0 (Windows scoop; `~/zig17` in WSL) | - |
| WSL Ubuntu | TSan / Linux parity | yes | 24.04.4, kernel 6.18, 36 cores | CI-only Linux job |
| MSVC `cl` | reproducing the `job(job)` behaviour | yes | 14.51.36231 (VS 18 Insiders) | not needed after Phase 2 |
| ImageMagick | `compare-mfc.sh` | yes | 7.1.2-32 | script is local/informational |
| `gh` | action version check | yes | authenticated | - |
| Python | YAML parse check only | yes | 3.14 | - |
| macOS arm64 | A1 | no | - | first CI run |

**Missing dependencies with no fallback:** none. **With fallback:** macOS execution -> CI.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest 2.5.3 (single header, `tests/*.cpp`, `zig build test`) |
| Config file | none (`build.zig` `test_sources` list; `run.setCwd(b.path("."))` - `test_presets.cpp` reads `ElectricField/ConfigDocs.cpp` relative to the repo root) |
| Quick run command | `zig build test -Dgui=false` (Debug+UBSan: 54 cases, 41 s; `-Doptimize=ReleaseFast`: 5 s) |
| Full suite command | the above + `zig build -Doptimize=ReleaseFast && ./zig-out/bin/ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` + `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` (Linux) |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CORE-04 | equipotentials posted at the interval along the first line; labels advance by one interval | unit | `zig build test -Dgui=false` (`test_tracer.cpp` "equipotential jobs are posted...") | Wave 0 (research-ref) |
| CORE-04 | de-dup: shortcut, grouping, last-group quirk; Quadrupole keeps all, Parallel dipoles loses duplicates | unit + integration | `test_finalizer.cpp`, `test_session.cpp` "de-duplication on Quadrupole..." | Wave 0 |
| CORE-04 | equipotential closes, weight centre finite | unit | `test_tracer.cpp` "equipotential trace closes..." | Wave 0 |
| CORE-05 | 1 thread vs 7 threads identical (1e-12) | integration | `test_session.cpp` "1 thread and N threads..." | Wave 0 |
| CORE-05 | `poll()` delivers each job exactly once; empty system finishes | integration | `test_session.cpp` "poll() drains...", "an empty charge system..." | Wave 0 |
| CORE-06 | cancel < 200 ms mid-run; destructor joins all 11 methods; no result after cancel | integration | `test_session.cpp` "cancel()...", "destroying a running session..." | Wave 0 |
| CORE-07 | `job(job)` copy semantics | unit | `test_tracer.cpp` "FieldLineJob job(job) regression" | Wave 0 |
| CORE-07 | `abs(double)` truncation / M_PI redefinition (`#define _USE_MATH_DEFINES` + `static_assert(kPi == M_PI)`) | unit/compile | `test_tracer.cpp` | Wave 0 |
| CORE-07 | `AdjustForBezier` divide-by-zero guard, passes through pt1/pt2 | unit | `test_bezier.cpp` | Wave 0 |
| CORE-07 | `-ffp-contract=off`, no `__FAST_MATH__` | unit/compile | `test_tracer.cpp` "numerics policy..." | Wave 0 |
| CORE-07 | warning-clean Clang | build | `core_flags` has `-Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -Wuninitialized -pedantic -Werror` (build fails otherwise) | existing |
| QA-02 | per-preset: source line count, finite points, labels multiples of the interval, sources are the + charges when net-zero | integration | `test_session.cpp` "field-line and equipotential invariants per preset", existing `test_seeds.cpp` (`sign(0)==+1`) | Wave 0 / existing |
| QA-03 | TSan clean on the whole suite incl. stress | CI (Linux) | `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` | Wave 0 (build option + job) |
| QA-04 | scene geometry (y-up, centre, 100 px/unit, colours, order, cull, closed) | unit | `test_scene.cpp` | Wave 0 |
| QA-04 | SVG well-formed for every preset | integration | `test_scene.cpp` "every preset renders to a finite SVG" | Wave 0 |
| QA-04 | `ef-cli` counts on 3 OSes within tolerance + artifacts | CI | `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` | Wave 0 |
| D-15/D-16/D-17 | invalid method fails cleanly; stage sharing; preset cross-check fails when unreadable | unit | `test_session.cpp`, `test_rk_order.cpp` (existing, unchanged), `test_presets.cpp` | Wave 0 / existing |

### Sampling Rate
- **Per task commit:** `zig build test -Dgui=false` (Debug) - 41 s; for pure doc/CI tasks the CI file YAML-parse.
- **Per wave merge:** Debug + ReleaseFast tests, `zig build` (GUI graph), `zig build smoke`, ef-cli parity run, TSan run (WSL or CI).
- **Phase gate:** CI green on all legs incl. `tsan-linux` and parity artifacts, then `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/test_{tracer,bezier,finalizer,session,scene}.cpp`, `tests/test_params.hpp` - copy from `research-ref/tests/`
- [ ] `build.zig` test list + `-Dsanitize=thread` + lazy deps; `build.zig.zon` `.lazy = true`
- [ ] `tools/parity/expected-counts.csv`; CI jobs
- [ ] none for the framework (doctest exists)

### Non-automatable / manual items
- First macOS arm64 parity run: read the uploaded `summary.txt` and confirm counts (A1).
- Visual MFC comparison (informational): run `tools/parity/compare-mfc.sh` locally (needs ImageMagick) and eyeball one preset; recorded numbers above.
- D-18 script changes need the MFC exe to run end to end; verify by parse + bad-path run only.

## Security Domain

`security_enforcement` is enabled (absent = enabled; `.planning/config.json` has `security_asvs_level: 1`).

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication / V3 Session / V4 Access Control | no | local CLI/desktop tool, no accounts |
| V5 Input Validation | yes | `ef-cli` bounds: `--threads 1..128`, `--width/--height 16..8192` (8192^2*4 B = 256 MiB surface cap), unknown preset/method rejected; `--expect` parsed with `atoi`/`getline` only (no code execution); `ComputeParams` sanitizing for Phase 4 config |
| V6 Cryptography | no | none used |
| V10/V14 Build & dependency integrity | yes | hash-pinned `build.zig.zon`; actions pinned by commit SHA (`setup-zig`, `upload-artifact`); `permissions: contents: read` |
| V12 Files | yes | output paths are user-supplied for a local tool (trust boundary = the invoking user); SVG contains only numbers and fixed strings (no user text) so no markup injection |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Resource exhaustion via huge size/threads | Denial of Service | bounds above; cancel/join design; session never spawns more than `numThreads` |
| Malicious/corrupt expected-counts file | Tampering | read-only parse, failures only produce exit 1 |
| Supply chain (floating action tags, unpinned deps) | Tampering | SHA-pinned actions, hash-pinned zig deps; `sudo sysctl` in the TSan job is harmless and scoped to the runner |
| Data races becoming memory-safety bugs | Elevation/Tampering | TSan CI job, immutable published lines, RAII join |
| Artifact upload leaking secrets | Information disclosure | only generated images/summary are uploaded; no secrets in the workflow |

## Plan decomposition hint (not binding)
1. **Wave 1 - hardening + tracer/bezier:** `.lazy` deps, `-Dsanitize`, D-15/16/17 changes, `params.hpp`, `tracer.hpp` equipotential + hooks + regression tests, `bezier.hpp` + tests. (All build.zig edits in one task to avoid conflicts.)
2. **Wave 2 - engine:** `finalizer`, `session`, tests (determinism, poll, cancel, stress, invariants).
3. **Wave 3 - output:** `scene`, `svg`, `imgui_painter`, `png_export`, `ef-cli`, scene tests.
4. **Wave 4 - gates:** `expected-counts.csv`, `ci.yml` (parity + TSan), `compare-mfc.sh`, D-18 script patch, docs; delete `research-ref/`.
Sequential on `build.zig`/`ci.yml`; the test files and engine files are independent.

## Sources

### Primary (HIGH confidence - read or run this session)
- `ElectricField/FieldLinesCalculator.{h,cpp}`, `ElectricFieldDoc.cpp`, `FieldLine.{h,cpp}`, `ElectricFieldView.cpp`, `TheElectricField.cpp`, `Charge.{h,cpp}`, `Options.cpp` - MFC behaviour with line numbers
- `src/core/ef/*`, `tests/test_seeds.cpp`, `tests/test_presets.cpp`, `build.zig`, `build.zig.zon`, `.github/workflows/ci.yml`, `src/gui/main.cpp`, `docs/baseline/mfc/README.md` - Phase 1 state
- `.planning/research/ARCHITECTURE.md`, `PITFALLS.md`, `01-REVIEW.md`, `01-03-SUMMARY.md` - built on, not redone
- `zig-pkg/sdl-0.6.0+3.4.18-.../include/SDL3/SDL_render.h`, `SDL_surface.h` - API signatures
- `C:/Users/jmfrank/scoop/apps/zig/0.17.0/lib/std/Build/Module.zig` (`sanitize_thread`) and `Build.zig` (`lazyDependency`)
- Prototype runs: `research-ref/` (Windows 11 + WSL Ubuntu 24.04), `cl` 14.51.36231 repro, ImageMagick 7.1.2-32 metrics, `gh api` for action versions

### Secondary (MEDIUM)
- none relied on

### Tertiary (LOW)
- none

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - nothing new, versions read from `build.zig.zon`
- Architecture / port spec: HIGH - compiled, tested (54 cases), TSan-clean, parity-measured against the MFC baselines
- Pitfalls: HIGH - each was hit or measured while prototyping
- macOS arm64 behaviour: LOW/ASSUMED until the first CI run

**Research date:** 2026-10-10
**Valid until:** 2026-11-10 (Zig 0.17 / SDL 3.4.x pinned; re-check only if the pins change)
