# Architecture Research

**Domain:** Cross-platform desktop physics visualiser (2D electric field lines + equipotentials), brownfield port of an MFC/Direct2D MDI app
**Researched:** 2026-10-10
**Confidence:** HIGH for the as-is analysis and the core/threading design (every claim below was read directly from the source in `ElectricField/`). MEDIUM for the per-framework notes (framework choice is being researched in parallel; those rows are design guidance, not verified against framework docs).

---

## 1. As-Is Analysis (what the code actually does)

### 1.1 Coupling inventory in the "core"

Every place the physics code reaches into MFC/Win32/D2D/GDI or the `theApp` global. This is the exact work list for the decoupling phase.

| # | Location | Coupling | Decoupling action |
|---|----------|----------|-------------------|
| 1 | `Charge.h/.cpp` `Draw(CHwndRenderTarget*, CRect&)`, `Draw(CDC*, float)` | D2D (`CD2DSolidColorBrush`, `FillEllipse`), GDI (`CBrush`, `Ellipse`) | Delete. Charges become plain data; drawing moves to scene/painter layer. |
| 2 | `Charge.h` `HitCharge()` | reads `theApp.options.chargeRadius` and `distanceUnitLength` | Take hit radius as data (`ChargeSystem::hitRadius`, world units = `25/1.5/100 = 0.1667`). |
| 3 | `Charge.h` includes `ElectricField.h` | pulls in whole MFC app header (`CWinAppEx`, `<gdiplus.h>`, `d2d1.lib`) for `theApp` | Remove include. Core headers include only `<vector>`, `<cmath>`. |
| 4 | `FieldLine.h/.cpp` `Draw(CHwndRenderTarget*...)`, `Draw(CDC*...)`, `DrawBezierAndLine` | D2D path geometry, GDI `PolyBezier`, `CRect`, `CPoint` | Delete. Replace with `PathBuilder` (pure geometry) + `Painter::strokePath`. |
| 5 | `FieldLine.h` `AddPoint()` | reads `theApp.options.distanceUnitLength` (point decimation in *pixels*) | Move to a `LineRecorder` that is constructed with `pixelsPerUnit` from `ComputeParams`. |
| 6 | `FieldLine.cpp` `AdjustForBezier` | uses MFC `ASSERT`, `DBL_MIN` | Keep algorithm, move to scene layer, replace `ASSERT` with a guarded fallback (see Pitfalls). |
| 7 | `TheElectricField.h/.cpp` `Draw(...)` x2 | D2D + GDI, reads colours/thickness from `theApp.options` | Delete. Split class (see 1.2). |
| 8 | `Options.h` | `COLORREF` (Win32 `0x00BBGGRR`) for 4 colours | Own `Color{r,g,b,a}` in the scene/settings layer. |
| 9 | `Options.cpp` `Load/Save` | `theApp.GetProfileInt/Binary` (registry) | `SettingsStore` file in user config dir. |
| 10 | `FieldLinesCalculator.cpp` `CalcThread` ctor | copies 3 values from `theApp.options` | Receive `ComputeParams` by value. |
| 11 | `FieldLinesCalculator.cpp` `StartCalculating` | reads 6 values from `theApp.options` (chargeRadius, distanceUnitLength, numLinesOnUnitCharge, calculateEquipotentials, potentialInterval, calculationMethod, numThreads) | Same - `ComputeParams` argument. |
| 12 | `FieldLinesCalculator.cpp` `GetNumberOfElectricFieldLines` | reads `theApp.options.numLinesOnUnitCharge` (function is unused) | Delete or take params. |
| 13 | `ElectricFieldDoc.cpp` `GetDataFromThreads` (equipotential de-dup) | **reads `theApp.options.distanceUnitLength` and `chargeRadius`** (line ~210) - easy to miss, it is in the "document" not the "calculator" | Becomes pure `FinalizeEquipotentials(raw, charges, params)` in core. |
| 14 | `ElectricFieldDoc.cpp` `OnCloseDocument` + `ElectricField.cpp` `OnIdle`/`ExitInstance` + `theApp.calculators` | Calculator hand-off to the app because worker threads are *detached* and only check `Terminate` between jobs | Eliminated by cooperative cancellation + RAII join (see 3.2). |
| 15 | `ElectricFieldView.cpp` `OnTimer` (100 ms `SetTimer`), `BeginWaitCursor`, `OnSetCursor` | Win32 timer polling `doc->GetData()` | Framework timer/frame loop calling `Document::poll()`. |
| 16 | `ConfigDocs.cpp/.h` (9 classes) | `IMPLEMENT_DYNCREATE`, `CDocument::OnNewDocument`; each differs *only* by its charge list; each calls `calculator->StartCalculating` | One data table `PresetRegistry` of `{id, name, charges}`. |
| 17 | `ElectricField.cpp` 9x `CMultiDocTemplate` + `.rc` doc-type strings | menu entries = document templates | GUI builds menu from `PresetRegistry`. |
| 18 | `ElectricFieldView.cpp` `OnPrint/OnPrepareDC` | GDI printing, `GetSystemMetrics` | Replace by export (SVG/PNG/PDF) driven by the same `renderScene()`. |
| 19 | `Vector2D.cpp`, `RungeKutta.inl` | `#include "stdafx.h"` (pulls all of MFC); `#define new DEBUG_NEW`; `.cpp`/`.inl` are *textually included at the end of the header* | Remove `stdafx.h`, fold impl into headers (or rename `.inl`), never list them as compile units. |
| 20 | `RungeKutta.h` | `#define sign(x)` macro, unqualified `abs`, `DBL_MAX/DBL_MIN` without `<cfloat>` | `inline constexpr` function `sign()` **preserving `sign(0) == +1`**, `std::abs`, `<cfloat>`. |
| 21 | `FieldLinesCalculator.cpp` | `const double M_PI = ...` (collides with POSIX `M_PI` macro), includes `PrecisionTimer.h` (Win32 `LARGE_INTEGER`; **never used**) | `constexpr double kPi`; delete `PrecisionTimer` (or `std::chrono::steady_clock`). |

Already portable and untouched: `Vector2D<T>`, all of `RungeKutta`, `std::thread/mutex/atomic` use, the integration/tracing loops themselves.

### 1.2 The three roles `TheElectricField` plays today (split them)

