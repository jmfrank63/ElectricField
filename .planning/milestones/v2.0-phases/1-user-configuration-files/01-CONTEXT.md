# Phase 1 (v2): User Configuration Files - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous (requirements fully specified by the milestone owner; design follows the established efapp patterns)

<domain>
## Phase Boundary

Named charge configurations live in a human-readable `configurations.ini` in the same per-user directory as `options.ini`. The preset chooser gains the user configurations (below the 9 built-ins) and an "Empty Configuration" entry (one +1 charge at the origin, immediately computable). File > Save As (Ctrl+S) stores the current tab's charges under a chosen name, with overwrite confirmation and built-in preset names reserved. Editing charges (mouse/keys/fields) is Phase 2; this phase only stores and loads them.

Not in this phase: the editor UI, per-tab editing state, Calculate button (Phase 2); import of arbitrary foreign formats; per-configuration compute params (a config is charges only; compute settings stay global options).

</domain>

<decisions>
## Implementation Decisions

### File format
- **D-01:** `configurations.ini`, sections with quoted names, one charge per line — mirroring the `options.ini` style:
  ```
  # Electric Field user configurations
  [configuration "My Dipole"]
  charge = 1, -2, 0
  charge = -1, 2, 0
  ```
  Values: charge as integer (the core Charge value is an integer), x/y as decimals written with `%.10g` (round-trips through `stod` to well below any meaningful world-unit precision). Empty lines and `#` comments ignored, same as options.
- **D-02:** Parser rules (std-only, in `efapp::configs`): a section needs a non-empty name and at least one valid charge line, else it is skipped (counted); an invalid charge line (`charge = ...` that does not parse as `int, x, y` with finite decimals) is skipped (counted); a duplicate name keeps the FIRST occurrence (later duplicates skipped); order of sections is preserved (chooser order = file order). Missing file → empty list, never an error.

### Core/app model
- **D-03:** `efapp::UserConfig { std::string name; std::vector<ef::PresetCharge> charges; }` reusing `ef::PresetCharge` (int value + x, y). `serializeConfigs` / `parseConfigs` / `loadConfigs` / `saveConfigs` mirror the options model; `upsertUserConfig(list, candidate, reservedNames)` returns an enum (`OK`, `RESERVED_NAME`, `EMPTY_NAME`) and overwrites an existing entry in place — the pure decision core for Save As, unit-testable.
- **D-04:** Reserved names for Save As = the 9 built-in display names (a preset can be Save-As'd under a NEW name, but never overwritten). Names are compared case-insensitively for reservation and dedupe (Windows-style file semantics; consistent in the ini).
- **D-05:** `Workspace::openUser(name, charges)` opens a tab titled "<name> <N>" with its own per-name counter (a `std::map<std::string, unsigned>` next to the preset counters); `Tab::preset` uses `kNoPreset = SIZE_MAX` for user/empty tabs (everything downstream keys off `title`, so the change is inert). `Workspace::openEmpty()` opens "Empty Configuration <N>" with a single +1 charge at the origin — computable from the first frame, and the Phase 2 editor's starting point. Both go through the same factory/session/failed-tab paths as `open()`.

### GUI integration
- **D-06:** The chooser lists 9 presets, a disabled separator row "— User configurations —", one row per user config (file order), and "Empty Configuration" last; keyboard Up/Down/Enter/double-click/Esc behaviour is unchanged (selection skips the separator). UiState gains `openUserConfig` (index into the App-loaded list) and `openEmpty`; the App-loaded configs are mirrored into `ui_.userConfigs` each frame (small copies, one owner).
- **D-07:** File > Save As (Ctrl+S, enabled with an active tab) opens a modal with a name field prefilled from the active tab's title without the counter suffix; OK runs `upsertUserConfig` — `RESERVED_NAME`/`EMPTY_NAME` show inline errors; when the name already exists, a second confirm popup ("Overwrite '<name>'?") guards the write. Success closes the dialog, writes `configurations.ini` (interactive modes only) and refreshes the chooser list.
- **D-08:** The App loads user configs at startup from `SDL_GetPrefPath(...)/configurations.ini`, exactly like options; headless modes (smoke/screenshot/export) skip load and save so CI never touches user files. `--smoke` gains a headless check that a user config and the empty entry open and compute (from an in-memory list, no file).

### Claude's Discretion
- Exact ini section syntax details, error message wording, chooser separator styling, save-modal layout.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/app/options.{hpp,cpp}`: the exact pattern to mirror — std-only ini model, per-key fallback, `SDL_GetPrefPath` resolved by the GUI, headless skip.
- `ef::PresetCharge { int charge; double x, y; }` (presets.hpp) — the per-charge storage type.
- `Workspace` (src/app): `open()` shows the full tab-creation path to replicate for user configs; counters array becomes array + map.
- Chooser modal (ui.cpp chooserModal) and Save/Options modal patterns (optionsModal) — the UI building blocks.

### Established Patterns
- Pure decisions in efapp (strict flags, doctested headless), SDL/ImGui mapping in efgui, headless smoke coverage for every new GUI behaviour, lazy GUI deps untouched by `-Dgui=false`.

</code_context>

<specifics>
## Specific Ideas

- `configurations.ini` is small; always rewrite the whole file on save (no in-place editing).
- The chooser shows "(from file)" nothing special — user configs are first-class rows.

</specifics>

<deferred>
## Deferred Ideas

- Renaming/deleting user configurations from the UI (file can be hand-edited; a manager UI is a later wish).
- Per-configuration compute settings (lines/thread count stored with the config).
- Import/export of configuration files to arbitrary paths.

</deferred>
