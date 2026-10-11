# Pitfalls Research

**Domain:** Brownfield port of a C++ MFC scientific-visualization app (multithreaded Runge-Kutta electric field lines) to a cross-platform GUI with a command-line build (zig build 0.17 or CMake) on Windows, Linux, macOS
**Researched:** 2026-10-10
**Confidence:** HIGH for code-level findings (read from the repo) and Zig 0.17 API facts (read from the installed `lib/std/Build`); MEDIUM for toolchain and ABI findings (web plus reasoning); LOW items are marked.

Evidence tags used below: **[CODE]** read directly from this repo, **[ZIG-LOCAL]** read from the installed Zig 0.17.0 `lib/std`, **[WEB]** web search or community source (MEDIUM), **[REASONED]** inferred and not independently verified (LOW to MEDIUM).

Phase names are suggestions, not the roadmap. They are used for mapping:
- **P1** Repo hygiene and build skeleton
- **P2** Core extraction (MFC-free library)
- **P3** Core tests and numeric baseline
- **P4** GUI shell and rendering
- **P5** Options, persistence and export
- **P6** Packaging and CI

---

## Critical Pitfalls

### Pitfall 1: Unqualified `abs()` on doubles silently truncates to int on GCC/Clang

**What goes wrong:**
The port compiles on Linux and macOS, but equipotential lines are missing or wrong and adaptive step control misbehaves. No error is raised. `abs(job.old_potential - potential) >= potentialInterval` (FieldLinesCalculator.cpp:31) becomes `abs(0.05)` = 0, so no equipotential job is ever posted. In `RungeKutta.h:71` and `:79`, `abs(h) <= min_step` has the same problem.

**Why it happens:**
The code calls unqualified `abs`, `fabs`, `floor`, `pow`, `sqrt`, `cos` and `sin`. On MSVC, `stdafx.h` pulls in `<afxwin.h>`, which drags in `<cmath>`, `<stdlib.h>` and the Windows headers, so `::abs(double)` is always visible. On libstdc++ and libc++, `::abs(double)` is only visible if `<cmath>` or `<math.h>` is included, and `<cstdlib>` alone gives only `int abs(int)`. `RungeKutta.h` includes only `<vector>`, `<valarray>` and `<array>`.
There is also a global template `abs(const Vector2D<T>&)` in `Vector2D.h:43`, which interacts with overload lookup. [CODE]

**How to avoid:**
- Replace every `abs`/`fabs` on doubles with `std::abs` plus an explicit `#include <cmath>`.
- Qualify `std::sqrt`, `std::pow`, `std::floor`, `std::cos` and `std::sin`.
- Rename the Vector2D helper to `Length(v)` or a member call so it cannot compete with `std::abs`.
- Add `#include <cfloat>` for `DBL_MAX`/`DBL_MIN` (RungeKutta.h:37), `<cstddef>` for `NULL`, and `<thread>`, `<algorithm>`, `<vector>` wherever they are used. `ElectricFieldDoc.cpp` uses `std::thread` without including it.
- Build the core with `-Wall -Wextra -Wconversion` and treat clang's `-Wabsolute-value` as an error.
- Do not rely on `stdafx.h` or a PCH. Delete it from core files.

**Warning signs:**
Equipotentials missing on Linux or macOS only. The compile log contains `-Wabsolute-value` or `-Wimplicit-int-float-conversion`. Adaptive methods produce suspiciously few or many steps.

**Phase to address:** P2 (fix includes and qualifiers); P3 (a test asserting that equipotentials are generated for the dipole proves it).

---

### Pitfall 2: `const double M_PI` redefinition and the `sign()` macro (with its special `sign(0) == +1`)

**What goes wrong:**
- **M_PI:** `FieldLinesCalculator.cpp:7` declares `const double M_PI = 3.14159...;`. On glibc (g++ defaults to `_GNU_SOURCE`) and on macOS, `M_PI` is already a macro in `<math.h>`/`<cmath>`. The line expands to `const double 3.14159... = ...` and fails with a confusing syntax error. On MSVC it only works because `_USE_MATH_DEFINES` is not set.
- **sign():** `RungeKutta.h:7` defines `#define sign(x) ((x) >= 0 ? 1 : -1)` globally. It leaks into every includer and can collide with GUI or system headers. Its semantics are load-bearing: `sign(0)` is `+1`.

**Why it happens:**
MSVC-ism. M_PI is not defined by default there, so a local definition looked harmless.
The sign macro is used for: `sign(total_charge) == sign(charge.charge)` (line choice for net-zero configurations such as the dipole), the `fieldLine` start criterion, and the equipotential sign test. With net charge 0, `sign(0)` must be +1 or dipoles seed lines from the wrong charge. A "cleaner" replacement with `std::copysign`, `(x>0)-(x<0)` or `signum` returns 0 and silently breaks every net-zero preset (Dipole, Quadrupole, both Two-dipoles, Parallel dipoles). [CODE]

**How to avoid:**
- Define `constexpr double kPi` in a core header, or use `std::numbers::pi` (C++20). Never name it `M_PI`.
- Replace the macro with `inline constexpr int sign(double x) noexcept { return x >= 0 ? 1 : -1; }`, in the core namespace, and name it `signNonNeg` or document the zero rule.
- Add a unit test: `sign(0) == 1`.

**Warning signs:**
`error: expected unqualified-id` at the M_PI line. Dipole renders only half of the lines or lines that never terminate.

**Phase to address:** P2; P3 covers the unit test.

---

### Pitfall 3: Latent UB `FieldLineJob job(job);` makes equipotentials compiler-dependent

**What goes wrong:**
In `PostCalculateEquipotential` (FieldLinesCalculator.cpp:37), `FieldLineJob job(job);` declares a new `job` in an inner block that shadows the parameter. The initializer refers to the new, uninitialized variable, not the outer parameter. It copies indeterminate values. On MSVC it may happen to reuse the same stack slot and appear to work. GCC and Clang warn (`-Wuninitialized`, `-Wshadow`) and may produce garbage jobs, so equipotential lines start at the wrong places. In Zig Debug builds, UBSan is enabled by default for C/C++, so this can also trap. [CODE; ZIG-LOCAL shows `sanitize_c` exists as off/trap/full, default not verified]

**Why it happens:**
Name shadowing plus MSVC's more lenient behavior. The bug is invisible with the reference compiler, which is why behavior parity is hard to judge by eye alone.

**How to avoid:**
- Rename to `FieldLineJob equipJob(job);` and set `equipJob.isEquipotential = true;`.
- Compile with `-Wshadow -Wuninitialized` during the port, and fix every warning rather than silencing it.
- Compare the fixed output against MSVC output for the Dipole before trusting either.

**Warning signs:**
Equipotential lines appear in different places or counts per platform or per build type. Valgrind/MSan flag uninitialized reads.

**Phase to address:** P2 (fix); P3 (regression test: equipotential count and start points for Dipole).

---

### Pitfall 4: The "core" is entangled with MFC, the `theApp` global, and logic hidden in the Document class

