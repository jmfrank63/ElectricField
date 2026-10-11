# Phase 2: Compute Engine & Headless Parity - Pattern Map

**Mapped:** 2026-10-10
**Files analyzed:** 36 (13 modified, 23 new; plus 1 ps1 patch and 1 deletion at close-out)
**Analogs found:** 36 / 36 (every file has a compile-tested reference in `research-ref/`; the MFC file is the numerics oracle)

Abbreviations: `REF` = `.planning/phases/02-compute-engine-headless-parity/research-ref`, `MFC` = `ElectricField/`.

## Key finding: how to use the analogs

`REF` is an overlay on the Phase 1 tree (commit 192baa8). I diffed it against the current repo (`diff --strip-trailing-cr`): the repo has no Phase 1 source changes since 192baa8, so every "changed" REF file applies cleanly. Planner rule: **copy the REF file to the same relative path, then apply only the deltas listed below.** Do not re-derive.

REF is NOT compiled by the repo build and must be deleted at phase close-out (Phase 1 precedent, commit d17dd2f). `REF/measure/*` is research only: do not copy.

## File Classification

| New/Modified File | Status | Role | Data Flow | Closest Analog (structure) | MFC source (numerics) | Match |
|---|---|---|---|---|---|---|
| `src/core/ef/params.hpp` | mod | config | n/a | `REF/src/core/ef/params.hpp` (+1 line `numThreads`) | `Options.cpp:22` | exact |
| `src/core/ef/method.hpp` | mod | utility | transform | `REF/.../method.hpp` | n/a | exact |
| `src/core/ef/rk.hpp` | mod | utility | transform | `REF/.../rk.hpp` (`computeStages`) | n/a | exact |
| `src/core/ef/tracer.hpp` | mod | service | transform/event-driven (hooks) | `REF/.../tracer.hpp` | `FieldLinesCalculator.cpp:23-150` | exact |
| `src/core/ef/bezier.hpp` | new | utility | transform | `REF/.../bezier.hpp` | `FieldLine.cpp:183-236` | exact |
| `src/core/ef/finalizer.{hpp,cpp}` | new | service | batch/transform | `REF/.../finalizer.*` | `ElectricFieldDoc.cpp:139-228` | exact |
| `src/core/ef/session.{hpp,cpp}` | new | service | event-driven (worker pool, pub-sub via `poll()`) | `REF/.../session.*` | `FieldLinesCalculator.cpp:153-205,250-282` | exact |
| `src/core/ef/scene.{hpp,cpp}` | new | model + utility | transform | `REF/.../scene.*` | `FieldLine.cpp:13-105`, `ElectricFieldView.cpp:127-137`, `Charge.cpp:27-32`, `Options.cpp:8-23` | exact |
| `src/core/ef/svg.{hpp,cpp}` | new | utility | file-I/O (string build) | `REF/.../svg.*` | n/a | exact |
| `src/render/imgui_painter.hpp` | new | component | streaming (draw list) | `REF/src/render/imgui_painter.hpp` | n/a | exact |
| `src/render/png_export.{hpp,cpp}` | new | service | file-I/O | `REF/src/render/png_export.*`; repo `src/gui/main.cpp` (`--smoke`) for SDL/ImGui init | n/a | exact |
| `src/cli/main.cpp` | new | controller (CLI) | request-response | `REF/src/cli/main.cpp`; repo `src/gui/main.cpp` for `main` conventions | n/a | exact |
| `build.zig` | mod | config | n/a | `REF/build.zig` (replace; +136 diff lines) | n/a | exact |
| `build.zig.zon` | mod | config | n/a | `REF/build.zig.zon` (+`.lazy = true` x2) | n/a | exact |
| `tests/test_params.hpp` | new | test helper | n/a | `REF/tests/test_params.hpp` | n/a | exact |
| `tests/test_tracer.cpp` | new | test | unit | `REF/tests/test_tracer.cpp`; repo `tests/test_seeds.cpp` style | `FieldLinesCalculator.cpp:37` | exact |
| `tests/test_bezier.cpp` | new | test | unit | `REF/tests/test_bezier.cpp` | `FieldLine.cpp:222` | exact |
| `tests/test_finalizer.cpp` | new | test | unit | `REF/tests/test_finalizer.cpp` | `ElectricFieldDoc.cpp:155-228` | exact |
| `tests/test_session.cpp` | new | test | integration/stress | `REF/tests/test_session.cpp` | n/a | exact |
| `tests/test_scene.cpp` | new | test | unit/integration | `REF/tests/test_scene.cpp` | n/a | exact |
| `tests/test_presets.cpp` | mod | test | unit | `REF/tests/test_presets.cpp` (D-17, 26-line diff) | `ConfigDocs.cpp` | exact |
| `tools/parity/expected-counts.csv` | new | config/data | n/a | `REF/tools/parity/expected-counts.csv` | measured | exact |
| `tools/parity/compare-mfc.sh` | new | utility (script) | batch | `REF/tools/parity/compare-mfc.sh` | `docs/baseline/mfc/*.png` | exact |
| `tools/baseline/capture-mfc-baseline.ps1` | mod | utility | batch | NO ref (text-only spec in RESEARCH D-18 row); self (existing lines 44, 50, 94, 128) | n/a | self-edit |
| `.github/workflows/ci.yml` | mod | config | n/a | `REF/.github/workflows/ci.yml` | n/a | exact |
| `research-ref/` (whole dir) | delete | n/a | n/a | n/a | n/a | close-out |
| `tests/test_rk_order.cpp` (IN-05 `#include <algorithm>`), `src/gui/main.cpp` (IN-03 `io.IniFilename = nullptr` in smoke) | mod (discretion) | test / gui | n/a | self | n/a | trivial |

