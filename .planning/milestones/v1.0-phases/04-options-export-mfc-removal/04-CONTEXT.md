# Phase 4: Options, Export & MFC Removal - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous smart discuss (roadmap criteria are fully specified; MFC behaviour captured from source before deletion)

<domain>
## Phase Boundary

Completes the replacement of the MFC application: a View ▸ Options… dialog with Computation and Draw sections backed by a persistent per-user config file, Draw changes repainting every open tab immediately while Computation changes affect only tabs opened afterwards, and File ▸ Export ▸ PNG…/SVG… of the active tab's view matching what is on screen. Then the MFC tree is deleted (no MFC, Win32, Direct2D, GDI, `.vcxproj`/`.sln`/`.rc` left), the README teaches a new contributor to build, run and test on Windows, Linux and macOS, and CI stays green on all three OSes plus TSan.

Not in this phase: recompute of open tabs after Computation changes (WF-01, v2), reset-to-defaults button (WF-02, v2), clipboard/PDF export (WF-03, v2), pan/zoom and progressive drawing (v2), registry-settings migration (out of scope per REQUIREMENTS).

</domain>

<decisions>
## Implementation Decisions

### Options model & validation
- **D-01:** `efapp::AppOptions` (std-only, `src/app/options.hpp/.cpp`) bundles the user-visible options: an `ef::ComputeParams` (linesPerUnitCharge, calculateEquipotentials, potentialInterval, numThreads, method) and an `ef::DrawStyle` (4 colours, 2 thicknesses). The internal constants (pixelsPerUnit 100, chargeRadiusPx 25) stay non-editable, as in MFC ("internal options", `Options.h:14-16`). Validation mirrors the MFC property pages: linesPerUnitCharge [4,128], numThreads [1,128], potentialInterval [0.01,0.50], both thicknesses [1,64]. **Fix, not parity bug:** MFC's `DrawPropertyPage.cpp` calls `DDV_MinMaxUInt(pDX, electricFieldLineThickness, ...)` twice and never validates `potentialFieldLineThickness`; the port validates both (roadmap criterion 1 explicitly requires the equipotential thickness to be rejected).
- **D-02:** Method values persist by stable name (`ef::kMethods[i].name`: "euler", "midpoint", "rk4", "adaptive-dormand-prince", …); colours persist as `#RRGGBB`; equipotentials as `true`/`false`; potentialInterval as a decimal. Numeric enum values are never persisted (matches `method.hpp` "Numeric values are NOT persisted: names are").

