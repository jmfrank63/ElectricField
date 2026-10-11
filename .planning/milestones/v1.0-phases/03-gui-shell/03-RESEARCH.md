# Phase 3: GUI Shell - Research

**Researched:** 2026-10-10
**Domain:** Dear ImGui 1.92.9b-docking + SDL3 3.4.18 (castholm/SDL) desktop shell over the Phase 2 `ComputeSession` / `renderScene` core, built with Zig 0.17.0
**Confidence:** HIGH for everything exercised on Windows 11 (Zig 0.17.0, x86_64-windows-gnu); MEDIUM for Linux/macOS behaviour (the prototype was only run on Windows; CI is the first Linux/macOS evidence)

## Summary

Phase 3 is not a green-field design. A working, tested prototype of the whole phase exists (snapshot in `.planning/phases/03-gui-shell/research-ref/`, live buildable copy in `%TEMP%/efproto/repo`). It implements every locked decision D-01..D-17 and the approved UI-SPEC: single host window with menu bar, reorderable tab bar, a preset chooser modal, an About modal, busy overlay + wait cursor, HiDPI scale `k`, an idle loop that blocks in `SDL_WaitEventTimeout`, clean cancel-all shutdown, a GUI-subsystem exe with embedded icon and a Windows `.rc` icon, version + git hash injection, and an extended `--smoke` that drives the real UI through ImGui's input queue. As of this session `zig build test` is green (69 core+app cases, 4 render-context cases) and `zig build smoke` is green in Debug and ReleaseFast (3 canvas-clipping checks, keyboard contract, middle-click close, failed tab, overlay clipping, idle loop, quit with busy sessions). The planner's job is to turn the prototype into reviewable plans and land it, not to re-research it.

The prototype splits cleanly into (a) **pure, std-only, unit-tested logic** (`src/app/`: tab model over `ComputeSession`, strings, metrics, frame pacer; built with the same strict `-Werror` flags as the core) and (b) a thin **ImGui/SDL layer** (`src/gui/`) that only maps that logic onto widgets and has no decisions of its own. Drawing goes through the unchanged Phase 2 `ef::renderScene` + `efrender::ImGuiPainter` path; the GUI canvas matches `ef-cli` PNG output to within a few dozen anti-aliasing pixels per 786,432 and matches the MFC baselines at the same metrics `tools/parity/compare-mfc.sh` uses (ink recall >= 0.988, precision >= 0.995, darkness ratio 0.996..1.010).

One real defect in committed Phase 2 code was found and fixed in the prototype: `efrender::renderPng` inside a running GUI trips `IM_ASSERT(io.BackendRendererUserData == nullptr)` because `ImGui::CreateContext()` does not make the new context current when one already exists. This is Phase 4's export path and the GUI smoke's reference render, so the fix (`renderSurface` + explicit `SetCurrentContext`) belongs in Phase 3 together with a regression test (`tests_gui/test_render_ctx.cpp`). Two further hazards discovered here are documented below: Zig 0.17 caches the configure phase so a `runFallible("git rev-parse")` hash goes stale, and injected key chords repeat when software frames are slow.

**Primary recommendation:** Land the prototype as-is in 3-4 plans (logic+tests, GUI shell+build, render-context fix+CI, smoke/validation+UAT), copy only the files listed in "Delta vs current repo", leave `probes/` and `shots2/` out of the product, and gate completion on `zig build test`, `zig build smoke -Doptimize=ReleaseFast` on all three CI OSes plus a human check of the real window, a real HiDPI display and macOS.

## User Constraints (from CONTEXT.md)

<user_constraints>
### Locked Decisions

