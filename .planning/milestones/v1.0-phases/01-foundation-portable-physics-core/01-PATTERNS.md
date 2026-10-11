# Phase 1: Foundation & Portable Physics Core - Pattern Map

**Mapped:** 2026-10-10
**Files analyzed:** 31 new (0 MFC files modified; D-01/D-02), plus 2 git-hygiene actions on existing files
**Analogs found:** 31 / 31 (all have a validated reference-impl analog; 13 also have an MFC numerics source)

Two analog families:
- **REF** = `.planning/phases/01-foundation-portable-physics-core/research-ref/` (validated on Windows + Ubuntu; copy structure from here). Abbreviated `REF/`.
- **MFC** = `ElectricField/` (numerical source of truth; never edited, only ported/cross-checked).

All new files go to the repo root (D-01). REF paths map 1:1 to repo-root paths (`REF/src/core/ef/vec2.hpp` -> `src/core/ef/vec2.hpp`).

## File Classification

| New File | Role | Data Flow | REF analog (structure) | MFC analog (numerics) | Match |
|---|---|---|---|---|---|
| `build.zig` | config | build graph | `REF/build.zig` | n/a (`ElectricField.vcxproj` only as contrast) | exact |
| `build.zig.zon` | config | dependency pin | `REF/build.zig.zon` (literal in RESEARCH.md l.128-150) | n/a | exact |
| `.gitignore` (rewrite) | config | n/a | `REF/gitignore.proposed` | existing `.gitignore` (anchored, broken) | exact |
| index cleanup (`git rm -r --cached ...`) | config | n/a | RESEARCH.md BUILD-05 (37 files + `.vcxproj.user`) | tracked `ElectricField/x64/*` | exact |
| `.github/workflows/ci.yml` | config | event-driven (push/PR) | `REF/ci.yml` | n/a | exact |
| `src/core/ef/math.hpp` | utility | transform | `REF/src/core/ef/math.hpp` | `RungeKutta.h:7` (`sign` macro), `FieldLinesCalculator.cpp:7` (`M_PI`) | exact |
| `src/core/ef/vec2.hpp` | model | transform | `REF/src/core/ef/vec2.hpp` | `Vector2D.h` + `Vector2D.cpp` | exact |
| `src/core/ef/rk.hpp` | service (numerics) | transform | `REF/src/core/ef/rk.hpp` | `RungeKutta.h` + `RungeKutta.inl` | exact |
| `src/core/ef/method.hpp` | model/dispatch | request-response | `REF/src/core/ef/method.hpp` | `Options.h:6-18`; `FieldLinesCalculator.cpp` `StartComputingThread` switch | exact |
| `src/core/ef/params.hpp` | model (value type) | n/a | `REF/src/core/ef/params.hpp` | `Options.h:22-33` + `theApp.options` reads | exact |
| `src/core/ef/charge_system.hpp` | model | transform | `REF/src/core/ef/charge_system.hpp` | `Charge.h`, `TheElectricField.cpp` | exact |
| `src/core/ef/seeds.hpp`, `seeds.cpp` | service | transform | `REF/src/core/ef/seeds.{hpp,cpp}` | `FieldLinesCalculator.cpp:223-259`, `:261-` | exact |
| `src/core/ef/presets.hpp`, `presets.cpp` | model (data table) | CRUD-read | `REF/src/core/ef/presets.{hpp,cpp}` | `ConfigDocs.cpp`, `ElectricField.rc:1082-1099` | exact |
| `src/core/ef/tracer.hpp` | service | transform (loop) | `REF/src/core/ef/tracer.hpp` | `FieldLinesCalculator.cpp:51-110`, `FieldLine.h:18-29` | exact |
| `src/gui/main.cpp` | entry point | event-driven (UI loop) | `REF/src/gui/main.cpp` | n/a | exact |
| `tests/test_main.cpp` | test | n/a | `REF/tests/test_main.cpp` | n/a | exact |
| `tests/test_vec2.cpp` | test | transform | `REF/tests/test_vec2.cpp` | `Vector2D.*` | exact |
| `tests/test_field.cpp` | test | transform | `REF/tests/test_field.cpp` | `Charge.h`, `TheElectricField.cpp` | exact |
| `tests/test_rk_order.cpp` | test | transform | `REF/tests/test_rk_order.cpp` | `RungeKutta.inl` tableaus | exact |
| `tests/test_seeds.cpp` | test | transform | `REF/tests/test_seeds.cpp` | `FieldLinesCalculator.cpp:229-250` | exact |
| `tests/test_presets.cpp` | test | file-I/O (parses MFC source) | `REF/tests/test_presets.cpp` | `ConfigDocs.cpp` | exact |
| `tools/baseline/capture-mfc-baseline.ps1` | utility/script | file-I/O + Win32 messages | `REF/tools/baseline/capture-mfc-baseline.ps1` | MFC `ElectricField.exe` (built via MSBuild) | exact |
| `docs/baseline/mfc/*.png` (9) + `README.md` | artifact | file-I/O | script output; README template in RESEARCH.md (QA-05 section, lines 452+) | n/a | exact |

