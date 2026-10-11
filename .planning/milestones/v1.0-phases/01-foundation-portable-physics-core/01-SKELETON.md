# Walking Skeleton — ElectricField Cross-Platform Port

**Phase:** 1
**Generated:** 2026-10-10

## Capability Proven End-to-End

On a fresh clone with only Zig 0.17.0 installed, `zig build test -Dgui=false` runs the doctest suite against the std-only `efcore` physics library, and `zig build smoke` starts the SDL3 + Dear ImGui hello window headlessly and prints `presets=9` read from the core. The same five `zig build` commands are the CI job body that must go green on Windows, Linux and macOS arm64.

This is a desktop/numerics project, so the template's web concepts are mapped to their honest equivalents below. There is no database, no HTTP routing and no auth; the plan does not invent them.

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Build system | Zig 0.17.0 `build.zig` + `build.zig.zon`, Zig 0.17 `Module` API (`b.createModule`, `mod.addCSourceFiles`, `.root_module`) | D-08/D-09: one command on every OS, no Visual Studio; API verified against the installed 0.17 `lib/std/Build` (web examples are stale) |
| Dependencies | Hash-pinned in `build.zig.zon`: SDL3 via `castholm/SDL` `release-3.4.x` @ `dbcb19aa3583c318556ecae3ecbeb29b102d37c5`, Dear ImGui `v1.92.9b-docking`, doctest `v2.5.3` | D-08/BUILD-03: content-hashed, commit-pinned, nothing else to install (Linux needs no apt packages; X11/Wayland are dlopen'ed). Reversibility: costly (switching to the Qt 6 + CMake fallback rewrites build + GUI; core unaffected) |
| Language / numerics policy | C++17; core and tests compiled with `-ffp-contract=off -Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -Wuninitialized -pedantic -Werror`; never fast-math; GUI/third-party code excluded from `-Werror` | D-03/D-04: numerics parity with the MFC x64 reference; strictness only where we own the code |
| Physics core ("data layer" equivalent) | `efcore` static library, namespace `ef`, header-mostly in `src/core/ef/`, standard library only, no globals; every former app-options read is an explicit `ComputeParams` argument | CORE-01/D-03: GUI-toolkit-independent, so it survives a Qt fallback unchanged; `-Dgui=false` proves it builds with no GUI dependency on the include path |
| Persistent data | None in Phase 1. The 9 presets are a compiled-in data table (`ef::presets()`, the `PresetRegistry` of D-07); the only "read" is that table, the only "write" is nothing — config-file persistence is Phase 4 (OPT-04) | No storage exists in the original app beyond registry options; inventing a DB would be scope creep |
| Auth | None (single-user desktop app, no accounts, no network service) | Out of scope by nature of the product |
| GUI stack ("UI" equivalent) | SDL3 window + Dear ImGui with `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3`; `-DImDrawIdx=unsigned int` in every ImGui TU; Windows exe is GUI subsystem via the SDL main header + `int main(int, char**)` | D-09/D-10: one backend on all OSes; the hello window reads core data, proving the GUI -> core link |
| Entry points ("routing" equivalent) | Build steps: `zig build` (install `efcore` + `ElectricField`), `zig build test` (doctest), `zig build run` (hello window), `zig build smoke` (headless start-up check); build option `-Dgui` (default true) | D-09/D-10: every capability is reachable as a `zig build` step; CI only orchestrates these |
| Windows target | `x86_64-windows-gnu` (Zig default), never `-msvc` | Avoids the MSVC-runtime vs libc++ clash; the SDL port's msvc target is experimental |
| Tests | doctest 2.5.3 in `tests/`, run with cwd = repo root, tolerance/invariant based (never bit-exact geometry) | D-05: cross-OS float differences must not fail the suite |
| CI ("deployment" equivalent) | GitHub Actions `.github/workflows/ci.yml`, matrix `windows-latest` / `ubuntu-latest` / `macos-latest` (arm64), `mlugg/setup-zig` pinned to commit `d1434d08867e3ee9daa34448df10607b98908d29` (v2.2.1), `permissions: contents: read`, core gate before GUI gate | D-11/BUILD-04: the go/no-go gate for the Zig + SDL3 + ImGui stack. First push needs explicit user consent (D-12, one-way) |
| Repo hygiene | Unanchored `.gitignore` for MSVC (`x64/`, `Debug/`, `Release/`, `.vs/`, `*.user`, `*.obj`, ...) and Zig (`.zig-cache/`, `zig-out/`, `zig-pkg/`); MSVC artifacts untracked | D-13/BUILD-05: `git status` stays clean after both MSVC and Zig builds |
| Directory layout | `build.zig`, `build.zig.zon`, `src/core/ef/` (physics), `src/gui/` (hello window now, real shell in Phase 3), `tests/` (doctest), `tools/baseline/` (MFC capture script), `docs/baseline/mfc/` (QA-05 images), `.github/workflows/`; the MFC tree `ElectricField/` stays untouched until Phase 4 | D-01/D-02: side-by-side coexistence so the MFC baseline comes from the real app |
| Fallback | If the CI gate cannot go green on Linux or macOS without hacks: Qt 6.12 LTS Widgets + CMake, core reused unchanged | D-12: decided up front so a red gate produces a decision record, not workarounds |

## Stack Touched in Phase 1

- [x] Project scaffold — `build.zig`/`build.zig.zon`, strict warning set, doctest runner (plan 01-01 tracer)
- [x] Routing equivalent — `zig build` steps `test`, `run`, `smoke` and the `-Dgui` option (plan 01-01)
- [x] Data layer equivalent — real read of the compiled-in preset table by the core and by the GUI (`presets=9`); no write path exists in Phase 1 (persistence is Phase 4, OPT-04) — stated honestly rather than invented
- [x] UI — the hello window renders real ImGui frames under SDL3 and reads core data; `--smoke` makes it runnable without a display (plan 01-01)
- [x] Deployment equivalent — the 3-OS CI gate (`ci.yml` in plan 01-01, first push behind the D-12 consent checkpoint and CI verdict in plan 01-04); local full-stack run command: `zig build test -Dgui=false && zig build test -Dgui=false -Doptimize=ReleaseFast && zig build && zig build test && zig build smoke`

## Out of Scope (Deferred to Later Slices)

- Threaded compute session, cancellation, equipotential lines and their de-duplication pass (Phase 2: CORE-04..06)
- Portability-defect regression tests for `FieldLineJob job(job)`, `AdjustForBezier` divide-by-zero, and the `t3` formula question (Phase 2: CORE-07)
- Dormand-Prince error-weight fix: Phase 1 keeps the MFC behaviour for parity and pins it with a characterisation test; whether to fix it is a Phase 2 decision (re-baseline needed if changed)
- Headless CLI renderer, SVG/PNG painter, cross-OS parity check in CI (Phase 2: QA-04)
- Real GUI shell: preset menu, tabs, busy indicator, Bezier/AA rendering, About box (Phase 3: VIEW-01..05)
- Options dialog, config-file persistence, PNG/SVG export, MFC deletion, README rewrite (Phase 4)
- Intel macOS, mobile/web targets, pan/zoom, progressive display (out of the v1 milestone)

## Subsequent Slice Plan

Each later phase adds one vertical slice on top of this skeleton without altering its architectural decisions:

- Phase 2: As the developer, I want a cancellable threaded compute engine with headless SVG/PNG output, so that CI can verify every preset on all OSes without a display.
- Phase 3: As a user, I want to open the 9 preset configurations in tabs, so that I can see their field lines and equipotentials on any desktop OS.
- Phase 4: As a user, I want to set options, keep them between runs and export images, so that the app fully replaces the MFC version.