## Deltas between REF and the current repo (read before copying)

1. **REF `ci.yml` keeps the step "Tests (default configuration)"** (`zig build test --summary all`). Open Question 3 is RESOLVED as "drop it". Remove that step after copying (current repo `ci.yml` lines 51-52).
2. **REF `ci.yml` uses `actions/checkout@v7` unpinned** (both jobs); RESEARCH IN-04 says "SHA-pin `actions/checkout`". Pin or leave as Phase 1 (discretion). `setup-zig` (`d1434d08...# v2.2.1`) and `upload-artifact` (`cf430e03...# v7.0.2`) are already SHA-pinned in REF.
3. **REF `build.zig` is a full replacement**, not a patch: `Ctx` struct, `coreModule()`, `GuiDeps`, `addGuiDeps` returning `?GuiDeps`, `addCli`. The Phase 1 functions `addCore/addTests/addGui` signatures change (`target, optimize` -> `ctx`). One task must own `build.zig` end to end (RESEARCH: "All build.zig edits in one task").
4. **`core_sources` and `test_sources` lists change**: add `ef/finalizer.cpp ef/session.cpp ef/scene.cpp ef/svg.cpp` and `test_tracer.cpp test_bezier.cpp test_finalizer.cpp test_session.cpp test_scene.cpp`. `bezier.hpp`, `tracer.hpp` are header-only.
5. **`FieldLine` stays in `tracer.hpp`** (Phase 1 type, reused by `finalizer.hpp`); REF adds `PotentialLine : FieldLine` there. REF keeps `traceFieldLine<Solver>` as a thin wrapper so `tests/test_seeds.cpp`/`test_rk_order.cpp` compile unchanged.
6. **D-18 (`capture-mfc-baseline.ps1`) has no REF code**: only the 3-point text spec (RESEARCH row D-18). Cannot be run without the MFC exe; verify via PowerShell AST parse + bad `-Exe` path run.
7. **D-02 amended**: `kCancelStride = 1024` (REF `tracer.hpp:39`), not 4096. CONTEXT already reflects this.
8. **REF uses `SDL_CreateSoftwareRenderer(surface)`**, not the `dummy` video driver + window path of CONTEXT D-11 wording; no `SDL_Init` needed. Phase 1 `--smoke` in `src/gui/main.cpp` keeps the dummy-driver path (unchanged).
9. **`ef-cli` is a console exe**: do NOT copy `exe.subsystem = .windows` from the GUI target (`addCli` in REF does not set it).
10. **TSan wiring**: `.sanitize_thread = if (ctx.tsan) true else null` on EVERY module of the link (core lib, tests, ef-cli) through `coreModule()`; `TSAN_OPTIONS` set on the test run step.