## Pattern Assignments

### `build.zig` (config, build graph)

**Analog:** `REF/build.zig` (copy verbatim, then run once to confirm). Zig 0.17 `Module` API, not `Compile.addCSourceFiles`.

**Flag split (lines 3-20)** - numerics policy lives here, applied to core AND tests, never to ImGui/SDL:
```zig
const core_flags = [_][]const u8{
    "-std=c++17",
    "-ffp-contract=off", // never fuse a*b+c
    "-Wall", "-Wextra", "-Wshadow", "-Wabsolute-value",
    "-Wconversion", "-Wsign-conversion", "-Wuninitialized", "-pedantic", "-Werror",
};
const gui_flags = [_][]const u8{ "-std=c++17", "-DImDrawIdx=unsigned int" };
```
(one flag per line in the real file; keep `-DImDrawIdx` identical in every TU including imgui.h.)

**Core lib (lines 47-54):** `b.createModule(.{ .target, .optimize, .link_libcpp = true })`, `mod.addCSourceFiles(.{ .root = b.path("src/core"), .files = &core_sources, .flags = &core_flags })`, `b.addLibrary(.{ .name = "efcore", .root_module = mod, .linkage = .static })`. Explicit file lists, no globbing (`core_sources = {"ef/seeds.cpp","ef/presets.cpp"}`).

**Tests (lines 57-68):** doctest via `mod.addSystemIncludePath(doctest_dep.path("."))`; `run.setCwd(b.path("."))` is required because `test_presets.cpp` opens `ElectricField/ConfigDocs.cpp` relative to the repo root.

**GUI (lines 71-117):** `exe.subsystem = .windows`; `run.addPassthruArgs()` (0.17 replacement of `b.args`); `smoke` step = second `addRunArtifact(exe)` with `addArg("--smoke")` + `setEnvironmentVariable("SDL_VIDEODRIVER","dummy")` / `("SDL_RENDER_DRIVER","software")`. `-Dgui=false` option (line 39) gates `addGui` so the core gate does not fetch SDL/ImGui.

**Rule:** never pass `-Dtarget` anywhere (macOS native only).

---

### `build.zig.zon` (config)

**Analog:** `REF/build.zig.zon`; literal content is in 01-RESEARCH.md lines 128-150. Contains fingerprint `0x640049fcb48a860b` (never change), `.minimum_zig_version = "0.17.0"`, three hash-pinned deps (sdl3 commit `dbcb19aa...`, imgui `v1.92.9b-docking`, doctest `v2.5.3`). Do not run `zig fetch --save` against `release-3.4.x`.

---

### `.gitignore` and index cleanup (config)

