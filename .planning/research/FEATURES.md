# Feature Landscape

**Domain:** Desktop electric field line / equipotential simulator (point charges, 2D), brownfield MFC -> cross-platform port
**Researched:** 2026-10-10
**Mode:** Ecosystem (features dimension), brownfield parity audit
**Overall confidence:** HIGH for "what exists today" (read directly from source), MEDIUM for "what modern users expect" (domain norms from general knowledge plus one thin web check of PhET "Charges and Fields"; no framework-specific verification done here)

## Ground Truth: What the MFC App Actually Does

Audited from `ElectricField.rc`, `ConfigDocs.cpp`, `MainFrm.cpp`, `ElectricFieldView.cpp`, `ElectricFieldDoc.cpp`, `ComputationPropertyPage.cpp`, `DrawPropertyPage.cpp`, `Options.cpp`, `Charge.cpp`. Several of these differ from the assumptions in PROJECT.md, so the requirements author should use this section, not memory.

| # | Observed behaviour | Source | Impact on port |
|---|--------------------|--------|----------------|
| 1 | **No progressive display.** The view shows a wait cursor and a 100 ms timer; `GetData()` only swaps results in once `CheckStatus()` reports all threads finished. Nothing is drawn until the whole computation completes. | `ElectricFieldView.cpp` `OnTimer`, `ElectricFieldDoc.cpp` `GetData` | PROJECT.md says "progressive display while computing". Parity floor is "draw when done"; progressive is a differentiator, not a regression to avoid. |
| 2 | **Computation options do not affect open documents.** Computation page `ApplyValues()` only saves; Draw page `ApplyValues()` saves and calls `UpdateAllViews` on every open doc. Computation options apply only to documents created afterwards. | Both `*PropertyPage.cpp` | Preserve as-is for parity, but surface it in the UI (see "Recompute" differentiator). |
| 3 | **Equipotential duplicate removal is a pixel-space heuristic living in the document layer.** `GetDataFromThreads()` sorts by potential, groups within 1/3 interval, picks a hard-coded reference point (-7, 5.5), and treats weight centres closer than `2 * chargeRadius / distanceUnitLength` (= 0.5 world units at defaults) as duplicates. Skipped entirely for exactly two charges of opposite sign. | `ElectricFieldDoc.cpp` | This is part of the visible output and must move into the core library (not the GUI), parameterised in world units, and covered by tests. See dependency with zoom below. |
| 4 | **Fixed view scale, no zoom/pan.** 100 px per world unit (`distanceUnitLength`), charge radius 25 px, origin at view centre, y up, white background. These are not user options. | `Options.cpp`, `ElectricFieldView.cpp`, `Charge.cpp` | Parity = fixed scale. Any zoom must be a pure view transform so it cannot change which lines the core produces. |
| 5 | **Presets are created via MFC's File > New template picker** (9 `CMultiDocTemplate`s). Each opens in its own MDI child; MDI tabbed groups are already enabled, so the UI is already "tabs". Charge geometry is hard-coded per preset (e.g. Dipole = +1 at (-2,0), -1 at (2,0); Quadrupole/Parallel Dipoles at (+-1.5, +-1.5); Four Charges at x = -3,-1,1,3; unequal = magnitudes 1 and 2). | `ConfigDocs.cpp`, `ElectricField.cpp` | Presets become data in the core (name + list of charges); the GUI just lists them. Charge values and positions are the behaviour-parity oracle. |
| 6 | **Options ranges/defaults.** Lines per unit charge 4-128 (default 16); threads 1-128 (default 4); field/equipotential thickness 1-64 px (default 1); potential interval slider 0.01-0.50 step 0.001 (default 0.10, shown with 2 decimals); equipotentials on (default); method default = Euler; colours: positive blue (0,0,255), negative red (255,0,0), field lines black, equipotentials light grey (192,192,192). | `Options.cpp`, `ComputationPropertyPage.cpp`, `DrawPropertyPage.cpp` | Carry over exactly; tests and "behaviour parity" constraint depend on these defaults. |
| 7 | **Known bug:** Draw page validates `electricFieldLineThickness` twice and never validates `potentialFieldLineThickness`. | `DrawPropertyPage.cpp` lines 41-42 | Do not port the bug; validate both. |
| 8 | **Persistence is split:** `Options` (registry `options` section, colours stored as raw `COLORREF` = BGR byte order) and `ApplicationLook` (theme id). `LoadStdProfileSettings(0)` so no MRU. | `Options.cpp`, `MainFrm.cpp`, `ElectricField.cpp` | Colour byte order must be normalised in the new config format (store `#RRGGBB`). No migration of old registry data. |
| 9 | **Close mid-computation is safe:** doc hands its calculator to the app (`theApp.calculators`) with `Terminate = true` until threads finish. | `ElectricFieldDoc::OnCloseDocument` | Table stakes: closing a tab or quitting must never block the UI or crash. |
| 10 | **UI chrome is mostly App Wizard boilerplate:** 8 "Application Look" themes (Win2000 ... Windows 7), docking/auto-hide panes, toolbar customization and user toolbars, user-image bitmap, menu personalization, status-bar CAPS/NUM/SCRL indicators, Windows manager dialog, per-doc menus duplicated 9 times. Only meaningful items: New, Close, Print/Preview/Setup, Exit, Options, Status bar toggle, About, New Window. | `ElectricField.rc`, `MainFrm.cpp` | Almost all of it is droppable. |
| 11 | **Shortcuts:** Ctrl+N (New), Ctrl+P (Print), Ctrl+O (Options - not shown in menu text), F6 (next/prev pane). | `ElectricField.rc` accelerators | Keep N and O equivalents; P becomes Export. |
| 12 | **About box:** title "Electric Field, Version 1.0" plus two links (GitHub repository `github.com/jmfrank63/ElectricField`, blog `compphys.go.ro/electric-field-lines/`). Note: no `README.md` exists at the repo root today (read failed), so PROJECT.md's "Update README" item is really "create README". | `ElectricField.rc` | Keep a minimal About with both links. |
| 13 | **Rendering:** field lines drawn as Bezier-smoothed geometry (`AdjustForBezier`), per-primitive anti-aliasing, charges as filled circles coloured by sign. No arrows, no labels, no grid, no axes. | `FieldLine.cpp`, `Charge.cpp` | "Recognisable output" = smooth AA curves + coloured discs. Bezier control-point maths lives in `FieldLine` and is core; only the draw call is GUI. |