`TheElectricField` simultaneously is (a) the immutable physics definition (`charges`, `E()`, `Potential()`, `HitCharge()`), (b) the result store (`electricFieldLines`, `potentialFieldLines`, filled by the UI thread via `swap` in `GetDataFromThreads`), and (c) the drawing entry point. `FieldLinesCalculator::field` is both the object worker threads read through a raw `const TheElectricField*` and the object the UI thread writes lines into. It works only because workers read `charges` while the UI writes the two line vectors after all workers are done. The target splits these into `ChargeSystem` (immutable), `FieldResult` (value), and `FieldScene` (render data).

### 1.3 Current threading and hand-off (what must be preserved vs. fixed)

```
UI thread                                   worker threads (detached, new CalcThread<Solver>)
---------                                   --------------------------------------------------
OnNewDocument -> StartCalculating()
   builds m_jobs (deque, mutex)  ------>    loop: lock m_jobsSection; pop job; ProcessJob()
   spawns numThreads threads                 E-line job: trace, may POST equipotential jobs
                                                (and RESTART a thread if finishedThreads > 0)
                                             push finished line under m_electricLinesSection /
                                                m_potentialLinesSection into calculator vectors
                                             on empty queue: `delete this; ++finishedThreads`
OnTimer every 100 ms:
   doc->GetData(): if !CheckStatus() return false   (finishedThreads < startedThreads)
   else swap lines into field, sort+dedupe
   equipotentials, AdjustForBezier on 2 std::threads, join
   -> Invalidate()
OnCloseDocument: if not finished: Terminate=true,
   hand calculator to theApp.calculators; OnIdle deletes it later; ExitInstance spins OnIdle.
```

Observations that drive the design:

1. **Display is all-or-nothing today.** The view only ever draws `calculator->field`, and lines are swapped into `field` only after *every* thread has finished (`GetData` returns false until `CheckStatus()`), with a wait cursor in between. `PROJECT.md` lists "progressive display while computing" as validated behaviour; the code does not do that. The mutexes around the result vectors exist but the UI never takes them. Parity = all-at-once; progressive display is a cheap, optional *new* feature that the new design makes possible.
2. **Cancellation is coarse.** `Terminate` is only tested between jobs. One job can run up to 5,000,000 steps (`has_different_signs && total_charge == 0`) or 1,500,000 equipotential steps. That latency is the sole reason for the detached-thread + calculator-hand-off machinery. Check the flag every ~4096 steps inside the loops and the hand-off disappears.
3. **Thread re-spawn hack.** A worker that posts an equipotential job restarts a finished thread (`--finishedThreads; StartComputingThread`) because workers exit on empty queue even when other workers still have jobs in flight that may post more. A standard "exit only when queue empty AND nothing in flight" condition-variable pool removes this.
4. **`GetData()` does O(total points) work on the UI thread** (sort, de-dup, Bezier fit with two extra threads + join). In the new design the UI thread only swaps a finished result in.
5. **Computation is independent of the viewport.** There is no pan/zoom; the view is a fixed 100 px/unit, y-up, origin at the window centre; resizing only reveals more of already-computed lines. This is a major simplification: compute once per document, render many times.
6. **Pixel constants are baked into the physics.** `distanceUnitLength=100` and `chargeRadius=25` (px) control: seed radius (`25/100 = 0.25` units), charge hit radius (`25/1.5/100`), the "needs precision" zone (`len*100 < 3000` px = 30 units), point decimation (`>= 8/64/256 px` spacing), and equipotential loop-closure (`< 6 px`). These are *design resolution* constants of the algorithm, not view settings. They must move into `ComputeParams` (fixed defaults, not user-editable - they are not in the Options dialog) so changing the view scale or exporting at another size cannot change the physics.
7. **`sign(0) == +1`** (macro `((x) >= 0 ? 1 : -1)`). For net-zero configurations (dipole, quadrupole) the *positive* charges are the line sources and negative charges the sinks because of this. Any replacement (`std::copysign`, `std::signbit`, `boost::sign`) breaks it silently.
8. **Empty lines are stored.** For sink charges, `ProcessJob` still pushes a `FieldLine` with zero points (the job exists only to spawn equipotentials). Harmless to the old `Draw` (early return) but should not enter the new result.
9. **Document options semantics.** Compute options (method, line count, threads, equipotentials, interval) are effectively captured when a document is created and only affect *new* documents. Draw options (colours, thickness) apply live to all open documents (`UpdateAllViews`). Keep exactly this split.

---

## 2. Standard Architecture (target)

### 2.1 System Overview

```
+---------------------------------------------------------------------------------+
|  GUI SHELL  (framework-specific; thin; the ONLY place with Qt/ImGui/wx types)    |
|  +------------+ +--------------+ +----------------+ +---------+ +--------------+ |
|  | MainWindow | | DocumentView | | OptionsDialog  | | Painter | | Export UI    | |
|  | + preset   | | (1 per open  | | (edits Settings| | backend | | (file dlg ->  | |
|  |   menu     | |  Document)   | |  struct)       | | (draws) | |  SVG/PNG/PDF)| |
|  +-----+------+ +------+-------+ +-------+--------+ +----+----+ +------+-------+ |
|        | owns          | poll() ~10 Hz  | apply          | calls        |         |
+--------+---------------+----------------+----------------+--------------+---------+
         v               v                v                v              v
+---------------------------------------------------------------------------------+
|  EFAPP  (GUI-agnostic, no framework types; UI-thread-affine objects)              |
|  +---------------+  +----------------+  +--------------+  +-------------------+  |
|  | Document      |  | Settings       |  | SettingsStore|  | renderScene()     |  |
|  | preset+params |  | ComputeParams  |  | (config file |  | + Painter iface   |  |
|  | +Session+Scene|  | + DrawStyle    |  |  in user dir)|  | + SvgPainter      |  |
|  +-------+-------+  +--------+-------+  +--------------+  +---------+---------+  |
|          |                   |                                      |            |
|  +-------+-------+   +-------+------+                       +-------+--------+   |
|  | SceneBuilder  |   | Color,       |                       | PathBuilder    |   |
|  | (FieldResult  |   | DrawStyle    |                       | (Bezier fit,   |   |
|  |  -> FieldScene)|  +--------------+                       |  clip, y-flip) |   |
|  +-------+-------+                                          +----------------+   |
+----------+----------------------------------------------------------------------+
           | reads FieldResult / starts ComputeSession
           v
+---------------------------------------------------------------------------------+
|  EFCORE  (pure C++17, <vector> <thread> <mutex> <cmath> only; no I/O, no GUI)     |
|  +-----------+ +------------+ +-------------+ +----------------+ +-------------+  |
|  | Vec2      | | RungeKutta | | ChargeSystem| | ComputeSession | | Finalizer   |  |
|  | (Vector2D)| | 11 methods | | E,V,HitCh.  | | JobQueue +     | | sort+dedupe |  |
|  +-----------+ +------------+ +-------------+ | worker pool    | | equipotent. |  |
|  +--------------+ +-----------+ +------------+ | cancel, drain  | +-------------+  |
|  | ComputeParams| | SeedPlanner| |LineTracer | +----------------+ +-------------+  |
|  | (value type) | | (jobs)     | |<Solver>   |                    | PresetReg.  |  |
|  +--------------+ +-----------+ +------------+                    | (9 configs) |  |
|                                                                   +-------------+  |
+---------------------------------------------------------------------------------+
```