**Analog:** `REF/gitignore.proposed` (copy as the new `.gitignore`):
```
.zig-cache/
zig-out/
zig-pkg/
.vs/
x64/
Debug/
Release/
*.user *.suo *.obj *.pdb *.ilk *.pch *.idb *.tlog *.ipch *.VC.db ...  (one per line in file)
/build/
/out/
*.log
```
Patterns are unanchored on purpose (MFC writes to both `<repo>/x64/` and `ElectricField/x64/`). Commit order (D-13): (1) `git rm -r --cached ElectricField/x64 ElectricField/ElectricField.vcxproj.user` + `.gitignore` in one commit; (2) the pending `v143 -> v145` `ElectricField.vcxproj` change in a separate commit. Verify with `git ls-files -ci --exclude-standard` (must be empty).

---

### `.github/workflows/ci.yml` (config, event-driven)

**Analog:** `REF/ci.yml` (copy verbatim). Key patterns:
```yaml
on:
  push: { branches: [feature/cross-platform] }
  pull_request:
  workflow_dispatch:
permissions: { contents: read }
strategy:
  fail-fast: false
  matrix: { include: [ {name: windows-x64, os: windows-latest}, {name: linux-x64, os: ubuntu-latest}, {name: macos-arm64, os: macos-latest} ] }
steps:
  - uses: actions/checkout@v7
  - uses: mlugg/setup-zig@d1434d08867e3ee9daa34448df10607b98908d29 # v2.2.1
    with: { version: 0.17.0 }
  - run: zig build test -Dgui=false --summary all
  - run: zig build test -Dgui=false -Doptimize=ReleaseFast --summary all
  - run: zig build --summary all
  - run: zig build test --summary all
  - run: zig build smoke --summary all
```
(file is the multi-line form; above is condensed.) Core gate before GUI gate so SDL failures cannot mask physics failures. Pushing triggers it: ask the user first (D-12).

---

### `src/core/ef/math.hpp` (utility)

**Analog:** `REF/src/core/ef/math.hpp` (lines 7-12). **MFC source:** `RungeKutta.h:7` `#define sign(x) ((x) >= 0 ? 1 : -1)`; `FieldLinesCalculator.cpp:7` `const double M_PI`.
```cpp
inline constexpr double kPi = 3.1415926535897932384626433832795028841971693993751;
inline constexpr int sign(double x) noexcept { return x >= 0 ? 1 : -1; }  // sign(0) == +1 is load-bearing
```

---

### `src/core/ef/vec2.hpp` (model)

**Analog:** `REF/src/core/ef/vec2.hpp` (all 45 lines). **MFC source:** `Vector2D.h:3-43`, `Vector2D.cpp`.
Differences from MFC (port decisions, all in REF): drop `stdafx.h`/`DEBUG_NEW`/`_VECTOR_2D_IMPL` include dance; `explicit constexpr Vector2D(const T& v)`; drop mixed-type template ops and the broken `operator*=(Vector2D<O>)`; `std::sqrt`; `abs(Vector2D)` (`Vector2D.h:43`) is removed and replaced by `rk::magnitude` in rk.hpp.
```cpp
namespace ef {
template <typename T> struct Vector2D {
  T X{}; T Y{};
  constexpr T operator*(const Vector2D& o) const { return X * o.X + Y * o.Y; }  // dot
  double Length() const { const double w = *this * *this; return std::sqrt(w); }
  Vector2D Normalize() const { const T len = static_cast<T>(Length()); if (len != 0) return *this / len; return *this; }
};
using Vec2 = Vector2D<double>;
}
```
Keep operation order identical to MFC for bit-level parity (`Length` = sqrt of dot product).

---

### `src/core/ef/rk.hpp` (service, numerics)

**Analog:** `REF/src/core/ef/rk.hpp`. **MFC source:** `RungeKutta.h:11-91` (algorithm), `RungeKutta.inl` (tableaus; tableaus copied verbatim).

