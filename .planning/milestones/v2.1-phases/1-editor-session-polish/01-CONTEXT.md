# Phase 1 (v2.1): Editor & Session Polish - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous (owner: auto-mode, recommended answers everywhere)

<domain>
## Phase Boundary

Completes the editor round-trip and adds workspace persistence: an Add-charge section in the panel (value + position, no canvas needed), arrow glyphs on the stepper buttons, per-tab renaming, and a `session.ini` that stores the open tabs - including each tab's calculation method - restoring them on startup, plus a Help entry that reveals the configuration folder.

Not in this phase: multi-select, undo/redo, autosave timers, per-tab draw styles, window-geometry persistence.

</domain>

<decisions>
## Implementation Decisions

- **D-01:** Add-charge section (EDIT-06): panel fields Value (int, default 1), X and Y (double, default 0) with an Add button -> `editor::addCharge` (appends + selects + stale). The values persist in UiState across frames.
- **D-02:** Stepper arrows (EDIT-07): the six -/+ buttons become arrow buttons - triangles drawn over 22 px buttons via the window draw list (the default font has no arrow glyphs): left/right for X, up/down for Y (y-up world: up increases), down/up for Value. Same press-and-hold repeat as before.
- **D-03:** Rename (TAB-01): `Workspace::rename(serial, name)` sets the exact trimmed title (empty refused, no counter suffix added); the panel top gets a Name field bound to the ACTIVE tab (buffer re-syncs when the active tab changes); the window title follows automatically. Renaming never marks stale (lines are unaffected) and never touches stored user configurations.
- **D-04:** Session file (SES-01/02): `session.ini` next to options.ini, sections `[tab "Title"]` with `preset = <index | none>`, `method = <kMethods name>`, one `charge = v, x, y` per charge. Saved on graceful quit (interactive only), restored at startup: tabs re-open with their EXACT saved titles, params.method from the file (unknown name -> default), charges as saved. Restored titles bump the preset/user counters so later opens never duplicate a restored title. Headless modes never read or write it.
- **D-05:** `src/app/session_file.{hpp,cpp}` (std-only) mirrors the configs model: `SavedTab {title, preset, method, charges}`, serialize/parse/save/load with skip-and-count rules; charge lines share the configs grammar (shared `ini_text.hpp` helpers factor the trim/parse utilities out of configs.cpp).
- **D-06:** Help > Open Configuration Folder (SES-03): `SDL_OpenURL` on a file:// URL of the pref directory; on failure the path is copied to the clipboard and noted (same pattern as the About links).

### Claude's Discretion
- Exact session section syntax, arrow triangle proportions, panel ordering, hint wording.

</decisions>

<code_context>
## Existing Code Insights

- `editor::addCharge(t, world, value)` already implements the append+select+stale semantics the button needs.
- `configs.{hpp,cpp}`: the ini grammar and helpers to factor/reuse; `Workspace::spawn`/`open` show the creation path; `ef::kMethods` names for method persistence.
- Smoke injection patterns cover button press flows; `SDL_OpenURL` is already used by the About links.

</code_context>

<deferred>
## Deferred Ideas

- Autosave interval, window geometry persistence, multi-select, per-tab draw styles.

</deferred>
