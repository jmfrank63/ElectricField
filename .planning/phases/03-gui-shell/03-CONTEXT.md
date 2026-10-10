# Phase 3: GUI Shell - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous smart discuss (all four areas: user accepted recommended answers)

<domain>
## Phase Boundary

Delivers the interactive cross-platform application that replaces the MFC GUI: a Dear ImGui + SDL3 desktop app (GUI subsystem on Windows) where the user opens any of the 9 preset configurations from a menu, each in its own tab, sees a busy indicator while the Phase 2 `ComputeSession` computes, and then sees charges, field lines and equipotentials drawn with the shared Phase 2 `renderScene()`/`ImGuiPainter` path — y-up, centred, 100 logical px per unit, sharp on HiDPI, visually matching `docs/baseline/mfc/`. Tabs (including busy ones) close instantly; quitting mid-computation exits cleanly. An About box shows version, author credit and project/blog links.

Not in this phase: Options dialog, config persistence, PNG/SVG export from the GUI, README rewrite, MFC deletion (Phase 4); pan/zoom, progressive drawing, status bar stats (v2).

</domain>

<decisions>
## Implementation Decisions

### Window & tab layout
- **D-01:** Single main window with an ImGui tab bar: one tab per open configuration (maps MFC's tabbed MDI). No ImGui docking/multi-viewport for tabs in this phase.
- **D-02:** Opening presets: **File ▸ New ▸** submenu listing the 9 presets by their MFC display names (from the core `PresetRegistry`), plus **Ctrl+N** opening a preset chooser modal (list of 9, OK/Cancel, double-click opens) mirroring MFC's "New" dialog. The same preset may be opened multiple times; tab titles are "<Name> 1", "<Name> 2", … (MFC "Dipole1" style, with a space).
- **D-03:** On startup the preset chooser is shown (parity with MFC's startup "New" chooser); Cancel leaves an empty window with a hint ("File ▸ New or Ctrl+N to open a configuration").
- **D-04:** Menus: **File** (New ▸ 9 presets, New… Ctrl+N, Close Tab Ctrl+W, Exit Alt+F4/Ctrl+Q) and **Help** (About Electric Field…). MFC's Application Look themes, toolbar/docking customisation, status-bar toggle and print menus are dropped. (View ▸ Options arrives in Phase 4.)

### Canvas rendering
- **D-05:** The canvas uses the Phase 2 shared path: `FieldScene` + `renderScene()` + `ImGuiPainter` (same code as `ef-cli` PNG), drawn into the tab's content region; white canvas; MFC default colours (positive blue, negative red, field lines black, equipotentials grey 192, thickness 1, charge radius per Phase 2 scene); `AntiAliasedLinesUseTex = false`, `CurveTessellationTol = 0.3f` as established in Phase 2.
- **D-06:** Resize behaviour: the world origin stays centred in the canvas at a fixed 100 logical px per unit; nothing is recomputed on resize (parity). Pan/zoom is v2.
- **D-07:** HiDPI: use SDL3 display content scale / window pixel density so 100 *logical* px per unit is sharp on HiDPI (Windows per-monitor DPI awareness, macOS Retina); scale ImGui fonts/style accordingly; handle display-scale-change events.
- **D-08:** ImGui **light** style for the window chrome (closest to the MFC look); canvas is white regardless.

### Computation lifecycle
- **D-09:** Opening a tab immediately starts its own `ComputeSession` with MFC default `ComputeParams` (no options UI until Phase 4).
- **D-10:** Busy indicator: while computing, the tab title carries a spinner/marker and the canvas shows the charges plus a "Computing… n / m lines" overlay (n from `poll()`, m from `expectedLineCount`); lines are drawn all at once when the session reports done (parity; progressive drawing is v2); wait cursor while hovering a busy canvas.
- **D-11:** Closing a busy tab or quitting cancels the session(s) and closes immediately — no confirmation dialog (cancel/destroy is bounded by the Phase 2 latency contract). App quit cancels all sessions before tearing down SDL/ImGui.
- **D-12:** Main loop: vsync on; when no tab is computing and no input is pending, block in `SDL_WaitEventTimeout` (no busy loop — fixes Phase 1 review WR-02 / Phase 2 IN-03); while any tab computes, poll sessions each frame at ≤ ~30–60 fps. Check ImGui backend init results and clean up on all error paths.

### About box & app identity
- **D-13:** About modal: "Electric Field", version + short git hash (injected at build time via build options), original-author credit, links to the blog (https://compphys.go.ro/electric-field-lines/) and the GitHub repo, opened via `SDL_OpenURL`.
- **D-14:** Reuse `ElectricField/res/ElectricField.ico` imagery: window icon on all OSes (SDL_SetWindowIcon from an embedded image) and the Windows exe icon via a resource file — the icon asset must be copied out of the MFC tree (e.g. `assets/`) so it survives MFC deletion in Phase 4.
- **D-15:** Window title "Electric Field — <active tab name>" ("Electric Field" when no tab); default window size large enough for a 1024×768 canvas plus menu/tab bar (clamped to the display's usable area).

### Carry-over
- **D-16:** The GUI keeps `--smoke` (headless start + render ≥1 frame with a preset open, exit 0) so CI keeps exercising the real GUI path on all 3 OSes; extend smoke to open a preset tab, wait for its session, draw it, and close it (exercising cancel/close paths).
- **D-17:** `renderPng` must run on the UI thread between frames (Phase 2 constraint) — relevant for Phase 4 export; Phase 3 must not break `ef-cli`/parity CI.

### Claude's Discretion
- Exact module split (`src/gui/` app/tab/canvas/menu files), spinner style, chooser layout, font choice (ImGui default font scaled, or an embedded TTF if needed for HiDPI quality), how version/git hash are injected, icon conversion pipeline.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/core/ef/session.hpp/.cpp`: `ComputeSession` (RAII workers, `poll()`, `wait()`, `cancel()`, status, `takeResult()`), latency contract ≤200 ms.
- `src/core/ef/scene.hpp/.cpp`: `FieldScene`, `buildScene`, `renderScene()`, `Painter` interface; `src/render/imgui_painter.hpp` (`ImGuiPainter`); `src/render/png_export.*` (offscreen PNG, RAII guards).
- `src/core/ef/presets.hpp`: 9 presets with MFC display names and slugs; `seeds.hpp` `expectedLineCount`.
- `src/gui/main.cpp`: current hello window with SDL3 + ImGui + sdlrenderer3 and `--smoke`.
- `build.zig`: GUI exe (Windows subsystem), lazy SDL3/ImGui deps, `smoke` step; CI runs smoke on 3 OSes.

### Established Patterns
- Parity first (MFC baselines in `docs/baseline/mfc/`); shared drawing path; strict core flags; GUI code uses `gui_flags` (no -Werror).

### Integration Points
- MFC reference for behaviour: `ElectricField/ElectricField.cpp` (doc templates, startup New chooser), `MainFrm.cpp` (menus, tabbed MDI), `ElectricFieldView.cpp` (timer polling, wait cursor, D2D transform), `ElectricField.rc` (menu strings, About dialog text).

</code_context>

<specifics>
## Specific Ideas

- Visual parity with `docs/baseline/mfc/*.png` remains the acceptance bar for the canvas.

</specifics>

<deferred>
## Deferred Ideas

- Pan/zoom, progressive drawing, status bar with line counts/timing (v2).
- Tabs that can be dragged out/docked (ImGui docking) — not now.
- View ▸ Options, export, persistence (Phase 4).

</deferred>