**Core stepping (REF lines 36-46)** equals MFC `RungeKutta.h:22-35`; takes `Func&&` (MFC took `Func&`, rejecting temporaries):
```cpp
template <typename Func>
T SolveStepFixed(Func&& f, const T& curVal, double t, double h) {
  T thesum(0.0);
  for (unsigned stage = 0; stage < Stages; ++stage) {
    T accum(0.0);
    for (unsigned j = 0; j < stage; ++j) accum += m_tab.coeff[stage - 1][j] * K[j];
    K[stage] = f(t + m_tab.nodes[stage] * h, curVal + h * accum);
    thesum += m_tab.weights[stage] * K[stage];
  }
  return curVal + h * thesum;
}
```
**Adaptive step control (REF lines 83-119)** equals MFC `RungeKutta.h:53-88`; only change is `std::abs`/`std::pow`/`rk::magnitude`:
```cpp
const double error = magnitude((sumHigh - sumLow) * h);
if (error <= tolerance || loop > 1000 || std::abs(h) <= min_step) {
  T result = curVal + h * sumHigh;   // HIGH-order solution is stepped with
  if (error <= DBL_MIN) next_h = h * 2.;
  else if (error < tolerance) next_h = h * 0.9 * std::pow(tolerance / error, 1. / (Order + 1));
  else next_h = h;
  ...clamp to [min_step, max_step] using sign()...
  return result;
}
h *= 0.9 * std::pow(tolerance / error, 1. / Order);
```
**Tableaus (REF lines 124-167):** `inline constexpr Tableau<N>` aggregates replace MFC's `NULL`/pointer tables (`RungeKutta.inl:41`). `dpLow` (REF line 166) intentionally keeps the MFC error-coefficient defect (RESEARCH defect 15); pin it with the characterisation test, do not "fix".
**Test hooks added vs MFC:** `SolveStepFixed` public on the base, `SolveStepPair` on the adaptive class. Required includes: `<cfloat>`, `<cmath>` (MFC had neither). Rule: one solver instance per thread (member `K` is stage state).

---

### `src/core/ef/method.hpp` (model/dispatch)

**Analog:** `REF/src/core/ef/method.hpp`. **MFC source:** `Options.h:6-18` (enum order), `ElectricField.rc:740-750` (display captions), `FieldLinesCalculator.cpp` `StartComputingThread` switch.
```cpp
enum class Method { Euler, Midpoint, Ralston, Heun, RK4, RK3per8, AdaptiveHeunEuler,
  AdaptiveBogackiShampine, AdaptiveCashKarp, AdaptiveFehlberg, AdaptiveDormandPrince };
inline constexpr std::array<MethodInfo, kMethodCount> kMethods{{ {Method::Euler, "euler", "Euler", false, 1}, ... }};
template <typename V> decltype(auto) withSolver(Method m, V&& visitor) {
  using S = Vec2;
  switch (m) { case Method::Euler: { rk::Euler<S> s; return visitor(s); } ... }
}
```
Names are the stable persistence keys (Phase 4); numeric enum values are not persisted, but their order must stay equal to the MFC enum.

---

### `src/core/ef/params.hpp` (value type)

**Analog:** `REF/src/core/ef/params.hpp` lines 7-19. **MFC source:** every `theApp.options` read: `Charge.h:25`, `FieldLine.h:23`, `FieldLinesCalculator.cpp:12,231,239,245,254-258`; fields from `Options.h:22-33`.
```cpp
struct ComputeParams {
  unsigned pixelsPerUnit = 100;  double chargeRadiusPx = 25.0;  unsigned linesPerUnitCharge = 16;
  bool calculateEquipotentials = true;  double potentialInterval = 0.1;  Method method = Method::Euler;
  double seedRadius() const { return chargeRadiusPx / static_cast<double>(pixelsPerUnit); }       // 0.25
  double hitRadius()  const { return chargeRadiusPx / 1.5 / static_cast<double>(pixelsPerUnit); }  // Charge.h:25 expression
};
```
Keep the original operation order (do not hard-code 0.25). No `numThreads` (Phase 2). Verify defaults against `Options.cpp` before finalising (`linesPerUnitCharge`, `potentialInterval`, default method).

