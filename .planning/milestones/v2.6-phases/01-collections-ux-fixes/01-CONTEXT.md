# Phase 1: Collections UX Fixes - Context

**Gathered:** 2026-10-13
**Status:** Ready for planning
**Mode:** Auto-generated (smart discuss, auto-accepted answers per standing auto-mode instruction)

<domain>
## Phase Boundary

Two owner-reported defects:

1. **REC-02** - "Recent collections does not show what has been opened when starting new": the File >
   Recent Collections menu (shipped 261012-h26) is empty on the first start after the update, because a
   session pointer written by earlier versions carries only `collection = <name>` and no `recent` lines.
2. **COL-10** - "We cannot delete a configuration from a collection": removal exists but is hidden
   (right-click a row inside the configuration chooser, v2.3 COL-05); owners never find it.

Out of scope: any change to the recents cap/order semantics, config.remove command semantics, the
collection file format.

</domain>

<decisions>
## Implementation Decisions (recommended answers, auto-accepted)

### REC-02 seeding
- Root cause: `App::init` loads `recents_` exclusively from `recent` lines of session.ini
  (src/gui/app.cpp, the `recents_ = efapp::loadSessionFull(sessionPath_).recents;` block). A pre-h26
  pointer has no such lines.
- Fix: after loading, if the pointer's `collection` value is non-empty and resolves to an existing
  collection file (names resolve via `App::collectionFile`, the only form the pointer ever stores) and it
  is not already in `recents_`, insert it at the FRONT (it is by definition the most recently opened).
- Persist at quit: `App::runInteractive`'s shutdown block (where `saveSessionNow()` runs) also calls
  `writeSessionPointer()` so the on-disk list is current even when the session ended without a
  collection switch. `writeSessionPointer` must never BLANK the pointer's `collection` line: when
  `currentCollection_` is empty it preserves the previously stored value (read-modify-write via
  `loadSessionFull(sessionPath_).collection`). (Today's relaxed guard writes an empty name - that would
  erase the very datum REC-02 seeds from.)
- Testable headlessly: a script seeds a legacy pointer (only `collection = Original`) + the seeded
  Collections/Original.ini in its sandbox user dir, starts, and expects `recents=Original` in app.state.

### COL-10 visible removal
- In `chooserModal` (src/gui/ui.cpp): add a `Remove...` button under the list, enabled only when the
  selected row is a configuration of the collection (`ui.chooserSel < cfgCount`; the last row is
  "Empty Configuration"). Clicking sets the existing `ui.removeConfigName` / `ui.removeConfigOpen`
  confirmation flow (removeConfigModal already handles confirm/cancel).
- Keep the right-click removal and the `config.remove` command exactly as they are.
- The chooser's Open button stays default-focused behaviour-wise; do not restructure the modal.

</decisions>

<code_context>
## Existing Code Insights

- Recents: `App::noteRecentCollection` (dedupe-front, cap 10), `recentMenus` (labels), the frame()
  mirror `ui_.recentCollections`, and the pointer writer `App::writeSessionPointer` (session.ini via
  efapp::saveSession with recents). Persistence round-trip is covered by tests/test_session_file.cpp and
  scripts 30/42/43 (all green) - do not regress them.
- The chooser: `chooserModal` renders rows as Selectables inside a framed child; `removeConfigModal`
  shows the "Remove configuration?" confirmation and sets `ui.removeConfigApply`.
- Script tests run headless with `--script <file> --user-dir <dir>`; CI pairs share user dirs
  (see .github/workflows/ci.yml, 41->40 and 43->42 mapping).

</code_context>

<specifics>
## Specific Ideas

- Add the REC-02 script as e.g. tests/scripts/44-recents-seed.efscript (own sandbox; seed a legacy
  session.ini with `collection = Original` + no recents before... note: scripts cannot pre-seed files -
  instead the seeding path is unit-testable? App is SDL-bound, so: cover the pure decision in a doctest
  if feasible (a small helper `seedRecents(recents, pointerCollection, existsFn)`) and/or verify
  interactively via smoke. The planner picks the test seam; a doctest on a pure helper is the
  recommended shape.)

</specifics>

<deferred>
## Deferred Ideas

None.

</deferred>