**What goes wrong:**
Deleting MFC removes behavior along with UI code, or the core still cannot compile without MFC.
- `FieldLine.h:8` includes `ElectricField.h`, so every core class sees `theApp`. `FieldLine::AddPoint`, `Charge::HitCharge` and `CalcThread`'s constructor and `StartCalculating` read `theApp.options.*`. This is also how the `Draw(CHwndRenderTarget*)`, `Draw(CDC*)` and `CRect` signatures leak in. [CODE]
- Real algorithm code sits in `CElectricFieldDoc`: `GetDataFromThreads()` contains the equipotential de-duplication (sorting by potential, `ref(-7., 5.5)` reference point, weight-centre distance `> 2*chargeRadius`, the `2/3` and `1/3` interval rules). `GetData()` runs `AdjustForBezier` on both line sets. If the Doc classes are deleted as "MFC UI", equipotentials render two to four times over for symmetric configurations. [CODE]
- Templates are implemented in `.cpp`/`.inl` files that include their own header at the bottom: `Vector2D.h` includes `Vector2D.cpp` at the end; `RungeKutta.h` includes `RungeKutta.inl` unless `__RK_IMPL`. Both start with `#include "stdafx.h"`. A build-system glob (`*.cpp`) compiles `Vector2D.cpp` as a separate TU. `Vector2D.cpp:10` has `#ifdef _DEBUG #define new DEBUG_NEW`, which on a non-MFC MSVC Debug build (where `_DEBUG` is set by `/MDd`) turns every `new` into an undeclared identifier. `__RK_IMPL` is a reserved identifier. [CODE]
- `CalcThread::PostCalculateEquipotential` and `StartComputingThread` rely on `theApp.options` being a process-wide global, so threads read it on restart.

**Why it happens:**
App-Wizard layering: the Doc was the controller, and "the model" grew inside it.

**How to avoid:**
- Introduce an immutable `Params`/`ComputationOptions` struct (distanceUnitLength, chargeRadius, numLinesOnUnitCharge, calculateEquipotentials, potentialInterval, numThreads, method). Pass it by value into the calculator, `FieldLine::AddPoint`, `HitCharge` and `CalcThread`. No globals in core.
- Move `GetData`/`GetDataFromThreads` (finish, swap, de-dup, Bezier adjust) into the core as a `FieldLinesCalculator::Finalize()` or `FieldResult` step, with tests.
- Make Vector2D a real header-only template (move the body into the `.h`, delete `Vector2D.cpp`). Do the same for RungeKutta (`.inl` becomes `.hpp`, drop `__RK_IMPL`, drop `stdafx.h`).
- Keep `RungeKutta.h` free of `Vector2D.h`; both can stay generic.
- Use an explicit source list for the core (no globs that sweep up MFC files such as `ChildFrm.cpp`, `MainFrm.cpp`, `PrecisionTimer.cpp`, which uses `LARGE_INTEGER`).
- Replace `PrecisionTimer` with `std::chrono::steady_clock`.
- Replace `ASSERT` (FieldLine.cpp:222) with the standard `assert` or explicit handling (see Pitfall 9).

**Warning signs:**
`#include <afxwin.h>` or `theApp` appears in a core translation unit. The core test binary needs `ElectricField.h`. Duplicate equipotential lines on the Quadrupole and Parallel dipoles.

**Phase to address:** P2 (this is the heart of P2). P3 verifies that the core links with no GUI dependencies.

---

### Pitfall 5: Shutdown with in-flight compute threads (detached threads, no cancellation inside the step loops)

**What goes wrong:**
Closing a window or quitting the app freezes for seconds, or crashes intermittently on exit on Linux and macOS but never on Windows.
- `ComputationThread::Start()` launches `std::thread(...).detach()`; the thread `delete this` and then does `++calc->finishedThreads`. Nothing joins them. [CODE]
- `Terminate` is checked only between jobs (FieldLinesCalculator.cpp:155, 162) and after a job. Inside `CalculateElectricFieldLine` the loop can run up to 40,000, 1,000,000 or 5,000,000 steps, and `CalculateEquipotential` up to 1.5 million steps. There is no `Terminate` check inside either loop, so "cancel on close" can take many seconds. [CODE]
- The MFC design hands a still-running calculator to the app (`theApp.calculators`) and `ExitInstance` busy-spins `while (calculators.size()) OnIdle(1);`, burning 100% of a core and, in a GUI loop, blocking the main thread. [CODE]
- Detached threads that outlive `main()` read and write global state. On Windows `ExitProcess` just kills them. On Linux and macOS, `exit()` runs static destructors while threads are still running, so a detached thread reads a destroyed object. This produces rare "crash on quit". [REASONED, standard C++ behavior]
- Restart path: a worker calls `StartComputingThread()` (PostCalculateEquipotential:46), and the new `CalcThread` constructor reads `theApp.options.*` on the worker thread while the UI thread may be writing options in the Options dialog. This is a data race. [CODE]
- Windows `ExitProcess` hides the problem, but macOS Cocoa shutdown and Qt's `QApplication` destructor order expose it.

**Why it happens:**
Fire-and-forget threads plus a global app object with a graveyard list; it works under MFC's message pump on Windows.

**How to avoid:**
- Check the cancellation flag every N steps (for example every 1,024) inside both loops. It does not change the numerics.
- Own the threads: use `std::jthread` with `std::stop_token`, or keep `std::thread` objects and join in the destructor. Do not detach.
- Make the computation state a `std::shared_ptr` captured by each worker, so a closed document can drop its handle without a global graveyard. Also join (with a short timeout) in the application's shutdown path.
- Snapshot `Params` once on the UI thread before starting any worker (see Pitfall 4). Restart paths pass the snapshot, never read globals.
- On app quit: set Terminate for all running calculators, then wait on a condition variable, never spin. Show a "closing..." state if needed.
- Run the shutdown test under TSan (Linux/macOS) and ASan.

**Warning signs:**
Quit takes more than about 1 s with a heavy configuration (Quadrupole, adaptive method, 4 threads). TSan reports races on `Options`. "Crash on exit" reports from Linux or macOS only.

**Phase to address:** P2 (cancellation and ownership design); P3 (TSan test and cancel-latency test: assert shutdown under 200 ms); P4 (document-close wiring).

---

### Pitfall 6: Touching UI from worker threads, and the "progressive display" mismatch

**What goes wrong:**
- The current design is already correct and portable: workers never touch the UI, and the view polls the document on a 100 ms timer (`OnTimer` calls `pDoc->GetData()`). The risk is "improving" it by having worker threads call `update()`, `Refresh()` or post draw calls directly. macOS Cocoa requires UI on the main thread (and crashes or logs main-thread-checker errors otherwise). Qt's `QWidget::update` is not thread-safe, wxWidgets needs `CallAfter`/`wxQueueEvent`, and SDL on macOS needs window creation and event pumping on the main thread. [REASONED, standard]
- **Requirement/implementation mismatch:** PROJECT.md lists "progressive display while computing" as an existing validated capability. The code does not do this. `GetData()` returns false until `CheckStatus()` (all threads finished), and only then swaps the line vectors into `field`. During computation the user sees only a wait cursor and a blank view. [CODE]
- If progressive display is added, `field.electricFieldLines` is unguarded UI-owned state, while `calculator->electricFieldLines` is mutex-guarded. The de-duplication and `AdjustForBezier` mutate lines in place and are only valid on a complete set. Reading the worker vectors without the lock is a data race.