Dependency direction is strictly downward. `efcore` never includes anything from `efapp`; `efapp` never includes a GUI header. The GUI shell can be swapped without touching either.

### 2.2 Component Boundaries

| Component | Layer | Responsibility | Talks to | Must NOT know about |
|-----------|-------|----------------|----------|---------------------|
| `Vec2` (was `Vector2D<T>`) | core | 2D vector math | everything in core | - |
| `RungeKutta::*` (11 integrators) | core | one-step ODE solvers, stateful per instance (`K[]` scratch) | `LineTracer` | threads, fields |
| `ChargeSystem` | core | immutable list of `Charge{int q; Vec2 pos}` + `E()`, `ENormalized()`, `Potential()`, `HitCharge()`, `totalCharge()`; carries `hitRadius` | `LineTracer`, `Finalizer` | lines, options, drawing |
| `ComputeParams` | core | value type: `numLinesOnUnitCharge, calculateEquipotentials, potentialInterval, numThreads, method` + internal constants `pixelsPerUnit=100, chargeRadiusWorld=0.25` | `SeedPlanner`, `ComputeSession` | where values came from (file/dialog) |
| `FieldLine` / `PotentialLine` | core | **raw** polyline in world coordinates (`vector<Vec2>`); potential line adds `potential`, `weightCenter` | everything | Bezier, pixels |
| `LineRecorder` | core | decimating `AddPoint` (needs `pixelsPerUnit`), used by tracer | `LineTracer` | - |
| `SeedPlanner` | core | pure function `plan(ChargeSystem, ComputeParams) -> vector<Job>` (angles, start points, sign logic) | `ComputeSession` | threads |
| `LineTracer<Solver>` | core | integrate one job: `traceElectricLine`, `traceEquipotential`, `maybeSpawnEquipotential`; takes `const std::atomic<bool>& cancel` | `RungeKutta`, `ChargeSystem` | queue internals (spawns via callback) |
| `ComputeSession` | core | RAII owner of job queue + worker pool + result sink; `poll()` drains finished lines + progress; destructor cancels and joins | `LineTracer`, `Finalizer` | GUI, timers, `Document` |
| `Finalizer` | core | pure `finalize(raw lines, ChargeSystem, params) -> FieldResult` (sort by potential, de-dup duplicate equipotentials, deterministic ordering) | `ComputeSession` | threads |
| `PresetRegistry` | core | `static const` table of the 9 configs (`id`, display name, `vector<Charge>`) | `Document`, GUI menu, tests | MFC doc templates |
| `Color`, `DrawStyle` | efapp | `Color{u8 r,g,b,a}`; 4 colours + 2 thicknesses + chargeRadius (px) | `Settings`, `renderScene` | `COLORREF`, `QColor`, `ImU32` |
| `PathBuilder` | efapp | `AdjustForBezier` fit, polyline -> cubic path commands, run-splitting at clip gaps | `SceneBuilder`, `renderScene` | any renderer |
| `FieldScene` | efapp | immutable render data in world coords: E-paths, potential-paths, charge marks (+ per-path bbox) | `renderScene` | threads, Options |
| `Painter` (interface) + `renderScene()` | efapp | ~4 virtual calls (`clear`, `strokePath`, `fillCircle`, optional `setTransform`); `renderScene` owns draw order, y-flip, clipping | GUI backends, `SvgPainter` | which toolkit is drawing |
| `SvgPainter` | efapp | text SVG backend; export feature **and** headless test oracle | `renderScene` | - |
| `Settings` / `SettingsStore` | efapp | `{ComputeParams user part, DrawStyle}`; load/save INI-or-JSON in platform config dir; clamp on load | `Document`, OptionsDialog | registry |
| `Document` | efapp | one open simulation: preset id, `ChargeSystem`, frozen `ComputeParams`, `ComputeSession`, latest `FieldScene`, status; `poll()` returns `{changed, finished}` | GUI view | any widget |
| GUI shell (`MainWindow`, `DocumentView`, `OptionsDialog`, painter backend, timer) | gui | windowing, menu, dialogs, file pickers, event loop, painter backend implementation | `Document`, `Settings`, `renderScene` | `ComputeSession` internals |

### 2.3 Recommended Project Structure