## Pattern Assignments

### `build.zig` (config) and `build.zig.zon`

**Analog:** `REF/build.zig` (full file, 188 lines), conventions from current `build.zig` (core flags lines 4-16, gui flags line 21).

**Keep from Phase 1 (current `build.zig:4-21`)** - strict flags are the core numerics policy:
```zig
const core_flags = [_][]const u8{
    "-std=c++17",
    "-ffp-contract=off", // never fuse a*b+c ...
    "-Wall", "-Wextra", "-Wshadow", "-Wabsolute-value", "-Wconversion", "-Wsign-conversion", "-Wuninitialized", "-pedantic", "-Werror",
};
const gui_flags = [_][]const u8{ "-std=c++17", "-DImDrawIdx=unsigned int" };  // no -Werror on 3rd-party/GUI code
```

**Option parsing + module helper (REF build.zig, new)**:
```zig
const Sanitize = enum { none, thread };
const Ctx = struct { target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, tsan: bool };
// in build():
var gui: ?GuiDeps = null;
if (with_gui) gui = addGuiDeps(b, ctx); // null while lazy deps are not fetched yet (Zig re-runs build() after the fetch)
addCli(b, ctx, core_lib, gui);
if (gui) |g| addGui(b, ctx, core_lib, g);
fn coreModule(b: *std.Build, ctx: Ctx) *std.Build.Module {
    return b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true,
        .sanitize_thread = if (ctx.tsan) true else null });
}
```

**Lazy deps (D-14)** - `addGuiDeps` must `orelse return null` (never `@panic`; Pitfall 6):
```zig
const sdl_dep = b.lazyDependency("sdl3", .{ .target = ctx.target, .optimize = ctx.optimize }) orelse return null;
const imgui_dep = b.lazyDependency("imgui", .{}) orelse return null;
```
`build.zig.zon`: add `.lazy = true,` inside the `sdl3` and `imgui` entries (REF zon lines 12 and 17); doctest stays eager.

**ef-cli PNG split** (`addCli`): with GUI deps -> `-DEF_CLI_PNG=1` + `efrender` static lib built with `gui_flags` (png_export.cpp); without -> SVG-only with `core_flags`. `b.installArtifact(exe)`.

**Test run step:** `if (ctx.tsan) run.setEnvironmentVariable("TSAN_OPTIONS", "halt_on_error=1:second_deadlock_stack=1");` keep Phase 1 `run.setCwd(b.path("."))` (test_presets reads `ElectricField/ConfigDocs.cpp` relative to repo root).

---

### `src/core/ef/method.hpp` (utility) - D-15

**Analog:** `REF/src/core/ef/method.hpp` (diff vs repo = 3 hunks). Add `#include <optional>`, `<stdexcept>`:
```cpp
inline constexpr const MethodInfo& info(Method m) { return kMethods.at(static_cast<std::size_t>(m)); }
inline constexpr std::optional<Method> methodFromName(std::string_view name) {
  for (const auto& mi : kMethods) if (mi.name == name) return mi.method;
  return std::nullopt;
}
// end of withSolver (replaces the Euler fallback at repo method.hpp ~69-70):
throw std::invalid_argument("ef::withSolver: invalid Method value");
```

### `src/core/ef/rk.hpp` (utility) - D-16

**Analog:** `REF/src/core/ef/rk.hpp` lines ~69-98. One `computeStages(f, curVal, t, h, sumHigh, sumLow)` called from both `SolveStepPair` and `SolveStep` (replaces duplicated loop in repo `rk.hpp:71-96`). The DP "low weight" quirk (`m_low`, D-06) stays untouched. Parity check: existing `tests/test_rk_order.cpp` must pass unchanged.

### `src/core/ef/params.hpp`

