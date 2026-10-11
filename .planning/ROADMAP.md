# Roadmap — Milestone v2.3: Editor Depth (title, collections UX, undo/redo, grid, continuous fix)

**Created:** 2026-10-11
**Status:** Ready for planning
**Milestone goal:** As a user, I want the window title to show the collection, to remove configurations from a collection, a proper New menu, a working continuous calculation, Esc reverting uncalculated edits with Undo/Redo in the menu, and a snap grid (Cartesian or polar, origin-definable, independently active and visible).

## Phases

### Phase 1: Editor Depth
**Goal:** All of the above, with the continuous-calculation regression fixed and pinned by a smoke test.
**Mode:** mvp
**Depends on**: -
**Requirements**: TITLE-01, COL-05, MENU-01, CALC-04, EDIT-11, EDIT-12, EDIT-13, GRID-01..04
**Success Criteria** (what must be TRUE):

1. The window title shows the collection (with a tab: "Electric Field - <Collection> - <Tab>"; without: "Electric Field - <Collection>")
2. Right-clicking a configuration in the chooser offers Remove (with confirmation); the configuration is deleted from the collection file; open tabs are unaffected
3. The File menu shows "New Configuration..." (opens the chooser) and "New Collection..." - the bare "New"/"New..." entries are gone
4. Continuous calculation works: after an edit (and only then), the tab recomputes once the 0.35 s debounce passes - pinned by a smoke regression test
5. Esc on a stale tab reverts the charges to the last calculated state; Edit > Undo (Ctrl+Z) / Redo (Ctrl+Y) steps through charge and method changes per tab
6. A grid can be enabled (snapping) and shown (rendering) independently, is Cartesian or polar, and its origin is definable; snapping applies to drag, double-click add and typed positions; the session persists grid settings
7. CI stays green on all three OSes plus TSan; smoke stays green

**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Editor Depth | 2/2 | Complete | 2026-10-11 |

## Notes

- Owner instruction: GSD auto-mode, always take the recommended answer (UAT gate auto-accepted and recorded).
- The interaction contract stays: double-click empty canvas adds a charge in one go; single click selects it for editing.