**Why it happens:**
Documentation of the old app was written from memory of its intent.

**How to avoid:**
- Keep the polling model (QTimer/wxTimer/per-frame poll). Workers write only into mutex-guarded containers and an atomic done-counter.
- Decide explicitly in the roadmap: either keep "wait cursor then full draw" (parity with the real behavior) or specify true progressive rendering (copy-under-lock snapshots of new lines, Bezier-adjust each line as it arrives, apply de-dup at the end, and ensure lines can still be added without breaking Bezier triplet alignment).
- Correct PROJECT.md's Validated list.
- Wait cursor: the replacement is a per-widget busy cursor or a status/progress indicator, not a global one.

**Warning signs:**
macOS logs `NSWindow ... should only be called from the main thread`. Intermittent crashes while resizing during computation. Requirements say "progressive" but the acceptance test is a final-image comparison.

**Phase to address:** Requirements correction before P4; P4 for the polling and UI-thread design.

---

### Pitfall 7: Numerical drift between compilers can change which lines end where (FMA, libm, fast-math)

**What goes wrong:**
The same configuration produces a visibly different line (different termination, a missing or extra line, a different equipotential closing) on Apple Silicon or Linux than on the Windows reference.
- **Reference baseline:** the x64 MSVC configurations in the vcxproj set no `FloatingPointModel` (default `/fp:precise`, SSE2, no AVX2 so no FMA). Only the 32-bit configs set Strict (Debug) and Fast (Release). So the visual reference is plain un-fused IEEE double arithmetic. [CODE]
- **Contraction defaults differ:** GCC defaults to `-ffp-contract=fast` in GNU mode, and clang defaults to `on` (fuse within one expression). On aarch64 (Apple Silicon, ARM Linux), FMA is always available, so `a*b + c` in `Charge::E`, `Vector2D` operators and the Butcher-tableau accumulation compile to fused instructions with different rounding. On x86-64 the result depends on `-march`. [WEB, MEDIUM]
- **libm differences:** `sin`, `cos`, `pow` (used in the adaptive step update `pow(tolerance/error, 1/(Order+1))`) differ in the last ulp between glibc, Apple libm and UCRT. `sqrt` is correctly rounded everywhere. [REASONED]
- **Sensitive spots:** line termination is threshold-based (`HitCharge`: radius/1.5 units; equipotential closing: `dist*unit < 6 && t > 2*dist`; per-job step caps). Symmetric presets seed lines exactly on a symmetry axis that runs into a saddle point (Two equal charges, Quadrupole, Four charges same sign). There `E` is zero or nearly so, and `ENormalized` returns the zero vector only when it is exactly `0`; a 1-ulp difference flips the direction. Different noise gives different (but equally valid) trajectories. [CODE]
- **`long double`:** not used anywhere (everything is `double`), so x87 extended precision is only a concern on 32-bit x86 Linux; ignore except to forbid `-m32`/`-mfpmath=387`.

**Why it happens:**
Chaotic or marginal trajectories amplify last-bit differences. Different compilers legitimately make different choices.

**How to avoid:**
- Define a numeric policy for the core and apply it in both build systems: `-ffp-contract=off` for GCC/Clang (matches the reference baseline), and `/fp:precise` plus `/fp:contract-`-style settings for MSVC if used. Never `-ffast-math`, `-Ofast`, `/fp:fast`, or Zig/Clang `-ffast-math` on core code. Apply this to the core target only; the GUI may differ.
- Write tests as tolerance-based invariants rather than bit-exact golden files: field/potential values to 1e-12 relative, integrator order of convergence (halving h reduces error by 2^p), line endpoints land on a charge (distance within `chargeRadius/1.5`), line counts equal `GetNumberOfElectricFieldLines`, no NaN/Inf, and Hausdorff distance between a platform's polyline and a stored reference below a pixel-scale tolerance.
- For symmetric presets, assert topology (every line ends on an opposite charge or leaves to far field) rather than exact points.
- Run the same test suite on all three OSes and both x86-64 and arm64 in CI. A passing x86 Linux run does not cover Apple Silicon.

**Warning signs:**
Tests pass on Windows and fail by tiny amounts on macOS arm64. A line count differs by one. Saddle-point lines end differently per platform.

**Phase to address:** P3 (numeric policy and tests); P6 (arm64 CI runner).

---

### Pitfall 8: Coordinate system, HiDPI, and unit scale mismatches

**What goes wrong:**
Output appears mirrored, shifted, too small or too large, or lines look jagged or sparse at HiDPI or when exported at a different resolution.
- **Transform:** the D2D view uses `Scale(1,-1) * Translation(w/2, h/2)` (ElectricFieldView.cpp:134): y-up logical space, origin at the view centre. All drawing multiplies model coordinates by `distanceUnitLength` (100) and `chargeRadius` (25), which are device-independent pixels in the old app. [CODE]
- **Pixel-unit constants in the core:** `FieldLine::AddPoint` thins points at 8, 64 and 256 pixel gaps in three radius bands (`len` measured in pixels of the 100 px/unit scale), and `needs_precision = len < 3000`. These are tied to the unit scale and are numeric parameters, not rendering parameters. [CODE]
- **Clipping rectangle bug:** the view subtracts half the client size from `rect.left` and `rect.top` only. `right` and `bottom` stay at `w`, `h`, so the clip rectangle is `[-w/2, w] x [-h/2, h]`, not the symmetric visible area. This is harmless in practice (it over-includes) but must not be ported as is; compute the visible rectangle in logical coordinates. [CODE]
- **Bezier layout:** lines are stored as groups `p0,[p1,p2,p3],[p4,p5,p6],...` (shared endpoints). `AdjustForBezier` rewrites controls `p1,p2` of each group so the curve passes through the points, and the draw loop calls `AddBezier(p1,p2,p3)` (cubicTo). A new renderer that treats points as a polyline, or as quadratic segments, or reorders them, gives wrong shapes. [CODE]
- **Framework specifics:** Qt 6 works in logical (device-independent) pixels with `devicePixelRatio`, which matches the old DIP behavior. In SDL/GLFW + Dear ImGui there are separate window-point and framebuffer-pixel scales (macOS Retina is 2x, Windows can be 125% to 200%, Linux varies), and there is no transform stack, so you apply y-flip and centering manually for each point. Qt `painter.scale(1,-1)` also mirrors anything text-like or pixmap-based. wxWidgets needs `wxGraphicsContext` and `GetContentScaleFactor()`.
- **Export/print:** the GDI print path uses a different zoom model and `PolyBezier` with 16-point chunks. A PNG/SVG/PDF export that changes `distanceUnitLength` to scale the image would also change point thinning and the core numerics; it must scale the drawing transform, not the model parameters.

**Why it happens:**
Unit scale and display scale were never separated; Windows hid it.

