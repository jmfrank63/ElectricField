# Phase 3: Charge Identity & Per-Charge Size - Context

**Gathered:** 2026-10-13
**Status:** Ready for planning
**Mode:** Auto-generated (smart discuss, auto-accepted answers per standing auto-mode instruction)

<domain>
## Phase Boundary

Owner asks:
1. "The size of a charge should be adjustable per charge and settings should only contain the default."
2. "Each charge should carry an id and a name and the name should be editable and should show up as
   tooltip when hovering over."

Scope: the charge model gains identity (stable id, editable name) and a per-charge drawn size; the
Options charge size becomes the DEFAULT for new charges and the fallback for charges without an
override; editor panel + canvas tooltip + persistence (collections, sessions) + command API.

Out of scope: physics changes (field of a charge never depends on its drawn size), changes to
colour/thickness options, multi-select editing.

</domain>

<decisions>
## Implementation Decisions (recommended answers, auto-accepted)

### Model
- `ef::Charge` (src/core/ef/charge_system.hpp) gains: `std::uint64_t id` (0 = unassigned/legacy),
  `std::string name` (empty = derived display), `float sizePx` (0 = inherit the Options default).
  Equality helpers updated wherever charges are compared. Physics functions untouched.
- `ef::PresetCharge` (src/core/ef/presets.hpp) gains the same fields so collections/sessions/API share
  one transport type (it already flows into workspace/editor/session_file).
- Presets and new charges: ids assigned max+1 within the charge system (presets: 1..N), names default
  "Charge N" (N = the per-charge index; a preset MAY override via its data later - out of scope),
  size 0 (= default).
- ID stability: ids never get reused after a charge is removed; undo/redo restore ids with the snapshot
  (HistoryEntry snapshots whole ChargeSystem - automatic). A removed charge's id is not recycled
  (next id = max over the system + 1).

### Persistence (backward compatible)
- Collection files (app/configs.cpp `[configuration "Name"]` sections) and session files
  (app/session_file.cpp `[tab ...]` sections) both serialize charges as `charge = value, x, y` lines.
  Extend to `charge = value, x, y, sizePx, id, name` with the extra fields OPTIONAL: parsers accept the
  legacy 3-field form (id=0, name="", size=0) and the extended 4-6 field form; missing middle fields
  keep their zero values. Names are serialized comma-free (strip ',' on write - the same approach the
  session titles use for '"'). Non-ASCII names must survive (the ini layer is UTF-8-safe; keep bytes
  as-is).
- app.state's tab line gains the extra per-charge fields appended (`charges=1,0.5,0,25,1,Charge 1;...`)
  - substring-compatible: existing scripts that expect `charges=1,0.5,0` keep passing.

### UI
- Editor panel: for the selected charge, a Name text field and a Size field (px, 2-120, same range as
  the Options charge size; empty/0 = "default") with the existing realtime-edit semantics (mark stale,
  arm auto-recalc; size/name do NOT require recalculation to redraw but DO mark dirty for the
  unsaved-changes prompt - follow the existing moveCharge dirty semantics).
- View > Options keeps its Charge size field; its help text becomes "default size for new charges".
  Applied defaults flow through Workspace::open/openUser/openEmpty (the same place ComputeParams
  defaults flow).
- Canvas: hovering a charge (hit test under the mouse, not dragging, not computing) shows a tooltip
  with the charge's name (fallback "Charge N"); ImGui::SetTooltip from drawCanvas.
- Hit testing: `editor::hitCharge` currently takes one radius; switch to per-charge effective radius
  (charge.sizePx > 0 ? sizePx : options default, converted to world units + the existing 1.5 margin
  rule) so big/small charges pick correctly.
- Rendering: `ef::SceneCharge` gains sizePx; `renderScene` draws
  `sizePx > 0 ? sizePx : style.chargeRadiusPx` (device px after the existing k scaling in
  scaledStyle - the per-charge value is already a device-px radius like the style one).

### API
- `charge.name <i> <name>` (quoted names with spaces per the existing tokenizer), `charge.size <i> <px>`
  (2-120 or 0 = default). Both realtime-edit like charge.move. app.state shows name/size (above).

</decisions>

<code_context>
## Existing Code Insights

- Charge type: ef/charge_system.hpp (Charge + ChargeSystem with ENormalized/Potential/HitCharge -
  HitCharge uses p.hitRadius(), the PHysics radius; the EDITOR hit test is efapp::editor::hitCharge
  (src/app/editor.cpp) with a GUI-provided radius - the per-charge logic belongs THERE, not in the
  physics HitCharge).
- Charge construction sites to sweep: presets.cpp (makeSystem), workspace.cpp (open/openUser/openEmpty/
  openRestored), editor.cpp (addCharge), configs.cpp (parse), session_file.cpp (parse), gui/app.cpp
  (mergeDirtyTabsInto, saveSessionNow), cli/main.cpp, tests/fixtures (test_editor dipoleTab etc.).
  Aggregate initializations of Charge/PresetCharge WILL break to compile - the sweep is mechanical but
  must be complete.
- Serialization seams: app/configs.cpp serialize/parse (charge lines), app/session_file.cpp
  serializeSession/parseSession (same line format). The 3-field CSV parse lives in both - extend both
  symmetrically.
- Editor panel: drawEditorPanel (src/gui/ui.cpp) - the selected-charge section (Value/X/Y rows) is the
  pattern for the Name/Size rows; Options dialog: optionsModal (env.options / optionsEdit).
- Tooltip: drawCanvas has mouseWorld + hitCharge already (the click-select path) - the tooltip reuses
  that hit result on hover-only frames.
- API dispatcher: App::command (src/gui/app.cpp) - charge.* commands cluster together; apiTokens handles
  quoted args.
- Undo: HistoryEntry{ChargeSystem field, Method} snapshots whole charge vectors - id/name/size ride for
  free; verify stepValue/flipSign/moveCharge PRESERVE the new fields (they mutate in place - they do).

</code_context>

<specifics>
## Specific Ideas

- Doctests: format round-trips (3-field legacy + 6-field extended, comma-stripped names, non-ASCII),
  id assignment (max+1, no reuse after remove), editor name/size edits mark dirty not stale, API
  commands, hit-test ordering with overlapping different-size charges.
- Script test: 45-charge-identity.efscript - charge.add, charge.name 0 "My Charge", charge.size 0 40,
  app.expect the extended charges= dump, collection.save + reopen round-trip, editor.undo restores.
- README/api.md updates (charge lines format, new commands, Options semantics).

</specifics>

<deferred>
## Deferred Ideas

- Preset-defined names (the built-in arrangements getting meaningful names) - future.
- Renaming by double-clicking the charge on canvas - future.

</deferred>
