# Roadmap — Milestone v2.1: Editor & Session Polish

**Created:** 2026-10-11
**Status:** Ready for planning
**Milestone goal:** As a user, I want an explicit add-charge button (with value and position), arrow glyphs on the stepper buttons, tab renaming, and my open tabs — including each tab's calculation method — persisted in a session file that restores on startup.

## Phases

### Phase 1: Editor & Session Polish
**Goal:** As a user, I want the editor round-trip completed: add charges without the mouse, readable stepper buttons, renamed tabs, and my workspace back after a restart.
**Mode:** mvp
**Depends on**: —
**Requirements**: EDIT-06, EDIT-07, TAB-01, SES-01, SES-02, SES-03
**Success Criteria** (what must be TRUE):

1. The editor panel has an Add-charge section (value, x, y inputs + Add button) that appends and selects the new charge without the canvas
2. The six stepper buttons render as arrows (left/right for X, up/down for Y and Value)
3. The active tab can be renamed from the editor panel; the window title follows; an empty name is refused
4. Open tabs (title, source, charges) persist in `session.ini` in the user config directory and restore on startup; each tab's calculation method is part of that file; counters never produce duplicate titles after a restore
5. Help > Open Configuration Folder reveals the folder holding options.ini / configurations.ini / session.ini; CI stays green on all three OSes plus TSan; smoke covers rename and the session round-trip

**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Editor & Session Polish | 0/TBD | Not started | - |

## Notes

- Owner instruction for this milestone: GSD auto-mode, recommended answers everywhere (the UAT gate is auto-accepted and recorded as such).
- Session persistence saves on graceful quit and restores at startup (interactive modes only; headless never reads or writes it).