**How to avoid:**
- Treat `distanceUnitLength`, `chargeRadius` and the AddPoint thresholds as fixed "model pixel" constants in the core. All renderers (screen, HiDPI, export) apply a view transform on top.
- Define the renderer contract once: input is lines in model units plus a `Viewport{ width, height, scale, flipY }`.
- Cache the built path or pixmap and rebuild on resize, option change, or data change. The D2D code rebuilt geometry on every `OnDraw`.
- Add a screenshot comparison on one preset at 1x and 2x scale against a stored image, with a pixel tolerance.

**Warning signs:**
Charges appear at the wrong corner, the picture is upside-down, line thickness changes with DPI, the export looks polygonal, or dipole charges are half the expected size on a Retina display.

**Phase to address:** P4 (renderer contract and HiDPI); P5 (export).

---

### Pitfall 9: `AdjustForBezier` division by zero is silently ignored in release, and ASSERT is an MFC macro

**What goes wrong:**
`FieldLine.cpp:222` has `ASSERT(abs(divi) > DBL_MIN);` and then divides by `divi`. `ASSERT` is an MFC macro that compiles to nothing in Release and does not exist outside MFC. When two consecutive points coincide, or `t1 == t2`, `divi` is 0 and the result is inf/NaN control points. A path with NaN points can draw garbage, throw in some backends, or hang a rasterizer (Qt may print warnings and skip; ImGui tessellation can loop badly). Because the thinning thresholds and step sizes depend on floating-point noise, it may appear on one platform only. [CODE]

**How to avoid:**
- Replace the assert with a guard: if `abs(divi)` is too small or any result is non-finite, keep the original `p1,p2` (or place them at 1/3 and 2/3 along the chord).
- Add a core test that runs `AdjustForBezier` on degenerate input and asserts all outputs are finite.
- Also verify `Normalize()` of a zero vector (`len != 0` check exists) and `Potential()` at a charge position (division by zero, returns inf).

**Warning signs:**
NaN in the point list; one line missing on one OS.

**Phase to address:** P2 (guard); P3 (test).

---

### Pitfall 10: Zig and system C++ GUI libraries: libc++ versus libstdc++ and MSVC-ABI mismatch

**What goes wrong:**
`zig build` links, or fails to link, the app against a system Qt, GTK/gtkmm or wxWidgets with undefined `std::` symbols, `__cxx11` mismatches, or crashes at startup.
- Zig's C++ support uses its own bundled libc++/libc++abi by default. Linux distributions and most prebuilt Qt/GTK/wx packages are built against libstdc++, which has different symbol names (`std::__cxx11::basic_string` versus `std::__1::basic_string`). Any API that exposes `std::` types across the boundary breaks. Qt's public API avoids exposing the standard library, so Qt on Linux often works; wxWidgets (std::string conversions), gtkmm, and any library with `std::string`/`std::function` in its API will not. [WEB; MEDIUM]
- A fresh (2026-05) Ziggit thread reports that setting `link_libcpp = false` still pulls in libc++ headers that conflict with libstdc++ headers. There is no supported "use system libstdc++" mode. [WEB; MEDIUM]
- **On Windows the target is `x86_64-windows-gnu` (MinGW ABI).** Official Qt binaries are MSVC-ABI (import `.lib`, MSVC name mangling) or MinGW built against libstdc++. Neither links cleanly against Zig's libc++. This is the same wall the project already hit with MFC (libc++ vs the MSVC runtime). [CODE: PROJECT.md prior finding; REASONED]
- Because of this, Zig is only a good fit when the GUI stack is built from source by the Zig build graph (SDL3, GLFW, Dear ImGui, raylib-style, and similar C or small C++ libraries vendored as `.zon` dependencies) or is a C-ABI library.

**How to avoid:**
- Decide the GUI stack and the build system together. If the stack is a system C++ framework (Qt, wxWidgets, gtkmm), choose CMake and use the platform's native compilers (MSVC or MinGW, GCC or Clang, Apple Clang). Use `zig c++` only as an optional compiler wrapper for the core and tests.
- If choosing Zig, vendor every dependency and build it from source with the same libc++. Keep the physics core independent of both so it can be built by either.
- Prove it early with a spike: a hello-window plus link of the core on all three OSes, before committing the roadmap.
- Check what the final binary links with `readelf -d`/`ldd`/`otool -L`/`dumpbin /dependents`.

**Warning signs:**
Linker errors with `__cxx11` or `__1` in symbol names. `-lstdc++` and Zig's libc++ both appearing in the link line. Headers like `<__config>` and `<bits/c++config.h>` mixed in the same compile.

**Phase to address:** P1 (spike and the build-system decision); P6 for CI.

---

### Pitfall 11: Zig 0.17 build API churn: most online examples fail to compile

**What goes wrong:**
Build scripts copied from blogs, LLM output or Zig 0.11-0.14 era examples fail with "no member named ..." errors.
Verified against the installed `0.17.0` `lib/std/Build` [ZIG-LOCAL]:
- `Step/Compile.zig` no longer has `addCSourceFiles`, `linkLibCpp`, `linkSystemLibrary`, `linkFramework`, `addIncludePath` or `addWin32ResourceFile`. These live on `Module` (`exe.root_module.addCSourceFiles(...)`, `.link_libcpp = true` in module options, `linkSystemLibrary("c++")` sets `link_libcpp`, `linkFramework`, `addFrameworkPath`, `addWin32ResourceFile`).
- `Module.addWin32ResourceFile` is marked Deprecated: "This functionality will be moved to an external package https://codeberg.org/ziglang/rc". Anything using it is on borrowed time. The resource script must not use MFC includes (`#include "afxres.h"`, `res\ElectricField.rc2`), or `zig rc` cannot find them.
- `win32_manifest` and `subsystem` are still on `Compile`.
- `sanitize_c` is an enum (`off`, `trap`, `full`), not a bool.
- `exe` creation takes `.root_module = b.createModule(...)`; the init template uses `b.addModule` / `b.path`.
- Zig does not discover C/C++ files via imports; every `.cpp` must be listed, and a `.cpp` file as `root_source_file` is an error.

**How to avoid:**
- Start from the `zig init` template of the installed version and the in-repo `lib/std/Build` source, not from web examples.
- Pin the Zig version in the repo (`build.zig.zon` `minimum_zig_version`, CI `setup-zig` pinned to 0.17.0) and do not float to nightly.
- Wrap deprecated pieces (resource files, manifests) in one small `build.zig` function so a later migration touches one place.
- If the CMake path is chosen, none of this applies, but record the decision.

**Warning signs:**
Compile errors in `build.zig` like `no field or member function named 'addCSourceFiles' in 'Build.Step.Compile'`.

**Phase to address:** P1.

---

### Pitfall 12: Git hygiene: tracked build outputs, wrong `.gitignore` anchors, `.vs/`

**What goes wrong:**
Builds dirty the working tree. `git status` shows `ElectricField/x64/Debug/*.obj`, `.pdb`, `.ilk`, `.pch`, `.idb`, `.tlog` as modified. 37 files under `ElectricField/x64/` are tracked, and a stray `git add -A` could commit `.vs/` (untracked now) with multi-hundred-MB `ipch` caches. [CODE: `git ls-files`]

