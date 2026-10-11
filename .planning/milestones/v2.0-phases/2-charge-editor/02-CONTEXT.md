# Phase 2 (v2): Charge Editor & Explicit Calculation - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous (requirements fixed by the milestone owner; design follows the established efapp/efgui patterns)

<domain>
## Phase Boundary

The tab becomes an editable workbench: charges can be selected, dragged, nudged, added, removed and fine-tuned through input fields with up/down buttons — with the charge glyphs following in realtime while nothing is recomputed. A Calculate button (F5) and a per-tab method selector make recomputation explicit. Edited tabs are marked stale until recalculated.

Not in this phase: continuous/3D charges, undo/redo, per-charge compute settings, multi-select, charge labels.

</domain>

<decisions>
## Implementation Decisions

### State model (efapp, testable)
- **D-01:** `Tab` gains `int selected = -1` and `bool stale = false`. All editor mutations live in a pure `efapp::editor` module operating on `Tab`: `hitCharge` (nearest within a radius, -1 when none), `moveCharge`, `addCharge` (appends + selects), `removeSelected` (fails when fewer than 2 charges — at least one charge always remains), `stepSelected` (arrow-key deltas), `stepValue` (+/- 1). Every mutator refuses while the tab has a live session (computing), keeps `scene.charges` index-aligned with `field.charges` (buildScene order, verified 1:1), and sets `stale = true`.
- **D-02:** Editing is allowed exactly when the session is gone (Done/Failed). `ComputeSession` takes the charge system BY VALUE, so the running computation could not observe edits anyway — but the stale/calculate story stays simple: while the busy overlay shows, the panel is disabled.
- **D-03:** `Workspace::calculate(serial)` restarts a Done/Failed tab from its CURRENT `field` and `params` — same factory, charges-only scene, fresh expectedLineCount, same Failed-on-error paths as `open()`. It returns false for unknown/still-computing tabs. Calculate clears `stale`; completion arrives through the existing poll pipeline.

### Realtime semantics
- **D-04:** Every edit mutates `field.charges` and the matching `scene.charges` entry in the same frame — the glyph moves at once, lines do not change. `stale = true` marks the drawn lines as belonging to the pre-edit charges; the panel shows the hint until Calculate. A method change in the panel also sets `stale` (the drawn lines were computed with the previous method) and applies only at the next Calculate — it edits the tab's captured `params.method` (which defaulted to the app option at open), so no extra override state exists.

### GUI layout & input
- **D-05:** The tab content splits into the canvas (left) and a 230-logical-px editor panel (right). Panel: Value (int, -/+ buttons), X and Y (InputDouble "%.3f", -/+ buttons; -/+ steps 0.1 world units with press-and-hold repeat), Remove (disabled as long as only one charge exists — EDIT-04's invariant), separator, Method combo (the 11 display names), Calculate (F5), and the stale hint. Without a selection: a short hint ("Click a charge; double-click empty canvas to add").
- **D-06:** Canvas input (only when not computing): click selects the nearest charge within 0.30 world units (charge radius 0.25 + slack) or deselects; drag moves the selected charge under the mouse; double-click on empty canvas adds a +1 charge and selects it; Delete removes the selected one; arrow keys nudge by 0.1 (Shift = 0.5) with key repeat. Screen→world conversions are pure `ui_logic` helpers (y-up origin at canvas centre, tested). The selected charge gets a ring drawn over the canvas draw list.
- **D-07:** F5 (no modal open) and the panel button both raise a `calculateActive` request consumed by drawUi through `Workspace::calculate` on the active tab; Ctrl+S/Save As keeps saving the CURRENT charges — an edited tab saved before recalculation stores exactly what is on screen (charges), independent of the stale lines.

### Verification
- **D-08:** The editor core gets `tests/test_editor.cpp` (hit/move/add/remove/step guards, stale, calculate recompute); the smoke gains a headless end-to-end block through the injected input queue: open the empty configuration, click-select the origin charge, drag it, double-click to add a second, Delete back to one, F5 to calculate to completion — plus `zig build test`/`smoke`/CI on all three OSes and the README editor section.

### Claude's Discretion
- Panel styling, exact repeat timings, selection-ring colour, hint wording, smoke injection details.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Tab.field`/`Tab.scene` (index-aligned charges), `Workspace` factory/Failed-tab paths, `expectedLineCount`.
- `ef::kMethods` display names for the panel combo; `ImGuiPainter` draw list for the selection ring.
- The smoke's injected input pattern (middle-click test) — mouse down/move/up through `io.Add*Event`.

### Established Patterns
- Pure decisions + mutators in efapp under strict flags with doctests; the GUI maps input onto them; headless smoke exercises every new interaction through the real event path.

</code_context>

<specifics>
## Specific Ideas

- The stale hint doubles as the save hint: saving an edited tab stores its current charges regardless of staleness.

</specifics>

<deferred>
## Deferred Ideas

- Undo/redo; multi-select and box-select; charge labels; snapping/grid; per-charge colours.

</deferred>