## Table Stakes

Features users (and the "behaviour parity" constraint) require. Missing = the port is not a port.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 9 preset configurations, selectable at runtime | Core value of the app (obs. 5) | Low | Store as data table in core: `{name, [{x, y, q}]}`. Picker = File > New submenu or a small chooser dialog; submenu is simplest and maps to old per-template "New" list. |
| One tab/window per configuration; multiple open at once | Existing MDI-tabbed behaviour (obs. 5) | Low-Med | Modern equivalent of MDI is a tab bar (or separate top-level windows). Prefer tabs: they already exist in the MFC UI. Tab title = configuration name. |
| Close tab; quit app | Existing | Low | Ctrl+W / tab close button plus window close and platform quit shortcut. |
| Field-line generation seeded around each charge, count proportional to |q| * lines-per-unit-charge | Core physics | Med | Already in `FieldLinesCalculator`; keep output identical (same seeding angles, same termination via `HitCharge`). |
| Equipotential lines at configurable interval, including the duplicate-removal pass | Core physics + visible output (obs. 3) | Med-High | Highest-risk parity item. Move heuristic into core, express thresholds in world units, add regression tests on preset line counts. |
| 11 integration methods selectable | Validated feature | Low | Radio group of 11 becomes a dropdown or grouped list (6 fixed-step, 5 adaptive). Keep the same ordering and enum values. |
| Multi-threaded computation, configurable thread count | Validated feature | Low (reuse) | Already `std::thread`; remove `theApp.options` reads from `FieldLinesCalculator`/`FieldLine::AddPoint`/`HitCharge` by passing an options struct in. Default threads: keep 4, optionally `hardware_concurrency()`. |
| Non-blocking UI while computing + busy indication | Wait cursor + disabled interaction in MFC (obs. 1) | Low | Wait cursor is a Windows idiom; use a status-bar text ("Computing...") or spinner. UI thread must never block on join. |
| Safe cancel when tab closed or app exits mid-computation | Obs. 9 | Med | Keep the `Terminate` atomic pattern; make shutdown deterministic (join or detach-with-ownership) instead of the `theApp.calculators` global. |
| Anti-aliased rendering: charges coloured by sign, smooth field and equipotential curves, y-up, centred origin, white background | Obs. 4, 13 | Med | Framework-dependent. Whatever 2D API is chosen must support AA stroked Beziers/polylines with width 1-64 and filled circles. |
| HiDPI correctness | Direct2D handled DPI automatically; any modern desktop app is blurry/mis-sized without it | Low-Med | Charge radius and line widths are in logical px; scale by device pixel ratio so output looks the same on 100%/200% displays. |
| Options dialog, two groups: Computation and Draw | Validated feature | Med | Single modal (or non-modal) dialog with two tabs/sections. Controls: checkbox, 2 integer spinners, method chooser, slider (0.01-0.50) with live value label, 2 thickness spinners, 4 colour pickers. Use native-feeling colour picker; no custom "More Colors..." needed. |
| Option validation with the original ranges (fix obs. 7) | Existing behaviour | Low | Clamp or reject; put ranges in core `Options` so tests and CLI share them. |
| Draw-option changes repaint all open tabs immediately | Obs. 2 | Low | Single "options changed" signal to every view. Colours and thickness need no recomputation. |
| Options persisted between runs in portable per-user config | Validated feature | Low-Med | Plain file in platform config dir (e.g. `%APPDATA%`, `~/.config`, `~/Library/Application Support`), human-readable (TOML/JSON/INI). Colours as `#RRGGBB`. Missing/corrupt file = defaults, never crash. |
| Export of the current view (replaces printing) | Printing is a Validated feature; native print is not portable | Med | Minimum: PNG at chosen scale. See "Replaced" table. |
| Keyboard shortcuts: New (Ctrl/Cmd+N), Close (Ctrl/Cmd+W), Options (Ctrl/Cmd+, or Ctrl+O to match), Export (Ctrl/Cmd+E or P), Quit | Obs. 11 plus platform norms | Low | macOS needs Cmd and the Preferences placement convention; handled by most toolkits if actions are declared properly. |
| About box with GitHub and blog links, version | Obs. 12 | Low | Version string from build system, not hard-coded. |
| Window/tab state needs no files: stateless startup | Obs. 5 | Low | On launch, either open a default preset (e.g. Dipole) or show an empty window with the New menu. Recommend opening Dipole so first-run is not a blank screen. |