---

### `src/core/ef/charge_system.hpp` (model)

**Analog:** `REF/src/core/ef/charge_system.hpp`. **MFC source:** `Charge.h:23-46` (math), `TheElectricField.cpp` (E, Potential, ENormalized, `GetTotalCharge` at ~65-80 per RESEARCH; line 70 derefs empty `begin()`).
```cpp
Vec2 E(const Vec2& pos) const {
  const Vec2 toPos = pos - position;
  const double len2 = toPos * toPos;
  Vec2 result = toPos / (len2 * std::sqrt(len2));
  result *= charge;  return result;
}
int totalCharge(bool& hasDifferentSigns) const {
  hasDifferentSigns = false;
  if (charges.empty()) return 0;                       // new guard (MFC defect 12)
  int s = charges.front().charge < 0 ? -1 : 1;
  ...  if (s != (c.charge > 0 ? 1 : -1)) hasDifferentSigns = true;  // q==0 counts as negative
}
```
Drop `Draw(...)`, `ElectricField.h`/`Options.h` includes. `Hit` takes the radius from `ComputeParams::hitRadius()`.

---

### `src/core/ef/seeds.hpp` / `seeds.cpp` (service, transform)

**Analog:** `REF/src/core/ef/seeds.{hpp,cpp}`. **MFC source:** `FieldLinesCalculator.cpp:229-250` (see RESEARCH.md lines 322-333 verbatim).
```cpp
const double angle_step = 2. * kPi / (std::abs(static_cast<double>(charge.charge)) * p.linesPerUnitCharge);
double angle_start = -angle_step / 2. - kPi;
const bool source = sign(total) == sign(charge.charge);
if (!source) angle_start += kPi + angle_step;
for (double angle = angle_start; angle < 2. * kPi + angle_start - angle_step / 4.; angle += angle_step) {
  const bool first = (angle == angle_start);
  if ((!first || !p.calculateEquipotentials) && !source) break;
  ...
```
`Seed` carries explicit named fields (fixes MFC defect 10 where `angle`/`angle_start` were swapped in the aggregate init). `expectedLineCount` uses `std::abs` (defect 11). Note: this file is a `.cpp` in the core library: it must be listed in `core_sources` in build.zig.

---

### `src/core/ef/presets.hpp` / `presets.cpp` (data table)

**Analog:** `REF/src/core/ef/presets.cpp` lines 5-31. **MFC source:** `ConfigDocs.cpp` (e.g. dipole lines 25-31), display names `ElectricField.rc:1082-1099` (doc-name field; preset 8 doc name is "Opposed", File-New text has typo "Oposed").
```cpp
{"dipole", "Dipole", {{{+1, -2, 0}, {-1, 2, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
{"quadrupole", "Quadrupole", {{{+1, -1.5, -1.5}, {-1, 1.5, -1.5}, {-1, -1.5, 1.5}, {+1, 1.5, 1.5}}}, 4},
```
Order = `ConfigDocs.cpp` / `AddDocTemplate` order. The cross-check test (below) is the oracle; never retype values without running it.

---

### `src/core/ef/tracer.hpp` (service, transform loop)