```
ElectricField/                      (repo root; keep the sln only on master)
|-- CMakeLists.txt  or  build.zig   (decided by STACK research; sources below are build-system neutral)
|-- core/                           -> target `efcore` (static lib, C++17, no deps)
|   |-- include/ef/
|   |   |-- Vec2.h                  (Vector2D + impl folded in; no .cpp/.inl include trick)
|   |   |-- RungeKutta.h            (+ RungeKutta.inl renamed .ipp or folded; never a compile unit)
|   |   |-- Charge.h  ChargeSystem.h
|   |   |-- Line.h                  (FieldLine, PotentialLine: data only)
|   |   |-- ComputeParams.h  Method.h  (enum CalculationMethod, values frozen for settings compat)
|   |   |-- SeedPlanner.h  LineTracer.h  ComputeSession.h  Finalizer.h
|   |   `-- PresetRegistry.h
|   `-- src/ (ComputeSession.cpp, Finalizer.cpp, SeedPlanner.cpp, PresetRegistry.cpp, dispatch.cpp)
|-- app/                            -> target `efapp` (static lib, depends on efcore only)
|   |-- Color.h  DrawStyle.h  Settings.h  SettingsStore.cpp
|   |-- PathBuilder.cpp  FieldScene.h  SceneBuilder.cpp
|   |-- Painter.h  RenderScene.cpp  SvgPainter.cpp
|   `-- Document.cpp
|-- cli/                            -> target `ef-cli` (headless: preset -> SVG; CI parity oracle)
|-- gui/                            -> target `ef-gui` (framework-specific; the only dir with GUI includes)
|-- tests/
|   |-- core/   (RK order, field values, seed counts, session cancel/determinism, finalizer)
|   `-- app/    (PathBuilder, renderScene via SvgPainter, settings round-trip)
`-- .planning/
```

### Structure Rationale

- **`core/` vs `app/` split:** `core` = the physics the user said must be GUI-independent and testable; `app` = everything that is neither physics nor toolkit-specific (scene geometry, settings, document lifecycle, SVG). Keeping `app` toolkit-free means the CLI, tests and *any* of Qt/ImGui/wx share it, and the framework decision cannot leak backwards.
- **Two libs, not five:** the codebase is ~4.3k lines. Finer-grained libs add build-system noise (especially for `zig build`) with no payoff. Directories express the layering; targets enforce it (`efcore` links nothing; `efapp` links `efcore`; `ef-gui` links both + the toolkit).
- **`cli/` early:** a headless `ef-cli --preset dipole --out dipole.svg` is the cheapest possible end-to-end proof that the port is correct on 3 OSes before any GUI exists, and it is what CI runs.
- **Public headers under `include/ef/`:** forces a stable surface and namespace (`ef::`); prevents the old "include `ElectricField.h` to get `theApp`" pattern.

---

## 3. Architectural Patterns

### Pattern 1: Immutable parameter injection (kills `theApp.options`)

**What:** All 20+ reads of `theApp.options` inside compute code become fields of two by-value structs handed to the session at construction. Compute code has no global access.
**When to use:** always for core; the GUI copies `Settings.compute` into a new `Document` at creation (matches today's "options affect new documents").
**Trade-offs:** + removes the data race between the Options dialog (UI thread writes) and `Charge::HitCharge`/`FieldLine::AddPoint` (worker threads read `theApp.options` live); + tests need no app object. - A little more plumbing through `LineTracer`.

```cpp
// core/include/ef/ComputeParams.h
namespace ef {
enum class Method { Euler, Midpoint, Ralston, Heun, RK4, RK3per8,
                    AdaptiveHeunEuler, AdaptiveBogackiShampine, AdaptiveCashKarp,
                    AdaptiveFehlberg, AdaptiveDormandPrince };     // same order as old enum (settings compat)

struct ComputeParams {
    // user-editable (Options dialog, "Computation" page)
    unsigned numLinesOnUnitCharge = 16;      // 4..128
    bool     calculateEquipotentials = true;
    double   potentialInterval = 0.1;        // 0.01..0.5 (slider 10..500 / 1000)
    unsigned numThreads = 4;                 // 1..128
    Method   method = Method::Euler;
    // algorithm design-resolution constants (NOT user-editable, NOT view settings)
    unsigned pixelsPerUnit = 100;            // was distanceUnitLength
    double   chargeRadiusWorld = 25.0 / 100; // was chargeRadius/distanceUnitLength
};
}
```

### Pattern 2: RAII compute session with cooperative cancellation (kills detach + hand-off)

**What:** `ComputeSession` owns the job queue, the workers and the result sink. Workers poll `cancel` every N steps inside the integration loops. The destructor sets `cancel`, notifies, and `join()`s.
**When to use:** the only way to run computation; one session per `Document`.
**Trade-offs:** + closing a document or quitting the app is just `delete doc` - no `theApp.calculators`, no `OnIdle`, no spin-wait in `ExitInstance`; + no `delete this` / raw `new CalcThread` ownership puzzle. - Requires touching the three inner loops (one `if ((i & 0xFFF) == 0 && cancel.load(relaxed)) return;`). Worst-case destructor latency becomes ~4096 steps (sub-millisecond to a few ms) instead of up to millions of steps.
**Use `std::thread` + `std::atomic<bool>` (C++17), not `std::jthread`/`std::stop_token`:** C++20 `jthread` availability differs across Apple libc++ versions and Zig's bundled libc++ (LOW confidence, unverified here); the project constraint is "C++17 or newer", so avoid depending on it.

```cpp
class ComputeSession {
public:
    ComputeSession(ChargeSystem field, ComputeParams params);   // plans seeds, starts workers
    ~ComputeSession();                                          // cancel + join; never blocks long
    struct Poll { std::vector<FieldLine> newELines; std::vector<PotentialLine> newPLines;
                  size_t jobsDone, jobsKnown; bool finished, failed; std::string error; };
    Poll poll();                       // UI thread; swaps pending vectors out under the mutex
    std::optional<FieldResult> takeFinal();   // set once, after finished (finalize already applied)
private:
    struct Shared;                     // queue, cv, atomics, sinks
    std::unique_ptr<Shared> s_;
    std::vector<std::thread> workers_;
};
```

### Pattern 3: Condition-variable worker pool with "in-flight" termination

**What:** Workers wait on a `condition_variable` for `!queue.empty() || cancel || (inFlight == 0 && queue.empty())`. A worker increments `inFlight` when it pops a job and decrements after pushing the result *and any spawned equipotential jobs*. Finished = `queue.empty() && inFlight == 0`. Fixed `numThreads` workers, started once.
**When to use:** the E-line job spawns equipotential jobs dynamically, so total work is not known up front.
**Trade-offs:** + removes the `finishedThreads` / `startedThreads` / `--finishedThreads; StartComputingThread()` restart hack and its subtle ordering assumptions; + gives a correct `finished` flag (replacement for `CheckStatus`). - Progress is "done / known-so-far" (indeterminate bar or spinner); the old UI used only a wait cursor, so no regression.

### Pattern 4: Drain-and-swap handoff to the UI thread (no shared mutable state in drawing)

**What:** Workers append finished lines to a mutex-guarded *pending* vector. The UI thread calls `poll()`, which `swap`s the pending vector with an empty one (O(1) under the lock) and returns it by value. The UI/`Document` merges it into UI-thread-private state. Drawing never touches anything a worker can write.
**When to use:** always; this is the "thread-safe snapshot of computed lines for the UI".
**Trade-offs:** + zero locking in the paint path; + works identically with timers (Qt/wx), frame loops (ImGui) or custom events (SDL); + enables optional progressive display for free. - Final equipotential de-dup needs the *complete* set, so while running the Document either (a) shows nothing (strict parity with today) or (b) shows E-lines immediately and equipotentials raw (possible overlapping duplicates, replaced atomically by the finalized set). Recommendation: ship (a) first, add (b) as an enhancement. Do **not** use `std::atomic<std::shared_ptr>` for publication (C++20, uneven library support); a plain mutex or the drain pattern is enough.

### Pattern 5: Display-list scene + a minimal `Painter` (render abstraction)

**What:** Not a rich "render abstraction" that mirrors D2D, but a **data-first scene** (`FieldScene`: world-coordinate cubic paths + charge circles + bboxes) and a **4-call `Painter`** that backends implement. `renderScene(Painter&, scene, style, view)` is the single shared implementation of draw order (E-lines, then potential lines, then charges), the y-up/centre-origin transform, 100 px/unit scale, clipping margin, closed-vs-open figures, and pen widths.

```cpp
struct View { double pixelsPerUnit = 100; double widthPx, heightPx; double originX, originY; };
struct Painter {                       // implemented per backend (~30-60 lines each)
    virtual ~Painter() = default;
    virtual void clear(Color) = 0;
    virtual void strokePath(const PathCmd* cmds, size_t n, bool closed, Color, float widthPx) = 0;
    virtual void fillCircle(float cx, float cy, float rPx, Color) = 0;   // device coords
};
void renderScene(Painter&, const FieldScene&, const DrawStyle&, const View&);
```

**When to use:** all rendering - window paint, SVG export, PNG export, print/PDF, tests.
**Trade-offs:** + the draw logic exists once, is unit-testable with `SvgPainter` (no GUI, no GPU) and is identical on every platform and toolkit; + export/print stops being a second code path (the old code had two: D2D and GDI, with different clipping and Bezier-grouping logic); + the "correct and recognisable" core value is testable. - Backends do not get to exploit toolkit-native retained geometry (e.g. `QPainterPath` caching); if paint ever becomes a bottleneck, the backend can cache converted geometry keyed by `FieldScene` pointer (scenes are immutable, so the key is stable).

### Pattern 6: Pure-function post-processing (Finalizer + PathBuilder)

**What:** The two non-trivial transformations that today live in `CElectricFieldDoc::GetDataFromThreads` (sort by potential, weight-centre de-dup against reference point `(-7, 5.5)`, two-charge/opposite-sign shortcut) and `FieldLine::AdjustForBezier` become pure functions with no state: `finalize(raw, charges, params) -> FieldResult`, `buildPath(polyline) -> PathCmds`.
**When to use:** always; they run on a worker (or the finalizing thread), never on the UI thread.
**Trade-offs:** + unit-testable with hand-made inputs (this is where behaviour-parity bugs will hide); + `AdjustForBezier` mutates `points` in place today so the same `vector<Vec2>` means "polyline" before and "Bezier control points" after - separating `FieldLine` (raw) from `Path` (control points) removes that overloading; + deterministic: results carry a `jobSeq` and are sorted by it before finalizing so output is independent of thread count/scheduling. - Must keep the odd-but-working de-dup heuristic bit-for-bit in behaviour; port, do not "improve", then pin with tests.

### Pattern 7: Data-driven preset registry (replaces 9 document classes + 9 templates)

**What:** One `const std::array<Preset, 9>` in core. The GUI builds its "New" menu from it; `Document(presetId, settings)` builds `ChargeSystem` from it.
**When to use:** always.
**Trade-offs:** + deleting `ConfigDocs.cpp/.h`, 9 `IMPLEMENT_DYNCREATE`, 9 `CMultiDocTemplate` and the `.rc` document-type strings; + adding a preset (or a future user-defined/serialized preset, currently out of scope) is data, not a class; + tests can sweep all 9 presets. - Display names move from `.rc` string tables into C++ (English only, as today).

```cpp
struct Preset { std::string_view id, name; std::vector<Charge> charges; };   // or fixed arrays
const std::vector<Preset>& presets();     // Dipole: (+1 @ -2,0), (-1 @ 2,0) ... (copy exactly from ConfigDocs.cpp)
```

---

## 4. Data Flow

### 4.1 Compute flow (open a configuration)

```
User picks "Dipole" in menu
    |