## Differentiators

Not required for parity; each is small and independent. Ordered by value per effort.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Export to SVG** (plus PNG) | Lossless, scalable, embeddable in blog posts and papers - this app is a companion to a blog | Med | SVG is trivial to emit from the core's geometry (polyline/cubic Bezier paths, circles) with no GUI dependency, which also makes it a perfect golden-file test artifact. PNG via the framework's offscreen render. PDF optional (only if the framework gives it for free). |
| **Pan and zoom** (wheel/pinch zoom, drag pan, double-click or key to reset) | Standard in every visualiser; lets users inspect the dense region near a charge | Med | Implement as a view transform only (scale/translate applied to drawing). Do **not** change `distanceUnitLength` in the core, because the duplicate-removal threshold and charge radius are expressed against it (obs. 3, 4). Line widths and charge radius stay in screen px so the picture stays crisp. Lines are computed on a finite region; zooming out far reveals where they end. Decide extent explicitly (see PITFALLS). |
| **Progressive drawing + progress indicator** | Obs. 1: today the user stares at a wait cursor; large line counts (128/unit charge) take noticeable time | Med | Requires the view to read partial results safely: either per-line completion push (thread-safe queue) or a snapshot copy under mutex. Equipotential de-duplication needs all lines, so show equipotentials only on completion. Keep the "draw when done" path as the simple first implementation. |
| **Recompute button / "Apply and recompute" for open tab** | Fixes obs. 2 surprise: changing line count/method does nothing to open tabs | Low-Med | Re-run calculator for the active tab (cancel the old one first). Could also be a checkbox "apply to open tabs". |
| **Field-line direction arrows** | Makes field direction readable at a glance (lines currently have no orientation cue); charge sign colour is the only hint | Low-Med | Midpoint chevron per line, toggle in Draw options, off by default to keep visual parity. |
| **Headless CLI render** (`electricfield --preset dipole --method rk4 --out dipole.svg`) | Falls out for free from the GUI-independent core; enables CI golden tests, docs generation, batch figures | Low-Med | Also the practical way to test "visually equivalent output" on three OSes in CI without a display. |
| **Reset-to-defaults button in Options** | Cheap; defaults are documented in obs. 6 | Low | |
| **Charge sign/value labels (+1, -2)** | Presets have differing magnitudes (unequal-charge presets); users otherwise cannot tell 1 from 2 | Low | Optional toggle. Needs text rendering; defer if framework text is awkward. |
| **Follow OS light/dark theme for window chrome** | Replaces the 8 "Application Look" themes with the one thing users actually want | Low (toolkit-provided) | Canvas stays white by default (parity); optional dark canvas with light lines is a later add. Needs line/charge colour defaults per theme if enabled. |
| **Copy image to clipboard** | One keystroke to paste into slides/documents | Low | Falls out of PNG export. |
| **Render time / line count in status bar** | Gives the status bar meaning (replaces CAPS/NUM/SCRL) and shows the cost of method choice | Low | e.g. "Dipole - RK4 - 32 field lines, 18 equipotentials - 0.42 s". Useful for comparing integrators, which is the educational point. |