**Why it happens:**
The `.gitignore` patterns are anchored at repo root (`/x64/Debug/ElectricField.exe`, `/.vs/ElectricField/...`), but the real build output is in `ElectricField/x64/...` and in `ElectricField/ElectricField/` projects. They never matched. It also lists individual `.vs` files rather than the directory. `ElectricField.vcxproj.user` is tracked too.

**How to avoid:**
- In its own early commit: `git rm -r --cached ElectricField/x64 ElectricField/ElectricField.vcxproj.user`, then replace `.gitignore` with directory-level patterns: `.vs/`, `**/x64/`, `**/Debug/`, `**/Release/`, `*.user`, `*.obj`, `*.pdb`, `*.ilk`, `*.pch`, `*.idb`, `*.tlog`, plus the new build dirs (`build/`, `zig-out/`, `.zig-cache/`, `out/`).
- Do not rewrite history on master (the project keeps the MFC version there). Old blobs stay in history; that is acceptable.
- Never combine this cleanup with source edits in one commit; it makes review impossible.
- Keep `.planning/` tracked or not as a deliberate choice.

**Warning signs:**
`git status` noise after a build; PR diffs of binary files; repository size growth.

**Phase to address:** P1 (first commit of the branch work).

---

### Pitfall 13: MSVC-only constructs that vanish or break outside MSVC

**What goes wrong:**
Individual items that fail or silently change behavior (all [CODE] unless marked):
- `stdafx.h` / `/Yu` precompiled header: every `.cpp` starts with `#include "stdafx.h"`. A missing file, or a PCH that must be first, breaks other compilers. Delete these includes and the file instead of emulating it.
- `#pragma comment(linker, "/manifestdependency:...")` (stdafx.h:44-52): ignored with a warning by clang and GCC. If the new GUI is Win32-native (wxWidgets), the visual-styles manifest silently disappears.
- `targetver.h` defines `_WIN32_WINNT 0x0600` and includes `<SDKDDKVer.h>`; this must not leak into portable code.
- `_T()`/`TCHAR`/`CString`/`LPCTSTR`: the core mostly uses `std::` types. UI strings and options keys use `L"..."` and `CString`. Wide strings are 16-bit on Windows and 32-bit on Linux/macOS; use UTF-8 `std::string` internally and the framework's string type at the UI edge.
- `COLORREF` is `0x00BBGGRR`. If used raw as 0xRRGGBB (Qt's `QRgb`, ImGui's `IM_COL32`, which is `0xAABBGGRR`, SDL), the positive charge colour (`RGB(0,0,255)`, blue) and negative (`RGB(255,0,0)`, red) swap channels. Store colours as `#RRGGBB` text.
- Windows `min`/`max` macros: the repo does not use them directly, but any file that includes `<windows.h>` (PrecisionTimer.h, `LARGE_INTEGER`) will break `std::min`/`std::max`. Keep `<windows.h>` out of the core, or define `NOMINMAX`.
- `M_PI` without `_USE_MATH_DEFINES` (see Pitfall 2).
- `typedef struct FieldLineJob { ... } FieldLineJob;` and aggregate initialisation `m_jobs.push_back({ charge, total_charge, has_different_signs, angle, angle_start, point, false, 0 })` need C++14. Note the initializer order `angle, angle_start` is swapped relative to the member order (`angle_start`, `angle`); it is harmless only because `PostCalculateEquipotential` compares them symmetrically. Use designated names or a constructor.
- Constructor init-order (`calcMethod` declared first, initialised last) triggers `-Wreorder`; do not enable `-Werror` until the first pass of warnings is cleaned.
- `Charge`'s implicit `Charge(int)` and `Vector2D(const T&)` converting constructors allow accidental conversions that MSVC accepts and clang flags in templates.
- `Vector2D::operator*=(const Vector2D<O>&)` returns `T` but does `return *this = *this * other;` (assigns a scalar into a vector, returns `Vector2D&` as `T`). It compiles on MSVC until instantiated, fails on clang if ever used. Remove it or fix it.
- `template<typename T> Vector2D<T> operator*(T o, const Vector2D<T>& t)` only works when both deduce the same `T`; `2 * v` (int) fails on every compiler, `2.0 * v` is fine.
- Set `/permissive-` / `-std=c++17 -pedantic` (or c++20) on the core target from the first commit; keep warnings visible.

**How to avoid:**
Compile the core with GCC and Clang early (CI Linux, plus `zig c++` locally) at `-Wall -Wextra -Wshadow -Wconversion -pedantic` and fix, not suppress.

**Phase to address:** P2.

---

### Pitfall 14: Windows resources, icons, manifest and subsystem in a non-MSVC build

**What goes wrong:**
The Windows binary opens a console window, has no icon, wrong DPI behaviour, or `zig rc` / `rc.exe` fails on the existing `.rc`.
- `ElectricField.rc` includes `afxres.h` and `res\ElectricField.rc2` (MFC files) and defines nine document-type icons (`IDR_*TYPE`) plus string-table entries that give each configuration its title and document name. The nine preset names, window titles and icons exist only inside the `.rc` string tables and `IDR_*TYPE` ids. Deleting the `.rc` without extracting them loses the user-visible preset names. [CODE]
- `zig rc`/resinator does not find MFC headers; a minimal new `.rc` must contain only `ICON` and optionally `VERSIONINFO`. [ZIG-LOCAL, WEB]
- The Windows GUI subsystem must be set explicitly (`exe.subsystem = .Windows` in Zig, `WIN32_EXECUTABLE` in CMake), otherwise a console window appears; with Zig you also need a `WinMain` or `main` entry that matches. [WEB]
- DPI awareness must come from the manifest or the framework: Qt 6 and SDL3 set it themselves; a plain Win32 or wxWidgets app needs a manifest (`win32_manifest` on `Compile` in 0.17, or an embedded `1 24` resource).
- macOS needs an `.icns` and `Info.plist`; Linux needs a `.desktop` file and PNG icon. `.ico` files cannot be reused.
- Registry-based file-type association (`RegisterShellFileTypes`, `ElectricField.reg`) has no equivalent and is not needed (no file format; `Serialize` is an empty stub).

**How to avoid:**
Extract the preset names and descriptions into a core table (id, title, charges) first, in P2; generate platform icons from `res/*.ico` once in P4/P6; put the subsystem, manifest and `.rc` handling in one build function.

**Warning signs:**
A black console behind the window; generic icon; blurry window on a 150% display; empty window titles.

**Phase to address:** P2 (preset table); P4/P6 (icons, manifest).

---

### Pitfall 15: macOS-specific build and runtime traps