**Analog:** `REF/src/core/ef/tracer.hpp`. **MFC source:** `FieldLinesCalculator.cpp:51-110`, `FieldLine.h:18-29`, functor `FieldLinesCalculator.h:49-60`.
**Thinning rule (REF 17-27 = MFC FieldLine.h:20-28):**
```cpp
const double dif = (old - pt).Length() * pixelsPerUnit;
const double len = pt.Length();   // WORLD units in the original; keep
if ((dif >= 256. && len >= 2000 && len < 3000) || (dif >= 64. && len > 1000 && len < 2000) || (dif >= 8. && len <= 1000)) points.push_back(pt);
```
**Loop (REF 41-85):** `steps` 40000 / 5000000 (diff signs and total 0) / 1000000 (diff signs); `needs_precision = point.Length()*px < 3000`; adaptive `precision = max_step = needs ? 0.001 : 0.1`, else `step = needs ? 0.001 : 0.5`; break on `field.HitCharge`; `t += step`; adaptive `step = next_step`. The MFC line 106 `PostCalculateEquipotential(job)` call is deliberately absent (Phase 2). Do not copy `FieldLineJob job(job)` (line 37) or `AdjustForBezier` (Phase 2 / CORE-07).

---

### `src/gui/main.cpp` (entry point, event-driven)

**Analog:** `REF/src/gui/main.cpp` (all 75 lines). No MFC analog.
**Imports + SDL_main (lines 2-11):**
```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // must be in the TU defining main (Windows GUI subsystem)
#include "ef/presets.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
```
**Smoke path (18-21):** `SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")` + `SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software")` before `SDL_Init`. Loop: `NewFrame` order is `ImGui_ImplSDLRenderer3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();`; smoke exits after 2 frames (first builds the font atlas). Error pattern: `fprintf(stderr, "... %s\n", SDL_GetError()); return 1;`. Output line `video=... renderer=... presets=9` proves the core links.

---

### Tests: `tests/test_main.cpp`, `test_vec2.cpp`, `test_field.cpp`, `test_rk_order.cpp`, `test_seeds.cpp`, `test_presets.cpp`

**Analogs:** same-named files in `REF/tests/`. Copy all six; list all in `test_sources` in build.zig.

**Runner (`test_main.cpp`, 2 lines):**
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
```
**Convention:** `#include "doctest/doctest.h"`, `using namespace ef;`, `TEST_CASE("sentence describing the invariant")`, `CAPTURE(preset.slug)` in loops, `doctest::Approx(x).epsilon(e)` (no `.margin()` in 2.5.3), tolerances never bit-exact golden values (D-05).

**Convergence order (`test_rk_order.cpp` lines 13-50):** two ODEs with closed-form solutions (rotation and a non-linear time-dependent one); strict `|p - nominal| <= 0.3` on the linear one, one-sided `p >= nominal - 0.5` on the non-linear one:
```cpp
template <class Solver> void checkOrder(int nominal, int n0Linear, int n0NonLinear) {
  Solver s;
  CHECK(std::abs(observedOrder<Rotation>(s, n0Linear) - nominal) <= 0.3);
  CHECK(observedOrder<NonLinear>(s, n0NonLinear) >= nominal - 0.5);
}
```
Also copy: method-table test (11 names/order), `withSolver` adaptivity test, embedded-low-order test, **DP characterisation test** (`(hi-lo).Length() ~ 0.1` lines 114-122), adaptive step-control tests (lines 124-163). Avoid the round-off floor: order-5 methods use n0 = 4 / 16.

**Seeds (`test_seeds.cpp` 22-53):** loop `lines in {4,16,32}` x `equip in {true,false}` x all presets; per-charge count `source ? |q|*L : (equip ? 1 : 0)`; every seed at distance 0.25 from its charge. Remaining file content (first-seed angles, zero charges, negative totals, tracer over all 11 methods) comes with the copy.

**Presets (`test_presets.cpp` 23-56):** regex-parses `ElectricField/ConfigDocs.cpp` (cwd = repo root, hence `setCwd` in build.zig); skips with `MESSAGE` if the file is missing; delete with `ElectricField/` in Phase 4.

---

### `tools/baseline/capture-mfc-baseline.ps1` and `docs/baseline/mfc/`

