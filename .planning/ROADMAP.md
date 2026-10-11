# Roadmap — Milestone v2.5: Collections Flow, Grid Style & Charge Size

**Created:** 2026-10-12
**Status:** Ready for planning
**Milestone goal:** As a user, I want the collection-first flow (Open Collection before configurations), a light-green grid with an intersections-only mode in a distinct colour, and an adjustable charge size from a tiny dot to very large.

## Phases

### Phase 1: Collections Flow & Grid/Size Options
**Goal:** All of the above, API-callable and session-persisted where applicable.
**Mode:** mvp
**Depends on**: -
**Requirements**: OPEN-01, GRID-05..07, SIZE-01
**Success Criteria** (what must be TRUE):

1. File > Open Collection... (Ctrl+O) shows a collection chooser (Original first, then alphabetical, plus New Collection...); starting with no restored tabs shows this chooser instead of the configuration chooser; after a UI-initiated switch to an empty workspace the configuration chooser follows; opening a collection never auto-opens configurations (but a collection's saved session tabs come back)
2. Grid lines are light green; in intersections-only mode only the lattice points are drawn (Cartesian and polar), in a distinctly different colour; switchable in the panel, via `grid.set intersections on|off`, and persisted per tab in the session
3. Charge size is a draw option: 2-120 px (dot to very large), default 25; settable in View > Options (Draw tab) and via `options.set charge-size`; persisted in options.ini; the physics hit radius is unchanged (parity preserved)
4. CI green on all three OSes plus TSan; smoke green; script suite updated and green; docs updated (api.md, README)

**Plans**: TBD

## Notes

- Owner instruction: GSD auto-mode, always take the recommended answer (UAT gate auto-accepted and recorded).
- Design choices (recommended): "Open Collection" is a chooser modal matching the configuration-chooser pattern (Enter/double-click/Esc/OK); the collection file-per-collection layout (v2.2) is untouched; charge size changes the DRAWN radius only (DrawStyle.chargeRadiusPx), not the physics seed/hit radius (ComputeParams.chargeRadiusPx stays 25 for parity).