**What goes wrong:**
- Linking frameworks (Cocoa, Metal, IOKit, CoreVideo) and Objective-C needs the macOS SDK. Native `zig build` on a Mac autodetects it; cross-compiling from Windows or Linux needs `SDKROOT`/`--sysroot` pointing to an SDK. `zig cc`/`zig c++` handled this less automatically than `zig build` in older versions, so verify on 0.17. [WEB; MEDIUM]
- Objective-C/Objective-C++ (`.m`/`.mm`) is needed by SDL, GLFW and Qt internals; if the core is built as C++ only, keep Cocoa-touching code in a separate target/module gated on the Darwin target.
- GUI and event loop must be on the main thread (Pitfall 6). Secondary threads get a 512 KB default stack on macOS versus 1 MB (Windows) and 8 MB (Linux); the CalcThread frames are small and solvers live on the heap, so this is not a current problem but do not add recursion or big stack arrays to the core.
- An unsigned app downloaded from CI is quarantined: Gatekeeper blocks it, and arm64 binaries must be at least ad-hoc signed to run at all. Notarization needs a hardened runtime, a timestamp and an Apple Developer account. [WEB; MEDIUM]

**How to avoid:**
Decide up front whether macOS artifacts are "build from source" only (README instructions) or signed releases. For a personal educational project, "build from source plus ad-hoc signed zip" is the pragmatic choice; list notarization as out of scope. Test on a real macOS arm64 runner, not only cross-compiled output.

**Phase to address:** P1 (decision), P6.

---

### Pitfall 16: Qt/CMake deployment: the app runs only on the developer's machine (if Qt is chosen)

**What goes wrong:**
The build runs, but the packaged app fails elsewhere: "could not find or load the Qt platform plugin 'windows'", missing `Qt6Core.dll`, or a macOS app that is rejected after signing.
- `windeployqt` must run after the build, pointed at the exe, and with the right Qt `bin` on `PATH`. Missing `platforms/qwindows.dll`, `styles`, `imageformats` (icons, PNG export) and the MSVC/MinGW runtime DLLs are the usual causes. [WEB; MEDIUM]
- `macdeployqt` can fail to include a framework and then fail the signing step; manually copied frameworks break the seal ("unsealed contents"). Sign after deployment, and avoid `--deep` as the only mechanism. [WEB; MEDIUM; Qt 5.12-5.14 era reports]
- Debug and Release Qt DLLs are not interchangeable; deploying a Debug build next to Release DLLs crashes at startup.
- Linux: system Qt versions differ between distributions; an AppImage or distro packages are separate work. Wayland vs X11 plugins (`xcb`, `wayland`) differ in what is installed.
- Qt licensing: LGPL requires dynamic linking (or an offer to relink) and notice text; GPL-only modules must be avoided. Check the project's LICENSE against this before choosing Qt. wxWidgets and SDL/ImGui are more permissive.
- CI Qt installation (`jurplel/install-qt-action`, aqtinstall) must pin versions and the same compiler that CI uses to build the app.

**How to avoid:**
Make "deployable artifact runs on a clean VM/container" an explicit acceptance criterion; script `windeployqt`/`macdeployqt` in CMake `install()` or CI; pin Qt minor version.

**Phase to address:** P6 (and the framework decision in P1).

---

### Pitfall 17: Options persistence rewrite loses fidelity

**What goes wrong:**
Defaults or saved values differ from the MFC app, or round-trip changes values.
- `potentialInterval` is stored as `int(1000 * value)` (truncation): `0.029*1000 = 28.999...` becomes 28. A text format that stores doubles, or rounding, avoids it. [CODE]
- Colours are stored as raw `COLORREF` binary (BGR layout; Pitfall 13). Registry values from the old app are not migrated; this is acceptable, but say so.
- Defaults must be taken from `Options::Options()`: `distanceUnitLength` 100, `chargeRadius` 25, lines 16, equipotentials on, interval 0.1, 4 threads, Euler method, black/grey lines, blue/red charges, thickness 1. Note: `Options::Load()` reads the interval default as `100` (= 0.1) and the other values consistently, but `numThreads` default 4 on a machine with 2 or 64 cores is a UX decision. [CODE]
- The enum `CalculationMethod` is persisted as an integer. Reordering the enum breaks old config files; persist names.
- Config location: use the platform's user config directory (`%APPDATA%`, `~/.config` or `$XDG_CONFIG_HOME`, `~/Library/Application Support`), through the framework's helper; do not write next to the exe (read-only on Linux and macOS app bundles).
- Validation: clamp loaded values (threads > 0, lines > 0, interval > 0). A zero interval or zero lines causes `floor(potential/0)` or `2*M_PI/0` (infinite loops or inf angles).

**Phase to address:** P5.

---

## Moderate Pitfalls

### Pitfall A: Line and thread counts scale badly; limits are in the core

**What goes wrong:** the net-zero case allows 5,000,000 steps per line, and an equipotential up to 1.5M steps with a 0.0001 step, with `numLinesOnUnitCharge` lines per unit charge. Higher charge or line settings multiply job count. On macOS laptops the default 4 threads may thermally throttle, and in Debug builds (Zig default `Debug`, `-O0`, UBSan) the same job takes 10-50x longer, so "it hangs" is common when testing in Debug.
**Prevention:** default the build to ReleaseSafe/RelWithDebInfo for runtime testing; make Debug tests use small `numLinesOnUnitCharge` and a step cap; expose a progress counter; document expected times per preset.

### Pitfall B: Using `hardware_concurrency()` blindly

**What goes wrong:** `numThreads` is a user option; zero or very large values are not validated. `std::thread::hardware_concurrency()` can return 0.
**Prevention:** clamp to `[1, max(1, hardware_concurrency())*2]`; keep the user setting.

### Pitfall C: Restarting finished threads can lose or double-start work

**What goes wrong:** `PostCalculateEquipotential` decrements `finishedThreads` and starts a new thread while holding `m_jobsSection`. Combined with `CheckStatus()` comparing `finishedThreads < startedThreads`, a narrow window exists where `CheckStatus()` reports completion while jobs remain, or a thread is counted twice. The counters mix "started" and "finished" semantics. [CODE; REASONED]
**Prevention:** replace with a job counter (pending + running jobs under one mutex) and a condition variable; a calculation is done when pending == 0 and running == 0. Write a stress test (many runs, TSan) in P3.

### Pitfall D: Rendering the whole scene on every paint

**What goes wrong:** the old D2D code rebuilt a path geometry per line per repaint and relied on GPU-backed rendering. With CPU painters (QPainter raster, wx GDI+, ImGui draw lists) a resize or a mouse hover triggers a full rebuild of tens of thousands of points.
**Prevention:** cache a `QPainterPath`/pixmap per document; invalidate only on resize, option changes or new data; avoid per-frame rebuilds in immediate-mode GUIs by caching a vertex buffer.

### Pitfall E: Multiple documents (MDI) have no portable equivalent

**What goes wrong:** MFC MDI gives tabbed/child windows with per-document state. Cross-platform frameworks differ: Qt has `QMdiArea` or tabs, wxWidgets has `wxDocMDIParentFrame`, ImGui/SDL have only your own windows. The macOS convention is one window per document.
**Prevention:** decide early: tabs or separate top-level windows, one "view" widget bound to one `Calculation` object; do not carry over MFC's doc/view framework as a layer.

### Pitfall F: `RungeKutta.inl` design details to preserve