**Analog:** `REF/tools/baseline/capture-mfc-baseline.ps1` (PowerShell 5.1; `P/Invoke` class `W`; params `-Exe -OutDir -PresetList -ClientW 1024 -ClientH 768 -TimeoutSec -ResetOptions`). Uses `SetProcessDPIAware`, warns if DPI != 96, `PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT)`, `LB_SETCURSEL` + `BM_CLICK` on the New-document chooser, backs up registry key `HKCU\Software\Free App\Electric Field\options`. Expected chooser names array (lines 56-65) holds the `Oposed` typo on purpose. Build input: MSBuild Release|x64, toolset v145, output to an untracked dir. Output: 9 PNGs named by slug + `README.md` (MSVC version, options, 1024x768, date).

---

## Shared Patterns

### Numerics policy (D-04)
**Source:** `REF/build.zig` lines 4-16 and `REF/src/core/ef/math.hpp`. **Apply to:** all `src/core/**`, all `tests/**`.
`std::abs/std::pow/std::sqrt/std::cos/std::sin` always qualified; `kPi` not `M_PI`; `ef::sign` function not macro; `-ffp-contract=off`; no fast-math; `-Werror` on core and tests only.

### Port, do not edit (D-02)
Every core file cites its MFC origin as `file:line` in a comment (see REF headers, e.g. tracer.hpp line 2-3, charge_system.hpp lines 14-15). Keep that convention. `ElectricField/` stays byte-identical (verify `git diff --stat -- ElectricField/` shows only the separately committed vcxproj toolset change).

### Explicit parameters, no globals (D-03)
Every function that read `theApp.options.*` takes `const ComputeParams&` (see `ChargeSystem::HitCharge(pos, p)`, `planSeeds(field, p)`, `traceFieldLine(solver, field, p, seed)`). Core headers include only `<std>` and `ef/*`; the `-Dgui=false` build is the proof.

### Compile-time solver dispatch
**Source:** `REF/src/core/ef/method.hpp` lines 53-71 and `tracer.hpp` template. **Apply to:** anything that runs a method:
```cpp
const FieldLine line = withSolver(m.method, [&](auto& solver) { return traceFieldLine(solver, field, p, seed); });
```

### Header conventions
`#pragma once`, `namespace ef { ... }`, include paths rooted at `src/core` (`#include "ef/vec2.hpp"`), 2-space indent, `<cstddef>/<cmath>/<cfloat>` included explicitly.

### Build/CI invocations
Everything is a `zig build` step (`test`, `run`, `smoke`, `-Dgui=false`, `-Doptimize=ReleaseFast`); CI YAML only orchestrates these.

## No Analog Found

None. Items with only partial analogs (planner should read RESEARCH.md instead of copying code):

| File/Activity | Reason |
|---|---|
| `docs/baseline/mfc/README.md` | No REF file; use the template in RESEARCH.md (QA-05 section, after line 451) |
| macOS arm64 specifics | Not executable locally; CI is the oracle (D-12) |
| Qt 6 + CMake fallback | Only triggered on irrecoverable CI failure; no code in this phase |

## Metadata

**Analog search scope:** `research-ref/` (all 23 files listed), `ElectricField/` (RungeKutta.h, Charge.h, Vector2D.h, FieldLine.h, FieldLinesCalculator.cpp 48-112, Options.h, ConfigDocs.cpp 1-40)
**Files read:** about 28
**Not read in detail (copy wholesale from REF):** `REF/tests/test_vec2.cpp`, `REF/tests/test_field.cpp`, rest of `REF/tests/test_seeds.cpp`, rest of the capture script, `REF/README.md`, `RungeKutta.inl`, `TheElectricField.cpp`, `Options.cpp`, RESEARCH.md lines 452-756 (CI details, Validation, Open Questions, QA-05 notes)
**Pattern extraction date:** 2026-10-10