GUI: Document(preset "dipole", Settings.compute  [copied by value])
    |
Document: ChargeSystem cs = fromPreset(...); ComputeSession sess(cs, params)
    |
ComputeSession ctor:  SeedPlanner::plan(cs, params) -> jobs --> Shared.queue
                      spawn params.numThreads workers
    |
worker i:  wait cv -> pop Job -> inFlight++
           dispatch(params.method, [&](auto& solver){ LineTracer<decltype(solver)> t(cs, params, cancel);
                                                    t.run(job, spawnFn, resultFn); })
             |-- E-line:   integrate (ENormalized, sign-flipped for sinks), LineRecorder decimates points,
             |             stop on HitCharge / step limit / cancel;
             |             first line of each charge posts equipotential jobs at each potentialInterval crossing
             `-- Equipotential: integrate perpendicular-to-E, accumulate weightCenter, close loop when
                               dist < 6 px-equivalent and t > 2*dist
           push {jobSeq, line} to Shared.pending (mutex)   ; spawned jobs -> queue (mutex) ; inFlight--
           cv.notify_all
    |
last worker (queue empty && inFlight==0 && !cancel):  Finalizer: sort by jobSeq, sort by potential,
           de-dup equipotentials  ->  FieldResult;  SceneBuilder: PathBuilder per line -> FieldScene
           publish under mutex; finished = true
    |
UI thread (timer / frame):  Document::poll() -> sess.poll()/takeFinal()
           if final result: doc.scene = shared_ptr<const FieldScene>; mark dirty; stop timer; end wait cursor
    |
GUI paint:  renderScene(backendPainter, *doc.scene, settings.draw, View{window size})
```

### 4.2 Settings / options flow