**What goes wrong:** the solver keeps stage values in a member `K`, so a solver instance is not thread-safe. The code is correct because each `CalcThread` owns one instance (`new`ed in `StartComputingThread`). Replacing the per-thread solver with a shared one for "cleanliness" gives data races. Also, the non-adaptive `SolveStep` ignores `max_step`, which affects `step` handling in the caller. The Dormand-Prince tableau row 6 equals the high-order weights (FSAL), not a bug. The default arguments `DBL_MIN` as `min_step` mean "smallest positive normal", not "most negative" (a typical misread).
**Prevention:** add a comment and a test that two threads use two solvers; keep tableaus as `constexpr` arrays and test each method's order of convergence (Pitfall 7) to catch typos.

### Pitfall G: Hidden reliance on integer/unsigned arithmetic

**What goes wrong:** `charge.charge * theApp.options.numLinesOnUnitCharge` multiplies `int` by `unsigned`; a negative charge wraps to a huge unsigned. In `GetNumberOfElectricFieldLines` the sum wraps back correctly only through modular arithmetic. `-Wsign-conversion` flags it and a "fix" with `size_t` changes the result.
**Prevention:** convert to `int` or `double` explicitly; test `GetNumberOfElectricFieldLines` for negative total charge.

---

## Minor Pitfalls