## Anti-Features

Deliberately NOT built.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Interactive charge placement/editing, drag charges, add/remove charge | Explicitly out of scope; a different product (PhET-style sandbox) | Fixed presets only; keep `Charge` list structure compatible so a later milestone can add it. |
| Save/Load user configurations to files (and `Serialize`, MRU file list, Open/Save/Save As) | Explicitly out of scope; `Serialize` is an empty stub today | None. Do not even create menu entries. |
| Native print, print preview, Page Setup | Not portable, large per-toolkit surface (Qt has QPrinter, wx has printing framework, ImGui/SDL has nothing); the GDI print path also depends on `GetSystemMetrics(SM_CXSCREEN)/2` as a scale hack (`OnPrepareDC`) | Export to PNG/SVG (and PDF if free); user prints from any viewer. If the chosen toolkit gives printing nearly free, treat it as a bonus, not a requirement. |
| "Application Look" themes (Win2000, Office XP/2003/2007, VS 2005/2008, Windows 7) and the persisted `ApplicationLook` setting | Windows-only, dated, MFC visual managers; zero value cross-platform | Follow the OS/toolkit native theme. |
| Docking panes, auto-hide panes, toolbar customization, user-defined toolbars, user-images bitmap, menu personalization (MRU commands), "Toolbars and Docking Windows" menu | App Wizard boilerplate; the app has no dockable panels | One fixed menu bar and, at most, a small fixed toolbar (New, Export, Options). |
| MDI window manager: Windows... dialog, Cascade, Tile H/V, Arrange Icons, Split | Superseded by tabs/OS window management | Tab bar; Window menu (if any) just lists tabs (most toolkits do this automatically on macOS). |
| "Window > New Window" (second view of the same document) | With a fixed, view-state-free document it gives two identical windows; no state to diverge | Revisit only if pan/zoom is added and users ask for synced/independent views. |
| Status-bar CAPS/NUM/SCRL indicators, "Pane 1/Pane 2" placeholders, F6 pane switching | Meaningless for this app | Status bar shows computation status (see differentiators) or is omitted. |
| Per-document-type duplicated menus and icons (9 menu resources, 9 doc icons) | MFC template artefact | One menu; presets are rows in a table. Optional small preset thumbnails in the picker are a nicety, not a requirement. |
| Migrating old Windows registry settings | Different key layout, BGR colour blobs; one-off value for a personal project | Start from defaults on first run of the new version. |
| Hard-coded display-resolution scaling hacks (`GetSystemMetrics`) | Platform-specific and wrong on multi-monitor/HiDPI | Device-pixel-ratio aware rendering; export has its own explicit pixel size. |
| New physics: 3D, dielectrics, continuous charge distributions, animations of test particles | Explicitly out of scope; "port only" | Keep core extensible but don't build. |
| Localisation/i18n framework | English-only today; no demand | Keep strings in one place so it is possible later. |
| Mobile/web targets, auto-updater, telemetry, plugin system | Out of scope for a personal educational desktop tool | Plain release binaries from CI. |
| 11 separate radio buttons | Faithful but clumsy; wastes dialog space | Dropdown/combo (or two grouped lists: fixed-step vs adaptive). This is a replacement, not a removal: all 11 methods stay. |