**Window & tab layout**
- **D-01:** Single main window with an ImGui tab bar: one tab per open configuration (maps MFC's tabbed MDI). No ImGui docking/multi-viewport for tabs in this phase.
- **D-02:** Opening presets: **File ▸ New ▸** submenu listing the 9 presets by their MFC display names (from the core `PresetRegistry`), plus **Ctrl+N** opening a preset chooser modal (list of 9, "Open Configuration" / "Close Chooser" buttons per UI-SPEC, double-click or Enter opens, Esc closes) mirroring MFC's "New" dialog. The same preset may be opened multiple times; tab titles are "<Name> 1", "<Name> 2", … (MFC "Dipole1" style, with a space).
- **D-03:** On startup the preset chooser is shown (parity with MFC's startup "New" chooser); Cancel leaves an empty window with a hint ("File ▸ New or Ctrl+N to open a configuration").
- **D-04:** Menus: **File** (New ▸ 9 presets, New… Ctrl+N, Close Tab Ctrl+W, Exit Alt+F4/Ctrl+Q) and **Help** (About Electric Field…). MFC's Application Look themes, toolbar/docking customisation, status-bar toggle and print menus are dropped. (View ▸ Options arrives in Phase 4.)

**Canvas rendering**
- **D-05:** The canvas uses the Phase 2 shared path: `FieldScene` + `renderScene()` + `ImGuiPainter` (same code as `ef-cli` PNG), drawn into the tab's content region; white canvas; MFC default colours (positive blue, negative red, field lines black, equipotentials grey 192, thickness 1, charge radius per Phase 2 scene); `AntiAliasedLinesUseTex = false`, `CurveTessellationTol = 0.3f` as established in Phase 2.
- **D-06:** Resize behaviour: the world origin stays centred in the canvas at a fixed 100 logical px per unit; nothing is recomputed on resize (parity). Pan/zoom is v2.
- **D-07:** HiDPI: use SDL3 display content scale / window pixel density so 100 *logical* px per unit is sharp on HiDPI (Windows per-monitor DPI awareness, macOS Retina); scale ImGui fonts/style accordingly; handle display-scale-change events.
- **D-08:** ImGui **light** style for the window chrome (closest to the MFC look); canvas is white regardless.

**Computation lifecycle**
- **D-09:** Opening a tab immediately starts its own `ComputeSession` with MFC default `ComputeParams` (no options UI until Phase 4).
- **D-10:** Busy indicator: while computing, the tab title carries a spinner/marker and the canvas shows the charges plus a "Computing… n / m lines" overlay (n from `poll()`, m from `expectedLineCount`); lines are drawn all at once when the session reports done (parity; progressive drawing is v2); wait cursor while hovering a busy canvas.
- **D-11:** Closing a busy tab or quitting cancels the session(s) and closes immediately — no confirmation dialog (cancel/destroy is bounded by the Phase 2 latency contract). App quit cancels all sessions before tearing down SDL/ImGui.
- **D-12:** Main loop: vsync on; when no tab is computing and no input is pending, block in `SDL_WaitEventTimeout` (no busy loop — fixes Phase 1 review WR-02 / Phase 2 IN-03); while any tab computes, poll sessions each frame at ≤ ~30–60 fps. Check ImGui backend init results and clean up on all error paths.

**About box & app identity**
- **D-13:** About modal: "Electric Field", version + short git hash (injected at build time via build options), original-author credit, links to the blog (https://compphys.go.ro/electric-field-lines/) and the GitHub repo, opened via `SDL_OpenURL`.
- **D-14:** Reuse `ElectricField/res/ElectricField.ico` imagery: window icon on all OSes (SDL_SetWindowIcon from an embedded image) and the Windows exe icon via a resource file — the icon asset must be copied out of the MFC tree (e.g. `assets/`) so it survives MFC deletion in Phase 4.
- **D-15:** Window title "Electric Field — <active tab name>" ("Electric Field" when no tab); default window size large enough for a 1024×768 canvas plus menu/tab bar (clamped to the display's usable area).

**Carry-over**
- **D-16:** The GUI keeps `--smoke` (headless start + render ≥1 frame with a preset open, exit 0) so CI keeps exercising the real GUI path on all 3 OSes; extend smoke to open a preset tab, wait for its session, draw it, and close it (exercising cancel/close paths).
- **D-17:** `renderPng` must run on the UI thread between frames (Phase 2 constraint) — relevant for Phase 4 export; Phase 3 must not break `ef-cli`/parity CI.

### Claude's Discretion
Exact module split (`src/gui/` app/tab/canvas/menu files), spinner style, chooser layout, font choice (ImGui default font scaled, or an embedded TTF if needed for HiDPI quality), how version/git hash are injected, icon conversion pipeline.

### Deferred Ideas (OUT OF SCOPE)
- Pan/zoom, progressive drawing, status bar with line counts/timing (v2).
- Tabs that can be dragged out/docked (ImGui docking) — not now.
- View ▸ Options, export, persistence (Phase 4).

### UI-SPEC (approved 2026-10-11, `03-UI-SPEC.md`) — treat as locked as well
The planner MUST read `03-UI-SPEC.md` in full; it fixes every metric, colour, string and key. The prototype implements it. Load-bearing points: light style + 3 colour overrides (`TextDisabled #595959`, `TextLink #0054CC`, `ModalWindowDimBg` 0.25 alpha); one borderless host window with menu bar -> tab bar -> canvas; `AddFontDefaultVector()` with `FontSizeBase = 16`, `FontScaleDpi = k`, `ScaleAllSizes(k)`; `io.IniFilename = nullptr` always; `ConfigFlags_NavEnableKeyboard` on, docking/viewports off; tab ids `"<title>###ef<serial>"`; busy marker `| / - \` at 8 steps/s; copy deck in "Copywriting Contract" (ASCII only in ImGui widgets, em dash only in the SDL window title); modal sizes 360 x k (chooser) and 400 x k (About); smoke sequence in "Smoke mode (D-16)".
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| VIEW-01 | Open any of the 9 presets from a menu, each in its own tab, charge data identical to MFC | `efapp::Workspace::open(presetIndex)` builds the tab from `ef::presets()` / `ef::makeSystem` (charge data comes from the Phase 2 core, no duplicate table). File > New submenu + Ctrl+N chooser in `src/gui/ui.cpp`. Covered by `tests/test_app.cpp` (titles/counters) and smoke (Ctrl+N, Down, Enter opens "Two Equal Charges 1"). |
| VIEW-02 | Several configurations open at once; close any tab incl. while computing | `Workspace::close/closeAll/cancelAll`; measured cancel+destroy <= 1.8 ms worst case (probe `lat`), 12.6 ms for 27 busy sessions; smoke covers close-while-busy, middle-click close, quit with 3 busy sessions (`quit_ms` 1-5). |
| VIEW-03 | Charges filled by sign, anti-aliased Bezier lines, y-up, centred, 100 px/unit, HiDPI-correct | `drawCanvas` -> `efrender::ImGuiPainter` + `ef::renderScene` with `canvasView(avail, k)` and `scaledStyle(k)`; HiDPI via `k = SDL_GetDisplayContentScale` (see HiDPI section). Smoke canvas checks at 640x480 / 1024x820 / 1280x1024; GUI-vs-MFC metrics table. Real HiDPI hardware = manual-only. |
| VIEW-04 | Busy indicator while computing, completed lines once done (parity) | Tab marker glyph, overlay with spinner + `Computing... n / m lines`, wait cursor, lines hidden until done (`Workspace::poll` builds the scene once on done). Smoke checks overlay stays inside the canvas at minimum window size. Wait-cursor visual = manual. |
| VIEW-05 | GUI application (no console window on Windows) with About box incl. project/blog links | `exe.subsystem = .windows` + `SDL_main.h` (verified PE subsystem = 2, icon extractable); About modal with `SDL_OpenURL` + clipboard fallback. Link click = manual. |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

Source: `.claude/CLAUDE.md` (project instructions) [VERIFIED: file loaded this session].

- C++17 or newer for the physics core; GUI code uses `gui_flags` (no `-Werror`); core/app/tests use the strict `core_flags`.
- Windows, Linux, macOS desktop; command-line build only; must not require MSVC/MFC; Zig 0.17.0 + `zig build`; target triple `x86_64-windows-gnu` on Windows (never `-msvc`).
- Do not use `allyourcodebase/SDL3`, `imgui_impl_opengl3/win32/dx11`, ImGui multi-viewport, ImPlot, `@cImport`; do not cross-compile macOS from Windows/Linux; no `-Dtarget` on the macOS CI job.
- Behaviour parity: same default options + configurations give visually equivalent output (MFC baselines in `docs/baseline/mfc/`).
- Licence: repo is GPL-3.0; ImGui (MIT) and SDL (zlib) compatible; no new dependency or asset except the icon copied from the repo's own MFC resources.
- GSD workflow enforcement: repo edits go through a GSD command (`/gsd-execute-phase` for this phase's plans). This research run only wrote under `.planning/phases/03-gui-shell/`.

## Architectural Responsibility Map

Native desktop app, so the "tiers" are process layers rather than web tiers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Field/potential computation, cancellation, line counts | Core library `efcore` (`src/core/ef`, worker threads inside `ComputeSession`) | — | Phase 2 contract; the UI thread only calls `poll()/cancel()/takeResult()` and never touches data a worker can write |
| Tab model, session ownership, titles, counters, busy state, scene cache | `efapp` (std-only, `src/app`) | — | Pure logic, unit-testable under the strict flags, no SDL/ImGui types |
| Pure UI decisions (strings, glyph, window title, scale clamp, window sizes, canvas view, frame pacing) | `efapp` (`src/app/ui_logic`) | — | Fixed by UI-SPEC; testable without a display |
| Widgets: host window, menus, tab bar, chooser, About, overlay, empty/failed states | ImGui layer (`src/gui/ui.cpp`) | `efapp` (supplies strings/metrics) | Immediate-mode mapping only; no decisions |
| Window, renderer, event loop, DPI scale, icon, vsync, clipboard/URL, message box, wait/poll | SDL3 layer (`src/gui/app.cpp`, `main.cpp`) | `efapp::FramePacer` (decides block vs poll) | Platform glue |
| Scene drawing (Bezier, circles, clip, AA) | `efrender::ImGuiPainter` + `ef::renderScene` (shared with `ef-cli`) | ImGui `ImDrawList` + sdlrenderer3 | One drawing path (D-05) |
| Offscreen render/PNG | `efrender::renderSurface/renderPng` (`src/render/png_export.*`) | — | Own ImGui context + software renderer; used by `ef-cli`, by GUI smoke as the reference, by Phase 4 export |
| Build info (version, git hash), icon, `.rc` | `build.zig` + `assets/` | — | Zig build graph |

## Standard Stack

All dependencies already exist and are hash-pinned from Phase 1; Phase 3 adds none. [VERIFIED: `build.zig.zon` read this session]

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Zig | 0.17.0 (`zig version` -> `0.17.0`) | compiler (Clang 22, libc++), build system | project constraint |
| Dear ImGui | v1.92.9b-docking (`https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz`, hash `N-V-__8AAAPHhAB86bkFSQlFlaioKXgArks51cXvTB1G5j7F`) | widgets, `ImDrawList` | locked in Phase 1 |
| SDL3 via castholm/SDL | 0.6.0 + SDL 3.4.18 (`sdl-0.6.0+3.4.18-SDL--p-gqAGWpnfTLlhs0dgjDtc4P4_CHeNLGY_SX_Fv`) | window, events, DPI, URL, clipboard, message box, PNG | locked in Phase 1 |
| `imgui_impl_sdl3` + `imgui_impl_sdlrenderer3` | shipped in the ImGui tree | platform + render glue | locked in Phase 1 |
| doctest | 2.5.3 (`N-V-__8AAB9aPgADa6XPcoVfyGpb-2fhHYgVMX9InfAngj-p`) | unit tests | locked in Phase 1 |

### Supporting
None new. The icon is embedded with Clang's C23 `#embed` (Clang 22 in Zig 0.17) with `-Wno-c23-extensions` for `icon.cpp` only; no `xxd`/generator step, no new library.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `#embed` icon bytes | generate a C array with a build step | more build-graph code; `#embed` is verified working on Zig 0.17/Clang 22 on Windows |
| Hand-rolled std-only tab model | put session handling in `ui.cpp` | untestable state machine; the `ISession` seam is what lets failed/cancelled paths be tested and smoke show the failed tab |

**Installation:** none (`zig build` fetches lazily; `-Dgui=false` fetches neither SDL nor ImGui).

**Version verification:** `zig version` = `0.17.0` this session; pinned URLs/hashes read from `build.zig.zon`. No registry lookup applies (Zig package manager, hash-pinned).

## Package Legitimacy Audit

No new external packages are introduced in this phase. The three existing dependencies were vetted in Phase 1 and are hash-pinned in `build.zig.zon`; `npm/pip/cargo` ecosystems do not apply.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| imgui v1.92.9b-docking | GitHub tarball (hash-pinned) | long-lived | n/a | github.com/ocornut/imgui | OK (Phase 1 vetted) | Approved, unchanged |
| castholm/SDL 0.6.0+3.4.18 | git+https (commit-pinned, hash-pinned) | n/a | n/a | github.com/castholm/SDL | OK (Phase 1 vetted) | Approved, unchanged |
| doctest v2.5.3 | GitHub tarball (hash-pinned) | long-lived | n/a | github.com/doctest/doctest | OK (Phase 1 vetted) | Approved, unchanged |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none
No `checkpoint:human-verify` install gate is required.

## Architecture Patterns

### System Architecture Diagram

```
 OS events (mouse/key/resize/DPI/close)                         worker threads (4 per tab)
        |                                                         ^   |  lines (shared_ptr<const>)
        v                                                         |   v
 SDL_PollEvent / SDL_WaitEventTimeout(250 ms when idle) ---> efapp::Workspace  <---- ef::ComputeSession (RAII)
        |  handleEvent: ImGui_ImplSDL3_ProcessEvent;                |  pollAll(): poll() once per computing tab;
        |  QUIT/CLOSE -> quit_, DISPLAY_(SCALE_)CHANGED -> scaleDirty|  done -> takeResult -> buildScene (once) -> Tab.scene
        v                                                           |  close(serial)/closeAll(): cancel() FIRST, then destroy (join)
 App::iterate()                                                     |
   FramePacer.shouldBlock(anyComputing)  (idle => block, else poll+render every vsync frame)
        |
        v
 App::frame():  ws.pollAll -> [refreshScale if dirty: style=base; ScaleAllSizes(k); FontScaleDpi=k]
        -> ImGui NewFrame -> drawUi(ws, ui, env)
              host window "##host" (NoTitleBar..MenuBar)
                menu bar (File > New > 9 presets | New... | Close Tab | Exit ; Help > About)
                shortcuts (Ctrl+N/W/Q) -- ignored while a modal is open
                open-requests applied BEFORE the tab bar (new tab visible this frame)
                tab bar (Reorderable|AutoSelectNewTabs|FittingPolicyScroll|TabListPopupButton) -- label "<glyph> <title>###ef<serial>"
                  active tab: drawCanvas = InvisibleButton + ImGuiPainter(window draw list, floor(cursor)) -> ef::renderScene(scene, scaledStyle(k), canvasView(avail,k))
                              Computing: busy overlay (spinner + "Computing... n / m lines") + ImGuiMouseCursor_Wait on hover
                              Failed: red heading + error text + hint
                  toClose[] collected, applied AFTER EndTabBar (never erase while iterating)
                zero tabs: empty state (heading + hint)
                chooserModal / aboutModal (BeginPopupModal, centred, fixed width)
        -> ImGui Render -> SDL_SetWindowTitle (only when changed) -> RenderClear(240,240,240) -> ImGui_ImplSDLRenderer3_RenderDrawData -> Present
 Shutdown order: ws.closeAll() (cancel all, then destroy) -> SDLRenderer3_Shutdown -> SDL3_Shutdown -> DestroyContext
                 -> DestroyTexture(about icon) -> DestroyRenderer -> DestroyWindow -> SDL_Quit   (idempotent, also used on every init error path)

 Headless: --smoke / --screenshot use hint VIDEO_DRIVER=dummy + RENDER_DRIVER=software, then the same App code;
           canvas checks compare SDL_RenderReadPixels with efrender::renderSurface (second ImGui context while the GUI context is live).
```

### Recommended Project Structure (the prototype's)
```
src/
├── app/                  # std-only, strict -Werror flags, links efcore only  (library `efapp`)
│   ├── workspace.{hpp,cpp}   # ISession seam, Tab, Workspace (open/close/pollAll/cancelAll/closeAll)
│   └── ui_logic.{hpp,cpp}    # busyGlyph, tabLabel, overlayText, windowTitle, versionLine, shortcutLabel,
│                             # clampScale, defaultWindowSize/minimumWindowSize, canvasView, scaledStyle, FramePacer
├── gui/                  # SDL3 + ImGui, gui_flags (no -Werror)
│   ├── main.cpp              # SDL_main entry, arg parsing, fatal-error box, quiet crash dialogs for headless modes
│   ├── app.{hpp,cpp}         # App: init/shutdown, loop, DPI, smoke, screenshot
│   ├── ui.{hpp,cpp}          # drawUi + modals + makeBaseStyle/applyScale
│   └── icon.{hpp,cpp}        # #embed of assets/ElectricField-256.png / -96.png
├── imgui_config/ef_imconfig.h  # IMGUI_USER_CONFIG: non-interactive IM_ASSERT (stderr + _Exit(3)) in every build mode
└── render/png_export.{hpp,cpp} # + renderSurface() and the context rule
assets/  ElectricField.ico  ElectricField.rc  ElectricField-256.png  ElectricField-96.png
tests/test_app.cpp          # efapp doctests (added to the core test exe, no SDL/ImGui)
tests_gui/                  # render-context regression tests (need SDL3 + ImGui), step `test-render`, also run by `test`
```

### Module / file map (what each file does, for plan boundaries)
| File | Role | Key symbols |
|------|------|-------------|
| `src/app/workspace.{hpp,cpp}` | Tab model | `ISession`, `SessionFactory`, `defaultSessionFactory()`, `TabState{Computing,Done,Failed}`, `Tab`, `Workspace` |
| `src/app/ui_logic.{hpp,cpp}` | Pure UI decisions | see structure above; `FramePacer{kSettleFrames=3,kModalFadeFrames=12,kIdleWaitMs=250}` |
| `src/gui/ui.cpp` | Widgets | `drawUi`, `makeBaseStyle`, `applyScale`; `UiState` (requests in, rects/flags out for smoke) |
| `src/gui/app.cpp` | Platform loop | `App::init/shutdown/iterate/frame/frames/refreshScale/chord/checkCanvas/smokeExtras/runSmoke/runScreenshot/runInteractive` |
| `src/gui/main.cpp` | Entry | `--smoke`, `--screenshot <slug> <png> [--size WxH] [--scale k] [--modal about|chooser]`, fatal box, `quietCrashDialogs()` (headless modes only) |
| `src/render/png_export.cpp` | Offscreen render | `renderSurface(scene,style,view,err)` returns RGBA32 `SDL_Surface*`; `renderPng` wraps it |
| `build.zig` | Graph | `addApp`, `addRender` (new lib `efrender` shared by cli/gui/tests), `addRenderTests`, `addGui` (+build info header, `.rc`, subsystem), `addProbe` (dev only) |

### Pattern 1: Tab model behind a session seam
**What:** `Workspace` owns `std::vector<std::unique_ptr<Tab>>`; each `Tab` holds `unique_ptr<ISession>` (real: wraps `ef::ComputeSession`), the world-unit `FieldScene` (charges from the first frame; lines after done) and counters. `serial` is a never-reused id; `number` is a per-preset never-reused counter. A session that throws on construction yields a `Failed` tab, never an exception into the UI.
**When to use:** always; the UI never touches `ComputeSession` directly.
**Example:** (quoted from the prototype `src/app/workspace.cpp`)
```cpp
unsigned Workspace::open(std::size_t presetIndex) {
  const ef::Preset& p = ef::presets().at(presetIndex);
  auto t = std::make_unique<Tab>();
  t->serial = nextSerial_++;
  t->preset = presetIndex;
  t->number = ++counters_[presetIndex];
  t->title = std::string(p.displayName) + " " + std::to_string(t->number);
  t->field = ef::makeSystem(p);
  t->params = defaults_;
  t->expectedLines = ef::expectedLineCount(t->field, t->params);
  t->scene = ef::buildScene(t->field, ef::FieldResult{});  // charges only: drawn from the first frame
  ...
}
void Workspace::closeAll() { cancelAll(); closed_ += static_cast<unsigned>(tabs_.size()); tabs_.clear(); }
```
`cancelAll()` requests cancel on every session first so the worker joins overlap; destruction then joins.

### Pattern 2: Per-frame canvas through the shared painter
**What:** `InvisibleButton` over the available rect (gives hover), then
```cpp
const ef::View view = efapp::canvasView(avail.x, avail.y, env.k);   // 100*k px/unit, floor(avail), >= 1x1
const ef::DrawStyle style = efapp::scaledStyle(env.k);              // thickness and radius x k, colours unchanged
{ efrender::ImGuiPainter painter(dl, p /* floor()'d top-left */); ef::renderScene(painter, t.scene, style, view); }
```
The painter pushes the canvas clip rect, so a smaller canvas clips and the origin stays at the canvas centre every frame; nothing is recomputed (D-06). The painter's destructor pops the clip rect even if `renderScene` throws.

### Pattern 3: Idle loop
`FramePacer` decides: `shouldBlock(anyComputing) = !anyComputing && settle_ == 0`. After any event or state change `notifyActivity()` arms `kSettleFrames = 3` extra frames (hover/focus/popup settle); a modal opening or closing arms `kModalFadeFrames = 12` (ImGui fades the dim layer over ~0.2 s). Blocked => `SDL_WaitEventTimeout(&e, 250)` then drain with `SDL_PollEvent`. Computing => poll events and render every frame (vsync paces to ~60 fps; without vsync `SDL_Delay(16)`). Minimised => keep polling sessions, `SDL_Delay(16)`, draw nothing.

### Anti-Patterns to Avoid
- **Calling `renderPng`/`renderSurface` with the committed Phase 2 code inside the GUI** (see Pitfall 1).
- **Erasing a tab inside the tab-bar loop** — collect serials, apply after `EndTabBar`.
- **Computing `k` from `SDL_GetWindowDisplayScale`** — it is content scale x pixel density; the sdl3 backend already applies pixel density as `io.DisplayFramebufferScale` on macOS/Wayland, so the UI would double.
- **`BeginListBox` for the chooser** — it is a separate nav scope; Up/Down never reach the rows until Enter enters it (Pitfall 4).
- **Letting `IM_ASSERT` be the default C `assert`** — modal dialog in a GUI-subsystem exe, silently compiled out under `NDEBUG`/ReleaseFast.
- **A second drawing path in the GUI** — parity with `ef-cli` is the acceptance bar.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Tab strip, scroll, list popup, close button, middle-click close, reorder | custom tab widget | `ImGui::BeginTabBar` with `Reorderable | AutoSelectNewTabs | FittingPolicyScroll | TabListPopupButton` and `bool* p_open` | All behaviours are built in; smoke proves middle-click closes |
| Shortcuts | key-state polling | `ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N)` | routing/focus handled; maps Ctrl to Cmd on macOS |
| Modal dialogs | own overlay/dim | `BeginPopupModal` + `SetNextWindowPos(centre, Appearing, (0.5,0.5))` | dim, focus trap, Esc handled |
| Link rendering | custom hyperlink | `ImGui::TextLink` + `SDL_OpenURL` | built in (1.92) |
| Hi-DPI font scaling | rebuild font atlas per scale | `AddFontDefaultVector()` + `style.FontScaleDpi = k` + `PushFont(nullptr, size)` | 1.92 dynamic fonts are crisp at any size |
| PNG decode for icons | libpng/stb | `SDL_LoadPNG_IO` over `SDL_IOFromConstMem` | in SDL core |
| Screenshot/PNG save | stb_image_write | `SDL_RenderReadPixels` + `SDL_SavePNG` | verified in Phase 1/2 |
| Config-time constants | scripted header generation | `b.addConfigHeader(.{ .style = .blank, .include_path = "ef_build_info.h" }, .{...})` | stock Zig API (but see Pitfall 6 for the git hash) |
| Windows exe icon | custom PE patching | `mod.addWin32ResourceFile(.{ .file = b.path("assets/ElectricField.rc") })` | Zig 0.17 compiles `.rc` with its bundled resource compiler; no `rc.exe`; ignored off Windows |
| Waiting for events | sleep loop | `SDL_WaitEventTimeout` | idle CPU measured 0.39% |

**Key insight:** every widget, scaling and event primitive the UI-SPEC needs already exists in ImGui 1.92 / SDL3; the prototype's only custom code is state (tabs, pacing) and the smoke harness.

## Exact APIs used (verified by build + run on Windows; Linux/macOS behaviour is CI-gated)

| Concern | API / setting (prototype `src/gui/app.cpp` unless stated) | Notes |
|---------|-----------------------------------------------------------|-------|
| Window | `SDL_CreateWindow("Electric Field", w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)`; `SDL_SetWindowMinimumSize(min(640k,w), min(480k,h))`; `SDL_SetWindowPosition(CENTERED)`; icon `SDL_SetWindowIcon(window, SDL_LoadPNG_IO(SDL_IOFromConstMem(...)))` (SDL copies pixels; ignored on macOS Dock) | initial size `defaultWindowSize(k0, usable.w, usable.h)` = (1024x824)*k clamped to 90% of `SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay())` |
| HiDPI | `k = clampScale(SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window)))` at init and on `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` / `SDL_EVENT_WINDOW_DISPLAY_CHANGED` (sets `scaleDirty_`, applied at the start of the next frame); `applyScale`: `style = base; style.ScaleAllSizes(k); style.FontScaleDpi = k;` | `clampScale` -> [0.75, 4.0], non-finite/<=0 -> 1.0. Windows: SDL 3.4.18 makes the process per-monitor-v2 aware by itself (probe `dpi`: `awareness=2 ... perMonitorV2=1` with no manifest). macOS: window units are points, `k = 1`, backend `DisplayFramebufferScale` supplies 2x |
| Vsync | `vsync_ = SDL_SetRenderVSync(renderer_, 1)` (non-fatal); `if (!vsync_) SDL_Delay(16)` | |
| Idle/poll loop | `SDL_WaitEventTimeout(&e, 250)` then `SDL_PollEvent` drain; else `SDL_PollEvent` each frame | see Pattern 3 |
| Wait cursor | `ImGui::SetMouseCursor(ImGuiMouseCursor_Wait)` when the busy canvas `InvisibleButton` is hovered | sdl3 backend maps it to `SDL_SYSTEM_CURSOR_WAIT` [VERIFIED: `backends/imgui_impl_sdl3.cpp:632` `bd->MouseCursors[ImGuiMouseCursor_Wait] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);`] |
| Open URL / clipboard | `if (ImGui::TextLink(label) && !SDL_OpenURL(url)) { SDL_SetClipboardText(url); linkNote = "Could not open a browser. The link was copied to the clipboard."; }`; tooltip shows the URL | `SDL_OpenURL` declared in `SDL_misc.h:72` |
| Fatal startup box | `main.cpp`: on `init()` failure print to stderr, and `if (!opt.headless()) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Electric Field", text, nullptr)`; text `"Electric Field could not start.\n<step>: <SDL_GetError()>"`; steps `SDL initialisation`, `Window creation`, `Renderer creation`, `Dear ImGui initialisation` | `init()` releases everything created so far via the idempotent `shutdown()` (fixes WR-02 / IN-03 leaks). Message-box gating: never in `--smoke`/`--screenshot` (a modal would hang CI). See Open Question 2 for an observed limitation |
| Backend init checks | `ImGui_ImplSDL3_InitForSDLRenderer` and `ImGui_ImplSDLRenderer3_Init` return values checked; flags `platformInit_/rendererInit_/imguiCtx_/sdlInit_` drive teardown | |
| ImGui config | `io.IniFilename = nullptr`; `io.ConfigFlags |= NavEnableKeyboard`; `io.Fonts->AddFontDefaultVector()`; `IMGUI_CHECKVERSION()`; first context via `ImGui::CreateContext()` (becomes current because none exists) | |
| Icon embedding | `icon.cpp`: `const unsigned char k256[] = { #embed "../../assets/ElectricField-256.png" };` (same for 96); compiled with `-Wno-c23-extensions` | 256 PNG = the PNG stored inside the original `.ico`, extracted byte-for-byte; 96 = resample for the About `ImGui::Image` (`SDL_CreateTextureFromSurface`, linear scale mode) |
| Windows exe icon + subsystem | `mod.addWin32ResourceFile(.{ .file = b.path("assets/ElectricField.rc") })` with `.rc` = `IDI_APPICON ICON "ElectricField.ico"`; `exe.subsystem = .windows`; `#include <SDL3/SDL_main.h>` in the TU defining `int main(int, char**)` | Verified on the built exe: PE optional-header Subsystem = 2 (GUI) and `ExtractAssociatedIcon` returns a 32x32 icon |
| Version + git hash | `const zon = @import("build.zig.zon");` + `b.addConfigHeader(.{ .style = .blank, .include_path = "ef_build_info.h" }, .{ .EF_VERSION = zon.version, .EF_GIT_HASH = gitShortHash(b) })`; `gitShortHash` = `b.runFallible(&.{"git","rev-parse","--short=7","HEAD"}, .{ .stderr_behavior = .ignore })` -> trimmed stdout, `""` on any failure (UI shows `(unknown)`) | generated header: `#define EF_VERSION "0.1.0"` / `#define EF_GIT_HASH ""`. **Hash goes stale** (Pitfall 6). Zig 0.17: `run.addPassthruArgs()` replaces `b.args` |
| Headless | `SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy")`, `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software")` inside `init()` when smoke/screenshot (no env dependence); `zig build smoke` also sets the env vars | |
| Crash dialogs (headless only) | `SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX)`, `_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)`, `_set_error_mode(_OUT_TO_STDERR)` | compiles under `x86_64-windows-gnu` |
| Assert policy | `src/imgui_config/ef_imconfig.h` via `-DIMGUI_USER_CONFIG="ef_imconfig.h"` in `gui_flags`; `IM_ASSERT` prints `file:line:expr` to stderr and `std::_Exit(3)` in every build mode | the define must reach EVERY TU that includes `imgui.h` (imgui lib, efrender, gui, tests_gui, probes) |

### In-repo values quoted from their source of truth (read this session)

- Preset names and slugs, `src/core/ef/presets.cpp:7-16` [VERIFIED: file read]:
  `{"dipole", "Dipole", ...}`, `{"two-equal-charges", "Two Equal Charges", ...}`, `{"two-unequal-charges-same-sign", "Two Unequal Charges Same Sign", ...}`, `{"two-unequal-charges-different-signs", "Two Unequal Charges Different Signs", ...}`, `{"quadrupole", "Quadrupole", ...}`, `{"four-charges-same-sign", "Four Charges Same Sign", ...}`, `{"two-dipoles-same-direction", "Two Dipoles Same Direction", ...}`, `{"two-dipoles-opposed-direction", "Two Dipoles Opposed Direction", ...}`, `{"parallel-dipoles", "Parallel Dipoles", ...}`; `inline constexpr std::size_t kPresetCount = 9;` (`presets.hpp:10`).
- `ef::DrawStyle` defaults, `src/core/ef/scene.hpp:21-30` [VERIFIED: file read]: `Color positiveCharge{0, 0, 255}; Color negativeCharge{255, 0, 0}; Color electricLine{0, 0, 0}; Color potentialLine{192, 192, 192}; Color background{255, 255, 255}; float electricThickness = 1.f; float potentialThickness = 1.f; float chargeRadiusPx = 25.f;`
- `ef::View`, `scene.hpp:33-37`: `int widthPx = 1024; int heightPx = 768; double pixelsPerUnit = 100.;`
- `ef::SessionStatus`, `session.hpp:36-44`: fields `done`, `cancelled`, `failed`, `error` (std::string), `jobsCompleted`, `jobsKnown`; `PollBatch` (`:46`) holds `electric` and `potential` vectors of `shared_ptr<const Keyed...>`; `cancel()` `:63`, `poll(PollBatch&)` `:66`, `takeResult(FieldResult&)` `:72`.
- MFC default `ComputeParams`, `params.hpp:10-14`: `unsigned linesPerUnitCharge = 16;  bool calculateEquipotentials = true;  double potentialInterval = 0.1;  unsigned numThreads = 4;  Method method = Method::Euler;`
- ImGui context creation, `imgui.cpp:4252-4261` in the pinned tarball [VERIFIED: file read]:
  `ImGuiContext* prev_ctx = GetCurrentContext(); ImGuiContext* ctx = IM_NEW(ImGuiContext)(shared_font_atlas); SetCurrentContext(ctx); Initialize(); if (prev_ctx != NULL) SetCurrentContext(prev_ctx); // Restore previous context if any, else keep new one.`
- Backend assert, `backends/imgui_impl_sdlrenderer3.cpp:302`: `IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");`

## The ImGui context rule (new finding; document as a project rule)

**Rule:** `ImGui::CreateContext()` makes the new context current ONLY when no context exists (quote above). Therefore any code that needs its own ImGui context while another may be live (offscreen export, tests, a future export button) must: (1) remember `prev = GetCurrentContext()`, (2) `ctx = CreateContext()`, (3) install a guard whose destructor restores `prev` on EVERY exit path, then (4) `ImGui::SetCurrentContext(ctx)` explicitly, and only then touch `GetIO()/GetStyle()` or init the backend.

**Why it matters:** the committed Phase 2 `renderPng` did `prev = Get...; ctx = CreateContext();` and relied on the return value being current. In `ef-cli` (no context) that works. Inside the GUI it initialises `ImGui_ImplSDLRenderer3` on the GUI's own context -> `IM_ASSERT "Already initialized a renderer backend!"`; with asserts compiled out it would silently rewrite the GUI's io, style and backend data and then `Shutdown` it.

**Fix (in the prototype's `src/render/png_export.cpp`):** new `SDL_Surface* renderSurface(scene, style, view, std::string& err)` (RGBA32, caller frees with `SDL_DestroySurface`) containing the corrected sequence; `renderPng` becomes a thin wrapper that calls it and `SDL_SavePNG`. Declaration added to `png_export.hpp` (`struct SDL_Surface;` forward declaration). Destruction order stays painter -> BackendGuard -> CtxGuard (restores caller context) -> renderer -> surface (Phase 2 WR-08).

**Regression test:** `tests_gui/test_render_ctx.cpp` (4 cases; own `main` in `tests_gui/test_main.cpp`; run by `zig build test-render` and by `zig build test`). It builds a "GUI-like" context + sdlrenderer3 backend on a software renderer, calls `renderSurface`/`renderPng` inside it and asserts: no assert, current context restored, `BackendRendererUserData` unchanged, `DisplaySize` untouched, `CurveTessellationTol` (0.7 in the test) not overwritten, pixels identical to a render with no GUI context, and repeated exports keep working. **Verified this session that the test is a real guard:** commenting out the `ImGui::SetCurrentContext(ctx);` line makes `zig build test-render` fail with `IM_ASSERT failed: ...imgui_impl_sdlrenderer3.cpp:302: io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!"` and `process exited with code 3`; restoring it passes 4/4.

The GUI smoke also exercises the rule in-process: `checkCanvas()` calls `renderSurface` while the GUI context with its initialised backend is live.

## Runtime State Inventory

Not a rename/refactor/migration phase. Omitted. (No stored data, services, OS registrations, or secrets exist; the app writes nothing in this phase: `io.IniFilename = nullptr`, config persistence is Phase 4. `--screenshot` writes only the path the developer passes.)

## Measurements (probes run this session, Windows 11, ReleaseFast, `probes/*.cpp` in research-ref)

| Measurement | Result | Source |
|-------------|--------|--------|
| Cancel + destroy of one busy session at ages 0/10/50/150/400/1000 ms, all 9 presets, MFC defaults (4 threads, 16 lines/unit, Euler, equipotentials 0.10) | 0.3 - 1.8 ms (1.8 ms is the very first thread start of the Dipole); sessions already done print `done` | probe `lat` (`zig build probe-lat -Doptimize=ReleaseFast`) |
| Cancel in the "finalizing" window (all jobs complete, not yet done) | 0.5 - 1.2 ms where the window could be hit (n/a for presets that finish too fast) | probe `lat` |
| Quit with N busy sessions (4 threads each; cancel all first, then destroy) | 9 sessions 3.5 ms; 27 sessions 12.6 ms; worst single close 1.8 ms. Contract is <= 200 ms | probe `lat` |
| Smoke `quit_ms` for `closeAll()` with 3 busy sessions | 1 - 5 ms (limit asserted 1000 ms) | `zig build smoke` |
| Idle CPU of the real GUI window (startup chooser open, no input, ReleaseFast, Windows) | 31.2 ms CPU over 8.0 s wall = **0.39 % of one core**; working set 45 MB | PowerShell `TotalProcessorTime` delta, process started and killed by the researcher |
| Smoke idle loop | 2 - 5 frames in 1.3 s with a finished tab and no input (assert <= 10) | `smoke extras: ... idle_frames=` |
| Per-frame cost of redrawing a finished scene every frame, 1024x768 (k=1) / 2048x1536 (k=2) | ImGui tessellation 0.34 - 1.47 ms (k=1), 0.49 - 1.54 ms (k=2); vertices 16.6k - 30.1k (k=1), 24.6k - 44.8k (k=2); **software** raster (CI/smoke path only) 4.9 - 13.1 ms (k=1), 7.9 - 18.7 ms (k=2). Hardware renderers (D3D11/Metal) do not pay the software raster | probe `frame` |
| Lines vs expectation (MFC defaults) | `received == expected` for all 9 (17, 32, 48, 33, 34, 64, 34, 34, 34). Note Dipole: `received=17` but `scene.electric=16` (sink line is counted by `poll()` but not kept), so the overlay clamps n to m | probe `lines` |
| Windows DPI | after `SDL_Init(SDL_INIT_VIDEO)` with the real driver the process is per-monitor-v2 aware (`awareness=2 perMonitorV2=1`) without any manifest; this machine's 4 displays all report `content_scale=1.000` (so 1x only) | probe `dpi` |
| Exe | PE subsystem 2 (GUI), ReleaseFast size 4.9 MB, icon extractable | PowerShell check |

## Parity evidence (`--screenshot` vs `ef-cli` vs MFC)

`ElectricField.exe --screenshot <slug> out.png --size 1024x820` renders the real UI (software renderer) with one finished tab, then `SDL_SavePNG`s the window; the canvas is rows 52..819 (menu bar 24 + tab bar 28 at k=1). Cropping `1024x768+0+52` gives the canvas. Same metric definitions as `tools/parity/compare-mfc.sh` (ink = darker than 97 % grey; recall/precision with 1 px dilation; darkness ratio = port/MFC total ink), computed against `docs/baseline/mfc/<slug>.png`:

| preset | source | AE (fuzz 10 %) | ink recall | ink precision | darkness ratio |
|--------|--------|---------------|-----------|---------------|----------------|
| dipole | GUI canvas / ef-cli | 6380 / 6375 | 0.988 / 0.988 | 0.995 / 0.995 | 0.997 / 0.997 |
| two-equal-charges | GUI / ef-cli | 8055 / 8056 | 0.990 / 0.990 | 0.995 / 0.995 | 1.010 / 1.009 |
| two-unequal-charges-same-sign | GUI / ef-cli | 11303 / 11308 | 0.991 / 0.991 | 0.995 / 0.995 | 0.996 / 0.996 |
| two-unequal-charges-different-signs | GUI / ef-cli | 8993 / 8984 | 0.991 / 0.991 | 0.996 / 0.996 | 1.005 / 1.005 |
| quadrupole | GUI / ef-cli | 8185 / 8184 | 0.992 / 0.992 | 0.996 / 0.996 | 0.998 / 0.998 |
| four-charges-same-sign | GUI / ef-cli | 14748 / 14749 | 0.992 / 0.992 | 0.996 / 0.997 | 0.999 / 0.999 |
| two-dipoles-same-direction | GUI / ef-cli | 7042 / 7042 | 0.993 / 0.993 | 0.996 / 0.996 | 1.002 / 1.002 |
| two-dipoles-opposed-direction | GUI / ef-cli | 8824 / 8825 | 0.992 / 0.992 | 0.996 / 0.996 | 0.998 / 0.998 |
| parallel-dipoles | GUI / ef-cli | 9911 / 9908 | 0.992 / 0.992 | 0.996 / 0.996 | 1.004 / 1.004 |

GUI canvas vs `ef-cli` PNG directly: ImageMagick `compare -metric AE -fuzz 10%` reports 0 - 32 for all 9 presets (out of 786,432 pixels). The smoke's own check (`diff_px`, differ by > 24 in any channel) is 0 / 28 / 50 pixels at 1280x1024 / 1024x820 / 640x480, threshold `cw*ch/1000`. Conclusion: the GUI canvas is the `ef-cli` picture, and `ef-cli` is MFC-equivalent at the Phase 2 tolerance (informational, not a gate; D2D and ImGui anti-alias differently). This is software-renderer evidence; a GPU renderer rasterises the same ImGui vertices (fringe AA with `AntiAliasedLinesUseTex = false`) and may differ by sub-pixel AA only. Visual UI evidence kept in `research-ref/shots2/`: `gui-dipole.png`, `cli-dipole.png`, `ui-about-k1.png` (About, licence line wraps to `v3.0.`), `ui-about-k2.png`, `ui-chooser-k2.png` (`--scale 2 --size 2048x1648`: canvas rect `(0,104)-(2048,1648)`, i.e. chrome and 1 px lines scaled by k and 100*k px/unit).

## Smoke sequence and canvas-clipping checks (implemented in `App::runSmoke` / `smokeExtras` / `checkCanvas`)

Order (all with the dummy video driver + software renderer; `KeyRepeatDelay` set to 1e9 first, see Pitfall 3):

1. `video=dummy renderer=software presets=9`.
2. Open "Four Charges Same Sign", close it within 2 frames (cancel path). Open "Dipole", `pumpUntilDone` (60 s timeout), 2 frames, assert the scene has field lines (`lines=16`).
3. **Canvas checks** at window sizes 640x480, 1024x820, 1280x1024 (`SDL_SetWindowSize` + `SDL_SyncWindow` + 4 frames): (a) canvas rectangle vs `efrender::renderSurface` of the same scene with the same `k`: pixels differing by > 24 in any channel `<= cw*ch/1000`; (b) **clipping backstop:** no dark scene ink in the chrome strip above the canvas to the right of x = 400*k (right of the tab text) -> `chrome_bleed_px == 0`; (c) blue and red charge pixels each > 1000 (origin and charges visible; observed 1913 / 1916). Observed: 640x480 -> canvas 640x428 at (0,52); 1024x820 -> 1024x768; 1280x1024 -> 1280x972.
4. Close the tab; empty-state frame; About opened by request and closed with Esc via the input queue; chooser likewise. Assert `tabs_opened == 2` and `closed == 2` (UI-SPEC).
5. `smokeExtras`: (i) keyboard contract: Ctrl+N opens chooser, row 1 preselected, Down -> row 2, Enter opens "Two Equal Charges 1", Ctrl+W closes it; (ii) Esc closes the chooser without opening; Ctrl+N is ignored while About is open; Esc closes About; (iii) middle-click on the tab closes it; (iv) failed surface: factory that throws -> `Failed` tab, failure copy drawn (`ui_.failedShown`); (v) busy overlay at the minimum window size stays inside the canvas rect (`EndlessSession` fake that never finishes); (vi) idle loop <= 10 frames in 1.3 s; (vii) quit with 3 busy sessions `closeAll()` <= 1000 ms; (viii) `opened - openedBase == closed - closedBase` (no tab leak).
6. `ws_.closeAll(); shutdown();` and the final line `smoke ok frames=<N> tabs_opened=2 lines=16 closed=2`, exit 0. No `imgui.ini` is written.

Key injection (`App::chord`): modifiers and key are fed through `io.AddKeyEvent` with settle frames between press/release; on macOS the logical Ctrl of `Shortcut(ImGuiMod_Ctrl|...)` is the physical `ImGuiMod_Super` (ImGui swaps Ctrl/Super inside `AddKeyEvent` when `ConfigMacOSXBehaviors`), handled in the helper. Not yet executed on macOS.

## Delta vs current repo (branch `feature/cross-platform`, HEAD `fcfb844`)

Computed by diffing the prototype tree against the repo working tree this session. `research-ref/` also holds unchanged copies of existing files (core, tests, tools, LICENSE, README, `build.zig.zon`) only so it can be built standalone; do not copy those.

**NEW files to add (copy from `research-ref/`):**
| Path | Purpose |
|------|---------|
| `src/app/workspace.hpp`, `src/app/workspace.cpp` | tab model, `ISession` seam |
| `src/app/ui_logic.hpp`, `src/app/ui_logic.cpp` | pure UI logic + `FramePacer` |
| `src/gui/app.hpp`, `src/gui/app.cpp` | App loop, DPI, smoke, screenshot |
| `src/gui/ui.hpp`, `src/gui/ui.cpp` | widgets |
| `src/gui/icon.hpp`, `src/gui/icon.cpp` | `#embed` icon |
| `src/imgui_config/ef_imconfig.h` | non-interactive `IM_ASSERT` |
| `tests/test_app.cpp` | efapp doctests (9 cases) |
| `tests_gui/test_main.cpp`, `tests_gui/test_render_ctx.cpp` | render-context regression tests (4 cases) |
| `assets/ElectricField.ico`, `assets/ElectricField.rc`, `assets/ElectricField-256.png`, `assets/ElectricField-96.png` | icon assets copied out of the MFC tree (D-14) |

**MODIFIED files (take the prototype version; changes verified by diff):**
| Path | Change |
|------|--------|
| `src/gui/main.cpp` | hello window -> real entry (`--smoke`, `--screenshot`, fatal box, quiet crash dialogs) |
| `src/render/png_export.cpp`, `src/render/png_export.hpp` | `renderSurface()` + the context rule; `renderPng` delegates |
| `build.zig` | `addApp` (efapp lib), `addRender` (efrender lib, now also used by `addCli`), `addRenderTests` (`test-render`, wired into `test`), `addGui` (app/ui/icon sources, build-info header, `.rc`, links efrender/efapp), `gui_flags` gets `-DIMGUI_USER_CONFIG="ef_imconfig.h"`, `imgui` lib and render/tests get `src/imgui_config` include path, `addTests` links efapp + `src` include, `test_app.cpp` added to `test_sources`, `run` step text |

**Recommended edits not present in the prototype (planner decides, see CI section):**
- `.github/workflows/ci.yml`: add a GUI-enabled full test step.
- `build.zig.zon` `.paths`: add `"assets"` and `"tests_gui"` (only matters when the repo is consumed as a Zig package; harmless otherwise).

**Do NOT copy into the product tree (research-only):**
- `research-ref/probes/` (`dpi.cpp`, `lat.cpp`, `frame.cpp`, `lines.cpp`) and the `addProbe(...)` function plus the `for (...) addProbe` loop in `build.zig` (dev measurements; `zig build probe-<name>`). Keep the file only if the team wants the latency probe as a dev tool; if kept, move it under `tools/` and leave it out of default builds.
- `research-ref/shots2/` (PNG evidence; ~1.1 MB of binaries).
- Unchanged duplicates in `research-ref/` (`src/core`, `src/cli`, other `tests/*`, `tools/`, `LICENSE`, `README.md`, `build.zig.zon`, `src/render/imgui_painter.hpp`).
- Debug leftovers (already removed this session): `navdbg.txt`, `_embedtest/`, `shots/`, the `EF_FPS` environment-variable tracing that was inside `handleEvent`/`frame`.

Prototype state notes: `research-ref/` was synchronised from the live prototype after this session's changes: `App::runSmoke` now sets `ImGui::GetIO().KeyRepeatDelay = 1e9f;` (the chooser-keyboard fix), `EF_FPS` tracing removed, `png_export.cpp` comment line reference corrected to `imgui.cpp:4258-4259`, `app.cpp` normalised to LF.

## CI changes (`.github/workflows/ci.yml`)

Current CI already runs `zig build smoke -Doptimize=ReleaseFast` on windows/ubuntu/macos-arm64 after "Build everything", and the parity step. Required/recommended:

1. **Keep** `Core tests (Debug + UBSan)` and `(ReleaseFast)` with `-Dgui=false`: they now also run `tests/test_app.cpp` (efapp links no SDL/ImGui) — verified `zig build test -Dgui=false` and `-Dgui=false -Doptimize=ReleaseFast`: 69/69 pass.
2. **Add** `zig build test -Doptimize=ReleaseFast --summary all` after "Build everything" (GUI-enabled): runs the 4 render-context tests on all three OSes (software renderer + dummy driver; no display needed).
3. **Keep** the smoke step (now long: ~170 frames, a few seconds) — it is the real-UI regression on every OS. No extra Linux packages are needed (dummy driver; X11/Wayland are `dlopen`ed).
4. The TSan Linux job (`-Dgui=false -Dsanitize=thread`) builds `efapp` with `sanitize_thread` too (coreModule) and runs the workspace tests under TSan — no change needed; watch the first run since `test_app.cpp` adds threaded cases.
5. Optional: after smoke, upload a `--screenshot dipole` PNG per OS as an artifact (visual evidence of the real UI on each OS). Not required.
6. Optional hardening for VIEW-05 on Windows only: a bash step reading the PE optional-header Subsystem field of `zig-out/bin/ElectricField.exe` and failing unless it is 2.
7. Never pass `-Dtarget` on the macOS job (unchanged rule).

## Common Pitfalls

### Pitfall 1: `renderPng` inside the running GUI
**What goes wrong:** assert "Already initialized a renderer backend!" (exit code 3 with `ef_imconfig.h`), or silent corruption of the GUI's ImGui state without it.
**Why:** `CreateContext()` only makes the new context current if none exists (quoted above).
**How to avoid:** the context rule (create, guard, then explicit `SetCurrentContext`). Keep `tests_gui/test_render_ctx.cpp` in `zig build test`.
**Warning signs:** any new code calling `ImGui::CreateContext()` while the GUI context is live.

### Pitfall 2: `IMGUI_USER_CONFIG` / `ImDrawIdx` not reaching every TU
**What goes wrong:** ODR/ABI mismatch (different `IM_ASSERT` or index width between the imgui library and its users). `-DImDrawIdx=unsigned int` is needed because field-line meshes exceed 64k vertices.
**How to avoid:** both defines live in `gui_flags`, and every module that includes `imgui.h` (imgui lib, efrender, gui, tests_gui, probes) must use `gui_flags` and the `src/imgui_config` include path. `ef-cli`/core/app do not include `imgui.h`.

### Pitfall 3: Injected key chords repeat when frames are slow
**What goes wrong:** smoke failed at "Down did not move the chooser selection to row 2 (sel=3)". Software frames of a finished 1024x820 scene take ~100 ms, so a key held for 3 injected frames outlives ImGui's key-repeat delay (0.275 s) and the chooser moved 0 -> 1 -> 2 -> 3.
**How to avoid:** `ImGui::GetIO().KeyRepeatDelay = 1e9f;` at the start of `runSmoke` (single press per chord regardless of frame time). Verified: smoke green 6 consecutive runs (Debug and ReleaseFast).
**Warning signs:** a keyboard check passing locally (fast frames) but failing in CI or Debug.

### Pitfall 4: `BeginListBox` as the chooser container
**What goes wrong:** the list box child is a separate nav scope; Up/Down do not reach the rows until Enter enters it.
**How to avoid (prototype):** a plain framed child `BeginChild("##presets", ..., ImGuiChildFlags_FrameStyle | ImGuiChildFlags_NavFlattened, NoScrollbar|NoScrollWithMouse)` with `Selectable` rows, `SetItemDefaultFocus()` on the preselected row, selection following keyboard focus; the "Open Configuration" button is deliberately NOT the default-focused item so Up/Down work immediately (Enter on a focused row opens it). See Open Question 3.

### Pitfall 5: Tab/selection handling
- Stable id via `"<title>###ef<serial>"` so the changing busy glyph does not change the id or lose selection.
- New tab becomes active via `ImGuiTabItemFlags_SetSelected` on the new serial for exactly one frame (`selectSerial` reset after the bar) in addition to `AutoSelectNewTabs`.
- Open requests are applied before the tab bar is submitted so the tab shows in the same frame; close requests (x, middle-click, Ctrl+W, menu) are collected and applied after `EndTabBar`.
- Ctrl+W/Ctrl+N/Ctrl+Q are ignored while a modal is open (`chooserVisible || aboutVisible || openChooser || openAbout`).

### Pitfall 6: Zig 0.17 caches the configure phase, so a configure-time git hash goes stale
**What goes wrong:** `b.runFallible({"git","rev-parse","--short=7","HEAD"})` runs only when `build.zig`/options change. **Verified in a scratch project this session:** after a new commit, `zig build` still emitted the old hash (`#define EF_GIT_HASH "003738d"` while HEAD was `ef8bd5a`).
**What works (verified):** a make-time step `b.addSystemCommand(&.{"git","rev-parse","--short=7","HEAD"})` with `run.has_side_effects = true` and `run.captureStdOut(.{ .basename = "githash.txt", .trim_whitespace = .all })` re-ran every build and installed the new hash (`38387a2` == HEAD). **But** with no git repository the Run step fails the whole build (`run git failure`, exit 1), so it must be used only when git is known to work at configure time. See Open Question 1.
**Impact in the prototype:** cosmetic (About shows the hash of the last reconfigure); in CI the checkout is fresh so the hash is right.

### Pitfall 7: HiDPI double scaling
Use the display CONTENT scale only for `k` (see Anti-patterns). Scale styles with `ScaleAllSizes(k)` from a pristine base style every time; never scale an already scaled style. `CurveTessellationTol` and AA flags are not scaled.

### Pitfall 8: Overlay count vs expectation
`poll()` also reports sink lines, so `linesReceived` can exceed `expectedLineCount` (Dipole 17 received vs 16 kept). `overlayText` clamps n to m. Do not "fix" it by changing the core.

### Pitfall 9: Modal fade needs frames
ImGui fades the modal dim layer in/out over ~0.2 s. The idle loop must keep rendering `kModalFadeFrames = 12` frames after a modal opens/closes, and screenshot mode renders 14 frames before reading pixels. Otherwise a half-faded or stuck dim layer remains until the next input.

### Pitfall 10: Headless modes must never show dialogs
`--smoke`/`--screenshot` skip `SDL_ShowSimpleMessageBox` and call `quietCrashDialogs()` (Windows error mode, abort behavior, CRT error mode); `IM_ASSERT` is already dialog-free. An unattended CI job or developer shell must not hang on a modal.

### Pitfall 11: Mixed line endings in new source files
The prototype's `app.cpp` had mixed LF/CRLF after edits; normalise new files to LF before committing (the repo's existing sources are LF in the working tree; git warns on CRLF conversion on this Windows machine).

### Pitfall 12: `test_presets.cpp` reads the MFC tree
`tests/test_presets.cpp` opens `ElectricField/ConfigDocs.cpp` relative to the repo root (`run.setCwd(b.path("."))` in `build.zig`). Unchanged in Phase 3 and passing; it will break when Phase 4 deletes the MFC tree (BUILD-06). Flag for the Phase 4 plan, not Phase 3.

## Code Examples

### Base style + scale (prototype `src/gui/ui.cpp`)
```cpp
ImGuiStyle makeBaseStyle() {
  ImGuiStyle s;
  ImGui::StyleColorsLight(&s);
  s.FontSizeBase = 16.f;
  s.WindowPadding = ImVec2(8, 8);  s.FramePadding = ImVec2(8, 4);  s.ItemSpacing = ImVec2(8, 4);
  s.ItemInnerSpacing = ImVec2(4, 4);  s.CellPadding = ImVec2(4, 4);  s.ScrollbarSize = 16;  s.IndentSpacing = 16;
  s.WindowBorderSize = 0;  s.PopupBorderSize = 1;  s.FrameBorderSize = 0;  s.TabBarBorderSize = 1;  s.TabBarOverlineSize = 0;
  s.WindowRounding = 0;  s.FrameRounding = 4;  s.PopupRounding = 4;  s.TabRounding = 4;
  s.CurveTessellationTol = 0.3f;  s.AntiAliasedLinesUseTex = false;
  s.Colors[ImGuiCol_TextDisabled] = ImVec4(0x59 / 255.f, 0x59 / 255.f, 0x59 / 255.f, 1.f);
  s.Colors[ImGuiCol_TextLink] = ImVec4(0.f, 0x54 / 255.f, 0xCC / 255.f, 1.f);
  s.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.f, 0.f, 0.f, 0.25f);
  return s;
}
void applyScale(const ImGuiStyle& base, float k) { ImGuiStyle& s = ImGui::GetStyle(); s = base; s.ScaleAllSizes(k); s.FontScaleDpi = k; }
```

### Frame (prototype `App::frame`)
```cpp
ws_.pollAll();
if (scaleDirty_) refreshScale();
ImGui_ImplSDLRenderer3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
drawUi(ws_, ui_, env_);
ImGui::Render();
if (ui_.wantedTitle != lastTitle_) { SDL_SetWindowTitle(window_, ui_.wantedTitle.c_str()); lastTitle_ = ui_.wantedTitle; }
SDL_SetRenderDrawColor(renderer_, 240, 240, 240, 255); SDL_RenderClear(renderer_);
ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
SDL_RenderPresent(renderer_);
```

### Zig 0.17 build pieces (prototype `build.zig`)
```zig
const gui_flags = [_][]const u8{ "-std=c++17", "-DImDrawIdx=unsigned int", "-DIMGUI_USER_CONFIG=\"ef_imconfig.h\"" };
...
mod.addCSourceFiles(.{ .root = b.path("src/gui"), .files = &.{ "main.cpp", "app.cpp", "ui.cpp" }, .flags = &gui_flags });
mod.addCSourceFiles(.{ .root = b.path("src/gui"), .files = &.{"icon.cpp"}, .flags = &(gui_flags ++ [_][]const u8{"-Wno-c23-extensions"}) });
mod.addConfigHeader(info);
mod.addWin32ResourceFile(.{ .file = b.path("assets/ElectricField.rc") }); // exe icon; ignored off Windows
const exe = b.addExecutable(.{ .name = "ElectricField", .root_module = mod });
exe.subsystem = .windows; // GUI subsystem on Windows; ignored elsewhere
run.addPassthruArgs();    // 0.17: replaces b.args
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ImGui bitmap font + manual atlas rebuild for DPI | `AddFontDefaultVector()` + `FontScaleDpi` + `PushFont(nullptr, size)` | ImGui 1.92 | no font assets, crisp at any k |
| `b.args` in build.zig | `run.addPassthruArgs()` | Zig 0.17 | already used |
| `@cImport` | not used (C++ includes SDL/ImGui headers normally) | Zig 0.17 | n/a |
| embedding assets with generators | C23 `#embed` in Clang 22 | Clang 19+ | `-Wno-c23-extensions` for that TU only |
| MFC `CMDIChildWnd` tabs, `BeginWaitCursor`, WM_TIMER 100 ms polling | ImGui tab bar, `ImGuiMouseCursor_Wait`, per-frame `poll()` | this port | see D-01/D-10 |

**Deprecated/outdated:** `allyourcodebase/SDL3` (breaks on Windows without symlink privilege), `imgui_impl_opengl3/win32/dx11`, ImGui multi-viewport (project rules).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Linux and macOS behave like Windows for the prototype (dummy driver + software renderer smoke, key injection incl. the macOS Ctrl<->Super swap, `SDL_GetDisplayContentScale`, `#embed`, `SDL_main`) | Exact APIs, Smoke | First CI run on ubuntu/macos may fail a smoke sub-check; fix is local to `app.cpp` |
| A2 | Changing `assets/*.png` retriggers compilation of `icon.cpp` (Clang depfile includes `#embed` files under Zig's cache) | Exact APIs | Stale icon after an asset edit until `.zig-cache` is cleared; cosmetic |
| A3 | `ConfigMacOSXBehaviors` makes `ImGuiMod_Ctrl` map to Cmd and swaps Ctrl/Super in `AddKeyEvent` (from a code comment in the prototype and ImGui docs, not exercised on macOS) | Smoke | macOS smoke key checks fail; GUI shortcut labels read "Cmd+" via `shortcutLabel(.., io.ConfigMacOSXBehaviors)` |
| A4 | A hybrid git-hash approach (configure-time `runFallible` probe, then a make-time `has_side_effects` Run step only if git works, embedded via a generated file/header) is implementable on Zig 0.17 | Pitfall 6 | The two halves were verified separately; the combination was not built |
| A5 | GPU renderers (D3D11/Metal/OpenGL/Vulkan chosen by `SDL_CreateRenderer`) draw the same ImGui vertices with AA differences below the visible threshold | Parity | Visual drift vs the software evidence; resolved by the manual real-window check |
| A6 | Real displays at 125/150/200 % report `SDL_GetDisplayContentScale` of 1.25/1.5/2.0 on Windows and 1.0 on macOS (UI-SPEC premise); this machine only has 100 % displays | HiDPI | Wrong `k` on a real HiDPI display; manual-only check |

## Open Questions (RESOLVED)

Resolved by the autonomous orchestrator (auto mode) on 2026-10-11: each Recommendation below is adopted as the decision (Q1: hybrid make-time git hash with fallback; Q3: chooser keyboard focus deviation accepted, record in SUMMARY).

1. **Git hash freshness in the About box.**
   - What we know: the prototype's configure-time hash goes stale after new commits (verified); a make-time `has_side_effects` Run step stays fresh but fails the build when git is absent (verified).
   - What's unclear: whether the added complexity is warranted for a cosmetic field.
   - **RESOLVED (recommendation adopted):** implement the hybrid: at configure time probe git with `runFallible`; if it succeeds, wire a `has_side_effects` Run step capturing `git rev-parse --short=7 HEAD` into a generated `githash.txt` that `ui`/`app` embed (or pass through a generated header step), else use `""` (shown as "(unknown)"). If the planner wants the smallest change, ship the prototype as-is and document "clear `.zig-cache` for a fresh hash". Either way keep `versionLine("", ...)` -> "(unknown)" covered by `tests/test_app.cpp`.

2. **Fatal-startup message box coverage.**
   - What we know: with `SDL_VIDEODRIVER=nosuch`, `SDL_Init` fails and `SDL_ShowSimpleMessageBox` returned false with "No message system available" (probe run this session), so nothing was displayed; the process exits 1 and the text goes to stderr, which a GUI-subsystem exe does not show.
   - What's unclear: whether a real-world failure on Windows (practically unreachable) or Linux without a display shows a box.
   - **RESOLVED (recommendation adopted):** keep the UI-SPEC behaviour (stderr + box), treat the bogus-driver case as expected, and add a manual UAT item "start with `SDL_VIDEODRIVER=nosuch` / without DISPLAY and confirm the exit code is 1 with the text on stderr". Do not add Win32 `MessageBox` fallbacks in this phase.

3. **Chooser default button vs UI-SPEC.**
   - What we know: UI-SPEC lists "Open Configuration" as the default button; the prototype leaves initial focus on the preselected list row so Up/Down work immediately, and Enter on a focused row opens it (Enter = Open Configuration still holds; smoke proves it).
   - What's unclear: whether the checker expects the button to look focused on open.
   - **RESOLVED (recommendation adopted):** keep the prototype behaviour (keyboard contract wins) and record it as an accepted deviation in the plan's summary; the button still has the accent colour when hovered/tabbed to.

4. **Keep the extra `--screenshot/--size/--scale/--modal` flags in the shipped GUI?**
   - What we know: they are not in the UI-SPEC but power the parity evidence and the HiDPI-by-`--scale` check, and Phase 4's export validation can reuse them; they write only the path the developer passes.
   - **RESOLVED (recommendation adopted):** keep them as an undocumented developer/CI flag set (mention in the code header comment), keep out of the README until Phase 4, and keep `--smoke` as the only CI-gated mode.

5. **First Linux/macOS run risk.**
   - What we know: nothing of the GUI was run off Windows.
   - **RESOLVED (recommendation adopted):** make "CI green on all 3 OSes for the Phase 3 commits" an explicit plan task with a checkpoint; if the macOS smoke key checks fail, first suspect the `chord()` Super/Ctrl swap, then window-size sync (`SDL_SyncWindow` with the dummy driver).

6. **HiDPI and macOS human verification.**
   - What we know: only `--scale 2` headless evidence exists (`shots2/ui-*-k2.png`, canvas rect doubled); all four local displays are 100 %.
   - **RESOLVED (recommendation adopted):** route to the end-of-phase human verification (config `human_verify_mode: end-of-phase`): run the app on a 125/150 % or 200 % Windows display and on a Retina Mac; check sharp 1 px lines, canvas same physical size as at 100 %, window moved between monitors rescales. Do not block automated gates on it.

7. **`build.zig.zon` `.paths`.**
   - **RESOLVED (recommendation adopted):** add `"assets"` and `"tests_gui"` for completeness; no functional effect for in-tree builds.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Zig | everything | ✓ | 0.17.0 | — |
| git | build-info hash, repo | ✓ | 2.56.0 (scoop) | hash "unknown" |
| ImageMagick `magick` | informational parity metrics (`tools/parity/compare-mfc.sh`, this research) | ✓ | scoop `imagemagick` | skip (not a gate) |
| PowerShell | idle-CPU / PE-subsystem checks (this research) | ✓ | Windows 11 | `od` / `objdump` |
| Display (real window) | manual checks only | ✓ (4 monitors, all 100 %) | — | headless dummy driver for everything automated |
| HiDPI display | HiDPI manual check | ✗ | — | `--scale 2` headless evidence; manual on real hardware before sign-off |
| macOS host | macOS manual/CI check | ✗ (CI `macos-latest` only) | — | CI |
| Linux host | Linux CI | ✗ locally (CI `ubuntu-latest`) | — | CI |

**Missing dependencies with no fallback:** none for automated gates.
**Missing with fallback:** HiDPI display and macOS/Linux hosts (CI and human verification).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest 2.5.3 (single header, hash-pinned) + the app's own headless `--smoke` harness |
| Config file | none (doctest macros in sources); `build.zig` steps `test`, `test-render`, `smoke` |
| Quick run command | `zig build test -Dgui=false` (core + efapp, no SDL/ImGui fetched; ~seconds when cached) |
| Full suite command | `zig build test && zig build smoke -Doptimize=ReleaseFast` (adds the 4 render-context tests and the full UI smoke) |
| CI | existing matrix (windows/ubuntu/macos-arm64) + TSan Linux; see CI changes |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| VIEW-01 | 9 presets open in own tabs with titles "<Name> <N>", never-reused counters; chooser keyboard path (Ctrl+N, Down, Enter) opens "Two Equal Charges 1" | unit + smoke | `zig build test -Dgui=false` (case "tab titles use a per-preset counter that is never reused"); `zig build smoke` (extras `keys=ok`) | prototype ✅ (`tests/test_app.cpp`, `app.cpp`) -> new in repo |
| VIEW-01 | charge positions/magnitudes identical to MFC | unit (Phase 2) | `zig build test -Dgui=false` (`tests/test_presets.cpp`) + parity `ef-cli --all --expect tools/parity/expected-counts.csv` | ✅ existing |
| VIEW-01 | File > New submenu click opens the preset | manual-only | real window | menu click is not injected by smoke (only the `openPresetNow` path); manual |
| VIEW-02 | several tabs, close any incl. busy; quit with busy sessions | unit + smoke | case "closing a computing tab and closeAll() return promptly and leave no session"; smoke (`middle_click=ok`, first 2-frame open/close, `quit_ms`, `opened == closed`) | ✅ prototype |
| VIEW-02 | cancel/destroy latency <= 200 ms | probe (dev) | `zig build probe-lat -Doptimize=ReleaseFast` (worst 1.8 ms); contract also asserted by Phase 2 `test_session.cpp` | probe not shipped; Phase 2 test ✅ |
| VIEW-03 | canvas = shared render path, y-up centred, clipping at 640x480 / 1024x820 / 1280x1024, charges visible | smoke | `zig build smoke` (`canvas check ... ok`) | ✅ prototype |
| VIEW-03 | `renderScene` geometry/colours | unit (Phase 2) | `zig build test -Dgui=false` (`tests/test_scene.cpp`) | ✅ existing |
| VIEW-03 | no ImGui context corruption by offscreen render inside the GUI | unit | `zig build test-render` | ✅ prototype (`tests_gui/`) |
| VIEW-03 | scale/metrics: `clampScale`, window sizes, `canvasView`, `scaledStyle` | unit | case "display scale and window metrics", "canvas view: ..." | ✅ prototype |
| VIEW-03 | visual match to MFC baselines | informational | `tools/parity/compare-mfc.sh` / `--screenshot` crops (table above); not a CI gate | ✅ existing script |
| VIEW-03 | sharp on a real HiDPI display | manual-only | real 125/150/200 % display, Retina Mac | — |
| VIEW-04 | tab marker glyph, stable id, overlay text clamping, new tab shows charges first then cached scene | unit | cases "busy glyph ...", "tab label keeps a stable id ...", "overlay text clamps n to m", "a new tab shows its charges at once ..." | ✅ prototype |
| VIEW-04 | overlay inside canvas at minimum size; failed tab surface; idle loop does not spin | smoke | `zig build smoke` (`overlay=ok failed_tab=ok idle_frames=`) | ✅ prototype |
| VIEW-04 | wait cursor over a busy canvas | manual-only | real window: hover during compute | — |
| VIEW-05 | GUI subsystem exe (no console), icon | automated (optional) / manual | PE subsystem byte == 2 (CI bash step, recommended); double-click the exe, no console appears | not yet a CI step |
| VIEW-05 | About modal renders; version line; links | smoke + unit | smoke renders About; case "window title, version line, ..."; link click manual | ✅ partial |
| D-12 | no busy loop while idle | smoke (`idle_frames <= 10`) + measured 0.39 % CPU | `zig build smoke`; manual CPU check optional | ✅ |
| D-17 | `ef-cli`/parity CI not broken | integration | CI parity step (`ef-cli --all --out-dir parity-out --expect ...`) | ✅ existing |

### Sampling Rate
- **Per task commit:** `zig build test -Dgui=false` (seconds) — and `zig build smoke` for any `src/gui`, `src/render`, `build.zig` change.
- **Per wave merge:** `zig build test` + `zig build smoke -Doptimize=ReleaseFast` (both Debug and ReleaseFast for the core tests as CI does).
- **Phase gate:** CI green on windows/ubuntu/macos-arm64 (including TSan Linux and parity), then the manual checks below, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] Add unit tests for `efapp::FramePacer` (block/poll decisions, settle/modal-fade counters) — currently only exercised indirectly by smoke.
- [ ] Add `Workspace` tests with scripted fake `ISession`s for the Failed / cancelled / "no result" paths (`setFactory` seam; currently only the throwing-factory path is hit by smoke).
- [ ] Optional CI step: PE subsystem == 2 check for VIEW-05.
- Framework install: none (doctest already pinned).

### Manual-only items (end-of-phase human verification)
| Item | Why manual |
|------|-----------|
| **Real-window interactive check** on Windows: launch `zig-out/bin/ElectricField.exe` by double-click (no console), chooser at startup, File > New > each of the 9, several tabs, close a computing tab, middle-click close, Ctrl+N/W/Q, hover wait cursor over a busy canvas, About links open the browser, window title "Electric Field — <Name> <N>", resize clips with origin centred | needs a real window and pointer; the user confirmed the prototype works when run |
| **Real HiDPI display** (Windows 125/150/200 %, move window between monitors) | no HiDPI hardware available to automation; local displays are all 100 % |
| **macOS real window** (Retina sharpness, Cmd shortcuts and "Cmd+N" labels, Dock icon from the bundle, Gatekeeper/startup) | macOS cannot be cross-built; only the CI headless run exists |
| Linux real window (X11 and Wayland) | CI is headless |
| Fatal-startup behaviour (`SDL_VIDEODRIVER=nosuch`: exit 1, text on stderr) | message box is not testable headlessly (Open Question 2) |

## Security Domain

`security_enforcement` is enabled (ASVS level 1, block on high). This phase is a local single-user desktop shell: no network, no authentication, no stored secrets, no persistence.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — (local app) |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | yes (small) | command-line flags only (`--smoke`, `--screenshot <slug> <png> [--size WxH] [--scale k] [--modal ...]`): slug is looked up in the compiled-in preset table (unknown -> exit 2); `--scale` is clamped by `clampScale`; `--size` parsed with `sscanf`; the output path is the developer's own. URLs opened by `SDL_OpenURL` are three compile-time constants (no user-controlled URL) |
| V6 Cryptography | no | — (no crypto; the git hash is display only) |
| V14 Configuration | yes | hash-pinned third-party deps (unchanged); no `imgui.ini` written (`IniFilename = nullptr`); `IM_ASSERT` non-interactive and active in every build mode |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Opening an attacker-controlled URL from the UI | Tampering | only fixed constant URLs; on failure copy to clipboard instead of invoking anything else |
| Arbitrary file write via `--screenshot` path | Tampering | developer-only CLI flag, writes exactly the given path with `SDL_SavePNG`; no UI path writes files in this phase (Phase 4 export will need its own path validation; note review IN-06 about ANSI vs UTF-8 paths on Windows) |
| UI-thread hang on tab close / quit (availability) | Denial of service | cancel-all-then-destroy, measured <= 13 ms for 27 sessions; contract 200 ms |
| Use-after-free of tab/session while iterating tabs | Tampering/Elevation | closes are deferred until after `EndTabBar`; sessions destroyed before SDL/ImGui teardown; verified by smoke tab-leak check and TSan CI job over the efapp tests |
| Silent memory corruption from compiled-out asserts | Tampering | `ef_imconfig.h` keeps `IM_ASSERT` active (stderr + `_Exit(3)`) in ReleaseFast |

## Sources

### Primary (HIGH confidence)
- Prototype and repo files read this session: `src/app/*`, `src/gui/*`, `src/render/png_export.*`, `build.zig`, `build.zig.zon`, `assets/ElectricField.rc`, `tests/test_app.cpp`, `tests_gui/test_render_ctx.cpp`, `src/imgui_config/ef_imconfig.h`, `.github/workflows/ci.yml`, `src/core/ef/{presets,scene,session,params}.*`, `.planning/phases/03-gui-shell/03-CONTEXT.md`, `03-UI-SPEC.md`, `.planning/REQUIREMENTS.md`, `.planning/phases/02-compute-engine-headless-parity/02-REVIEW.md`, `.claude/CLAUDE.md`.
- Pinned ImGui tarball in `zig-pkg/`: `imgui.cpp:4252-4261`, `backends/imgui_impl_sdlrenderer3.cpp:302`, `backends/imgui_impl_sdl3.cpp:632`, `imgui.h` (`FontScaleDpi`, `AddFontDefaultVector`).
- Pinned SDL headers in `zig-pkg/sdl-0.6.0+3.4.18-...`: `SDL_misc.h:72` (`SDL_OpenURL`), `SDL_events.h:1364` (`SDL_WaitEventTimeout`), `SDL_render.h:2784` (`SDL_SetRenderVSync`), `SDL_video.h:805` (`SDL_GetDisplayContentScale`).
- Zig 0.17.0 std sources on this machine: `std/Build/Step/Run.zig` (`captureStdOut`, `has_side_effects`), `std/Build.zig` (`runFallible`).
- Commands run this session (Windows 11, Zig 0.17.0): `zig build test`, `zig build test -Dgui=false` (Debug and ReleaseFast), `zig build smoke` (Debug and ReleaseFast, 6 runs), `zig build probe-{dpi,lat,frame,lines}`, `--screenshot` renders and ImageMagick comparisons, idle CPU measurement, PE subsystem/icon check, git-hash scratch project, message-box probe, context-rule regression experiment.

### Secondary (MEDIUM confidence)
- `.claude/CLAUDE.md` Technology Stack section (spike results from earlier research) for platform claims about Linux dlopen of X11/Wayland and macOS cross-build limits.

### Tertiary (LOW confidence)
- Linux/macOS runtime behaviour of the prototype (never run there) — see Assumptions A1, A3.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — unchanged pinned dependencies, built and run this session.
- Architecture: HIGH — prototype implements it; `zig build test` + `smoke` green in Debug and ReleaseFast.
- Pitfalls: HIGH for the five reproduced ones (context rule, key repeat, stale git hash, message box with bogus driver, list-box nav scope in prototype comments); MEDIUM for Linux/macOS-specific expectations.
- Parity/measurements: HIGH on Windows with the software renderer; GPU/HiDPI/macOS pending manual verification.

**Research date:** 2026-10-10
**Valid until:** 2026-11-09 (stack pinned by hash; revisit only if Zig/ImGui/SDL pins change)
