---
gsd_state_version: 1.0
milestone: v2.6
milestone_name: collections-ux-adaptive-zoom-charge-identity
status: Executing phases
last_updated: "2026-10-13T00:00:00.000Z"
last_activity: 2026-10-13
last_activity_desc: Milestone v2.6 created from owner feedback (5 items)
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
stopped_at: Milestone created, starting Phase 1
current_phase: 1
current_phase_name: Collections UX Fixes
---

## Current Position

Phase: 1 (Collections UX Fixes)
Plan: —
Status: Starting
Last activity: 2026-10-13 — Milestone v2.6 created from owner feedback (5 items)

## Operator Next Steps

- Run autonomous execution (auto-mode, subagents per phase)

## Decisions (auto-answered per standing auto-mode instruction)

- REC-02: seed the recents from the session pointer's collection at startup; rewrite the pointer at quit.
- COL-10: visible Remove button for the selected chooser row; right-click removal stays; config.remove unchanged.
- ZOOM-02: view-adapted seed/hit radii + debounced recalculation on zoom settle; CLI/parity untouched.
- CHARGE-01/02: id/name/size on ef::Charge; backward-compatible collection/session formats; Options size = default for new charges.