Add one member after `method` (REF line 13): `unsigned numThreads = 4;  // Options::numThreads (Options.cpp:22)`. `sanitized()` (IN-02) is optional/discretion (Phase 4 loader's job).

---

### `src/core/ef/tracer.hpp` (service, transform + hooks)

**Analog:** `REF/src/core/ef/tracer.hpp` (195 lines) over current repo `tracer.hpp` (87 lines). **MFC oracle:** `FieldLinesCalculator.cpp:23-49` (post), `:51-110` (E-line), `:113-150` (equipotential).

Unchanged from Phase 1: `FieldLine::addPoint` thinning (repo lines 17-27; D-08 mixes pixel `dif` and world `len` on purpose), `FieldDirection` functor.

**New pieces to copy verbatim from REF:**
- `PotentialLine : FieldLine { double potential; Vec2 weightCenter; }` (REF 32-35; `potential` is the LABEL = `job.old_potential`).
- `inline constexpr std::size_t kCancelStride = 1024;` and `NoHooks` (REF 39-45).
- `PerpendicularDirection` (FunctorForV, REF 56-65).
- `shouldPostEquipotential(...)` (REF 73-85) - the CORE-07 fix for `FieldLineJob job(job)` (`FieldLinesCalculator.cpp:37`); child carries parent's current point + UPDATED label:
```cpp
if (!seed.first || !p.calculateEquipotentials) return false;
const double potential = field.Potential(point);
if (sign(seed.charge) * sign(potential) > 0 && std::abs(oldPotential - potential) >= p.potentialInterval) {
  if (oldPotential == 0) oldPotential = std::floor(potential / p.potentialInterval) * p.potentialInterval;
  else oldPotential += sign(potential - oldPotential) * p.potentialInterval;
  return true;
}
return false;
```
- Cancel poll idiom inside both loops (REF 117-120, 165-168): `if ((i % kCancelStride) == 0 && hooks.cancelled()) { if (status) status->cancelled = true; break; }`
- Post call after `t += step` (REF 139): `if (shouldPostEquipotential(field, p, seed, point, oldPotential)) hooks.postEquipotential(point, oldPotential);`
- `traceEquipotential` (REF 146-185): budget `solver.IsAdaptive() ? 800000 : 1500000`, step `0.001`/`0.0001`, `SolveStep(functorV, point, t, step, next_step, 1E-3, 0.01)`, `weightCenter += point * step`, closing rule `dist * px < 6. && t > 2.*dist` -> push `startPoint`, `weightCenter /= t`. Quirk: budget exhaustion leaves the undivided sum.
- Keep `traceFieldLine<Solver>` as `NoHooks` wrapper (REF 188-193).

Needs `#include <cmath>` (REF line 5). Warnings are errors: the `-Wshadow` flag is what catches a reintroduced `job(job)`.

---

### `src/core/ef/bezier.hpp` (utility, transform) - D-07

**Analog:** `REF/src/core/ef/bezier.hpp` (63 lines). **MFC:** `FieldLine.cpp:183-236` (`ASSERT(abs(divi) > DBL_MIN)` at 222 is debug-only).

Core excerpt to copy:
```cpp
inline constexpr double kBezierMinDivisor = 1e-9;       // RESOLVED Q4: keep 1e-9
// PARITY QUIRK (D-07): t3 = t1 + t2 + |pt3-pt2| (t2 already contains t1); do not "fix" it.
const double divi = 3. * t1 * t2 * (1. - t1) * (1. - t2) * (t2 - t1);
if (!(std::abs(divi) > kBezierMinDivisor)) return false;   // also catches NaN (t3 == 0)
...
for (std::size_t i = 0; i + 3 < points.size(); i += 3) {   // groups share endpoints; trailing <4 points untouched
```
Returns `false` (inputs untouched) on degenerate input or non-finite output; the vector overload returns the fallback count.

---

### `src/core/ef/finalizer.{hpp,cpp}` (service, batch) - D-05

**Analog:** `REF/src/core/ef/finalizer.{hpp,cpp}` (25 + 74 lines). **MFC:** `ElectricFieldDoc.cpp:155-228` (`GetDataFromThreads`), ref point at `:185`, 2/3 rule at `:211`.

Port-faithful points (do not "clean up"):
```cpp
if (field.charges.size() == 2 && hasDifferentSigns) return raw;           // shortcut: unsorted
std::stable_sort(... a.potential < b.potential);                          // stable => D-04 determinism (MFC: std::sort)
const Vec2 ref(-7., 5.5);
if (std::abs(potential - line.potential) < interval / 3. || 0 == potential) { potential = line.potential; equipotentials.push_back(line); }
else { /* sort group by (ref - weightCenter).Length(); push iff
         (out.back().weightCenter - eq.weightCenter).Length() * px > 2. * p.chargeRadiusPx
         || abs(potential - out.back().potential) > interval * 2. / 3. */ }
// the last group is appended WITHOUT de-dup (original quirk)
out.insert(out.end(), std::make_move_iterator(equipotentials.begin()), std::make_move_iterator(equipotentials.end()));
```
`finalize()` drops empty (sink) E-lines, de-dups, then `adjustForBezier` on both sets. `FieldResult{electric, potential}` lives in `finalizer.hpp`. Units: `theApp.options.distanceUnitLength/chargeRadius` -> `p.pixelsPerUnit` / `p.chargeRadiusPx`. Expected effect (test oracle): Quadrupole keeps 142->142, Parallel dipoles loses 150->136.

---

### `src/core/ef/session.{hpp,cpp}` (service, event-driven worker pool) - D-01..D-04

**Analog:** `REF/src/core/ef/session.{hpp,cpp}` (107 + 218 lines). **MFC (replaced):** `FieldLinesCalculator.cpp:153-205` (`CalcThread`, `delete this`, `finishedThreads` restart hack lines 41-47, 277-282), `ElectricFieldDoc.cpp:237-251` (detached-thread teardown).

Patterns to copy as-is (they are the contract, see RESEARCH Patterns 1-3):

**Worker wait predicate / termination (in-flight counter):**
```cpp
cv_.wait(lk, [this] { return stop_.load(std::memory_order_relaxed) || !queue_.empty() || inFlight_ == 0; });
if (stop_.load(std::memory_order_relaxed) || queue_.empty()) return;  // nothing can appear any more
job = std::move(queue_.front()); queue_.pop_front(); ++inFlight_;
```
**Cancel without lost wake-up (Pitfall 3):**
```cpp
void ComputeSession::cancel() {
  stop_.store(true);
  { std::lock_guard<std::mutex> lk(m_); }   // pairs with the predicate check under the lock
  cv_.notify_all(); doneCv_.notify_all();
}
```
**RAII teardown** (`~ComputeSession`: `cancel()` then `join()` every worker; constructor catches `emplace_back` failure, stops and joins, rethrows).
**Per-worker solver:** `withSolver(params_.method, [this](auto& solver) { workerLoop(solver); })` inside try/catch -> `fail(e.what())` (so an invalid `Method` becomes `failed` + `error`, D-15). `K[]` is per-instance state; never share solvers.
**Last worker finalizes:** `if (activeWorkers_.fetch_sub(1, std::memory_order_acq_rel) == 1) finish();` - sorts by `JobKey{parent=seedIdx, child}` (D-04), copies to raw vectors, `ef::finalize`.
**Child posting hook** (`JobHooks::postEquipotential`) pushes under `m_`, `++jobsKnown_`, `cv_.notify_one()`; child key = `{job->key.parent, nextChild++}`.
**Published lines are `shared_ptr<const Keyed...>`** in both `allE_/allP_` and `pendingE_/pendingP_`; `poll(PollBatch&)` is drain-and-swap under the lock; `wait()` on `doneCv_`; `takeResult()` is true once.

Public API for Phase 3: `ComputeSession(ChargeSystem, ComputeParams)`, `cancel()`, `poll(PollBatch&)`, `wait()`, `takeResult(FieldResult&)`, `SessionStatus{done,cancelled,failed,error,jobsCompleted,jobsKnown}`. No `onWake` callback (RESOLVED Q5).

---

### `src/core/ef/scene.{hpp,cpp}` (model + utility, transform) - D-11

**Analog:** `REF/src/core/ef/scene.{hpp,cpp}` (76 + 103 lines). **MFC:** draw order `TheElectricField.cpp:21-31`, `Clear(White)` `ElectricFieldView.cpp:129`, transform `:134`, colours/defaults `Options.cpp:8-23`, charge `Charge.cpp:27-32`, path building `FieldLine.cpp:13-105`.

API shapes to preserve (Phase 3 reuses them): `Color`, `DrawStyle` (MFC defaults: +charge `{0,0,255}`, -charge `{255,0,0}`, E `{0,0,0}`, potential `{192,192,192}`, background white, thickness 1, radius 25), `View{1024,768,100.}`, `ScenePath{points, closed}`, `FieldScene`, `buildScene(field, result)`, abstract `Painter{begin, strokePath(pts,n,bezierSegments,closed,color,width), fillCircle, end}`, `renderScene(...)`.

**Transform and cull (scene.cpp 41-84):**
```cpp
dev[i].x = static_cast<float>(cx + path.points[i].X * view.pixelsPerUnit);
dev[i].y = static_cast<float>(cy - path.points[i].Y * view.pixelsPerUnit);  // y up
const std::size_t segs = (n - 1) / 3, tail = (n - 1) % 3;  // trailing 1-2 points => straight lines
flush(path.closed && !culledAny);
```
Per-cubic-segment culling (control-polygon bbox vs view +-10 px) instead of MFC's index-misaligning clip (RESEARCH section 8); draw order electric, potential, charges. `closed` = `points.size() > 1 && front == back` (scene.cpp:14).

---

### `src/core/ef/svg.{hpp,cpp}` (utility, string building)

**Analog:** `REF/src/core/ef/svg.{hpp,cpp}` (25 + 93 lines). `SvgPainter : Painter`, `renderSvg(scene, style, view)`. Number formatting is `snprintf("%.2f")` + trailing-zero trim (never `std::to_chars` for floats; locale stays "C"). One `<path fill="none" stroke="#rrggbb" ... d="M x y C ... L ... [Z]">` per visible run, `<circle>` per charge, device coordinates written directly (no transform group).

---

### `src/render/imgui_painter.hpp` and `src/render/png_export.{hpp,cpp}` (component / service, file-I/O)

**Analog:** `REF/src/render/*`; SDL/ImGui software-renderer init idiom already in current `src/gui/main.cpp` (`--smoke` path, two-frame font-atlas warm-up).

**Painter mapping (imgui_painter.hpp 20-31):**
```cpp
dl_->PathLineTo(pt(p[0]));
for (s...; i += 3) dl_->PathBezierCubicCurveTo(pt(p[i]), pt(p[i+1]), pt(p[i+2]), 0);
for (; i < n; ++i) dl_->PathLineTo(pt(p[i]));
dl_->PathStroke(col(c), closed ? ImDrawFlags_Closed : ImDrawFlags_None, width);
void fillCircle(...) { dl_->AddCircleFilled(pt(center), radius, col(c), 0); }
```
**PNG export (png_export.cpp 63-106):** `SDL_CreateSurface(RGBA32)` -> `SDL_CreateSoftwareRenderer(surface)` -> own ImGui context (save/restore previous) -> `io.IniFilename = nullptr` -> `style.CurveTessellationTol = 0.3f` -> **`style.AntiAliasedLinesUseTex = false`** (mandatory; Pitfall 5, shared with Phase 3) -> `ImGui_ImplSDLRenderer3_Init` -> 2 frames into `GetBackgroundDrawList()` -> `SDL_RenderReadPixels` -> `SDL_SavePNG`. RAII via `unique_ptr` with custom deleters; errors returned as `std::string` (empty = ok), no exceptions across the render boundary. Compile with `gui_flags` (needs `-DImDrawIdx=unsigned int`, same define in every TU including `imgui.h`).

---

### `src/cli/main.cpp` (controller, request-response) - D-10, D-12

**Analog:** `REF/src/cli/main.cpp` (181 lines). `main(int, char**)` style matches current `src/gui/main.cpp` but stays a console exe.

Patterns:
- Arg parse with a `next()` lambda, bounds `--threads 1..128`, `--width/--height 16..8192` (parse failure exit 2; unknown preset exit 2; compute/IO failure exit 1).
- Method lookup via `ef::methodFromName` (returns `optional`; unknown -> error, no silent default).
- Compute path: `ComputeSession session(field, params); st = session.wait(); if (st.failed) ...; session.takeResult(result); buildScene; renderSvg / efrender::renderPng`.
- Summary line (CI/scripts parse this): `preset=<slug> charges=N field_lines=N equipotentials=N ms=N`.
- `--all --out-dir D [--expect FILE]`: charges and field_lines exact, equipotentials `+-max(2, 3%)`, prints `PARITY FAIL ...` to stderr, exit 1. PNG only under `#ifdef EF_CLI_PNG`.
- Windows `--list` prints CRLF: scripts must `tr -d '\015'` (Pitfall 7).

---

### Tests (`tests/test_*.cpp`) - doctest

**Repo convention (all existing tests, e.g. `tests/test_seeds.cpp:1-30`):** `#include "doctest/doctest.h"`, `using namespace ef;`, anonymous-namespace helpers, `CAPTURE(slug)` inside preset loops, tolerance/invariant assertions, `tests/test_main.cpp` provides the doctest main. `tests/test_params.hpp` adds `lightParams()` (4 lines/unit, interval 0.5, 4 threads) - **use it for every preset sweep** (Pitfall 1: full defaults make Debug take ~5 min).

| Test file | Copy from | Must assert (REQ) |
|---|---|---|
| `test_tracer.cpp` | `REF/tests/test_tracer.cpp` | posted child state == parent state (finite, on parent line, label within one interval, sign(label)==sign(charge), labels advance by one interval; CORE-04/07, D-09); `static_assert(kPi == M_PI)`-style M_PI and `abs(double)` regressions; FMA canary `volatile double x = 1.0 + 0x1p-30, y = 1.0 - 0x1p-30; CHECK(x*y - 1.0 == 0.0)`; no `__FAST_MATH__` |
| `test_bezier.cpp` | `REF/tests/test_bezier.cpp` | passes through pt1 at t1 and pt2 at t2 (1e-9); coincident points -> `false`/untouched; group stride/trailing points |
| `test_finalizer.cpp` | `REF/tests/test_finalizer.cpp` | shortcut, grouping with `interval/3`, last-group quirk |
| `test_session.cpp` | `REF/tests/test_session.cpp` | 1 vs 7 threads equal (1e-12); `poll()` exactly-once; empty system; cancel < 200 ms (1000 ms under TSan via `__SANITIZE_THREAD__`/`__has_feature`); destroy running session for all 11 methods; invalid Method -> failed; 60-session random cancel stress; Quadrupole loses nothing, Parallel dipoles loses duplicates; per-preset invariants (QA-02, sign(0)==+1) |
| `test_scene.cpp` | `REF/tests/test_scene.cpp` | y-up/centre/100 px/unit, colours, order, cull, closed; every preset renders a finite SVG |
| `test_presets.cpp` (mod) | `REF/tests/test_presets.cpp` | D-17: `REQUIRE_MESSAGE(in.good(), ...)` instead of skip (repo lines 26-29); reset `cur = PresetCharge{1, 0, 0}` on each `IMPLEMENT_DYNCREATE`; `REQUIRE(!docs.empty())` before `docs.back()` |

---

### `tools/parity/expected-counts.csv` and `compare-mfc.sh`

**Analog:** `REF/tools/parity/*`. CSV header `slug,charges,field_lines,equipotentials`, 9 rows for default Euler (dipole 2/16/75 ... parallel-dipoles 4/32/136; full table in RESEARCH "Expected counts"). The first macOS arm64 run is the confirmation (A1/Q2): on mismatch adjust tolerance or re-baseline from `parity-macos-arm64/summary.txt`; never skip. `compare-mfc.sh <ef-cli> [out-dir] [baseline-dir]` is ImageMagick-only, informational, never a CI gate (D-13); metrics `ae_fuzz10, ink_recall, ink_precision, darkness_ratio`.

---

### `tools/baseline/capture-mfc-baseline.ps1` (D-18, no REF code)

Edit self at: `PrintWindow` call (line 94: capture `$ok = [W]::PrintWindow(...)`, on false dispose bitmap and `return $null`; polling loop must `continue`, the final capture must `throw`), `Dark` (line 50: count only when BGRA alpha byte `buf[i+3] == 255`), before `Start-Process` (line 128): `if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Exe not found: $Exe" }`. Style precedent in the same file: `Test-Path -LiteralPath` (line 113) and `throw "..."` (line 117).

---

### `.github/workflows/ci.yml` (config)

**Analog:** `REF/.github/workflows/ci.yml` (apply then fix deltas 1 and 2 above). Keep Phase 1 structure (matrix `windows-x64/linux-x64/macos-arm64`, `fail-fast: false`, `permissions: contents: read`, concurrency group, setup-zig SHA pin). Additions:
- `defaults.run.shell: bash` on the matrix job.
- Order: Core tests Debug (`-Dgui=false`) -> Core tests ReleaseFast -> `zig build -Doptimize=ReleaseFast --summary all` -> `zig build smoke -Doptimize=ReleaseFast` -> parity step -> upload.
- Parity: `mkdir -p parity-out; ./zig-out/bin/ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv | tee parity-out/summary.txt` (relies on bash `pipefail`), then `actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9 # v7.0.2`, `if: always()`, `name: parity-${{ matrix.name }}`, `retention-days: 14`.
- `push.branches: [feature/cross-platform, master]` (IN-04).
- New `tsan-linux` job (ubuntu-latest, `timeout-minutes: 30`): sysctl precaution `sudo sysctl -w vm.mmap_rnd_bits=28 || true`, `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe --summary all`, then `zig build -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` and `ef-cli --preset parallel-dipoles --threads 8 --svg /tmp/p.svg`. Never pass `-Dtarget` on macOS.

## Shared Patterns

### Numerics policy (all `src/core` and `tests`)
**Source:** `build.zig` `core_flags` (current lines 4-16). `-ffp-contract=off` is load-bearing (Clang default is contraction ON); strict warnings as errors; std-only, no globals, no MFC. **Apply to:** every new core `.cpp`, tests, and the CLI (REF `cli_flags = core_flags`). GUI/render code uses `gui_flags` (no `-Werror`).

### Parity-quirk documentation
**Source:** REF `tracer.hpp`, `bezier.hpp`, `finalizer.cpp` comments: each retained MFC quirk carries a `PARITY QUIRK` / "original behaviour" comment plus the MFC file:line, and a characterisation test pins it. Apply to D-05..D-08 code (t3 formula, last-group no-dedup, unsorted two-opposite-charge shortcut, pixel/world mixing, DP low weights).

### Error handling
**Source:** REF `session.cpp:86-95`, `png_export.cpp`, `cli/main.cpp`. Core throws `std::exception` subclasses only for programmer errors (`out_of_range`, `invalid_argument`); workers catch everything and convert to `SessionStatus.failed/error`; the render layer returns an error `std::string`; the CLI maps to exit codes 0/1/2. No MFC `ASSERT`.

### Threading / ownership
**Source:** REF `session.{hpp,cpp}`. `std::thread` + `std::mutex` + `std::condition_variable` + `std::atomic` only (no `jthread`), RAII join in destructor, immutable `shared_ptr<const ...>` hand-off, one solver per worker, no detached threads/`delete this`.

### Test sizing
**Source:** `REF/tests/test_params.hpp`. Sweep tests use `lightParams()`; only `ef-cli` in ReleaseFast (CI) runs full MFC defaults. Latency assertions widen 5x under TSan.

### One draw path
**Source:** `ef::renderScene` + `ef::Painter`; `efrender::ImGuiPainter` is the only ImDrawList mapping; Phase 3 reuses it with `ImGui::GetWindowDrawList()` and must set `AntiAliasedLinesUseTex = false` and `CurveTessellationTol = 0.3f`.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| D-18 patch of `capture-mfc-baseline.ps1` | script | batch | No REF code (cannot run without the MFC exe); follow the three-point spec above and verify by parse + bad-path run |
| macOS arm64 execution of any of the above | CI | n/a | Unverified (A1); first CI run is the confirmation |

## Metadata

**Analog search scope:** `research-ref/` (all 33 files), current `src/core/ef`, `src/gui`, `tests`, `tools/baseline`, `build.zig(.zon)`, `.github/workflows/ci.yml`, `ElectricField/` (FieldLinesCalculator, ElectricFieldDoc, FieldLine line-level greps)
**Files scanned:** ~45 (REF-vs-repo diffs for build.zig, zon, params, method, rk, tracer, test_presets, ci.yml)
**Pattern extraction date:** 2026-10-10