## MFC Feature -> Modern Cross-Platform Mapping

| MFC feature | Disposition | Replacement |
|-------------|-------------|-------------|
| `CMDIFrameWndEx` + `CMDIChildWnd` per document | Replace | Single main window with tab bar; one tab per configuration |
| Doc/View + `CMultiDocTemplate` x9 | Replace | `Preset` table in core + a `FieldView` widget/panel per tab; no document class needed |
| File > New (template picker) | Keep (re-implemented) | File > New submenu listing 9 presets, Ctrl/Cmd+N shows the list or opens last-used preset |
| File > Close, Exit | Keep | Close tab, Quit |
| File > Print / Print Preview / Print Setup / toolbar print button | Replace | File > Export Image... (PNG, SVG; PDF optional); toolbar button becomes Export |
| View > Status Bar toggle | Keep (optional) | Status bar with computation status |
| View > Options (Ctrl+O) | Keep | Options dialog (2 sections), same ranges/defaults; plus Reset to defaults |
| View > Toolbars and Docking Windows / Customize | Drop | none |
| View > Application Look (8 themes) | Drop | OS theme |
| Window > New Window / Windows... / Cascade / Tile | Drop | Tabs / OS window management |
| Help > About (dialog with `MfcLink` controls) | Keep | About dialog with two hyperlinks (GitHub, blog) |
| `COLORREF` options + `MfcColorButton` | Replace | Toolkit colour picker; persist as `#RRGGBB` |
| Registry (`GetProfileInt/Binary`) | Replace | Config file in user config dir |
| Spin controls (`msctls_updown32`) and trackbar | Replace | Toolkit spin boxes and slider; keep numeric display next to slider |
| Direct2D `CHwndRenderTarget`, per-primitive AA | Replace | Toolkit 2D API or immediate-mode draw lists; AA required |
| GDI `CDC` print path (`Draw(CDC*, float zoom)`) | Drop | Delete; export uses the same drawing code path as the screen |
| Wait cursor (`BeginWaitCursor`/`OnSetCursor`) | Replace | Status text / spinner; cursor change optional |
| 100 ms polling `OnTimer` for results | Replace | Toolkit-native cross-thread notification (queued signal, posted event) or a cheap per-frame check in immediate-mode UIs |
| `theApp.options` global read by core types | Remove | Options struct passed into core objects (prerequisite for tests and CLI) |
| `theApp.calculators` leftover list for closed docs | Replace | Calculator owned by a shared handle that outlives the tab until threads end |

## Feature Dependencies