```
App start:   SettingsStore::load()  -> Settings{compute, draw}  (missing/invalid keys -> defaults, clamp ranges)
OptionsDialog: edits a COPY of Settings -> OK:
                 settings = copy;  SettingsStore::save(settings)
                 draw part   -> every open DocumentView repaints now         (== UpdateAllViews today)
                 compute part-> used by the NEXT Document created           (== today's behaviour)
                              (optional later: "Recompute" button = new ComputeSession with new params)
```

### 4.3 Export flow (replaces print/print-preview)

```
File > Export...  -> file dialog -> format by extension
   .svg : SvgPainter + renderScene(View{w,h,pixelsPerUnit chosen by user})  -> write text
   .png : toolkit offscreen surface + backend Painter + renderScene          -> save image
   .pdf : toolkit PDF/print surface + backend Painter + renderScene          (Qt/wx only; see 6)
Same renderScene, same scene, different Painter and View; no second geometry path.
```

### 4.4 Ownership and thread-affinity rules (state management)

| Object | Created by | Mutated by | Read by | Notes |
|--------|-----------|-----------|---------|-------|
| `ChargeSystem`, `ComputeParams` | Document ctor | nobody (immutable) | workers + UI | copied into session; no sharing hazards |
| `Shared` (queue, pending, inFlight, cancel) | session | workers + UI `poll()` | both | mutex + atomics only here |
| `FieldResult` | worker (finalize) | nobody | UI | published once via `takeFinal()` |
| `FieldScene` | worker (SceneBuilder) | nobody (immutable) | UI paint; exporters | held as `shared_ptr<const>`; safe to hand to an export thread |
| `Settings` | UI | UI only | UI; copied to Document | never read by workers |
| `Document`, `DocumentView` | UI | UI only | UI only | UI-thread-affine; no locking |

---

## 5. Build / Migration Order

Dependencies drive the order. Everything through Phase 6 is **GUI-framework-independent and can start before the framework decision lands**; only Phase 7+ depends on it. The build-system decision (zig build vs CMake) only affects Phase 1 scaffolding, because all sources are plain C++ with no generated files.