- **`PrecisionTimer`** uses `QueryPerformanceCounter`; replace with `std::chrono::steady_clock`.
- **`README` still says VS2015**; update at the end (Active requirement).
- **Case-sensitive file systems:** Linux is case-sensitive. Check `#include "Stdafx.h"` style mismatches; the repo uses consistent names today, but `ElectricField.h` vs the project dir `ElectricField/` is fine. Windows will not warn you.
- **Line endings and BOMs:** sources are probably CRLF. A `.gitattributes` with `* text=auto` avoids noisy diffs and shell-script failures with CRLF.
- **Filename vs class mismatch:** `ConfigDocs.cpp` uses namespace `ChargeConfigurations` while the declared names are `CDipoleDoc`... Rename to plain data (a preset list) when removing MFC.
- **Window cursor:** `BeginWaitCursor`/`RestoreWaitCursor` have no direct portable equivalent (Pitfall 6).
- **`ElectricField.reg`** (tracked): delete with the MFC code.
- **Unicode paths/usernames:** config directory must handle non-ASCII user names on Windows (use the framework API or UTF-8 plus wide conversions).

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keep `theApp.options` as a global singleton "just in the new app" | Fastest compile of the core | Races (Pitfall 5), untestable core, UI coupling | Never in core; an app-level options object is fine if converted to a `Params` snapshot |
| Emulate `stdafx.h`/PCH with a force-include | Avoids touching every file | Hides missing includes (Pitfall 1), breaks on other compilers | Never |
| Suppress warnings (`-w`) to get GCC/Clang building | Quick first build | Hides the abs() truncation, shadowing and UB findings | Only for third-party code |
| Bit-exact golden files for line geometry | Easy to write | Fails on every platform/compiler difference (Pitfall 7) | Only same-compiler, same-flags, single-platform smoke test |
| Detached threads with `delete this` | No ownership design | Exit races, un-cancellable work | Never (replace with owned/joinable threads) |
| Copy MFC doc/view structure into the new GUI | Mirrors old design | Carries MFC concepts and the logic-in-Doc trap | Never |
| `-ffast-math` for speed | Faster | Changes termination and closure tests (Pitfall 7) | Never for core |
| One monolithic target (core + GUI) | Simple build file | Core cannot be tested headless in CI | Never; keep core a separate library |
| Floating `zig` version in CI | Always new | Build-script breakage on every release (Pitfall 11) | Never |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| System Qt/GTK/wx + `zig c++` | Assume libc++ and libstdc++ interoperate | Use CMake with the native compiler, or vendor and build the GUI from source with Zig (Pitfall 10) |
| Windows resources in Zig 0.17 | Use `exe.addWin32ResourceFile` (removed from Compile) or the MFC `.rc` | `exe.root_module.addWin32ResourceFile` with a minimal `.rc` (icon, version), noting its deprecation |
| macOS frameworks | Expect cross-compile from Windows to work | Native macOS runner, or provide an SDK via `SDKROOT`/`--sysroot` |
| Config storage | Registry or a file next to the exe | Platform config dir, text format with names and `#RRGGBB` |
| Colours | Pass `COLORREF` into a Qt/ImGui/SDL colour | Convert per-channel explicitly |
| Threads and UI | Call widget methods from workers | Workers fill guarded containers; UI polls on a timer or receives a queued signal |
| Export instead of print | Re-run `AddPoint` thinning at a new scale | Re-draw the same model lines with a larger view transform |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Debug-mode compute (`-O0`, UBSan trap) | "App hangs" on a net-zero preset | Test compute in ReleaseSafe; scale parameters down in Debug tests | Dipole/Quadrupole with 5M-step caps |
| Rebuilding path geometry every repaint | Laggy resize | Cache the path/pixmap | Tens of thousands of points, HiDPI |
| Full scene redraw per ImGui frame | 100% CPU idle | Retained vertex buffer or redraw on change | Immediate-mode GUI |
| Busy-wait for thread completion | CPU burn on close | Condition variable / join | Any close with running threads |
| Over-subscription (`numThreads` > cores in Debug) | Slower than 1 thread | Clamp and default to cores | Small laptops |
| Bezier adjust on the UI thread (spawns 2 threads and joins) | UI freeze when finished | Do it on the worker side or a finalize thread | Large result sets |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Trusting config file values (threads, lines, thickness) | Resource exhaustion / infinite loops from a hand-edited or corrupt config | Clamp and validate on load (Pitfall 17) |
| Unsigned binaries from CI | Gatekeeper/SmartScreen warnings; supply-chain trust | Publish checksums; document; sign only if releases matter |
| Third-party dependencies fetched at build time without pinning | Supply-chain drift | Pin versions/hashes (`build.zig.zon` hashes, CMake `FetchContent` tags/hashes) |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| No feedback during long computations (the old app's wait cursor is gone) | App looks frozen | Status bar progress ("N of M lines") from the atomic counters |
| Print replaced by nothing | Loss of an existing capability | Export PNG (and SVG/PDF if the framework supports it) |
| Dialog behaviour differences (modal Options with Apply) | Settings not applied to running calculations | Define: options apply to new documents; running documents keep their snapshot |
| Menus/shortcuts differ by OS (Ctrl vs Cmd, app menu on macOS, About/Quit placement) | Feels foreign | Use the framework's standard roles/shortcuts |
| Pixel-based sizes on HiDPI | Tiny or huge UI | Use logical units and test at 100% and 200% |

## "Looks Done But Isn't" Checklist

- [ ] **Core extraction:** compiles with no MFC but still reads a global `theApp` — verify with `grep -r "theApp\|afx\|CString\|COLORREF\|CRect" core/`.
- [ ] **Equipotentials:** appear, but are duplicated (Doc de-dup logic lost) — verify counts on Quadrupole and Parallel dipoles against the MFC app.
- [ ] **Equipotentials on GCC/Clang:** abs() truncation or the `job(job)` UB — verify the Dipole produces non-empty equipotentials on all three OSes.
- [ ] **Net-zero presets:** `sign(0)` altered — verify Dipole lines start only from the positive charge and end on the negative.
- [ ] **Cancellation:** window closes instantly in the UI but the process lingers — verify quit latency under 200 ms with a heavy preset and no leftover threads (TSan clean).
- [ ] **Rendering:** y-axis is flipped correctly but Bezier groups aren't used — verify smooth curves, not polylines, at 2x zoom.
- [ ] **HiDPI:** looks right at 100% only — verify at 200% on Windows and a Retina Mac.
- [ ] **Colours:** red and blue swapped — verify the positive charge is blue.
- [ ] **Options:** saved and loaded, but defaults differ from `Options::Options()` — verify a fresh profile equals the old defaults and `0.029` round-trips.
- [ ] **Export:** exports at screen size only — verify a 2x/4x export keeps lines smooth and charges correctly sized.
- [ ] **Packaging:** runs on the dev machine only — verify on a clean VM (Windows), clean container (Linux), and an arm64 Mac.
- [ ] **Git:** artifacts removed from the index but the ignore patterns still anchored wrongly — verify `git status` is clean after a full build.
- [ ] **CI:** green on Linux only — verify the matrix covers windows, ubuntu, macos (arm64) and runs the core tests on each.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| System C++ GUI chosen with Zig (Pitfall 10) | MEDIUM | Switch the build to CMake; keep the core build files portable; Zig only for tests |
| Equipotential logic lost with the Doc | MEDIUM | Re-extract from `git show master:ElectricField/ElectricFieldDoc.cpp`; add the Quadrupole test |
| Numerical drift discovered late | MEDIUM | Add `-ffp-contract=off`; switch tests to tolerance; re-baseline on a reference platform |
| Exit crashes from detached threads | MEDIUM | Introduce owned threads and the shutdown barrier; run TSan |
| Wrong `.gitignore` after commits of artifacts | LOW | `git rm --cached`, fix patterns; history stays |
| Zig API break on a version bump | LOW | Pin the version; migrate in one `build.zig` helper |
| Packaging failures (Qt) | LOW-MEDIUM | Fix `windeployqt`/`macdeployqt` invocation; test on clean machines |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 1 abs()/missing includes | P2, P3 | Build core with GCC+Clang `-Wall -Wextra`; equipotential test |
| 2 M_PI and sign() macro | P2, P3 | Unit test `sign(0)==1`; Linux build |
| 3 `job(job)` UB | P2, P3 | `-Wshadow -Wuninitialized` clean; ASan/MSan run |
| 4 Core entanglement, logic in Doc | P2 | Core target links with no GUI headers; de-dup test |
| 5 Shutdown and detached threads | P2, P3, P4 | TSan; cancel latency test |
| 6 UI threading, progressive mismatch | Requirements fix, P4 | Main-thread-only UI audit; updated PROJECT.md |
| 7 Numerical drift | P3, P6 | Tolerance tests on 3 OSes x arm64/x64; `-ffp-contract=off` |
| 8 Coordinates and HiDPI | P4, P5 | Screenshot tests at 1x/2x |
| 9 Bezier divide by zero | P2, P3 | Degenerate-input test |
| 10 libc++/libstdc++ | P1 | Spike: GUI hello + core link on 3 OSes |
| 11 Zig 0.17 API | P1 | `zig build` from pinned version in CI |
| 12 Git hygiene | P1 | Clean `git status` after build |
| 13 MSVC-isms | P2 | Clang+GCC+MSVC core builds |
| 14 Resources/manifest/subsystem | P2 (preset table), P4/P6 | Windows exe has icon, no console, correct DPI |
| 15 macOS traps | P1, P6 | arm64 runner build, ad-hoc signed |
| 16 Qt deployment | P6 | Clean-machine run |
| 17 Options persistence | P5 | Round-trip tests; defaults test |

## Sources

- Repository sources read: `RungeKutta.h/.inl`, `FieldLinesCalculator.h/.cpp`, `Vector2D.h/.cpp`, `FieldLine.h/.cpp`, `Charge.h/.cpp`, `TheElectricField.h/.cpp`, `ElectricFieldDoc.cpp`, `ElectricFieldView.cpp`, `ElectricField.cpp`, `Options.h/.cpp`, `ComputationThread.*`, `stdafx.h`, `ElectricField.vcxproj`, `.gitignore`, `git ls-files` (HIGH).
- Installed Zig 0.17.0 `lib/std/Build/Module.zig`, `Step/Compile.zig`, `std/zig.zig`, `init/build.zig` (HIGH, [ZIG-LOCAL]).
- Ziggit: "Neat way to use libstdc++ in zig" (2026-05-31; unanswered, reports `link_libcpp=false` header conflicts): https://ziggit.dev/t/neat-way-to-use-libstdc-in-zig/15816 (MEDIUM).
- Ziggit: "Zig C++: Undefined Symbols When Linking C++20 Shared Library" (libc++ vs libstdc++ symbol names): https://ziggit.dev/t/zig-c-undefined-symbols-when-linking-c-20-shared-library/12356 (MEDIUM).
- Ziggit: "Using Zig build system for a C++ project": https://ziggit.dev/t/using-zig-build-system-for-a-c-project/7156 (MEDIUM, older API).
- Zig `rc` package (resinator): https://codeberg.org/ziglang/rc and Zig commit adding `.manifest` support: https://git.jakstys.lt/motiejus/zig/src/commit/5f15acc463d39baedd8de367330286b91c8bafc8 (MEDIUM).
- LLVM discourse and cfe-dev threads on `-ffp-contract` defaults (GCC `fast`, clang `on`): https://discourse.llvm.org/t/question-about-fused-multiply-add-and-clang-gnu-modes/38249 and https://lists.llvm.org/pipermail/cfe-dev/2017-March/053109.html (MEDIUM; defaults not re-verified for current releases).
- Qt forum threads on macdeployqt/notarization (Qt 5.12-5.14 era): https://forum.qt.io/post/574938 (MEDIUM-LOW, old).
- macOS SDK/frameworks with Zig cross-compilation: https://macroquad.rs/articles/zigbuild-osx/ and cargo-zigbuild notes (MEDIUM-LOW, not Zig 0.17-specific).

## Gaps

- Zig 0.17 release notes were not found online; API statements come from the installed `lib/std`. Re-check `zig build --help`, the default `sanitize_c` mode for C++ in Debug, and whether `zig c++` auto-detects the macOS SDK on a real Mac.
- `FieldLineJob job(job)` behavior on MSVC (why it appears to work) was not reproduced; Phase P3 should build a minimal repro and compare outputs before and after the fix.
- Whether MSVC `/fp:precise` on the installed toolset contracts to FMA with `/arch:AVX2` was not checked; the vcxproj sets no `/arch`, so SSE2 (no FMA) is assumed.
- GUI-framework-specific HiDPI and thread rules (Qt, wx, SDL3+ImGui) are given in general terms; re-verify against the chosen framework's current docs in P4 research.
- Qt/wx licensing versus the repository LICENSE has not been checked.

---
*Pitfalls research for: C++ MFC to cross-platform GUI port (ElectricField)*
*Researched: 2026-10-10*
