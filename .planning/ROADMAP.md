# Roadmap — Milestone v2.2: Collections

**Created:** 2026-10-11
**Status:** Ready for planning
**Milestone goal:** As a user, I want to organise my configurations into collections: the Original collection holds everything that exists today, a new collection starts empty so I can build it up, and a Calculate All opens every configuration of the current collection in tabs.

## Phases

### Phase 1: Collections
**Goal:** As a user, I want to group my configurations into collections stored as one file per collection, switch between them, and open a whole collection at once.
**Mode:** mvp
**Depends on**: —
**Requirements**: COL-01, COL-02, COL-03, COL-04, EDIT-08, EDIT-09
**Success Criteria** (what must be TRUE):

1. Collections live as one human-readable file per collection under `Collections\` in the user config directory; on first run the Original collection is seeded with the 9 built-in presets plus any legacy user configurations from configurations.ini (which is then left untouched)
2. The menu bar shows the current collection with a switcher; File > New Collection… creates and switches to an empty collection (no tabs, no configurations); switching saves the current session and closes its tabs
3. Calculate All (menu-bar button and File menu) closes the open tabs and opens every configuration of the current collection in tabs, each computing as usual
4. The session file records the current collection and restores it (with its tabs) on startup; Save As stores into the current collection file; CI stays green on all three OSes plus TSan; smoke stays green
5. The New-charge entry is always visible in the panel; a switch-sign button flips the selected charge's value and marks the tab stale

**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Collections | 2/2 | Complete | 2026-10-11 |

## Notes

- Owner instruction: GSD auto-mode, always take the recommended answer (UAT gate auto-accepted and recorded).
- Design choices (recommended): one .ini file per collection under `Collections\` (not a folder per collection), reusing the configurations grammar; per-configuration method stays a per-TAB property (session), not a collection property; File > New keeps the 9 built-ins as quick access while the chooser shows the current collection.
