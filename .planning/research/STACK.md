# Technology Stack

**Project:** ElectricField cross-platform port (MFC to portable GUI, command-line build)
**Domain:** C++ scientific-visualization desktop app (vector drawing, options dialog, worker threads)
**Researched:** 2026-10-10
**Overall confidence:** HIGH on Windows build and rendering pipeline (spiked on this machine with Zig 0.17.0); MEDIUM on Linux and macOS runtime (cross-compiled or inferred, not executed)

## Recommendation (one paragraph)

Use **Zig 0.17.0 `zig build`** as the single build system, with **Dear ImGui 1.92.9b (docking tag) + SDL3 3.4.x via the `castholm/SDL` Zig port + the ImGui `sdlrenderer3` backend**, drawing the field with `ImDrawList` (`AddBezierCubic`, `AddCircleFilled`). Test with **doctest 2.5.3** (single header, built as a `zig build test` step). CI is a 3-OS GitHub Actions matrix using `mlugg/setup-zig`, each OS building natively. The alternative, if native-look widgets and real printing become mandatory, is **Qt 6.12 LTS Widgets + CMake/Ninja** (Zig is then dropped as the build system).

Why this wins: it is the only candidate for which the "zig build, no IDE, no system GUI dev packages, no MSVC" goal is actually achievable today. The user's strong preference for `zig build` is satisfied rather than traded away, and the rendering primitives needed (AA Bezier, filled circles, color picker, timer-style redraw while worker threads run) all exist in ImGui with zero extra dependencies. What the app gives up is native widget look, native MDI, and native printing. Of those, only printing is a listed requirement, and PROJECT.md already pre-authorizes replacing it with export.

## What was verified (spike, not assumed)

A throwaway project was built on this machine (Windows 11, Zig 0.17.0, no MSVC involved) at `C:/Users/jmfrank/AppData/Local/Temp/spike`. Results:

| Check | Result |
|-------|--------|
| `zig build` compiles ImGui 1.92.9b-docking (C++17) + `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3` + SDL3 3.4.18 (castholm port) | PASS (~25-50 s cold, cached afterwards) |
| doctest 2.5.3 built and run by `zig build test` | PASS |
| `std::thread` / `std::mutex` / `std::atomic` / `std::condition_variable` under Zig's libc++ on `x86_64-windows-gnu` | PASS (this is what blocked the MFC build; it is not an issue without MFC/MSVC runtime) |
| Headless render: SDL `dummy` video driver + `software` renderer + ImGui `AddBezierCubic` (3 px) + `AddCircleFilled`, then `SDL_RenderReadPixels` + `SDL_SavePNG` | PASS, anti-aliased output visually confirmed. Gives PNG export AND CI golden-image testing with no GPU/display |
| `SDL_GetPrefPath("org","app")` for config directory | PASS (resolved to `%APPDATA%\org\app\`) |
| GUI-subsystem exe (no console window) using `exe.subsystem = .windows` + `#include <SDL3/SDL_main.h>` and `int main(int, char**)` | PASS (PE32+ GUI) |
| Cross-compile `-Dtarget=x86_64-linux-gnu` from Windows | PASS (links, no Linux dev packages needed) |
| `-Dtarget=aarch64-macos` from Windows | FAILS BY DESIGN: SDL port demands `-Dsystem_include_path/-Dsystem_framework_path/-Dlibrary_path` (Apple SDK licensing). macOS must be built on a macOS host. |
| `allyourcodebase/SDL3` (the other SDL3 port) on Windows | FAILS: eagerly fetches a Wayland git dependency containing symlinks; `unable to create symlink ... PermissionDenied` without Developer Mode/admin. Do not use it. |

Not verified (no Linux/macOS machine available): native execution on Linux/macOS, SDL's Cocoa/Metal path compiled by Zig on a real Mac. The `castholm/SDL` README lists `aarch64-macos` as first-class when built on a macOS host with Xcode 14.1+; confirm in the first CI run (see PITFALLS-style flags below).

## Recommended Stack

### Build System and Toolchain

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Zig (`zig build`, `zig c++`) | 0.17.0 (installed) | Compiler (Clang 22.1.8 + bundled libc++), build system, package manager, cross-compiler | One command on every OS, no MSVC/CMake/Ninja/Make required, built-in package manager fetches and hash-pins SDL/ImGui/doctest, caches incrementally. Spike proved it builds the whole GUI stack. | HIGH |
| Target triple on Windows | `x86_64-windows-gnu` (Zig default) | Windows build | Zig's default ABI. Avoids the MSVC/libc++ clash that killed the MFC attempt. SDL port lists it first-class. Do not use `-msvc` (listed "experimental"). | HIGH |
| Pin file `build.zig.zon` | `.minimum_zig_version = "0.17.0"` | Reproducibility | Zig's `build.zig` API breaks between minors (0.17 split configurer/maker processes and renamed Build APIs); pin hard. | HIGH |

### GUI / Platform Layer

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Dear ImGui (docking tag) | v1.92.9b-docking (released 2026-07-31; stable `1.92.x`) | Widgets (menus, property tabs, color pickers, tables), all drawing via `ImDrawList` | Plain C++ source drop-in, trivial under `zig c++`. `ColorEdit4`/`ColorPicker4` replace the property-sheet color pickers; `BeginTabBar` replaces the property sheet; `BeginChild`/windows (or docking) replaces MDI. Large meshes (64k+ vertices) are supported by the sdlrenderer3 backend (`RendererHasVtxOffset`), so thousands of field-line points are fine. MIT. | HIGH |
| SDL3 via `castholm/SDL` (Zig port, branch `release-3.4.x`) | 0.6.0 + SDL 3.4.18 (`minimum_zig_version = "0.17.0"`, commit 2026-10-09) | Window, input, event loop, file dialogs, pref path, PNG save, high-DPI, timers | Only SDL3 port found that declares Zig 0.17.0 as its minimum and was spiked OK on Windows. Linux deps are lazy and supplied (`SDL_linux_deps`): X11/Wayland are `dlopen`ed at runtime, so no `-dev` packages at build time. `SDL_ShowOpenFileDialog`/`SDL_ShowSaveFileDialog`, `SDL_GetPrefPath`, `SDL_SavePNG` are in core, which removes three would-be dependencies. zlib license. | HIGH (Windows), MEDIUM (Linux/macOS execution) |
| ImGui backends | `imgui_impl_sdl3.cpp` + `imgui_impl_sdlrenderer3.cpp` (shipped in the ImGui tree) | Platform + render glue | `SDL_Renderer` picks D3D11 / Metal / OpenGL / Vulkan per OS automatically (no GL loader, no hand-written Metal/ObjC++), AND has a `software` driver which makes headless CI screenshots possible. Prefer this over `imgui_impl_opengl3` (needs loader, deprecated on macOS) and `imgui_impl_sdlgpu3` (needs shader pipeline, overkill). | HIGH |

### Drawing Mapping (MFC/Direct2D to ImGui)

| Existing (Direct2D) | Replacement | Notes |
|---------------------|-------------|-------|
| `CD2DPathGeometry` + `AddBezier` + `DrawGeometry(thickness)` | `ImDrawList::AddBezierCubic(p1,p2,p3,p4,col,thickness)` per segment, or `PathBezierCubicCurveTo` + `PathStroke` for a continuous path | Existing `AdjustForBezier` control-point math can be reused verbatim; it already yields cubic control points. Closed equipotentials: `PathStroke(..., ImDrawFlags_Closed)`. Tune `style.CurveTessellationTol` (default 1.25; lower to about 0.3 for smooth large Beziers). |
| `FillEllipse` for charges | `AddCircleFilled(center, radius, col, 0)` (0 = auto segments) | Prefer `AddCircleFilled` over ImPlot; no plotting lib needed. |
| `SetTransform(Scale(1,-1) * Translate(w/2,h/2))` | A 3-line world-to-screen lambda (`x*zoom + cx`, `cy - y*zoom`) applied to points before submit | ImDrawList has no transform stack; apply the transform on CPU. Cost is trivial for this data size. |
| `D2D1_ANTIALIAS_MODE_PER_PRIMITIVE` | `ImDrawListFlags_AntiAliasedLines | AntiAliasedFill` (default ON) | Fringe-based AA; visually good (verified), slightly softer than D2D on very thin lines. Keep `thickness >= 1.0f`. |
| WM_TIMER 100 ms poll + `Invalidate` | Render loop. While `calculator` has work, redraw each frame (cap at about 30-60 Hz); when idle use `SDL_WaitEventTimeout` | Immediate mode makes progressive display trivial: read the document's completed-lines list under its existing mutex each frame. |
| Print / print preview (GDI) | **Export**: PNG (offscreen `SDL_Texture` target, `SDL_RenderReadPixels`, `SDL_SavePNG`) and SVG (write `<path d="M ... C ...">` by hand; about 60 lines because the geometry is already Bezier) | Native printing is not available in SDL/ImGui. SVG is vector (print from any browser/viewer at any DPI), which is better for a physics figure than a GDI print path. |
| Registry persistence | `SDL_GetPrefPath` + small `key=value` file (hand-rolled, about 50 lines, no dependency) | Add nlohmann/json only if Options grows; not needed for about 12 values. |

### Testing

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| doctest | 2.5.3 (2026-07-06, MIT) | Unit tests for physics core (field/potential values, integrator order of accuracy, line sanity) | Single header, compiles in about 1 s, no library to build, `doctest::Approx` is enough for numerics. Spiked via `zig fetch --save` + `addExecutable` + `addRunArtifact`; non-zero exit fails `zig build test`. Active again (2.5.x in 2026). | HIGH |
| Headless render golden tests | SDL `dummy` driver + `software` renderer (SDL hints `SDL_HINT_VIDEO_DRIVER=dummy`, `SDL_HINT_RENDER_DRIVER=software`) | Detect visual regressions of the draw path with no GPU/display | Verified: a PNG was produced headlessly. Compare against stored PNG with per-channel tolerance (AA differs across SDL/ImGui versions, so use tolerance and re-baseline on upgrades). Run only on one OS (Linux) to avoid cross-OS float drift, or keep tolerance generous. | MEDIUM |

### Infrastructure / CI

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| GitHub Actions matrix | `ubuntu-latest`, `windows-latest`, `macos-latest` (arm64) | Native build + `zig build test` on all three OSes | macOS cannot be cross-built from Linux/Windows (Apple SDK licensing, confirmed by spike error and port README), so each OS builds natively. Intel macOS is effectively dropped (`x86_64-macos` marked obsolescent in Zig 0.17 notes; GitHub's Intel runners are retired). | HIGH |
| `mlugg/setup-zig` | v2.2.1 (`version: 0.17.0`) | Installs Zig, caches global package cache | De-facto standard action. | MEDIUM (confirm it accepts 0.17.0 on first run; fall back to downloading the tarball from ziglang.org) |
| `actions/upload-artifact` | current | Publish per-OS binaries | Statically linked exe (libc++ and SDL are static), one file per OS. | HIGH |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| stb_image_write | vendored single header | PNG writing | NOT needed: `SDL_SavePNG` (verified) already covers it. Only if SDL's PNG output proves insufficient. |
| plutovg | 1.3.3 (2026-05) | Software vector rasterizer (true AA strokes/Beziers, PNG out) | Only if ImGui's fringe-AA export quality is judged inadequate for publication-grade PNGs. Defer; it is a contained swap behind an `IRenderer`-style export function. |
| nativefiledialog-extended | 1.4.1 | Native file dialogs | NOT needed: SDL3 provides them. Listed only as a fallback if SDL's Linux portal/zenity path is unreliable. |
| Catch2 | 3.16.1 | Alternative test framework | Use instead of doctest if you want BDD sections or richer matchers (`WithinRel`/`WithinAbs`). Costs a compile of `catch_amalgamated.cpp` (cached by Zig). |

## build.zig skeleton (verified on Zig 0.17.0, Windows)

```zig
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const sdl_lib = b.dependency("sdl3", .{ .target = target, .optimize = optimize }).artifact("SDL3");
    const imgui_dep = b.dependency("imgui", .{});
    const doctest_dep = b.dependency("doctest", .{});

    // ImGui as a static lib
    const imgui_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libcpp = true });
    imgui_mod.addCSourceFiles(.{
        .root = imgui_dep.path("."),
        .files = &.{ "imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
                     "backends/imgui_impl_sdl3.cpp", "backends/imgui_impl_sdlrenderer3.cpp" },
        .flags = &.{"-std=c++17"},
    });
    imgui_mod.addIncludePath(imgui_dep.path("."));
    imgui_mod.addIncludePath(imgui_dep.path("backends"));
    imgui_mod.linkLibrary(sdl_lib);
    const imgui_lib = b.addLibrary(.{ .name = "imgui", .root_module = imgui_mod, .linkage = .static });

    // App: also add core.a (physics, GUI-free) and link it into both app and tests
    const exe_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libcpp = true });
    exe_mod.addCSourceFiles(.{ .files = &.{"src/app/main.cpp"}, .flags = &.{"-std=c++17"} });
    exe_mod.addIncludePath(imgui_dep.path("."));
    exe_mod.addIncludePath(imgui_dep.path("backends"));
    exe_mod.linkLibrary(imgui_lib);
    exe_mod.linkLibrary(sdl_lib);
    const exe = b.addExecutable(.{ .name = "ElectricField", .root_module = exe_mod });
    exe.subsystem = .windows;               // ignored on non-Windows
    b.installArtifact(exe);

    // Tests: doctest, no SDL/ImGui
    const t_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libcpp = true });
    t_mod.addCSourceFiles(.{ .files = &.{"tests/test_main.cpp"}, .flags = &.{"-std=c++17"} });
    t_mod.addIncludePath(doctest_dep.path("."));
    const t = b.addExecutable(.{ .name = "core-tests", .root_module = t_mod });
    b.step("test", "Run physics-core tests").dependOn(&b.addRunArtifact(t).step);
}
```

`build.zig.zon` dependencies (created with `zig fetch --save`):

```zig
.sdl3    = .{ .url = "git+https://github.com/castholm/SDL?ref=release-3.4.x#<commit>", .hash = "..." },
.imgui   = .{ .url = "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz", .hash = "..." },
.doctest = .{ .url = "https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz", .hash = "..." },
```

On Windows, `main` must be `int main(int, char**)` (C++ name mangling of `SDL_main` otherwise breaks the link; observed) and the main TU must `#include <SDL3/SDL_main.h>`.

## Installation

```bash
# one-time, from repo root (Zig 0.17.0 already installed)
zig fetch --save=sdl3    "git+https://github.com/castholm/SDL#release-3.4.x"
zig fetch --save=imgui   "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz"
zig fetch --save=doctest "https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz"

zig build                 # build app (Debug)
zig build -Doptimize=ReleaseFast
zig build test            # physics-core tests
```

Linux runtime needs only a normal desktop (X11 or Wayland libs present at runtime; SDL `dlopen`s them) and, for file dialogs, `xdg-desktop-portal` or `zenity`/`kdialog`. macOS build needs Xcode Command Line Tools (Xcode 14.1+) on the build machine.

Add to `.gitignore`: `.zig-cache/`, `zig-out/`, `zig-pkg/` (Zig 0.17 unpacks dependencies into the project's `zig-pkg/`; it was 429 MB in the spike), plus `.vs/` and `ElectricField/x64/` as already planned.

## Alternatives Considered

| Category | Recommended | Alternative | Why Not (or When) |
|----------|-------------|-------------|-------------------|
| GUI | ImGui + SDL3 | **Qt 6.12 LTS Widgets (QPainter)** | THE fallback. Maps 1:1 to MFC (QMdiArea, QPainter AA Bezier, QColorDialog, QSettings, QPrinter, QSvgGenerator, QTimer). But: (a) CMake only, Zig cannot realistically be the build system: prebuilt Qt is libstdc++ (Linux) / MSVC or MinGW ABI (Windows) while `zig c++` uses its own libc++, and Qt's `moc/uic/rcc` + `find_package(Qt6)` wants a CMake flow; (b) multi-GB install on every dev machine and CI runner (aqtinstall); (c) LGPLv3 means dynamic linking + `windeployqt`/`macdeployqt`/AppImage packaging; (d) needs system GL/xcb/font packages on Linux. Choose Qt if native printing, native look, or accessibility become hard requirements. Version: Qt 6.12 LTS (2026-09-30); 6.11.x is the prior minor. |
| GUI | ImGui + SDL3 | wxWidgets 3.3.3 (3.2.11 is the "stable" ABI branch) | Closest MFC analogue (native widgets, `wxGraphicsContext` AA Beziers, `wxPrintout`, `wxConfig`). But: MDI is emulated/limited off Windows, Linux needs GTK3 dev packages, source build of wx takes minutes per CI job, official build path is CMake, and Objective-C++ / GTK sources under `zig c++` are unproven (no maintained Zig port found). 3.3.x is "production-usable but not ABI-stable". Second-best native option after Qt. |
| GUI | ImGui + SDL3 | ImGui + GLFW | GLFW is equally buildable (`zglfw`), but SDL3 adds file dialogs, pref path and PNG save, saving three dependencies. |
| GUI | ImGui + SDL3 | raylib 6.0.0 + raygui | `build.zig` is first-class and requires Zig 0.17.0, so it builds well. But raygui is thin (no tabbed property sheet, weak layout, basic color picker), fewer text/UI primitives, and no native file dialogs/pref path. Reasonable only if the UI were trivial. |
| GUI | ImGui + SDL3 | FLTK 1.4 | Small and CMake/autotools-buildable, but AA vector drawing is backend-dependent (GDI on Windows is not anti-aliased), dated look, no Zig build story. |
| GUI | ImGui + SDL3 | Slint (C++ API) | Needs the Rust toolchain at build time (compiler + `slint-compiler`), contradicting the "just Zig" goal; GPL/royalty-free/commercial licensing model. |
| GUI | ImGui + SDL3 | GTK4 / libui-ng | GTK is not cross-platform on macOS/Windows in practice; libui-ng has AA path drawing but is sparsely maintained and has no printing, color-picker-in-sheet, or MDI. |
| SDL3 Zig port | `castholm/SDL` | `allyourcodebase/SDL3` (Games-By-Mason `sdl_zig`) | Spike failed on Windows: eager Wayland git fetch with symlinks dies without symlink privilege; also fetches about 25 Linux-only deps on every OS. Its `minimum_zig_version` is 0.16.0, not 0.17. |
| ImGui packaging | Compile ImGui sources directly in `build.zig` (about 15 lines) | `zig-gamedev/zgui`, `cimgui.zig`, `dear_imgui_zig` | Those are Zig-language bindings; the app is C++ and uses ImGui's native C++ API. zgui targets Zig 0.16 and ImGui 1.92.1 (stale). |
| ImGui plotting | Hand-written draw code | ImPlot | ImPlot targets axes/plots, not arbitrary Bezier world-space geometry with a y-up transform; it adds a dependency without helping. |
| Build system | `zig build` | CMake + Ninja | Only if Qt/wx is chosen. CMake works with the ImGui stack too (FetchContent), but requires a separately installed C++ compiler per OS (MSVC on Windows), i.e. what the project is trying to escape. |
| Test | doctest | GoogleTest 1.18.0 | Heavier (builds gtest/gmock libs), pthread/ABI knobs, no benefit for a few hundred numeric asserts. |
| Test | doctest | Catch2 3.16.1 | Fine and equally Zig-compatible; pick it if you prefer matchers/sections. Not a blocker either way. |
| Test | doctest | Zig `test` blocks | Cannot call C++ directly (needs a C ABI shim); do not split the test language. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| MSVC / MFC / `msbuild` / `.vcxproj` | The thing being removed; tied to one machine's toolset | `zig build` |
| `x86_64-windows-msvc` target | Marked experimental in the SDL port; invites the old MSVC-runtime vs libc++ clash | `x86_64-windows-gnu` |
| Prebuilt Qt/GTK/wx libraries linked from `zig c++` | libstdc++ vs Zig's libc++ ABI mismatch, symlinked sysroots, unfindable libs when `-Dtarget` is explicit (Zig only searches system paths for the native target) | Use CMake with the system compiler if you go Qt |
| `allyourcodebase/SDL3` | Breaks on Windows unless symlinks are permitted (spiked) | `castholm/SDL` `release-3.4.x` |
| `imgui_impl_opengl3` / `imgui_impl_win32` / `imgui_impl_dx11` | Per-OS backends = per-OS code and loaders; OpenGL is deprecated on macOS | `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3` |
| ImGui multi-viewport (`ViewportsEnable`) | Spawns real OS windows; flaky with SDL on macOS/Wayland; unnecessary since each configuration is just an ImGui window or dock tab | Docking only, viewports OFF |
| ImPlot | See table above | `ImDrawList` |
| `Zig @cImport` for SDL | Removed in Zig 0.17.0 (deprecated 0.16) | Not relevant: the app is C++ and includes SDL headers normally. If you ever add Zig code, use the ZSF `translate-c` package |
| `zig fmt`-style assumptions about ZLS | Zig 0.17.0's configurer/maker split breaks ZLS until Build Server Protocol is extended | Do not depend on editor ZLS for `build.zig`; use clangd for C++ (`compile_commands.json` can be produced separately) |
| Cross-compiling macOS from Windows/Linux | Not supported by Apple SDK licensing | Native `macos-latest` runner |

## Stack Patterns by Variant

**If native printing or OS-native widget look becomes a hard requirement:**
- Use Qt 6.12 LTS Widgets + CMake + Ninja + doctest/Catch2 via CTest; install Qt in CI with `jurplel/install-qt-action` (or aqtinstall); keep the physics core as a plain static library with its own `CMakeLists.txt` and ALSO its `build.zig` for `zig build test`. This is why the core must have zero GUI dependencies (already a project goal).
- Because: QPainter, QPrinter, QSvgGenerator and QMdiArea correspond one-to-one to the MFC features.

**If "ImGui looks too tool-like" is the only complaint:**
- Stay on the stack; load one TTF (embedded as a C array) and adjust `ImGuiStyle`. Do not switch frameworks for cosmetics.

**If export quality (thin-line AA) is the only complaint:**
- Keep ImGui for the interactive view; render exports with plutovg 1.3.3 (software AA rasterizer, C, builds trivially under Zig). Same world-to-screen mapping, different back end.

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Zig 0.17.0 | `castholm/SDL` `release-3.4.x` (min Zig 0.17.0) | Verified together |
| SDL 3.4.18 | ImGui 1.92.9b `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3` | Verified together |
| ImGui 1.92.9b-docking | C++11 or newer; build with `-std=c++17` to match project | No `imconfig.h` changes required |
| doctest 2.5.3 | `zig c++` / Clang 22 / libc++ | Verified |
| Zig 0.17.0 `zig c++` | Clang 22.1.8, libc++ 22 | Per release notes |
| Zig 0.17.0 known issues | Weak Zig libc symbol override (#36986), `Step.Run` response-file regression (#36444) | Neither affects this stack |

## Risks to Flag for Roadmap (research flags)

1. **macOS first CI run (MEDIUM risk).** Zig compiling SDL's Objective-C/Cocoa/Metal sources on a real Mac runner has not been executed here. Do a one-day spike PR in Phase 1 (hello-window + `zig build test` on `macos-latest`) before building the UI. Mitigation if it fails: pass `-Dsystem_include_path/-Dsystem_framework_path/-Dlibrary_path` from `xcrun --show-sdk-path`, or fall back to SDL via Homebrew + `linkSystemLibrary`.
2. **Linux first CI run (LOW-MEDIUM).** Needs `SDL_VIDEODRIVER=dummy` or `xvfb-run` for any test touching SDL. Physics tests need neither.
3. **File dialogs on Linux** depend on portal/zenity/kdialog at runtime; provide an in-app fallback path field.
4. **High-DPI.** Request `SDL_WINDOW_HIGH_PIXEL_DENSITY`, scale ImGui fonts/style by `SDL_GetWindowDisplayScale`; draw coordinates in logical pixels.
5. **Idle CPU.** Immediate-mode loops spin by default; gate rendering on events/active computation.
6. **Windows GUI subsystem + `SDL_main`** needs `int main(int, char**)` (observed link failure otherwise).

## Sources

- Zig 0.17.0 release notes: https://ziglang.org/download/0.17.0/release-notes.html (HIGH) - build system split, `@cImport` removal, Clang 22.1.8, macOS/Windows tier info
- castholm/SDL (`release-3.4.x`, `build.zig.zon`: SDL 3.4.18, min Zig 0.17.0): https://github.com/castholm/SDL (HIGH, read raw `build.zig.zon` and README; spiked)
- allyourcodebase/SDL3 (Games-By-Mason `sdl_zig`, SDL 3.4.16, min Zig 0.16.0): https://github.com/allyourcodebase/SDL3 (HIGH, spiked failure on Windows)
- Dear ImGui releases (v1.92.9b, 2026-07-31) and `imgui_impl_sdlrenderer3.cpp` (large-mesh support): https://github.com/ocornut/imgui (HIGH)
- SDL3 releases (3.4.18, 2026-10-02) and headers `SDL_dialog.h`, `SDL_surface.h` (`SDL_SavePNG`), `SDL_GetPrefPath` docs: https://github.com/libsdl-org/SDL, https://wiki.libsdl.org/SDL3/SDL_GetPrefPath (HIGH)
- raylib 6.0.0 `build.zig.zon` (min Zig 0.17.0): https://github.com/raysan5/raylib (HIGH)
- zgui `build.zig.zon` (min Zig 0.16.0, ImGui 1.92.1): https://github.com/zig-gamedev/zgui (HIGH)
- doctest v2.5.3, Catch2 v3.16.1, GoogleTest v1.18.0, plutovg v1.3.3, setup-zig v2.2.1 via GitHub releases API (HIGH)
- wxWidgets 3.2.11 / 3.3.3 release (2026-07-07): https://www.wxwidgets.org/news/2026/07/wxwidgets-3.2.11-and-3.3.3-released/ (HIGH)
- Qt 6.12 LTS (2026-09-30): https://www.qt.io/blog/qt-6.12-released, https://www.qt.io/development/qt-framework/latest-releases (HIGH); LGPLv3 licensing from package listings (MEDIUM; verify at download)
- zig c++ vs libstdc++ ABI mismatch discussion: https://ziggit.dev/t/neat-way-to-use-libstdc-in-zig/15816 (MEDIUM, community; the Qt-specific conclusion is my inference, not a source claim)
- Local spike (Zig 0.17.0, Windows 11): `C:/Users/jmfrank/AppData/Local/Temp/spike` (HIGH for Windows only)
