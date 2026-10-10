# Phase 1: Foundation & Portable Physics Core - Research

**Researched:** 2026-10-10
**Domain:** Zig 0.17 build graph for a C++17 project (SDL3 + Dear ImGui + doctest), 3-OS GitHub Actions gate, port of a header-template C++ numerics core (Runge-Kutta field-line tracer), git hygiene, automated baseline capture from a legacy MFC app
**Confidence:** HIGH for Windows and Linux (built and executed this session); MEDIUM for macOS arm64 (cannot be executed here, CI is the oracle)

> **Validated reference implementation:** `.planning/phases/01-foundation-portable-physics-core/research-ref/` (see its README). Everything marked `[VERIFIED: skeleton]` below was built and run from that code with Zig 0.17.0 on Windows 11 and on Ubuntu 24.04 (WSL2, clean Zig cache, no apt packages). The planner may tell executors to copy it instead of re-deriving it.

## Summary

The Phase 1 stack works as researched. A complete skeleton (static C++17 core library, doctest suite wired to `zig build test`, SDL3 + ImGui hello window with a `--smoke` path, `run` and `smoke` steps) builds warning-free under `-Werror -Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -pedantic -ffp-contract=off` and passes 27 test cases / 4382 assertions on Windows (Debug, ReleaseSafe, ReleaseFast, ReleaseSmall) and on Ubuntu 24.04 with only Zig installed. Linux needs **no** apt packages (X11/Wayland are `dlopen`ed; the binary links only libc/libm), and the headless smoke run works without `DISPLAY`. The only unverifiable platform is macOS arm64; CI is the go/no-go gate (decision D-12) and the plan must budget iteration time for it.

The numerics port is mechanical but has several traps that are now pinned down with file:line evidence: the `sign()` macro and its `sign(0)==+1` rule, unqualified `abs`, `M_PI`, `stdafx.h` includes in textually-included templates, a non-const `Func&` solver signature that rejects temporaries, and one **latent defect that must be a conscious decision**: in the MFC source the Dormand-Prince "low" weights are the *error coefficients* (b - b*), so its step-size control sees an "error" of about `h*|f|` instead of the local error (measured: estimate 0.0999 vs true local error 2.8e-10 at h=0.1). The stepping solution itself is fifth order, so lines are accurate; the control just behaves like a fixed step of about `tolerance`. Recommendation: preserve it for visual parity and pin it with a characterisation test.

The seeding algorithm (`FieldLinesCalculator::StartCalculating`) is fully reproduced: sources get `|q| x L` seeds, sinks get exactly one seed (only when equipotentials are on), `sign(0)==+1` makes the positive charges the sources of every net-zero preset, and the MFC `FieldLineJob` aggregate initializer swaps `angle`/`angle_start` (the swap is harmless, "first seed of each charge" semantics are what matter). The MFC baseline (QA-05) turned out to be **fully automatable**: all 9 presets were captured from a Release|x64 MSBuild of the MFC app at 1024x768 in about 55 seconds using Win32 messages and `PrintWindow`, with no input simulation.

**Primary recommendation:** Start from `research-ref/` (build.zig, build.zig.zon, `src/core/ef`, tests, ci.yml, capture script), commit git hygiene first, run the MFC baseline capture before touching anything else, then push (after asking the user) and treat the first 3-OS CI run as the go/no-go gate with the Qt/CMake fallback pre-decided in D-12.

## Project Constraints (from `.claude/CLAUDE.md`)

- Keep C++ for the physics core (C++17 or newer); reuse the proven numerics. `[VERIFIED: .claude/CLAUDE.md]`
- Windows, Linux, macOS desktop; command-line build only; must not require MSVC/MFC; Zig 0.17 preferred. `[VERIFIED: .claude/CLAUDE.md]`
- Repo is open source; GUI dependency licenses must be compatible (SDL zlib, ImGui MIT, doctest MIT: compatible). `[VERIFIED: GitHub API licenses]`
- Behaviour parity: same default options and configurations must give visually equivalent output. `[VERIFIED: .claude/CLAUDE.md]`
- GSD workflow enforcement: file edits happen inside a GSD command (`/gsd-execute-phase` for planned work); no direct repo edits outside it. `[VERIFIED: .claude/CLAUDE.md]`
- Stack section of CLAUDE.md already pins Zig 0.17.0 + ImGui 1.92.9b-docking + SDL3 via castholm + sdlrenderer3 + doctest 2.5.3 (consistent with CONTEXT.md D-08).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
(copied verbatim from `01-CONTEXT.md` `## Implementation Decisions`)