```
Core library extracted (no theApp / MFC types)
  -> Presets as data table
       -> New menu / preset picker
            -> Tab per configuration
  -> Options struct in core (ranges, defaults, validation)
       -> Config file persistence (portable)
       -> Options dialog
            -> Draw option change repaints all tabs
            -> Reset to defaults
       -> Calculator takes options explicitly
            -> Safe cancel / lifetime of calculator (tab close, quit)
            -> Headless CLI render
            -> Core regression tests (physics, integrators, presets, dedupe)
                 -> CI on 3 OSes
  -> Equipotential de-dup moved from Doc layer into core (world-unit thresholds)
       -> Zoom must stay a view transform (does not alter core units)

Drawing abstraction (screen)  ->  Export PNG / SVG / clipboard (reuse same geometry)
Bezier smoothing in core (AdjustForBezier) -> SVG path export (cubic segments) and screen drawing

Pan/zoom  ->  Reset view
Pan/zoom  ->  (decision) computed extent of field lines, see PITFALLS
Progressive display -> thread-safe partial-result handoff  (equipotentials only on completion)
Recompute button -> Cancel in-flight calculation (same mechanism as tab close)
Direction arrows -> per-line geometry access (midpoint + tangent) from core
Charge labels -> text rendering in chosen toolkit
```

## MVP Recommendation

Prioritise for the port milestone (all table stakes, in dependency order):

1. Core library extraction with options passed in, presets as data, equipotential de-dup moved into core, tests - nothing else can be verified without it.
2. Tabbed main window with File > New (9 presets), Close, Quit, and a canvas that draws charges + field lines + equipotentials with AA at HiDPI and the original defaults (draw-when-done, matching obs. 1).
3. Options dialog (Computation + Draw), validation with original ranges, portable config file, immediate repaint of all tabs on Draw changes.
4. Safe cancel on tab close / quit; status text while computing.
5. Export to PNG and SVG (replaces printing), About with links, shortcuts.

Add as cheap differentiators once the above is stable (each is independently shippable):
- Pan/zoom as a pure view transform (highest user value).
- Recompute on options change (fixes the biggest UX surprise in the original).
- Headless CLI render (nearly free and strengthens CI golden tests).
- Progressive drawing and a useful status bar.

Defer:
- Direction arrows, charge labels, dark canvas, clipboard: nice but not needed to call the port complete.
- PDF export: only if the chosen GUI toolkit provides it without extra dependencies.
- "Apply to open tabs" granularity beyond a simple Recompute action.

## Notes for the Framework Decision (feature-driven constraints)

These are the features that can actually discriminate between candidate GUI stacks, so the STACK researcher should check them:

- Needs AA stroked cubic Beziers / polylines (width 1-64) and filled circles, plus offscreen render to PNG: all candidates can do it, but immediate-mode stacks (Dear ImGui) need line AA verified and have limited thick-line quality unless custom-tessellated.
- Needs tabbed documents, menu bar with platform-correct macOS app menu, modal options dialog with spinners, slider, combo, colour picker, hyperlink labels. Qt and wxWidgets give all natively; ImGui provides them but not native look and has no native menu bar on macOS.
- SVG export is core-owned (no toolkit dependency); PNG export needs an offscreen surface or framebuffer readback; PDF only if toolkit provides it.
- Cross-thread notification must exist (queued signal / posted event / per-frame poll).
- HiDPI support on all three OSes.

## Sources

- Source code audit (HIGH confidence): `ElectricField/ElectricField.rc`, `ConfigDocs.cpp`, `MainFrm.cpp`, `ElectricFieldView.cpp`, `ElectricFieldDoc.cpp`, `ComputationPropertyPage.cpp`, `DrawPropertyPage.cpp`, `Options.cpp`, `Charge.cpp`, `FieldLine.cpp` (grep), `FieldLinesCalculator.cpp` (grep), `.planning/PROJECT.md`.
- PhET "Charges and Fields" teacher guide and search results, https://phet.colorado.edu/files/teachers-guide/charges-and-fields-html-guide_en.pdf (LOW-MEDIUM: used only to confirm that comparable educational field visualisers centre on charge arrangement, field display, equipotentials and a measuring probe; web search snippets were thin and no grid/export feature could be confirmed or ruled out).
- Domain norms for pan/zoom, image/SVG export, HiDPI, and OS-theme following are from general knowledge of desktop scientific visualisers (MEDIUM, not individually verified in this pass).
- Not available: `README.md` at repo root (file not found), so no project-authored feature claims beyond PROJECT.md and the source.