| Phase | Deliver | Depends on | Why this order / what it proves |
|-------|---------|-----------|----------------------------------|
| 0 | Repo hygiene: untrack `ElectricField/x64/`, `.vs/`; fix `.gitignore` | - | `git status` noise today hides real diffs; independent of everything else |
| 1 | **Portable numerics lib `efcore` (part 1):** `Vec2`, `RungeKutta`; remove `stdafx.h`, `sign` macro, `M_PI`, `abs`->`std::abs`, `.cpp/.inl` include tricks; builds with clang/gcc/MSVC/zig; first unit tests (each method's convergence order on `y'=y` / circular motion; `Vec2` ops) | 0 | Smallest, lowest-risk slice; surfaces all compiler-portability issues (Pitfalls 1-4) before any design work rides on them |
| 2 | **Physics model:** `ChargeSystem` (E, V, HitCharge with injected radius), `ComputeParams`, plain-data `FieldLine/PotentialLine`, `LineRecorder`, `SeedPlanner`; `PresetRegistry` (9 configs) | 1 | Removes coupling items 1,2,3,5,7,10,11,16. Tests: dipole analytic `E` and `V`; seed counts = `numLines*|q|` for line sources, 1 for sinks; `sign(0)=+1` convention pinned |
| 3 | **Compute engine:** `LineTracer<Solver>` + `ComputeSession` (CV pool, cooperative cancel, drain `poll`, `Finalizer`) | 2 | The riskiest refactor (threading). Tests: determinism across 1 vs N threads (same set of lines after sort by `jobSeq`), cancel-latency bound (destroy mid-run in < ~50 ms), dipole lines end within hit radius of the opposite charge, equipotentials close, finalizer idempotence, TSan run on Linux CI |
| 4 | **Settings:** split `Options` into `ComputeParams` + `DrawStyle`; `Color`; `SettingsStore` (user config dir; clamp/validate; unknown keys ignored); default-parity test against old `Options::Options()` | 2 | Needed by Document and GUI; GUI-free; tests round-trip. Optional one-time registry import on Windows is a nice-to-have, not required |
| 5 | **Scene + painter (efapp):** `PathBuilder` (port `AdjustForBezier`, fix clip-run alignment), `SceneBuilder`, `FieldScene`, `Painter`, `renderScene`, `SvgPainter`; `Document` | 3, 4 | Everything the GUI will need except pixels. Tests via SVG: path counts, bbox, closed figures, y-flip, colours/widths from `DrawStyle` |
| 6 | **`ef-cli` headless exporter + CI on 3 OSes** (preset -> SVG, runs all 9 presets) | 5 | End-to-end vertical slice without a GUI; the parity oracle (compare against screenshots of the MFC build for the 9 presets, taken once on Windows by hand); unblocks CI before the GUI choice |
| 7 | **GUI shell, minimal:** one window, preset menu -> `Document` -> painter backend -> paint; timer/frame poll; wait indicator | 5 + framework decision | Proves the render backend and thread marshalling in the chosen toolkit |
| 8 | **Multi-document UX:** multiple open configurations (MDI/tabs/sub-windows/dock windows per toolkit), close = `delete Document` | 7 | Replaces `CMultiDocTemplate/CChildFrame`; verifies the "close mid-computation" path with the RAII session |
| 9 | **Options dialog:** two pages bound to `Settings`; draw changes repaint all docs; compute changes affect new docs | 4, 7 | Replaces `OptionsPropertySheet` + two property pages |
| 10 | **Export in GUI** (SVG/PNG/PDF per toolkit capability); remove print code paths | 5, 7 | Replaces GDI print/preview |
| 11 | Docs/CI polish, README, remove MFC project | all | - |

**Optional enhancements that become cheap after Phase 5 (do not do before parity):** progressive display (draw E-lines as they arrive), "Recompute", per-path bbox culling, adjustable zoom/pan (the compute is already viewport independent; only the equipotential de-dup constants and decimation spacing assume ~100 px/unit, so zooming in far will show polyline facets from the 8 px decimation).

---

## 6. Where the GUI framework choice changes this design

The core, `efapp`, CLI and tests are identical for all three. These rows are what the GUI shell has to do differently.

| Concern | Qt 6 | Dear ImGui (+ SDL3/GLFW) | wxWidgets |
|---------|------|--------------------------|-----------|
| Painter backend | `QPainter` on a `QWidget::paintEvent` with antialiasing; `QPainterPath::cubicTo` maps 1:1 to `PathCmd` | `ImDrawList` (`AddBezierCubic`/`AddPolyline`, `AddCircleFilled`); tessellation happens every frame, so cache a flattened screen-space polyline per path keyed by (scene pointer, view size) and redraw only when dirty | `wxGraphicsContext` (`wxGraphicsPath::AddCurveToPoint`); D2D on Windows, Cairo on GTK, CoreGraphics on macOS |
| Wake / marshalling | Timer (`QTimer` ~100 ms) calling `doc.poll()`, or push: core's optional wake callback -> `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. **Keep core on `std::thread`** (do not introduce `QThread`/`QtConcurrent` into core) | No event queue to marshal into: just call `poll()` once per frame while `!finished`. If using an event-wait loop, post an empty event on completion to wake it (`glfwPostEmptyEvent` / `SDL_PushEvent`) | `wxTimer` or worker -> `wxQueueEvent`/`CallAfter` |
| Multiple configs | `QMdiArea` (closest to current MDI) or tabs | one ImGui window per `Document` (docking branch optional) | `wxMDIParentFrame`/`wxAuiNotebook`. Do **not** adopt `wxDocument/wxView`: it re-creates the MFC doc/view coupling this port removes |
| Options dialog | Dialog with `QSpinBox`, `QSlider`, `QRadioButton`, `QColorDialog`; edits a `Settings` copy | Modal popup with `InputInt`, `SliderFloat`, `ColorEdit4`; edit a `Settings` copy, commit on OK | `wxPropertySheetDialog`/`wxNotebook` with `wxSpinCtrl`, `wxColourPickerCtrl` |
| Settings location | Use own `SettingsStore` with `QStandardPaths::AppConfigLocation` for the path (do not let `QSettings` become the only implementation: the CLI/tests must read the same file without Qt) | Own `SettingsStore`; `SDL_GetPrefPath` gives the dir when using SDL; otherwise small per-OS helper (APPDATA / XDG_CONFIG_HOME / Application Support) | Own `SettingsStore`; `wxStandardPaths::GetUserConfigDir` for the path |
| Colour type | adapter `Color <-> QColor` in `gui/` only | `Color <-> ImU32/ImVec4` | `Color <-> wxColour` |
| Export | SVG via own `SvgPainter` (or `QSvgGenerator`); PNG via `QImage`+`QPainter`; PDF/print via `QPdfWriter`/`QPrinter` - same `Painter` backend reused. Best fit for "replace printing" | SVG via `SvgPainter` (easy). PNG needs an offscreen render + pixel readback or a software rasteriser - extra work; printing/PDF not available. Likely scope: SVG (+ PNG if cheap) | SVG via `SvgPainter`; PNG via `wxBitmap`+`wxMemoryDC`+`wxGraphicsContext`; printing via `wxPrintout` is possible but only worth it if required |
| Build-system fit | Qt's moc/rcc/uic steps are natural in CMake, awkward in `zig build` (flag for STACK research) | Small C++ sources, trivial in both CMake and `zig build` | Prebuilt libs or build-from-source; heavier in `zig build` |
| Wait-cursor equivalent | `QApplication::setOverrideCursor(Qt::WaitCursor)` per view, or status-bar spinner | Spinner text in the document window | `wxBusyCursor` / status bar |

**Opinion:** regardless of toolkit, keep three things out of the toolkit's hands: the settings file format, the compute threading, and the drawing algorithm (`renderScene`). They are what the "behaviour parity" and "CLI/CI on three OSes" requirements depend on.

---

## 7. Scaling Considerations

Not users - this is a single-user desktop tool - but compute and geometry size. Numbers are from reading the loops (HIGH for the step caps; per-line point counts are estimates, MEDIUM).

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Today's presets (2-4 charges, 16 lines/unit charge, 4 threads) | Everything above is overkill-proof: per-line cost ~ steps x stages x charges; neutral configs cap at 5M steps/line. Drain-and-swap handoff is trivially cheap. |
| User-defined configs (20+ charges, 128 lines/charge, 128 threads) - **out of scope now, but the registry/ChargeSystem make it a data change** | Cap `numThreads` at `std::thread::hardware_concurrency()`-aware default (the dialog allows 128); the `E()` loop is O(charges) per RK stage, so per-line cost grows linearly; consider a spatial cutoff only if needed. The CV pool and drain pattern already scale. |
| Large geometry (a line that runs to infinity keeps points out to 3000 units, ~tens of thousands of points, most off-screen) | Precompute per-path bbox + per-run culling in `PathBuilder`; render only runs intersecting the viewport. The old code re-tested every point against the clip rect every paint. |

### Scaling Priorities

1. **First bottleneck:** cancellation latency and long single jobs (up to 5M steps) - solved by in-loop cancel checks, not by more threads. Consider dynamic job ordering (long neutral-config lines first) only if profiling shows a long tail.
2. **Second bottleneck:** paint cost of far-off-screen points on each repaint - solved by bbox/run culling and, for immediate-mode toolkits, by caching flattened geometry.

---

## 8. Anti-Patterns

### Anti-Pattern 1: A drawing interface that mirrors the old D2D calls

**What people do:** define `IRenderer` with brush/geometry-sink/figure-begin/figure-end calls so the old `Draw(...)` bodies can be transliterated.
**Why it's wrong:** drags Direct2D's retained-geometry model into every backend; ImGui/Qt/wx have different primitives; keeps geometry decisions (Bezier grouping, clipping) inside backends so they diverge across platforms.
**Do this instead:** scene-as-data + 4-call `Painter`; geometry decisions in `PathBuilder`/`renderScene` once.

### Anti-Pattern 2: Keeping a process-wide `Options` singleton "just for now"

**What people do:** move `Options` into core and keep `extern Options g_options` so the diff is small.
**Why it's wrong:** workers read it concurrently with the UI writing it (existing data race); untestable; blocks the CLI and tests.
**Do this instead:** by-value `ComputeParams`/`DrawStyle`; `Settings` lives only in the UI/app layer.

### Anti-Pattern 3: Porting detached threads + `delete this` + hand-off as-is

**What people do:** wrap `CalcThread` and `theApp.calculators` into a generic "app" object.
**Why it's wrong:** it exists only to compensate for coarse cancellation; it leaks threads at exit and needs an `OnIdle` equivalent that no toolkit gives the same way.
**Do this instead:** in-loop cancel checks + RAII join (Pattern 2).

### Anti-Pattern 4: Using the toolkit's thread/queue/signal types inside core

**What people do:** `QThread`, `QMutex`, `QtConcurrent::run`, `wxThread`, `wxCriticalSection` in the calculator because "we already have Qt".
**Why it's wrong:** core can no longer build for the CLI/tests/other toolkits; the existing code is already standard C++ and that is an asset.
**Do this instead:** `std::thread/mutex/condition_variable/atomic` in core; the shell adapts events at the boundary.

### Anti-Pattern 5: Computing from the view (resolution-dependent physics)

**What people do:** let the viewport/zoom feed `distanceUnitLength`, so exports at a different size recompute with different decimation/hit radii.
**Why it's wrong:** line geometry and equipotential closure would differ between screen, PNG and SVG.
**Do this instead:** `pixelsPerUnit=100` and `chargeRadiusWorld=0.25` are `ComputeParams` constants; the view scale only multiplies at render time.

### Anti-Pattern 6: One `Document` subclass per configuration

**What people do:** keep a class hierarchy because MFC needed one doc class per template.
**Why it's wrong:** the classes differ only by data (see `ConfigDocs.cpp`).
**Do this instead:** `PresetRegistry` table.

### Anti-Pattern 7: Bit-exact golden files for line geometry

**What people do:** snapshot the polyline points and diff against a stored file across OSes.
**Why it's wrong:** adaptive step control and discrete decisions (loop-closure at `< 6 px`, `t > 2*dist`) make results sensitive to FMA contraction / libm differences between MSVC, GCC, Apple clang and Zig; tests will flake per platform.
**Do this instead:** assert invariants and tolerances (endpoints land on the opposite charge, closed equipotentials, line count, bbox within epsilon, symmetry of symmetric presets). Consider `-ffp-contract=off` for `efcore` to reduce (not eliminate) cross-compiler drift (MEDIUM confidence).

---

## 9. Integration Points

### External Services

None. The app is offline, single-process. (The only "external" resources are the platform config directory and user-chosen export paths.)

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| GUI shell <-> `Document` | Direct calls on the UI thread; `poll()` returns by value | Document is not thread-safe by design; never call it from workers |
| `Document` <-> `ComputeSession` | Construct with values; `poll()`/`takeFinal()`; destroy to cancel | Optional `std::function<void()> onWake` invoked from a worker (must be cheap and thread-safe); GUI uses it to post an event; polling alone is acceptable |
| `ComputeSession` <-> workers | Mutex+CV queue; atomics for `cancel`, `inFlight` | Only place with locks |
| `efapp` <-> `efcore` | Include `ef/*.h`; read `FieldResult`; start sessions | `efcore` is unaware of `efapp` |
| `renderScene` <-> `Painter` | Virtual calls with device-space numbers | Painter never sees world coordinates, `Settings`, or `FieldScene` |
| `Settings` <-> `SettingsStore` | Plain struct <-> text file | Versioned: include a `version=1` key; ignore unknown keys; clamp on load (the MFC dialog validates thickness only for the electric line twice, never the potential line, so loaded values need sanitising) |
| Old registry -> new file | Optional Windows-only one-time import | Colours are `COLORREF` `0x00BBGGRR` binary blobs; `potentialInterval` was stored as int x1000. Skip unless the author cares about migrating own settings |

---

## 10. Key Risks Flagged for Roadmap

1. **Silent numeric behaviour drift when leaving MSVC** (unqualified `abs(double)` can bind to integer `abs` if only C headers are visible; `M_PI` redefinition; `sign(0)`). Add compile flags `-Wall -Wextra -Wconversion -Wabsolute-value` early (Phase 1).
2. **Threading rewrite (Phase 3) is the highest-risk step.** Budget TSan/ASan runs on Linux CI and a stress test that creates/destroys sessions mid-run.
3. **Behaviour parity of de-dup and Bezier.** Port exactly, test with invariants; decide explicitly whether to keep the existing clip-gap quirk (after a gap the Bezier triple grouping restarts at an arbitrary point index, while `AdjustForBezier` fitted control points on indices that are multiples of 3). Recommendation: fix in `PathBuilder` by splitting runs on multiples-of-3 boundaries; the visual effect is limited to the viewport edge.
4. **`AdjustForBezier` divides by `divi` that can be ~0** (the old code only `ASSERT`ed in debug). Provide a fallback (leave control points as a straight cubic) rather than emitting NaN/Inf into `Painter`.
5. **`TheElectricField::GetTotalCharge` dereferences `charges.begin()` without an empty check.** `ChargeSystem` must reject or special-case empty sets.
6. **Worker exceptions** (e.g. `bad_alloc` on huge point vectors) currently call `std::terminate`; catch in the worker, record `failed/error`, surface via `poll()`.
7. **Research/doc inconsistency:** `PROJECT.md` "Validated" states progressive display; the current code is all-at-once. Treat progressive display as a new optional feature, not a parity requirement.

---

## Sources

- Direct source reading (HIGH confidence): `ElectricField/FieldLinesCalculator.h/.cpp`, `ComputationThread.h/.cpp`, `TheElectricField.h/.cpp`, `Charge.h/.cpp`, `FieldLine.h/.cpp`, `Options.h/.cpp`, `ElectricFieldDoc.h/.cpp`, `ElectricFieldView.h/.cpp`, `ConfigDocs.h/.cpp`, `ElectricField.h/.cpp`, `RungeKutta.h`, `Vector2D.h/.cpp`, `ComputationPropertyPage.cpp`, `DrawPropertyPage.cpp`, `PrecisionTimer.h`, `stdafx.h`; `.planning/PROJECT.md`.
- Pattern knowledge (MEDIUM): standard C++ worker-pool / RAII-join / drain-and-swap handoff practice; scene-as-data render abstraction used by SVG-capable plotting libraries.
- Framework rows in section 6 are design guidance only (MEDIUM/LOW); verify against the framework chosen by the parallel STACK research before Phase 7.
- Companion blog post referenced by the project (algorithm background): https://compphys.go.ro/electric-field-lines/

---
*Architecture research for: cross-platform electric-field-lines simulator (MFC port)*
*Researched: 2026-10-10*