**Source layout & coexistence with MFC**
- **D-01:** New code lives at the repo root, separate from the MFC tree: `build.zig`, `build.zig.zon`, `src/core/` (physics library, namespace `ef`), `src/gui/` (hello-window now, real shell in Phase 3), `tests/` (doctest). The `ElectricField/` MFC directory, `.sln` and `.vcxproj` are left **untouched** and must still build with MSBuild until Phase 4 (needed for the QA-05 baseline and side-by-side comparison).
- **D-02:** The core is a **port (copy + refactor)** of the MFC-era classes, not an in-place edit: MFC source files are not modified in this phase.
- **D-03:** Core is header-mostly C++17 (C++20 allowed if Zig's clang handles it cleanly; no C++ modules). No `stdafx.h`, no MFC/Win32 types, no globals; parameters passed explicitly (`ComputeParams` value type incl. fixed constants `pixelsPerUnit = 100`, `chargeRadiusWorld = 0.25`).

**Numerics policy**
- **D-04:** Research numerics traps are hard rules: `std::abs` only (no unqualified `abs` on doubles; rename the `abs(Vector2D)` helper), `kPi` constant instead of `M_PI`, `inline constexpr int sign(double x){ return x >= 0 ? 1 : -1; }` preserving `sign(0) == +1`, core compiled with `-ffp-contract=off`, never fast-math, strict warnings (`-Wall -Wextra -Wshadow -Wabsolute-value` ...) as errors for core code.
- **D-05:** Tests use tolerances/invariants, never bit-exact golden geometry. Integrator convergence-order tests use an ODE with a closed-form solution (e.g. rotation field / harmonic oscillator): observed order within +-0.3 of nominal; adaptive methods tested for error <= tolerance and actual step adaptation.
- **D-06:** All 11 integrators exposed in the **original enum order** (Euler, Midpoint, Ralston, Heun, RK4, RK3/8, adaptive Heun-Euler, Bogacki-Shampine, Cash-Karp, Fehlberg, Dormand-Prince) with stable string names (used later for config persistence).

**Presets**
- **D-07:** The 9 presets become a data table (`PresetRegistry`) with display names from the `.rc` string table and charge positions/magnitudes copied exactly from `ElectricField/ConfigDocs.cpp`. A test asserts each preset's charges match the MFC values.

**Build & dependencies**
- **D-08:** Zig 0.17.0 exactly (`.minimum_zig_version = "0.17.0"`); deps hash-pinned in `build.zig.zon`: Dear ImGui `v1.92.9b-docking`, SDL3 via `castholm/SDL` `release-3.4.x` (pin a commit), doctest `2.5.3`. Do not use `allyourcodebase/SDL3`. Use the Zig 0.17 `Module` API (`b.createModule`, `module.addCSourceFiles`, `.root_module`); start from `zig init` output of the installed version, not web examples. - **Reversibility:** costly - switching to the Qt/CMake fallback rewrites the build and GUI layer (core unaffected).
- **D-09:** Build steps: `zig build` (core lib + hello-window exe), `zig build test` (doctest suite), `zig build run` (hello window). Windows target `x86_64-windows-gnu`; Windows GUI exe uses `#include <SDL3/SDL_main.h>` + `int main(int, char**)`.
- **D-10:** The hello-window supports a `--smoke` flag: create window, render one ImGui frame, exit 0; runnable under SDL's `dummy` video driver so CI can start it on all 3 OSes without a display.

**CI gate**
- **D-11:** GitHub Actions workflow `.github/workflows/ci.yml`, matrix `windows-latest`, `ubuntu-latest`, `macos-latest` (arm64); Zig via `mlugg/setup-zig` pinned to 0.17.0; runs `zig build`, `zig build test`, and the `--smoke` run. Triggers: push to `feature/cross-platform` and pull requests. Cache the Zig global cache.
- **D-12:** CI only goes green once the branch is pushed to `origin` (github.com/jmfrank63/ElectricField). Pushing is outward-facing -> the executor must **ask the user before the first push**. If the gate fails irrecoverably on Linux or macOS, stop and record the Qt 6 + CMake fallback decision rather than hacking around it. - **Reversibility:** one-way - a pushed branch and CI runs are published on GitHub.

**Git hygiene**
- **D-13:** In its own commit: `git rm -r --cached ElectricField/x64 ElectricField/ElectricField.vcxproj.user` (and anything under `.vs/` if tracked); rewrite `.gitignore` with unanchored patterns (`x64/`, `Debug/`, `Release/`, `.vs/`, `*.user`, `*.obj`, `*.pdb`, `*.ilk`, `*.pch`, `*.tlog`, `*.idb`) plus Zig outputs (`.zig-cache/`, `zig-out/`, `zig-pkg/`). The pending `v143 -> v145` toolset change in `ElectricField/ElectricField.vcxproj` is committed separately so the MFC baseline build is reproducible on VS 18.

**MFC baseline (QA-05)**
- **D-14:** Build the MFC app with MSBuild (Release|x64, toolset v145; MSVC 14.51 has MFC) into an **untracked** output dir; open each of the 9 presets with default options (reset the app's registry options to defaults first) at a fixed client size (target 1024x768), wait for computation to finish, capture the view to PNG. Store as `docs/baseline/mfc/<preset-slug>.png` with a `docs/baseline/mfc/README.md` noting MSVC version, options, window size and date. Automation preferred (PowerShell + Win32 `PrintWindow`, or computer-use); if impractical, make it a human-assisted checkpoint.

### Claude's Discretion
(verbatim) Exact file split inside `src/core/`, class names (guided by research ARCHITECTURE.md: `Vec2`, `ChargeSystem`, `ComputeParams`, `SeedPlanner`, `PresetRegistry`), test file organisation, CI caching details, whether `Vector2D` keeps its template parameter.

### Deferred Ideas (OUT OF SCOPE)
`01-CONTEXT.md` has no "Deferred Ideas" section. Its `## Phase Boundary` excludes: threaded compute session/cancellation, equipotential de-dup/finalizer, SVG/CLI rendering (Phase 2); real GUI (Phase 3); options dialog/persistence/export/MFC deletion (Phase 4). Research does not cover them beyond noting where Phase 1 code must leave a seam.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| BUILD-01 | `zig build` (Zig 0.17.0) on Windows, Linux, macOS without VS/MFC | Verified build graph on Windows + Linux; macOS flow and risks in "CI gate" and "Zig build specifics" |
| BUILD-02 | `zig build test` on all three OSes | doctest suite verified Debug/ReleaseSafe/ReleaseFast on Windows, Debug/ReleaseFast on Linux; ~0.65 s run |
| BUILD-03 | Deps (ImGui, SDL3, doctest) hash-pinned in `build.zig.zon`, nothing else to install | Literal `build.zig.zon` with commit-pinned SDL and tarball hashes; Linux clean-cache fetch verified; no apt packages |
| BUILD-04 | CI on windows/ubuntu/macos-arm64 for every push | `ci.yml` candidate, runner facts (`macos-latest` = macOS 26 arm64), setup-zig behaviour, consent gate |
| BUILD-05 | Artifacts untracked, `.gitignore` covers MSVC + Zig | Exact `git rm --cached` set (37 files under `ElectricField/x64/` + `.vcxproj.user`), `.gitignore` verified with `git ls-files -ci` |
| CORE-01 | GUI-free std-only C++ core, no global state | `ComputeParams` pattern, `-Dgui=false` build proves std-only, header allowlist grep in Validation |
| CORE-02 | 11 integrators in original order produce field lines | `Method` table + `withSolver` dispatch, tracer test over all 11 methods, tableaus verbatim |
| CORE-03 | Seeds proportional to `|q| x L`, `sign(0)==+1` | Exact algorithm, expected counts per role, tests for all 9 presets x L x equipotential flag |
| QA-01 | Tests: vector math, analytic field/potential, integrator convergence order | Test design with measured orders for all 11 methods; DP defect characterisation |
| QA-05 | MFC baseline images of all 9 presets before MFC removal | Verified automation script, registry key, risks, fallback, README template |
</phase_requirements>

## Architectural Responsibility Map

Phase 1 has no client/server tiers; the "tiers" are layers of the repo. The map exists to stop work landing in the wrong layer (for example: GUI types leaking into the core, or build logic hidden in CI YAML).

| Capability | Primary Layer | Secondary Layer | Rationale |
|------------|--------------|-----------------|-----------|
| Vector math, field/potential, RK integrators, seeds, presets, line tracing | `src/core` (std-only C++17 static lib `efcore`, namespace `ef`) | - | Must compile and test without SDL/ImGui/MFC (CORE-01); `-Dgui=false` proves it |
| Numerics policy (`-ffp-contract=off`, `-Werror`) | `build.zig` (`core_flags`, applied to core AND tests) | - | One place; GUI/third-party code deliberately excluded |
| Dependency pinning and fetching | `build.zig.zon` | `build.zig` (`b.dependency`) | Hash + commit pinned; zero manual installs |
| Window/renderer start-up check | `src/gui/main.cpp` (hello window, `--smoke`) | `build.zig` `smoke` step | Proves the SDL3+ImGui stack on each OS; real shell is Phase 3 |
| 3-OS verification and go/no-go | `.github/workflows/ci.yml` | build.zig steps | CI only orchestrates `zig build ...`; all logic lives in build.zig |
| Test oracle for presets | `tests/` + parsing of `ElectricField/ConfigDocs.cpp` | `src/core/ef/presets.cpp` | Mechanical proof that the data table equals the MFC source (D-07) |
| Visual reference | `docs/baseline/mfc/*.png` + `tools/baseline/capture-mfc-baseline.ps1` | MSBuild of `ElectricField.sln` | One-time capture from the real app (QA-05) |
| Repo hygiene | `.gitignore`, index (`git rm --cached`) | - | BUILD-05; two separate commits |

## Standard Stack

### Core
| Library / Tool | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| Zig (`zig build`, `zig c++` = Clang 22.1.8 + bundled libc++) | 0.17.0 (released 2026-10-01) `[VERIFIED: ziglang.org/download/index.json]`; installed `zig version` -> `0.17.0` `[VERIFIED: skeleton]` | Compiler, build system, package manager | Locked by D-08; one command on every OS |
| castholm/SDL (Zig port of SDL 3.4.18) | `0.6.0+3.4.18`, branch `release-3.4.x`, pinned commit `dbcb19aa3583c318556ecae3ecbeb29b102d37c5` (= branch tip on 2026-10-09 `[VERIFIED: gh api]`) | Window, events, software/Metal/D3D renderers, PNG save, pref path | Only SDL3 Zig port requiring Zig 0.17.0; spiked and now Linux-verified |
| Dear ImGui | `v1.92.9b-docking` (tag exists, release 2026-07-31 `[VERIFIED: gh api]`) | Hello-window UI; `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3` | Locked by D-08 |
| doctest | `2.5.3` (2026-07-06 `[VERIFIED: gh api]`) | Unit tests, single header | Locked by D-08; note: `doctest::Approx` has **no** `.margin()` in 2.5.3 `[VERIFIED: skeleton compile error]` |

### Supporting
| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| `mlugg/setup-zig` | `v2.2.1` = commit `d1434d08867e3ee9daa34448df10607b98908d29` (`v2` floating tag points to the same commit `[VERIFIED: gh api]`) | Installs Zig, caches the global Zig cache | CI; `version: 0.17.0` resolves through `ziglang.org/download/index.json`, which lists 0.17.0 `[VERIFIED]` (not yet executed in a real workflow: first CI run confirms) |
| `actions/checkout` | `v7` (v7.0.1, 2026-07-20 `[VERIFIED: gh api]`) | Source checkout | CI |
| ImageMagick `magick compare` | installed locally (scoop) | Pixel diff of baseline runs | Optional, local only |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `-std=c++17` | `-std=c++20` | Core + tests also compile warning-free at C++20 `[VERIFIED: skeleton]`; nothing in Phase 1 needs 20; stay on 17 (ImGui files are compiled with 17 anyway) |
| Eager SDL/ImGui deps | `.lazy = true` + `b.lazyDependency` | Lazy would also skip fetching for core-only builds but complicates build.zig; the `-Dgui=false` option already isolates the core gate without it |
| `zig fetch --save` workflow | literal zon (given below) | The pinned URL/hash pairs were validated from a clean global cache on Linux; no need to re-run `zig fetch --save`, only `zig build` |

**Installation:** none beyond Zig 0.17.0. Dependencies come from `build.zig.zon` (literal file below, validated). Do not pass `-Dtarget` on macOS: the SDL port then demands `-Dsystem_include_path/-Dsystem_framework_path/-Dlibrary_path` for non-native targets `[VERIFIED: castholm build.zig:100-107]`.

```zig
// build.zig.zon  [VERIFIED: skeleton, clean-cache fetch on Ubuntu 24.04 matched all three hashes]
.{
    .name = .electricfield,
    .version = "0.1.0",
    .fingerprint = 0x640049fcb48a860b, // generated by Zig for this name; never change it
    .minimum_zig_version = "0.17.0",
    .dependencies = .{
        .sdl3 = .{
            .url = "git+https://github.com/castholm/SDL?ref=release-3.4.x#dbcb19aa3583c318556ecae3ecbeb29b102d37c5",
            .hash = "sdl-0.6.0+3.4.18-SDL--p-gqAGWpnfTLlhs0dgjDtc4P4_CHeNLGY_SX_Fv",
        },
        .imgui = .{
            .url = "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz",
            .hash = "N-V-__8AAAPHhAB86bkFSQlFlaioKXgArks51cXvTB1G5j7F",
        },
        .doctest = .{
            .url = "https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz",
            .hash = "N-V-__8AAB9aPgADa6XPcoVfyGpb-2fhHYgVMX9InfAngj-p",
        },
    },
    .paths = .{ "build.zig", "build.zig.zon", "src", "tests", "LICENSE", "README.md" },
}
```

The SDL package itself pulls one lazy dependency, `sdl_linux_deps` (castholm, commit-pinned inside SDL's own `build.zig.zon`), only when the target OS is Linux `[VERIFIED: skeleton log "info: fetching lazy dependency sdl_linux_deps"]`.

## Package Legitimacy Audit

These are Zig URL dependencies and one GitHub Action, not npm/PyPI/crates packages, so `gsd-tools query package-legitimacy check` (registry based) does not apply; provenance was checked directly against upstream and the content hash is enforced by `build.zig.zon`.

| Package | Registry | Age | Popularity | Source Repo | Verdict | Disposition |
|---------|----------|-----|------------|-------------|---------|-------------|
| Dear ImGui `v1.92.9b-docking` | GitHub tag (official `ocornut/imgui`) | 12 yrs | 76.5k stars, MIT | github.com/ocornut/imgui | OK | Approved (hash-pinned tarball) |
| doctest `v2.5.3` | GitHub tag (official `doctest/doctest`) | 12 yrs | 6.9k stars, MIT | github.com/doctest/doctest | OK | Approved (hash-pinned tarball) |
| castholm/SDL `release-3.4.x` @ `dbcb19a` | GitHub git dep (fork of libsdl-org/SDL by Carl Astholm; zlib via REUSE) | 1.8 yrs | 426 stars, active (push 2026-10-09) | github.com/castholm/SDL | OK, third-party maintainer | Approved; commit + content hash pinned; fallback = Qt/CMake (D-12) |
| castholm/SDL_linux_deps (lazy, via SDL) | GitHub git dep | 1.8 yrs | 4 stars | github.com/castholm/SDL_linux_deps | SUS (low popularity, same maintainer, header/soname data only) | Accepted: commit-pinned by SDL's zon, Linux-only; no checkpoint needed, but note in plan |
| `mlugg/setup-zig` v2.2.1 | GitHub Action (mirror of codeberg.org/mlugg/setup-zig; Zig core contributor) | 2.3 yrs | 239 stars, MIT | codeberg.org/mlugg/setup-zig | OK | Approved; pin to commit SHA |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** `SDL_linux_deps` (transitive, see above). No npm-style `postinstall` equivalent exists in Zig packages; Zig build scripts of dependencies do run at configure time, which is why commit pinning matters.

## Architecture Patterns

### System Architecture Diagram

```
                        fresh clone (Windows | Linux | macOS arm64)
                                         |
                       build.zig.zon  (url + hash + commit pins)
                                         |  zig fetch (global cache: p/*.tar.gz, unpacked to ./zig-pkg)
                                         v
   +-------------------------------- build.zig ---------------------------------+
   |  core_flags = -std=c++17 -ffp-contract=off -W... -Werror   gui_flags = no -Werror
   |                                                                              |
   |  efcore (static lib, std-only) <-- src/core/ef/*.hpp,*.cpp                   |
   |        ^                 ^                                                   |
   |        |                 +--------------------+                              |
   |   efcore-tests (doctest, -isystem doctest)    |  ElectricField (GUI exe)     |
   |        |   cwd = repo root                    |  main.cpp + imgui + SDL3     |
   |        v                                      v                              |
   |  step `test`                         step `run` / step `smoke`               |
   |  (reads ElectricField/ConfigDocs.cpp         (--smoke: dummy video driver,   |
   |   for the MFC cross-check)                    software renderer, 2 frames)   |
   +------------------------------------------------------------------------------+
                                         |
        GitHub Actions matrix (windows-latest | ubuntu-latest | macos-latest)
        steps: core tests (Debug+UBSan) -> core tests (ReleaseFast) -> zig build -> zig build test -> zig build smoke

 core data flow (what the tests exercise):
   PresetRegistry --makeSystem--> ChargeSystem --planSeeds(ComputeParams)--> [Seed]
        Seed --(Method -> withSolver -> RK solver, ENormalized functor)--> traceFieldLine --> FieldLine points
        (Phase 2 adds: equipotentials, de-dup, threaded session. Phase 3: painter.)
```

### Recommended Project Structure
```
build.zig, build.zig.zon
src/core/ef/            # namespace ef, std-only
  math.hpp              # kPi, sign()
  vec2.hpp              # Vector2D<T> (header-only, explicit ctor), using Vec2 = Vector2D<double>
  rk.hpp                # RungeKutta<T,S>, AdaptiveRungeKutta<T,S,Order>, constexpr tableaus, 11 solvers
  method.hpp            # enum class Method (original order), kMethods table (name/display/order), withSolver()
  params.hpp            # ComputeParams
  charge_system.hpp     # Charge, ChargeSystem (E, Potential, ENormalized, HitCharge, totalCharge)
  seeds.hpp/.cpp        # planSeeds(), expectedLineCount()
  presets.hpp/.cpp      # 9 presets as data
  tracer.hpp            # FieldLine::addPoint, traceFieldLine<Solver>() (no equipotentials yet)
src/gui/main.cpp        # hello window + --smoke
tests/                  # doctest
docs/baseline/mfc/      # 9 PNG + README.md  (QA-05)
tools/baseline/capture-mfc-baseline.ps1
.github/workflows/ci.yml
```

### Pattern 1: Explicit parameters instead of `theApp.options`
**What:** One `ComputeParams` value passed to every function that used to read `theApp.options.*`.
**When to use:** all core code. In the original, `theApp.options` is read at `Charge.h:25` (HitCharge), `FieldLine.h:23` (AddPoint), `FieldLinesCalculator.cpp:12,231,239,245,254-258` (constructor and StartCalculating).
**Example:** `[VERIFIED: skeleton src/core/ef/params.hpp]`
```cpp
struct ComputeParams {
  unsigned pixelsPerUnit = 100;       // Options::distanceUnitLength
  double chargeRadiusPx = 25.0;       // Options::chargeRadius (float 25.0f) -> 0.25 world units
  unsigned linesPerUnitCharge = 16;   // Options::numLinesOnUnitCharge
  bool calculateEquipotentials = true;
  double potentialInterval = 0.1;
  Method method = Method::Euler;
  double seedRadius() const { return chargeRadiusPx / static_cast<double>(pixelsPerUnit); }      // 0.25
  double hitRadius()  const { return chargeRadiusPx / 1.5 / static_cast<double>(pixelsPerUnit); } // Charge.h:25 expression
};
```
`numThreads` is intentionally absent (Phase 2). Keep the original *expressions* for `seedRadius`/`hitRadius` (same operation order) rather than hard-coding 0.25 so the last-bit behaviour stays identical.

### Pattern 2: Header-only templates + compile-time dispatch (no virtual call in the hot loop)
**What:** `Vector2D` and the RK classes become real headers; `withSolver(Method, visitor)` constructs one solver of the right static type and calls `visitor(solver)`; the tracer is a template on the solver type. This mirrors `StartComputingThread`'s switch (`FieldLinesCalculator.cpp:285-`) without type erasure.
**Example:** `[VERIFIED: skeleton src/core/ef/method.hpp, tests/test_seeds.cpp]`
```cpp
const FieldLine line = withSolver(m.method, [&](auto& solver) { return traceFieldLine(solver, field, p, seed); });
```
Solvers hold stage state in member `K` (`RungeKutta.h:18`): **one solver per thread**, never shared (Phase 2 rule; add a comment now).

### Pattern 3: Numerics policy lives in `build.zig`, applied to core and tests, not to third-party code
**What:** `core_flags` (including `-Werror`) for `src/core` and `tests/`; `gui_flags` (no `-Werror`) for ImGui, the backends and `src/gui`; doctest included with `addSystemIncludePath` so its warnings do not fail the build. `[VERIFIED: skeleton]`

### Pattern 4: Headless smoke path sets its own SDL hints
`--smoke` calls `SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy")` and `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software")` before `SDL_Init`, so it works even when launched by hand without environment variables; the build's `smoke` step additionally sets `SDL_VIDEODRIVER`/`SDL_RENDER_DRIVER` env vars (SDL honours the legacy `SDL_VIDEODRIVER`, `SDL_hints.c:95-97`). It renders two frames (first builds the font atlas), prints `video=dummy renderer=software presets=9`, exits 0. `[VERIFIED: skeleton on Windows and Ubuntu]`

### Anti-Patterns to Avoid
- **Globbing sources or textual `#include "X.cpp"` templates:** `Vector2D.h:45-46` includes `Vector2D.cpp`, `RungeKutta.h:165-167` includes `RungeKutta.inl`, both start with `#include "stdafx.h"` (`Vector2D.cpp:1`, `RungeKutta.inl:1`). Port to plain headers; list core `.cpp` files explicitly.
- **Passing `-Dtarget` on macOS in CI** (see Installation).
- **Letting `zig build test` depend on the GUI stack:** keep `-Dgui=false` as the "core gate".
- **Using web examples for build.zig:** 0.17 removed `b.args`, `b.pathFromRoot`, and moved C/C++ calls to `Module` (list under "Zig 0.17 API facts").

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Unit test runner / reporting | custom `main` + asserts | doctest (`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`) | Exit code drives `zig build test`; `Approx` covers numerics |
| Headless GUI start-up check | Xvfb setup, display forwarding | SDL `dummy` driver + `software` renderer | Works on all three OSes with no packages `[VERIFIED: Windows, Ubuntu]` |
| Dependency fetching/pinning | git submodules, vendored copies, scripts | `build.zig.zon` url+hash | Content-hashed, cached, same on every OS |
| Zig toolchain install in CI | curl + tar steps | `mlugg/setup-zig` | Verifies minisign, caches the global cache |
| Window capture of a Direct2D MFC view | `CopyFromScreen` + focus/topmost juggling | `PrintWindow(hwnd, hdc, PW_CLIENTONLY \| PW_RENDERFULLCONTENT)` | Identical pixels to `CopyFromScreen` for all 9 presets, no foreground requirement `[VERIFIED]` |
| Selecting a preset in MFC's "New" chooser | keyboard simulation | `LB_SETCURSEL` + `BM_CLICK` Win32 messages | The list box exposes no UIA items (tested); messages are deterministic and unaffected by user input `[VERIFIED]` |
| Pixel comparison of two captures | custom diff code | `magick compare -metric AE -fuzz 10%` | One line |

**Key insight:** the only genuinely custom code in this phase is the physics port; everything else (build, CI, tests, capture) has a one-line standard tool.

## Source Port Map (core)

All line numbers are in the MFC tree as of this session (`ElectricField/`). `[VERIFIED: Read/grep this session]`

### File-by-file disposition

| MFC source | Becomes | Notes |
|-----------|---------|-------|
| `Vector2D.h` + `Vector2D.cpp` | `ef/vec2.hpp` (`Vector2D<T>` kept as a template, `using Vec2 = Vector2D<double>`) | Single header; drop `stdafx.h`, `#ifdef _DEBUG #define new DEBUG_NEW` (`Vector2D.cpp:10-11`), `_VECTOR_2D_IMPL` include dance; make the one-argument constructor `explicit` (`Vector2D.h:10`); drop mixed-type template operators and the broken `operator*=(Vector2D<O>)` (`Vector2D.h:30`, `Vector2D.cpp:89`: returns `T`, assigns a scalar into a vector) |
| `RungeKutta.h` + `RungeKutta.inl` | `ef/rk.hpp` (+ `ef/method.hpp`) | Tableaus copied verbatim as `inline constexpr` aggregates; algorithm unchanged; see defects table |
| `Options.h:6-18` (`CalculationMethod`) | `ef::Method` + `kMethods` table | Original order, stable names; display strings = `.rc` radio captions (`ElectricField.rc:740-750`) |
| `Charge.h` (`Potential`, `E`, `HitCharge`) | `ef/charge_system.hpp` `Charge` | Drop `Draw(...)` (`Charge.cpp`), `ElectricField.h`/`Options.h` includes; `HitCharge` takes the radius |
| `TheElectricField.h/.cpp` (`E`, `Potential`, `ENormalized`, `HitCharge`, `GetTotalCharge`) | `ChargeSystem` | Drop `Draw`, `Clear`, line vectors; guard `GetTotalCharge` against an empty charge list (`TheElectricField.cpp:70` dereferences `charges.begin()`) |
| `FieldLine.h` `AddPoint` (`FieldLine.h:18-29`) | `FieldLine::addPoint(pt, pixelsPerUnit)` in `ef/tracer.hpp` | Thresholds copied verbatim (see tracer notes) |
| `FieldLine.cpp` `AdjustForBezier` (lines 183-235) | **Phase 2** (CORE-07: guard `ASSERT(abs(divi) > DBL_MIN)` at `FieldLine.cpp:222`) | Out of Phase 1; note `const double t3 = t1 + t2 + (pt3 - pt2).Length();` (`FieldLine.cpp:215`) looks unusual (total length plus the first segment), preserve in Phase 2 unless proven a bug |
| `FieldLinesCalculator.cpp:223-259` `StartCalculating` | `planSeeds()` | Seeding only; no job queue, no threads |
| `FieldLinesCalculator.cpp:261-` `GetNumberOfElectricFieldLines` | `expectedLineCount()` | Unsigned-wrap defect fixed with `std::abs` |
| `FieldLinesCalculator.cpp:51-110` `CalculateElectricFieldLine` | `traceFieldLine<Solver>()` | Without the `PostCalculateEquipotential(job)` call at line 106 (Phase 2) |
| `FieldLinesCalculator.cpp:23-49,113-, 155-` equipotentials, job queue, threads | **Phase 2** | `FieldLineJob job(job)` (line 37), unqualified `abs` (line 31), `M_PI` (line 7), `finishedThreads` race |
| `ConfigDocs.cpp` | `ef/presets.cpp` | 9 data rows |
| `ElectricField.rc:1080-1099` | display names | see preset table |

### Portability defects (file:line) and what Phase 1 does

| # | Location | Defect | Phase 1 action |
|---|----------|--------|----------------|
| 1 | `RungeKutta.h:7` | `#define sign(x) ((x) >= 0 ? 1 : -1)` leaks everywhere; `sign(0)==+1` is load-bearing | `inline constexpr int sign(double) noexcept` in `ef/math.hpp`; test `sign(0)==1`, `sign(-0.0)==1` |
| 2 | `RungeKutta.h:69,71,79,80` | unqualified `abs(...)` on `double`/`Vector2D` (resolves via MSVC `<cmath>` leakage and ADL to `Vector2D.h:43`); `pow` unqualified at `:76,:86` | `std::abs`/`std::pow`; `Vector2D` norm through `rk::magnitude()` overloads; rename `abs(Vector2D)` |
| 3 | `RungeKutta.h:3-5` | no `<cfloat>`/`<cmath>`; uses `DBL_MAX`/`DBL_MIN` (`:37,:53`) | include them |
| 4 | `RungeKutta.h:37,53` and calls | solver takes `Func& Function` (non-const lvalue): a temporary functor does not compile | take `Func&&` (done in the skeleton) |
| 5 | `RungeKutta.h:165-167`, `RungeKutta.inl:1,3` | textual include of `.inl`, `stdafx.h`, reserved identifier `__RK_IMPL` | plain header, no macro |
| 6 | `RungeKutta.inl:41` | `NULL` for the Euler coefficient table | tableau aggregates, no pointers |
| 7 | `Vector2D.cpp:1,10-11,7` | `stdafx.h`, `DEBUG_NEW` macro, `<math.h>` + unqualified `sqrt` (`:109`) | `std::sqrt`, header-only |
| 8 | `Charge.h:7-8,25,41` | includes `ElectricField.h`/`Options.h`; reads `theApp.options`; unqualified `sqrt` | explicit params, `std::sqrt` |
| 9 | `FieldLinesCalculator.cpp:7` | `const double M_PI = ...` redeclares a macro on glibc/macOS | `ef::kPi` |
| 10 | `FieldLinesCalculator.cpp:250` + `FieldLinesCalculator.h:23-24` | aggregate initializer passes `angle, angle_start` into members declared `angle_start, angle`: the two are swapped | `Seed{angle, first}` explicit fields |
| 11 | `FieldLinesCalculator.cpp:269` | `charge.charge * numLinesOnUnitCharge` is `int * unsigned`: wraps for negative charges, result negative for all-negative configs | `std::abs(q) * L` |
| 12 | `TheElectricField.cpp:70` | `it->charge` on `charges.begin()` of an empty vector | guard |
| 13 | `FieldLine.cpp:222` | `ASSERT` (MFC macro) before a division by `divi`; Release divides by 0 | **Phase 2** (CORE-07) |
| 14 | `FieldLinesCalculator.cpp:37` | `FieldLineJob job(job);` self-initialisation | **Phase 2** (CORE-07) |
| 15 | `RungeKutta.inl:191` | Dormand-Prince "low" weights are the error coefficients `b - b*` (71/57600 ...) not `b*` | **Decision needed** (see Open Questions); keep + characterise by default |

`-Wshadow -Wconversion -Wsign-conversion -pedantic -Werror` compile clean on Windows (LLP64) and Linux (LP64) with the skeleton `[VERIFIED]`; macOS arm64 is LP64 like Linux so no new class of warning is expected `[ASSUMED]`.

### Seeding algorithm (exact; `FieldLinesCalculator.cpp:229-250`)

Verbatim source:
```cpp
229		bool has_different_signs;
230		const int total_charge = theField->GetTotalCharge(has_different_signs);
231		const double r = theApp.options.chargeRadius / theApp.options.distanceUnitLength;
...
239			const double angle_step = 2.*M_PI / (fabs(charge.charge)*theApp.options.numLinesOnUnitCharge);
241			angle_start = - angle_step / 2. - M_PI;
242			if (sign(total_charge) != sign(charge.charge))	angle_start += M_PI + angle_step;
244			for (double angle = angle_start; angle < 2.*M_PI + angle_start - angle_step / 4.; angle += angle_step) {
245				if ((angle != angle_start || !theApp.options.calculateEquipotentials) && sign(total_charge) != sign(charge.charge)) break;
247				point.X = charge.position.X + r*cos(angle);
248				point.Y = charge.position.Y + r*sin(angle);
250				m_jobs.push_back( { charge, total_charge, has_different_signs, angle, angle_start, point, false, 0 } );
```
Rules (all `[VERIFIED: skeleton tests test_seeds.cpp, 9 presets x L in {4,16,32} x equipotentials on/off]`):
1. `total = Σ q`; `hasDifferentSigns` from `GetTotalCharge` (`TheElectricField.cpp:65-80`): sign of the first charge is the reference and `q == 0` counts as negative (`it->charge > 0 ? 1 : -1`).
2. `r = chargeRadius / distanceUnitLength` = `25.0f / 100u` = **0.25** (float division, exact). No integer-division defect (`Options.h:22-23`: `unsigned int distanceUnitLength; float chargeRadius;`).
3. Skip charges with `q == 0`.
4. `angle_step = 2π / (|q| · L)`.
5. **Source** (`sign(total) == sign(q)`; `sign(0)==+1`): `angle_start = -π - step/2`; emits exactly `|q|·L` seeds at `angle_start + k·step` (accumulated by `+=`, loop bound has `step/4` slack so floating drift cannot add or drop one).
6. **Sink** (`sign(total) != sign(q)`): `angle_start = -π - step/2 + π + step = +step/2`; emits **one** seed if `calculateEquipotentials` is true, **none** otherwise (the `break` at line 245 fires on the second iteration, or on the first when equipotentials are off).
7. Seed point = `charge.position + r·(cos angle, sin angle)`.
8. Net-zero presets (dipole, quadrupole, both two-dipole presets, parallel dipoles): `sign(0)==+1` makes the **positive** charges the sources; negative charges are sinks. Negative total flips roles (test `negative total charge flips the roles`).
9. "First seed of its charge" (`angle == angle_start`, i.e. loop index 0) is the only seed later allowed to spawn equipotentials (`PostCalculateEquipotential` compares `job.angle != job.angle_start`; because of defect 10 the members hold swapped values, but the predicate is equivalent). Keep `Seed::first`.
10. Expected totals at default L = 16 with equipotentials on: dipole 16+1, two equal charges 32 (both sources), +1/+2 same sign 48, +2/-1 (total +1) 32+1, quadrupole 32+2, four same sign 64, both two-dipole presets 32+2, parallel dipoles 32+2.

Seed tests in the skeleton: counts per charge, total count, every seed at distance 0.25 from its charge, first seed angles (`-π - step/2` and `+step/2`), zero charges skipped, `expectedLineCount` for negative totals.

### Preset table (`ConfigDocs.cpp`, order = `AddDocTemplate` order `ElectricField.cpp:113-185`)

Verbatim statements, extracted with line numbers (`charge.position`/`charge.charge` pairs are pushed in this order):
```
14 IMPLEMENT_DYNCREATE(CDipoleDoc ...)           25 (-2, 0)  26 charge = 1     30 charge = -1   31 (2, 0)
42 CTwoEqualChargesDoc                           53 (-2, 0)  54 charge = 1     58 charge = 1    59 (2, 0)
68 CTwoUnequalChargesSameSignDoc                 79 (-2, 0)  80 charge = 1     84 charge = 2    85 (2, 0)
95 CTwoUnequalChargesDifSignDoc                  106 (-2, 0) 107 charge = 2    111 charge = -1  112 (2, 0)
123 CQuadrupoleDoc   134 (-1.5, -1.5) 135 = 1 | 139 = -1  140 (1.5, -1.5) | 147 (-1.5, 1.5) 148 = -1 | 152 = 1  153 (1.5, 1.5)
164 CFourChargesSameSignDoc  175 (-3, 0) 176 = 1 | 180 = 1 181 (-1, 0) | 188 (1, 0) 189 = 1 | 193 = 1 194 (3, 0)
205 CTwoDipolesSameDirDoc    216 (-3, 0) 217 = 1 | 221 = -1 222 (-1, 0) | 229 (1, 0) 230 = 1 | 234 = -1 235 (3, 0)
246 CTwoDipolesDifDirDoc     257 (-3, 0) 258 = 1 | 262 = -1 263 (-1, 0) | 270 (1, 0) 271 = -1 | 275 = 1 276 (3, 0)
287 CParallelDipolesDoc      298 (-1.5, -1.5) 299 = 1 | 303 = -1 304 (1.5, -1.5) | 311 (-1.5, 1.5) 312 = 1 | 316 = -1 317 (1.5, 1.5)
```
Resulting data (slug, display name from `ElectricField.rc:1082-1099` doc-name field, charges as `q @ (x,y)`):

| # | slug | display name (`.rc`) | charges |
|---|------|----------------------|---------|
| 1 | `dipole` | Dipole | +1 @ (-2,0); -1 @ (2,0) |
| 2 | `two-equal-charges` | Two Equal Charges | +1 @ (-2,0); +1 @ (2,0) |
| 3 | `two-unequal-charges-same-sign` | Two Unequal Charges Same Sign | +1 @ (-2,0); +2 @ (2,0) |
| 4 | `two-unequal-charges-different-signs` | Two Unequal Charges Different Signs | +2 @ (-2,0); -1 @ (2,0) |
| 5 | `quadrupole` | Quadrupole | +1 @ (-1.5,-1.5); -1 @ (1.5,-1.5); -1 @ (-1.5,1.5); +1 @ (1.5,1.5) |
| 6 | `four-charges-same-sign` | Four Charges Same Sign | +1 @ (-3,0); +1 @ (-1,0); +1 @ (1,0); +1 @ (3,0) |
| 7 | `two-dipoles-same-direction` | Two Dipoles Same Direction | +1 @ (-3,0); -1 @ (-1,0); +1 @ (1,0); -1 @ (3,0) |
| 8 | `two-dipoles-opposed-direction` | Two Dipoles Opposed Direction | +1 @ (-3,0); -1 @ (-1,0); -1 @ (1,0); +1 @ (3,0) |
| 9 | `parallel-dipoles` | Parallel Dipoles | +1 @ (-1.5,-1.5); -1 @ (1.5,-1.5); +1 @ (-1.5,1.5); -1 @ (1.5,1.5) |

Verbatim `.rc` string table (`ElectricField.rc:1082-1099`) shows the MFC doc-string format `"\n<docName>\n<fileNewName>\n\n\n\n"`; for preset 8 the **doc name** is `Two Dipoles Opposed Direction` but the **File>New chooser text** is `Two Dipoles Oposed Direction` (typo). Use the doc name for `displayName` (matches VIEW-01); the baseline script validates the chooser text including the typo.

`[VERIFIED: skeleton]` `tests/test_presets.cpp` parses `ElectricField/ConfigDocs.cpp` at test time (cwd = repo root) and compares all 9 presets; mutating one charge in a copy made the test fail as intended. Delete this test with `ElectricField/` in Phase 4.

### Line tracer parameters (kept verbatim from `FieldLinesCalculator.cpp:51-110`, `FieldLine.h:18-29`)

- `steps = 40000`; `5000000` if `has_different_signs && total_charge == 0`; `1000000` if `has_different_signs`. (Lines 53,62-63.)
- Start `step = 0.001`; per iteration `len = |point|·pixelsPerUnit`; `needs_precision = len < 3000`; non-adaptive: `step = needs ? 0.001 : 0.5`; adaptive: `precision = max_step = needs ? 0.001 : 0.1`.
- Direction functor `FunctorForE` (`FieldLinesCalculator.h:49-60`): returns `ENormalized(pos)` when `charge_sign > 0`, else `-ENormalized(pos)`, with `charge_sign = sign(job.charge.charge)` (`FieldLinesCalculator.cpp:195`).
- Points are recorded only if `sign(total) == sign(charge)` (sources); `HitCharge(point)` ends the line; `t += step`; adaptive `step = next_step`.
- `AddPoint` thinning: record when `(dif >= 256 && 2000 <= |pt| < 3000) || (dif >= 64 && 1000 < |pt| < 2000) || (dif >= 8 && |pt| <= 1000)` where `dif = |old - pt| · pixelsPerUnit` (pixels) but `|pt|` is in **world units** (so the 8-px rule is effectively the only one in use). Port verbatim.
- Skeleton test: all 11 methods trace the dipole line from seed 4 to within 0.35 of the negative charge in Debug in < 1 s total.

## Convergence-order test design (QA-01 / D-05)

**How `RungeKutta.inl` propagates** `[VERIFIED: Read RungeKutta.h:53-91, RungeKutta.inl:134-203, plus measurements]`
- Non-adaptive: one `SolveStep` = classic explicit RK with the given tableau.
- Adaptive: each stage computes both `thesumHigh` (`m_weights`) and `thesumLow` (`m_low_order_weights`); `error = |(high - low)·h|`; accepts when `error <= tolerance || loop > 1000 || |h| <= min_step`; **the result is `curVal + h·thesumHigh`** (the higher-order solution). Rejected steps shrink `h` in place by `0.9·(tol/error)^(1/Order)`; accepted steps set `next_h = h·0.9·(tol/error)^(1/(Order+1))` (or `2h` if `error <= DBL_MIN`), clamped to `[min_step, max_step]`. `Order` template arguments: HeunEuler 2, BS 3, CK 5, Fehlberg 5, DP 5 (`RungeKutta.h:131,137,143,149,156`: `AdaptiveRungeKutta<T, 2, 2>`, `<T, 4, 3>`, `<T,6,5>`, `<T,6,5>`, `<T,7,5>`).

**ODEs** (state `Vec2`; exact solutions in closed form; `[VERIFIED: skeleton tests/test_rk_order.cpp]`):
- A, rotation `y' = (-y2, y1)`, `y(0)=(1,0)`: exact `(cos t, sin t)`. Linear, autonomous: clean asymptotic orders. **Strict check: |p - nominal| <= 0.3** on the last refinement pair.
- B, `u' = u(1-u)`, `v' = -2 t v`, `y(0)=(1/2, 1)`: exact `(1/(1+e^-t), e^(-t²))`. Non-linear **and** time dependent, so it exercises the nonlinear order conditions and the node coefficients `c_i` (a linear autonomous problem cannot see wrong nodes). **One-sided check: p >= nominal - 0.5** because pre-asymptotic super-convergence appears (Ralston 3.06 at n=8, DP 7.83 at n=2).

**Fixed-step entry point for adaptive classes:** the adaptive `SolveStep` hides the 4-argument fixed step, so the port exposes `SolveStepFixed(f, y, t, h)` on the base class (the high-order solution with no step control) and a test hook `SolveStepPair(f, y, t, h, high, low)` on `AdaptiveRungeKutta`.

**Measured orders** (T = 1; `p = log2(e_n / e_2n)`; last pair of the chosen sequence) `[VERIFIED: skeleton]`:

| Method | nominal | rotation p | non-linear p | n sequence used |
|--------|---------|-----------|--------------|-----------------|
| Euler | 1 | 1.00 | 1.01 | 64,128 |
| Midpoint | 2 | 2.00 | 2.02 | 64,128 (test: 32,64) |
| Ralston | 2 | 2.00 | 2.12 | 128 |
| Heun | 2 | 2.00 | 2.00 | 128 |
| RK4 | 4 | 4.00 | 4.00 | 16,32 |
| RK 3/8 | 4 | 4.00 | 4.07 | 16,32 |
| Heun-Euler (stepping = Heun) | 2 | 2.00 | 2.00 | 64,128 |
| Bogacki-Shampine (stepping = 3rd order) | 3 | 3.00 | 2.96 | 64,128 |
| Cash-Karp (stepping = 5th) | 5 | 5.00 | 5.07 | 16,32 (rot test uses 4,8) |
| Fehlberg (stepping = 5th) | 5 | 5.00 | 5.45 | 16,32 |
| Dormand-Prince (stepping = 5th) | 5 | 5.00 | 4.79 | 16,32 |

Avoid the round-off floor: order-5 methods reach 1e-12 by n = 32, so use n0 = 4 (rotation) and n0 = 16 (non-linear). Embedded **low** solutions (stepping with `low` via `SolveStepPair`): Heun-Euler 1.01, Bogacki-Shampine 2.00, Cash-Karp 4.00, Fehlberg 4.00 (one order lower, as designed); Dormand-Prince **0.00** (it is not a solution, see defect 15).

**Adaptive behaviour tests** (rotation to T = 2, `max_step = 1`): steps for tol 1e-3 / 1e-6 / 1e-9: Heun-Euler 53 / 1657 / 52379, Bogacki-Shampine 8 / 64 / 634, Cash-Karp 4 / 8 / 27, Fehlberg 4 / 11 / 38; global error stays within ~20x tol. Plus: an oversized `h = 1.5` shrinks in place and the accepted step meets the tolerance; an undersized `h` grows (`next_h > h`); `next_h` is clamped to `max_step`. **Dormand-Prince** with these controls takes 3760 steps at tol 1e-3 and > 1e6 at 1e-6 (the error estimate is about `h·|f|`): the skeleton test therefore only characterises it (`|hi - lo| ≈ 0.1` for h = 0.1 while the true local error is < 1e-9) and does not assert step-count scaling for it.

## Zig build specifics

### Zig 0.17 API facts `[VERIFIED: lib/std/Build/*.zig and compile errors this session]`
- C/C++ calls live on `Module`: `b.createModule(.{ .target, .optimize, .link_libcpp = true })`, `mod.addCSourceFiles(.{ .root = LazyPath, .files, .flags })` (`Module.zig:380-413`), `mod.addIncludePath`, `mod.addSystemIncludePath` (`Module.zig:470,476`), `mod.linkLibrary(lib)`.
- `b.addLibrary(.{ .name, .root_module, .linkage = .static })`, `b.addExecutable(.{ .name, .root_module })`; `exe.subsystem = .windows` still exists.
- **Removed:** `b.args` (use `run.addPassthruArgs()`, as `lib/init/build.zig:114` does), `b.pathFromRoot`. `Run.setCwd(LazyPath)`, `Run.setEnvironmentVariable`, `Run.addArg` exist (`Run.zig:567,647,675`). A run step fails the build on a non-zero exit code by default.
- Do not call `zig fetch --save` blindly on `release-3.4.x`: use the literal commit-pinned URL.
- `zig build` unpacks dependencies to `./zig-pkg/` (about 430 MB incl. Linux deps) and stores the tarballs in the **global** cache `p/*.tar.gz` `[VERIFIED: listing]`. 0.17 release notes: "`zig build` always fetches locally (in addition to globally)".
- Debug **and** ReleaseSafe C/C++ builds trap undefined behaviour (UBSan): a signed-overflow test panicked in Debug and exited non-zero in ReleaseSafe; ReleaseFast/Small do not check `[VERIFIED: skeleton experiment]`. `-Doptimize` accepts both `safe`/`fast` and `ReleaseSafe`/`ReleaseFast`.

### Per-OS link requirements
| OS | What is needed | Evidence |
|----|----------------|----------|
| Windows (`x86_64-windows-gnu`, Zig default) | Nothing to install. SDL's artifact links `kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32 dinput8` transitively; GUI exe needs `exe.subsystem = .windows`, `#include <SDL3/SDL_main.h>` in the TU defining `int main(int, char**)`. PE subsystem of the Debug exe = 2 (GUI), 31 MB | `[VERIFIED: castholm build.zig:1369-1381, PE header read]` stdout of a GUI-subsystem exe still reaches a piped caller (smoke output visible in `zig build smoke`) |
| Linux (Ubuntu 24.04) | **No apt packages**: X11/Wayland/ALSA/etc. are `dlopen`ed with soname data from the lazily fetched `sdl_linux_deps`; the binary's `ldd` shows only `libm`, `libc`, `ld-linux`. Smoke needs no `DISPLAY`. A real window needs a display server at runtime (WSLg Wayland worked: `video=wayland renderer=opengl`) | `[VERIFIED: WSL Ubuntu 24.04, only Zig 0.17.0 installed]` |
| macOS arm64 | Native build only on a mac with Xcode/CLT. SDL links `CoreMedia CoreVideo Cocoa UniformTypeIdentifiers(weak) IOKit ForceFeedback Carbon CoreAudio AudioToolbox AVFoundation Foundation GameController Metal QuartzCore CoreHaptics(weak)` and compiles its Objective-C sources with `-fobjc-arc` | `[CITED: castholm build.zig:1386-1401, 710, 1210-1264]`; not executed |

### Build step layout (verified)
`zig build` (default install: `efcore.lib/.a` + `ElectricField` exe), `zig build test`, `zig build run -- <args>`, `zig build smoke`, options `-Dgui=false` (core + tests only), `-Doptimize=...`. The test run uses `setCwd(b.path("."))`. Cold timings: Windows GUI build ~47 s; Ubuntu first `zig build test` incl. fetches and building libc++ ~2m47s on a 36-thread box (budget more on 4-core runners); warm `zig build test -Dgui=false` ~4 s on Windows, test run itself 0.4-0.65 s.

## CI gate (BUILD-04, D-11, D-12)

### Workflow
Candidate file: `research-ref/ci.yml` (YAML-parses; not executed on GitHub). Key choices:
- Triggers: `push` to `feature/cross-platform`, `pull_request`, `workflow_dispatch`; `permissions: contents: read`; `concurrency` cancels superseded runs; `fail-fast: false`; `timeout-minutes: 30`.
- Matrix: `windows-latest` (Server 2025 x64), `ubuntu-latest` (24.04 x64), `macos-latest` (**macOS 26, arm64**) `[VERIFIED: actions/runner-images README]`. The skeleton's Linux evidence is Ubuntu 24.04, the same image family.
- Steps per OS: `setup-zig` (version `0.17.0`) -> `zig version` -> **core gate** `zig build test -Dgui=false` (Debug+UBSan) -> `zig build test -Dgui=false -Doptimize=ReleaseFast` (catches optimisation/FMA-sensitive numerics; arm64 contraction is the reason) -> **GUI gate** `zig build` -> `zig build test` -> `zig build smoke`. The core gate first means a failing SDL/ImGui build on one OS cannot hide a physics failure.
- Never pass `-Dtarget` (macOS native build only).

### `mlugg/setup-zig`
- Repo moved to Codeberg; GitHub copy is a read-only mirror, usable as `mlugg/setup-zig@v2`; latest `v2.2.1` (2026-01-19) `[VERIFIED: gh api, README]`. Inputs: `version`, `mirror`, `use-cache` (default true), `cache-key`, `cache-size-limit` (default 2048 MiB), `use-tool-cache`. A concrete version string is passed through unchanged (`common.getVersion()`); tarball name logic uses `zig-<arch>-<os>-<version>` and the minisign key is hard-coded `[VERIFIED: common.js, main.js]`. `ziglang.org/download/index.json` lists `0.17.0` dated 2026-10-01 `[VERIFIED]`. It has not yet been run with 0.17.0 in a real workflow `[ASSUMED]` -> first CI run confirms; fallback: download the tarball from `https://ziglang.org/download/0.17.0/` and verify the sha256 from `index.json` (this exact procedure worked in WSL).
- Caching: it exports `ZIG_GLOBAL_CACHE_DIR` and `ZIG_LOCAL_CACHE_DIR` pointing at one cache directory and saves it in the post step (size-limited, cleared above 2 GiB). It does **not** mention `zig-pkg`; that is fine, because the global cache holds the `p/*.tar.gz` tarballs (including the git dependency) and `zig build` re-unpacks into `./zig-pkg` locally `[VERIFIED: global cache listing contains sdl-0.6.0+3.4.18-... .tar.gz]`. No extra `actions/cache` step is needed.

### Headless smoke per OS
`zig build smoke` runs the exe with `--smoke` (+ env `SDL_VIDEODRIVER=dummy`, `SDL_RENDER_DRIVER=software`). `SDL_VIDEO_DRIVER_DUMMY` is compiled for windows, linux, macos in the port `[VERIFIED: castholm build.zig:435]`. Windows and Linux verified; macOS expected to work (software renderer on a dummy window needs no GPU) `[ASSUMED]`.

### First-push protocol (D-12, one-way door)
`feature/cross-platform` has no upstream; `origin` is `github.com/jmfrank63/ElectricField`, a **public fork**, Actions enabled, zero workflows `[VERIFIED: gh]`. Pushing publishes 8 existing commits (all of `.planning/` research and requirements docs) plus the Phase 1 commits. The executor must stop and ask the user before `git push -u origin feature/cross-platform`.

### Failure playbook (macOS is the unverified OS)
1. Zig/SDL configure error on macOS about SDK paths -> check no `-Dtarget` was passed; check `xcrun --show-sdk-path` on the runner.
2. Link error for a framework -> inspect `zig build -v`; SDL declares them (list above).
3. `macos-latest` (26) newer than the SDK Zig knows -> pin `macos-15` once to compare. Zig 0.17.0 states "This release includes macOS system headers for version 27.0" `[CITED: release notes]`.
4. If the gate cannot be made green on Linux or macOS without hacks: stop, write the decision record, switch to Qt 6.12 LTS + CMake (core unaffected) per D-12.

## MFC baseline capture (QA-05, D-14)

### What was verified on this machine
- Build: `MSBuild.exe ElectricField.sln -p:Configuration=Release -p:Platform=x64 "-p:OutDir=<dir>\out\" "-p:IntDir=<dir>\obj\" -m -nologo -v:m` with `C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe` finished in **13 s** (v145 toolset from the working-tree `.vcxproj`), wrote only to the temp dirs, and left `git status` unchanged `[VERIFIED]`. Default (no OutDir) writes to `<repo>/x64/Release/` (OutDir = `$(SolutionDir)$(Platform)\$(Configuration)\`) and `ElectricField/x64/Release/` (IntDir): both covered by the new `.gitignore`. Use an out-of-repo OutDir anyway.
- Registry key (from `SetRegistryKey(_T("Free App"))` at `ElectricField.cpp:96` and `AFX_IDS_APP_TITLE "Electric Field"`, `ElectricField.rc:980`): `HKCU\Software\Free App\Electric Field\options` (values written only by `Options::Save`, `Options.cpp`; value names: `calculateEquipotentials numLines numThreads potentialInterval method elFieldLineThick potFieldLineThick positiveChargeColor negativeChargeColor electricFieldLineColor potentialFieldLineColor`). On this machine the `options` subkey exists and is **empty**, so defaults already apply. Reset = `reg export` backup then `reg delete ... /f` (implemented by `-ResetOptions`; restore with `reg import`). `Workspace` (docking state) is left alone.
- Start-up: because `InitInstance` registers nine `CMultiDocTemplate`s (`ElectricField.cpp:113-185`) and processes the default shell command, MFC shows a **"New" chooser dialog** (`#32770`, title `New`, owned by the main frame) containing a classic **list box id 100**, OK id 1 (UI Automation sees no list items, so use Win32 messages: `LB_GETCOUNT` = 9, `LB_GETTEXT`, `LB_SETCURSEL`, `BM_CLICK` on id 1). Item order = template order and the texts are the `.rc` `fileNewName` fields (preset 8: `Two Dipoles Oposed Direction`).
- After OK: main frame title `Dipole1 - Electric Field` (class `Afx:...`), MDI tabs keep the child maximised, the view is class **`AfxFrameOrView140u`**. The script restores the frame and resizes it until the **view's client area is exactly 1024x768**.
- Completion: the view paints lines only when all threads are done (`ElectricFieldView.cpp` `OnTimer` -> `GetData`); before that only the charges show. Reliable detector: count near-black pixels (> 1500; field lines are black, charges blue/red, equipotentials grey 192) and require 2 s of unchanged image hash. Compute time is < 1 s per preset in Release; whole run of 9 presets ~55 s (5.5-5.9 s per preset, mostly app start-up).
- Capture: `PrintWindow(view, hdc, PW_CLIENTONLY | PW_RENDERFULLCONTENT)`; results are **byte-identical** to `Graphics.CopyFromScreen` for all 9 presets, so no topmost/focus handling is needed and user mouse/keyboard input cannot disturb it (messages only, no input simulation).
- Reproducibility: three presets were captured twice; Four Charges and Two Dipoles Same Direction were byte-identical, Quadrupole differed in one pixel value (`magick compare -metric AE` = 0.047; 0 at `-fuzz 10%`; thread scheduling affects equipotential de-dup). A later full 9-preset run of the final script matched the earlier captures with 0 differing pixels at `-fuzz 10%`. Treat the baseline as "visually identical" and compare later with a fuzz tolerance.
- Script: `research-ref/tools/baseline/capture-mfc-baseline.ps1` (Windows PowerShell 5.1: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ... -Exe <exe> -OutDir docs\baseline\mfc [-ResetOptions]`). It validates the 9 chooser texts, warns when system DPI != 96 (the MFC app is not DPI-aware; run the baseline at 100% scaling; this machine is at 96 DPI), only kills the process it started, and writes `<slug>.png`.

### Risks and human-assisted fallback
| Risk | Mitigation |
|------|-----------|
| Display scaling != 100% makes DWM bitmap-scale the MFC window | script warns; run at 100% or capture manually |
| Different monitor size: initial window is maximised (3440x1400 here) | script restores and resizes explicitly |
| Computation takes longer on another machine | `-TimeoutSec` (default 180); script throws with the dark-pixel count instead of capturing a half-empty image |
| A modal dialog (crash dialog, MFC assertion) blocks | Release build has no ASSERTs; script times out and kills the process |
| MSVC with MFC missing on the executing machine | human checkpoint: build in VS, run each preset (File > New), crop the 1024x768 client area, save as `<slug>.png` |

### README template for `docs/baseline/mfc/README.md`
Record: date; MSVC `14.51.36231`, toolset v145, VS 18 Insiders, Windows 11 build 10.0.26100, system DPI 96; build command; options = defaults (`Options::Options()`, `Options.cpp:8-23`: 16 lines/unit charge, equipotentials on, interval 0.1, 4 threads, Euler, charges blue/red, lines black / grey 192, thickness 1); client size 1024x768; capture method `PrintWindow`; "Quadrupole is reproducible to within 1 pixel value"; slug list; note that the preset-8 chooser typo exists in the MFC resource.

## Git hygiene (BUILD-05, D-13)

`[VERIFIED: git ls-files, git ls-files -ci --exclude-from, scratch clone]`
- Tracked artifacts today: 37 files under `ElectricField/x64/Debug/` (19 `.obj`, 10 `.tlog` + 1 `.lastbuildstate`, `.pdb`, `.idb`, `.pch`, `.ilk`, `.log`, `.res`, `.recipe`) **plus** `ElectricField/ElectricField.vcxproj.user`; `.vs/` and `ElectricField/x64/Debug/vc145.*`, `link.secondary.1.tlog` are untracked noise; `x64/Debug/ElectricField.exe,pdb` at the repo root are untracked (matched by the old anchored pattern `/x64/Debug/ElectricField.exe`). The old `.gitignore` is entirely anchored at the root, which is why it never matched `ElectricField/x64/`.
- Commit order (each its own commit, no source edits mixed in): (1) `ElectricField/ElectricField.vcxproj` toolset change `v143 -> v145` (working-tree diff, 4 lines); (2) `git rm -r --cached ElectricField/x64 ElectricField/ElectricField.vcxproj.user` + new `.gitignore`. `git rm --cached` works on files with unstaged local modifications (36 of the 37 are currently modified in the working tree) `[VERIFIED: scratch clone]`; working-tree files stay on disk.
- Proposed `.gitignore` (`research-ref/gitignore.proposed`); `git ls-files -c -i --exclude-from=<it>` lists exactly 38 files = the 37 `x64` artifacts + `.vcxproj.user`, nothing wanted; no untracked file escapes it:

```gitignore
# ---- Zig ----
.zig-cache/
zig-out/
zig-pkg/
# ---- Visual Studio / MSVC (unanchored: MSBuild writes to <repo>/x64/ AND <repo>/ElectricField/x64/) ----
.vs/
x64/
Debug/
Release/
*.user
*.suo
*.obj
*.pdb
*.ilk
*.pch
*.idb
*.tlog
*.ipch
*.VC.db
*.VC.db-shm
*.VC.db-wal
*.aps
```
(The candidate file additionally contains `/build/`, `/out/`, `*.log`; drop them if you want the minimal set.) `.claude/CLAUDE.md` and `.planning/` stay tracked.
- Success-criterion check: `git ls-files | grep -E "(^|/)x64/|(^|/)\.vs/|\.vcxproj\.user$"` must print nothing, and `git status --porcelain` must be empty after `zig build` and after an MSBuild run (both verified to write only into ignored paths).
- Line endings: `core.autocrlf=true` here (index is LF, working tree CRLF); no `.gitattributes` exists. Adding `* text=auto` is optional and harmless; new files from the skeleton are LF.

## Common Pitfalls

### Pitfall 1: Dormand-Prince error estimate is not an error estimate
**What goes wrong:** treating DP like the other adaptive pairs in tests ("steps grow as tolerance tightens") fails; "fixing" the weights silently changes the DP lines vs the MFC baseline.
**Why:** `RungeKutta.inl:191` stores `b - b*` as the low weights; `thesumHigh - thesumLow = b*.K`, about `|f|`.
**Avoid:** keep the data, add the characterisation test (in the skeleton), record the decision in the plan; if fixed later, re-baseline.
**Warning signs:** DP takes ~3760 steps at tol 1e-3 for a unit-speed ODE.

### Pitfall 2: `sign()` rewritten as a three-valued sign
Returns 0 for zero charge/total and breaks every net-zero preset (sources would be empty or doubled). Keep `x >= 0 ? 1 : -1`; test `sign(0)==1`, `sign(-0.0)==1`.

### Pitfall 3: Passing a temporary functor to the solver
`SolveStep(Func& ...)` (original) rejects `Rot{}`; take `Func&&`. Observed as a compile error in the first experiment.

### Pitfall 4: Brace elision with nested `std::array` tableaus
Nested `std::array<std::array<...>>` aggregates need `{{{...}}}` counting and produced "excess elements" errors; plain C arrays inside an aggregate `Tableau<Stages>` initialise cleanly (used in the skeleton).

### Pitfall 5: `b.args` / `b.pathFromRoot` / C-source calls on `Compile`
All gone in 0.17 (see API facts); the `zig init` template of the installed version is the only reliable reference.

### Pitfall 6: `-Dtarget` on macOS, or a stale `-Dtarget` habit from the Windows dev machine
Triggers the SDK-path requirement of the SDL port. CI must build natively.

### Pitfall 7: Git Bash mangles Windows paths passed to `wsl`/`reg`
`MSYS_NO_PATHCONV=1` (or doubled slashes) is required when scripting local checks from Git Bash; `reg query HKCU\...` needs single quotes plus the same variable. (Only affects executors on this machine.)

### Pitfall 8: Capture script run at a scaled display
See baseline risks; a DPI-unaware MFC window is bitmap-scaled by DWM and the recorded "1024x768" would be wrong.

### Pitfall 9: Treating the first CI run as a formality
macOS is unverified. Budget iteration; keep jobs independent (`fail-fast: false`); decide on the Qt fallback by the D-12 rule, not by repeated hacks.

### Pitfall 10: Tests that depend on the MFC source after Phase 4
`test_presets.cpp` reads `ElectricField/ConfigDocs.cpp`; it skips with a message if the file is missing, but remove it deliberately in Phase 4.

## Code Examples

Everything below is in `research-ref/` and was compiled and run.

### Core flags and module wiring (`build.zig`)
```zig
const core_flags = [_][]const u8{ "-std=c++17", "-ffp-contract=off", "-Wall", "-Wextra", "-Wshadow",
    "-Wabsolute-value", "-Wconversion", "-Wsign-conversion", "-Wuninitialized", "-pedantic", "-Werror" };
const mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libcpp = true });
mod.addCSourceFiles(.{ .root = b.path("src/core"), .files = &core_sources, .flags = &core_flags });
mod.addIncludePath(b.path("src/core"));
const lib = b.addLibrary(.{ .name = "efcore", .root_module = mod, .linkage = .static });
// tests
tests_mod.addSystemIncludePath(doctest_dep.path("."));   // doctest warnings are not ours
const run = b.addRunArtifact(tests_exe); run.setCwd(b.path("."));
b.step("test", "Run the physics-core tests (doctest)").dependOn(&run.step);
// smoke
smoke.addArg("--smoke"); smoke.setEnvironmentVariable("SDL_VIDEODRIVER", "dummy");
```

### Convergence check (`tests/test_rk_order.cpp`)
```cpp
template <class Problem, class Solver> double globalError(Solver& s, int n) {
  Vec2 y = Problem::y0(); const double h = 1.0 / n;
  for (int i = 0; i < n; ++i) y = s.SolveStepFixed(Problem{}, y, i * h, h);
  return (y - Problem::exact(1.0)).Length();
}
CHECK(std::abs(std::log2(globalError<Rotation>(s, n0) / globalError<Rotation>(s, 2 * n0)) - nominal) <= 0.3);
```

### Seed planning (`src/core/ef/seeds.cpp`)
```cpp
const double angle_step = 2. * kPi / (std::abs(static_cast<double>(charge.charge)) * p.linesPerUnitCharge);
double angle_start = -angle_step / 2. - kPi;
const bool source = sign(total) == sign(charge.charge);
if (!source) angle_start += kPi + angle_step;
for (double angle = angle_start; angle < 2. * kPi + angle_start - angle_step / 4.; angle += angle_step) {
  const bool first = (angle == angle_start);
  if ((!first || !p.calculateEquipotentials) && !source) break;
  ...  // point = position + r * (cos angle, sin angle)
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `b.args` for passthrough args | `run.addPassthruArgs()` | Zig 0.17.0 | build.zig from web examples fails |
| Compile-step C/C++ helpers | `Module.addCSourceFiles/addIncludePath/linkLibrary` | Zig 0.16/0.17 | use `.root_module` everywhere |
| `@cImport` | removed (0.17) | 0.17.0 | irrelevant: C++ includes SDL headers directly |
| setup-zig on GitHub only | GitHub is a mirror of codeberg.org/mlugg/setup-zig | 2025/26 | keep using `mlugg/setup-zig@v2`; pin SHA |
| `macos-latest` = macOS 14/15 | `macos-latest` = macOS 26 arm64 | 2026 | Zig 0.17 ships macOS 27 headers; verify first run |

**Deprecated/outdated:** `allyourcodebase/SDL3` (symlink failure on Windows, Zig 0.16 minimum); Intel macOS (`x86_64-macos` obsolescent, no CI target); `x86_64-windows-msvc` (experimental in the SDL port).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The Zig+SDL3+ImGui stack builds, links and runs `--smoke` on macOS 26 arm64 (frameworks propagate transitively from the SDL static lib) | CI gate, Per-OS link | Phase gate fails -> Qt/CMake fallback decision (D-12); highest-impact unknown |
| A2 | `mlugg/setup-zig@v2.2.1` accepts `version: 0.17.0` without changes | CI gate | Trivial: fall back to tarball download + sha256 |
| A3 | `-Wconversion -Wsign-conversion -pedantic -Werror` stay clean on macOS arm64 (LP64, same Clang/libc++) | Defects | A warning on one OS fails the core gate; fix at source, never relax |
| A4 | Software renderer on SDL `dummy` window works on macOS runners | Headless smoke | Smoke step fails; fallback: skip window creation in `--smoke` on macOS, keep `SDL_Init` + renderer-less ImGui frame |
| A5 | 4-core GitHub runners finish a cold build well inside the 30 min timeout | CI | Raise `timeout-minutes`; caching helps after the first run |
| A6 | MSVC x64 reference arithmetic has no FMA contraction (default `/fp:precise`, no `/arch:AVX2`), so `-ffp-contract=off` matches it | Numerics | Last-bit differences on aarch64; tests are tolerance-based so impact is cosmetic |
| A7 | Keeping the DP quirk is the right call for "behaviour parity" | Open Questions | User may prefer the fix; changes DP output slightly |
| A8 | The first-push consent can be given in the same session (public fork, 8 planning commits become public) | CI gate | Delay of the gate |

## Open Questions

1. **Dormand-Prince error weights (defect 15).**
   - Known: `DPweightsLow` are `b - b*`; step control degenerates to roughly fixed step `~0.9 tol`; stepping solution is 5th order.
   - Unclear: whether the user wants parity (keep) or correctness (use `b*`: 5179/57600, 0, 7571/16695, 393/640, -92097/339200, 187/2100, 1/40).
   - Recommendation: keep + characterisation test in Phase 1, raise as a Phase 2 decision (re-baseline needed if changed).
2. **Is `traceFieldLine` in Phase 1 scope?** CORE-01/CORE-02 ("produce field lines") say yes, the phase boundary text lists only seed planning. Recommendation: include the pure single-line tracer (no threads, no equipotentials); it makes CORE-02 demonstrable and is the unit Phase 2 wraps.
3. **`-Dgui=false` option.** Not required by the criteria; recommended because it keeps the core gate independent of SDL/ImGui configuration on macOS. Costs one `if`.
4. **CI action pinning.** `setup-zig` pinned to a commit SHA in the candidate; `actions/checkout@v7` by tag. Acceptable at ASVS L1; tighten if desired.
5. **`macos-latest` vs pinned `macos-15`.** Start with `macos-latest` per D-11; pin only if the 26 image misbehaves.
6. **`AdjustForBezier` `t3` formula** (Phase 2, `FieldLine.cpp:215`, `t3 = t1 + t2 + (pt3 - pt2).Length()`): looks like total length plus the first segment; preserve for parity, flag when porting.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Zig | everything | yes | 0.17.0 (scoop) | - |
| Git | hygiene commits | yes | 2.56.0 | - |
| `gh` CLI (authenticated) | push, CI status | yes | authenticated | - |
| MSBuild (VS 18 Insiders) + MSVC 14.51.36231 with MFC | QA-05 baseline | yes | build verified (13 s) | human build in VS |
| Windows PowerShell 5.1 (`powershell.exe`) | capture script | yes | built-in | - |
| WSL2 Ubuntu 24.04 | optional local Linux check | yes (stopped by default) | 24.04.4 | CI only; research installed and then removed its own Zig copy |
| ImageMagick (`magick`) | optional pixel diff | yes | scoop | PowerShell hash compare |
| macOS machine | local macOS check | no | - | CI is the only macOS verification |
| Internet to github.com / ziglang.org | dependency fetch | yes | - | - |

**Missing dependencies with no fallback:** none for Phase 1 execution (macOS is CI-only by design).
**Missing dependencies with fallback:** macOS local verification -> CI.

## Validation Architecture

> `workflow.nyquist_validation` is `true` in `.planning/config.json`.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest 2.5.3 (single header, hash-pinned) |
| Config file | none (`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in `tests/test_main.cpp`); build wiring in `build.zig` `addTests` |
| Quick run command | `zig build test -Dgui=false` (about 4 s warm on Windows; test binary 0.4-0.65 s) |
| Full suite command | `zig build test -Dgui=false && zig build test -Dgui=false -Doptimize=ReleaseFast && zig build && zig build test && zig build smoke` (= the CI job body) |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BUILD-01 | `zig build` succeeds on 3 OSes | CI build step | `zig build --summary all` (job `build-test` x3) | Wave 0 (build.zig, ci.yml) |
| BUILD-02 | `zig build test` succeeds on 3 OSes | CI | `zig build test --summary all` | Wave 0 |
| BUILD-03 | 3 deps hash-pinned, no manual installs | static check + clean-cache CI run | `grep -c '\.hash = ' build.zig.zon` == 3 and no `.path =` in dependencies; fresh CI runner builds with only Zig | Wave 0 |
| BUILD-04 | CI green on windows/ubuntu/macos-arm64 per push | GitHub API | `gh run list --branch feature/cross-platform --workflow ci.yml --limit 1 --json conclusion,headSha` -> `success`; `gh run view <id> --json jobs --jq '.jobs[] | [.name,.conclusion]'` shows 3 successes | Wave 0 (`.github/workflows/ci.yml`) |
| BUILD-05 | artifacts untracked, ignores correct, tree stays clean | shell | `git ls-files \| grep -E "(^\|/)x64/\|(^\|/)\.vs/\|\.vcxproj\.user$"` empty; run `zig build` and the MSBuild baseline build; `git status --porcelain` empty | Wave 0 |
| CORE-01 | core is std-only, no globals | build + grep | `zig build test -Dgui=false` (no SDL/ImGui on the include path) and `grep -rEn "afx\|theApp\|windows\.h\|SDL\|imgui\|CString\|COLORREF" src/core` empty; `grep -rhoE '#include <[^>]+>' src/core \| sort -u` within the std allowlist | Wave 0 |
| CORE-02 | 11 methods, original order, produce lines | doctest | `zig build test -Dgui=false` (`method table`, `withSolver`, `every method traces a dipole field line`) | Wave 0 (`tests/test_rk_order.cpp`, `tests/test_seeds.cpp`) |
| CORE-03 | seeds = `|q|*L`, sinks 1, `sign(0)==+1` | doctest | same (`seeds are proportional...`, `net-zero configurations...`, `seed angles...`) | Wave 0 (`tests/test_seeds.cpp`) |
| QA-01 | vector math, analytic field/potential, convergence order | doctest | same (`test_vec2.cpp`, `test_field.cpp`, `test_rk_order.cpp`) | Wave 0 |
| QA-05 | 9 baseline PNGs + README recorded | script + file check + human look | `powershell.exe -File tools\baseline\capture-mfc-baseline.ps1 ...`; then `ls docs/baseline/mfc/*.png \| wc -l` == 9; `magick identify` each = 1024x768; README present | Wave 0 (script) |
| D-07 | preset data equals MFC source | doctest | `test_presets.cpp` parses `ElectricField/ConfigDocs.cpp` | Wave 0 |

### Sampling Rate
- **Per task commit:** `zig build test -Dgui=false` (core/tests tasks) or `zig build` (build/GUI tasks); `git status --porcelain` for hygiene tasks.
- **Per wave merge:** full suite command above on Windows; plus `wsl`-based Linux run when available (recipe: download the Zig tarball, verify its sha256 from `index.json`, copy the tree without `zig-pkg/.zig-cache/zig-out`, run the same commands).
- **Phase gate:** CI green on all three jobs for the pushed head commit, hygiene check clean, 9 baseline PNGs committed, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `build.zig`, `build.zig.zon`, `.gitignore` rewrite, `.github/workflows/ci.yml` (all from `research-ref/`)
- [ ] `src/core/ef/*`, `src/gui/main.cpp`, `tests/*.cpp` (from `research-ref/`)
- [ ] `tools/baseline/capture-mfc-baseline.ps1`, `docs/baseline/mfc/` (images + README)
- [ ] Framework install: none (doctest arrives through `build.zig.zon`)

### Not automatable / human steps
1. **First push consent (D-12)**: the user must approve before `git push`.
2. **Visual review of the 9 baseline images** (one look at each: charges at the right place and colour, lines complete); the automation guarantees capture, not correctness.
3. **macOS local execution**: impossible here; the CI log is the evidence. If the first macOS run fails, an interactive diagnosis may need a Mac or iterative CI runs.
4. **Real interactive window on each OS** (not just `--smoke`): not required in Phase 1 (Linux WSLg run succeeded incidentally; Windows `zig build run` is a one-minute manual check).

## Security Domain

> `security_enforcement` is enabled (ASVS level 1, `security_block_on: high`). Phase 1 has no network service, user data, auth or crypto; the relevant surface is supply chain and CI.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V1 Architecture (build/dependency boundaries) | yes | core has no third-party dependency; GUI deps isolated behind `-Dgui` |
| V2 Authentication | no | no accounts |
| V3 Session Management | no | - |
| V4 Access Control | yes (CI) | workflow `permissions: contents: read`; no secrets; no `pull_request_target` |
| V5 Input Validation | minimal | the only input is the `--smoke` flag (exact `strcmp`); preset file parsing in the test reads a repo file only |
| V6 Cryptography | no | none implemented; Zig package hashes and setup-zig's minisign check are library behaviour |
| V10 Malicious code / V14 Configuration (dependencies) | yes | content-hash + commit pinning (`build.zig.zon`), pinned action SHA, version-pinned Zig, no `curl | sh` |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Dependency substitution / moved tag | Tampering | `.hash` on every dependency, SDL pinned to a commit; clean-cache fetch verified |
| Compromised third-party GitHub Action | Tampering / Elevation | pin `mlugg/setup-zig` to the commit SHA (`d1434d08...`), `contents: read` token |
| Build script of a dependency runs code at configure time | Elevation | only three reviewed upstreams; lazy `sdl_linux_deps` commit-pinned; CI has no secrets |
| Untrusted downloaded toolchain | Tampering | setup-zig verifies minisign; manual route verifies sha256 from `ziglang.org/download/index.json` (done for the WSL check) |
| Local capture script misuse | Tampering | script only starts the given exe, kills its own PID, edits only the app's own `options` key after exporting a backup |
| Undefined behaviour in numerics | Tampering/DoS | Debug and ReleaseSafe builds trap UB (UBSan); `-Werror` warnings |

## Sources

### Primary (HIGH confidence)
- Repository sources read this session: `ElectricField/Vector2D.h/.cpp`, `RungeKutta.h/.inl`, `Charge.h/.cpp`, `TheElectricField.h/.cpp`, `FieldLine.h/.cpp`, `FieldLinesCalculator.h/.cpp`, `ConfigDocs.h/.cpp`, `ElectricField.cpp/.h`, `ElectricFieldDoc.cpp`, `ElectricFieldView.cpp`, `Options.h/.cpp`, `ElectricField.rc`, `resource.h`, `ElectricField.vcxproj`, `.gitignore`, `git ls-files`, `.planning/*`.
- Installed Zig 0.17.0 `lib/std/Build.zig`, `Build/Module.zig`, `Build/Step/Run.zig`, `lib/init/build.zig` (API facts).
- castholm/SDL `release-3.4.x` @ `dbcb19a` (`build.zig`, `build.zig.zon` read from the unpacked package).
- Local executions: Windows 11 + Zig 0.17.0 (skeleton, MSBuild of the MFC app, capture script) and Ubuntu 24.04 WSL2 with a sha256-verified Zig 0.17.0 tarball.
- GitHub API via `gh`: castholm/SDL, SDL_linux_deps, ocornut/imgui, doctest/doctest, mlugg/setup-zig, actions/checkout, upload-artifact, cache, repo `jmfrank63/ElectricField` (public fork, Actions enabled).
- `ziglang.org/download/index.json` (0.17.0 listed 2026-10-01) and Zig 0.17.0 release notes (https://ziglang.org/download/0.17.0/release-notes.html: macOS tier, headers for macOS 27, `addPassthruArgs`, `ZIG_LOCAL_PKG_DIR`).
- mlugg/setup-zig `action.yml`, `common.js`, `main.js`, `post.js` (https://codeberg.org/mlugg/setup-zig, mirrored at github.com/mlugg/setup-zig).
- actions/runner-images README (label mapping: `macos-latest` -> macOS 26 arm64, `ubuntu-latest` -> 24.04, `windows-latest` -> Server 2025).

### Secondary (MEDIUM confidence)
- `.planning/research/STACK.md`, `PITFALLS.md` (earlier project research; cross-checked and corrected where this session found differences: `b.args` removal, `pathFromRoot`, line numbers).

### Tertiary (LOW confidence)
- None used for claims; macOS execution claims are tagged `[ASSUMED]`.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - built and run on two OSes from a clean cache; pins verified against upstream.
- Architecture / port map: HIGH - source read line by line, 27 tests green, seeds/orders measured.
- CI: MEDIUM - YAML and action facts verified, but never executed on GitHub; macOS unverified.
- MFC baseline: HIGH on this machine (all 9 captured twice), MEDIUM for other machines (DPI/monitor assumptions documented).
- Pitfalls: HIGH - each was hit or measured in this session.

**Research date:** 2026-10-10
**Valid until:** about 2026-11-10 (Zig 0.17.x, SDL 3.4.x and ImGui 1.92.x are stable; re-check `release-3.4.x` tip only if the pin is changed)