### Persistence
- **D-03:** Plain-text `options.ini` in `SDL_GetPrefPath("ElectricField", "ElectricField")` (Windows `%APPDATA%\ElectricField\ElectricField\`, Linux `$XDG_CONFIG_HOME/ElectricField/ElectricField/`, macOS `~/Library/Application Support/ElectricField/ElectricField/`). Format: `key = value` lines, `#` comments. Loader rules: missing/unreadable file → all defaults; a value that fails to parse or is out of range → that key's default, the rest of the file still applies; unknown keys ignored. Save happens when the Options dialog is accepted (parity: MFC's `Options::Save()` ran on the property sheet's Apply). The GUI resolves the pref path via SDL and hands the GUI-free layer a `std::filesystem::path` so `efapp` stays std-only and unit-testable (UTF-8 handling per D-08/IN-06).

### Options dialog (UI-SPEC 04)
- **D-04:** View ▸ Options… opens an ImGui modal with a two-tab bar (Computation, Draw), mirroring the MFC property sheet (`ComputationPropertyPage` / `DrawPropertyPage`). Computation: method selector with the 11 display names from `kMethods[i].display`, "Calculate equipotentials" checkbox, lines-per-unit-charge and threads numeric edits, potential-interval slider 10–500 mapped to 0.01–0.50 shown as `%.2f`. Draw: four colour pickers (ImGui ColorEdit3, no alpha) for positive/negative charge, electric line, equipotential line; two thickness edits. OK validates and applies; Cancel discards. Out-of-range input is **rejected** (error text in the dialog, dialog stays open) — no silent clamping.
- **D-05:** Application semantics (OPT-03): `DrawStyle` is app-global; accepting Draw changes re-renders all open tabs from their cached `FieldScene` on the next frame — no recomputation. `ComputeParams` is captured per tab at open (already true: Workspace stores params with each session); Computation changes affect only tabs opened afterwards. Failed/empty tabs just re-render what they have.

### Export
- **D-06:** File ▸ Export ▸ PNG… and SVG…, enabled only while the active tab's session is done (its `FieldScene` is complete; partial export would not "match what is shown"). The save location comes from SDL3's native save dialog (`SDL_ShowSaveFileDialog`, filters `png`/`svg`); the dialog callback stores the path and the export runs on the UI thread at the next frame boundary. PNG goes through `efrender::renderPng` (the established ImGui-context rule, phase-2 D-17); SVG via `ef::renderSvg` + file write. Exported image = tab's cached `FieldScene` + current global `DrawStyle` + current canvas `View` — i.e. exactly the on-screen content at the canvas's current pixel size. GUI `--smoke` gains a headless export mode (fixed output dir) so CI verifies PNG/SVG export on all three OSes; the exported PNG must equal `ef-cli` output for the same scene/style/view.
- **D-07:** File writes for export and for the config file must handle non-ASCII paths on Windows (SDL hands out UTF-8; narrow `fopen` would mangle them) — use `std::filesystem::path` with wide/native conversion on Windows. This closes Phase 2 review IN-06 for the new I/O paths.

### MFC removal (BUILD-06)
- **D-08:** Delete `ElectricField/` (all MFC sources, `res/`, `.rc`) and `ElectricField.sln`. Before deletion the behaviour reference has been captured: this context file, `tests/` characterisation comments, and the QA-05 baselines in `docs/baseline/mfc/` (recorded and human-approved in Phases 1–2). `tests/test_presets.cpp` currently **parses `ElectricField/ConfigDocs.cpp` at test runtime** — it is rewritten to inline expected values (they have been locked since Phase 1; after removal the source of truth is the test + baselines). `tools/baseline/capture-mfc-baseline.ps1` and `tools/parity/compare-mfc.sh` lose their subject and are deleted; `tools/parity/expected-counts.csv` and the baselines stay. Comments in `src/gui/icon.cpp` etc. that cite MFC paths are reworded. A CI/grep guard asserts no `.vcxproj`, `.sln`, `.rc`, `afx`, `CWinApp`, `Direct2D`, `d2d1` references remain in the tree.
- **D-09:** README rewrite (BUILD-07): what the app does with a screenshot, prerequisites (Zig 0.17 only — no VS, no MFC), `zig build` / `zig build run` / `zig build test` / `zig build smoke` on Windows, Linux, macOS, options & config-file location, PNG/SVG export, project layout, license, blog link. CI stays green on windows-x64, linux-x64, macos-arm64, tsan-linux; while touching CI, add `cancel-in-progress` and avoid duplicate runs on master (Phase 2 review IN-04).

### Carry-overs closed in this phase
- **D-10:** Phase 2 review IN-06 (UTF-8 paths) via D-07; IN-04 (CI concurrency) via D-09. IN-05 (Painter contract asserts) is addressed as a cheap contract-check test on `renderSvg`/`renderPng` in the export plan; IN-02 (compare-mfc.sh robustness) becomes moot by deleting the script (D-08).

### Claude's Discretion
- Exact ini key names and dialog layout details, colour-picker presentation, error-message wording, smoke-export CLI shape, README structure, where the grep guard lives (script or inline CI step).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/core/ef/params.hpp`: `ComputeParams` with the MFC defaults (16, on, 0.10, 4, Euler).
- `src/core/ef/scene.hpp`: `DrawStyle` with the MFC defaults (blue/red/black/grey192, thickness 1, radius 25); `renderScene(Painter&, scene, style, view)`.
- `src/core/ef/method.hpp`: `Method`, `kMethods` with stable persistence names + UI display strings; `methodFromName()`.
- `src/core/ef/svg.hpp/.cpp`: `SvgPainter` / `renderSvg(scene, style, view)` — SVG export needs only file I/O.
- `src/render/png_export.hpp/.cpp`: `renderPng(scene, style, view, path)` with the ImGui-context rule (UI thread, between frames).
- `src/app/workspace.*`: per-tab params capture already isolates Computation-option changes to new tabs.
- `src/gui/ui.cpp`: menu bar (File, Help) — Options goes under a new View menu, Export under File.
- `assets/`: window/about icons already extracted from the MFC `.ico` (survive MFC deletion).
- SDL 3.4.x (castholm port): `SDL_GetPrefPath`, `SDL_ShowSaveFileDialog` available.

### Established Patterns
- efapp is std-only with strict `-Werror` flags and runs in `zig build test -Dgui=false` without fetching SDL3/ImGui.
- Pure decision logic (like `ui_logic.hpp`) is separated from SDL/ImGui calls so it is unit-testable headlessly.
- GUI behaviours get headless smoke modes so CI exercises them on all 3 OSes.

### Integration Points
- MFC reference for dialog fields/validation: `ElectricField/Options.{h,cpp}`, `ComputationPropertyPage.cpp`, `DrawPropertyPage.cpp` (captured above and in this file before deletion).
- `tests/test_presets.cpp` ↔ `ElectricField/ConfigDocs.cpp` coupling must be cut in the same commit that deletes the tree.

</code_context>

<specifics>
## Specific Ideas

- Export parity bar: exported PNG pixel-identical to `ef-cli` output for the same scene/style/view (same render path).
- Keep the dialog functional at 100% scale first; HiDPI follows the existing scale handling.

</specifics>

<deferred>
## Deferred Ideas

- WF-01 recompute open tabs after Computation changes; WF-02 reset-to-defaults; WF-03 clipboard copy / PDF export (v2).
- Importing the old MFC registry settings (out of scope by REQUIREMENTS).
- Options editor in ef-cli (not required).

</deferred>
