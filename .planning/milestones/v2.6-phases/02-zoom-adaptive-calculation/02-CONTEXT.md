# Phase 2: Zoom-Adaptive Calculation - Context

**Gathered:** 2026-10-13
**Status:** Ready for planning
**Mode:** Auto-generated (smart discuss, auto-accepted answers per standing auto-mode instruction)

<domain>
## Phase Boundary

Owner report: "We can still zoom in to show white areas." The h25 zoom ceiling (>= 1 world unit visible)
bounds but does not eliminate the uncalculated disk around each charge, because the calculation starts at
the FIXED seed circle (radius 0.25 world units) and ends at the FIXED hit radius (1/6 world units) while
the drawn charge stays a fixed ~25 px dot. Measured: at zoom 8 the white annulus around a charge is
~200 px; at the old zoom-20 ceiling it filled the screen.

Goal: the calculation ADAPTS to the view - seed/hit radii shrink with the zoom so field lines start and
end at the charges' drawn edge on screen - with a debounced automatic recalculation, and the zoom-in
ceiling rises to what the numerics allow.

Out of scope: changes to the tracer's step sizes/thinning thresholds (they are arc-length based and
already zoom-independent), changes to ef-cli defaults, changes to what the user edits as "diameter of
the calculation" (the seed circle on screen is ~1 px - the smallest sensible; larger circles are
harmless, sub-pixel ones are pointless - owner's words).

</domain>

<decisions>
## Implementation Decisions (recommended answers, auto-accepted)

### Adapted radii
- `ef::ComputeParams` gains a view scale (device px per world unit the computation should target;
  0/unset = legacy). `seedRadius()` returns `clamp(kSeedPx / viewScale, floor, 0.25)` and `hitRadius()`
  `clamp(kHitPx / viewScale, floor, 1/6)` with kSeedPx ~ 2 (a ~2 px seed circle; owner's "diameter of 1,
  the smallest" in screen terms), kHitPx = chargeRadiusPx / 1.5 (= 16.7: endpoints stay hidden behind
  the 25 px drawn dot). At viewScale 100 both clamp to today's exact values - the default view is
  bit-identical. A floor (e.g. 1e-4 world) guards the degenerate case.
- ef-cli passes no view scale: parity untouched.

### Recompute trigger
- When the zoom CHANGES, arm the tab's existing pendingRecalcAt deadline (the CALC-03 debounce,
  kAutoRecalcDelay = 0.35 s). Zooming continuously keeps re-arming; one recompute fires when the wheel
  settles. The recompute passes the CURRENT device ppu (100 * k * zoom) as the view scale. A
  view-scale-driven recompute is NOT an edit: it must not set stale/dirty and must not create undo
  entries.
- Reset view / zoom back to ~100 px/unit: the tab recomputes with the legacy radii (same mechanism, the
  clamp makes the params identical).

### Zoom ceiling
- With lines hugging the charges the h25 "1 world unit" rule can be relaxed: the new ceiling derives
  from keeping the seed circle >= ~1 px on screen AND from the equipotential budget below.

### THE TRAP - equipotential explosion (must be addressed)
- Potential at the seed circle is V ~ q/r_seed. Shrinking r_seed 100x multiplies V by 100x;
  `shouldPostEquipotential` posts one ring per interval crossing along the line, so the ring count
  between the seed and V=4 grows proportionally (from ~36 to thousands of equipotential jobs per line ->
  time/memory blow-up), AND rings whose on-screen spacing is sub-pixel render as a solid blob anyway.
- Recommended rule: only post rings whose on-screen spacing is at least ~1 px: with ring spacing
  dr = r^2 * interval, post labels only while `r^2 * interval * viewScale >= kRingPx` (kRingPx ~ 1.25).
  Equivalently cap the posted label at `V_max = sqrt(viewScale * interval / kRingPx)` per unit charge -
  the planner derives the exact per-charge form (V ~ q/r is per-charge; the existing label logic is
  generic in the tracer, so the cap likely lands in `shouldPostEquipotential` as a view-scale-aware
  interval: effectiveInterval = max(interval, kRingPx * r^2 * viewScale ... ) - pick ONE formulation,
  document it, and doctest it). Inside the cap the field lines alone fill the space (they start ~2 px
  from the charge), so no white remains.
- The zoom ceiling then follows from the equipotential budget: bound the worst-case ring count per line
  (the planner derives the closed form; target: at the new ceiling the total jobs stay within ~4x of
  today's zoom-1 worst case).

</decisions>

<code_context>
## Existing Code Insights

- Seeds: ef/seeds.cpp `planSeeds` uses `p.seedRadius()`; hits: `field.HitCharge(point, p)` uses
  `p.hitRadius()` (ef/params.hpp, ef/charge_system.hpp). The tracers (ef/tracer.hpp) parametrize by
  arc length with NORMALIZED E - no field-magnitude blow-up from close seeds; steps are
  h=0.001 (world) near origin.
- Equipotential posting: `shouldPostEquipotential` (ef/tracer.hpp) with the interval from
  ComputeParams; jobs spawn per field line (ef/session.cpp).
- View plumbing: `drawCanvas` (src/gui/ui.cpp) computes view.pixelsPerUnit = 100*k*zoom; the h25 ceiling
  lives in editor::zoomCeiling (src/app/editor.{hpp,cpp}) applied in ui.cpp + the view.zoom command.
- Debounce precedent: Tab::pendingRecalcAt + autoRecalcTick (src/app/workspace.{hpp,cpp},
  armAutoRecalc in ui.cpp). Reuse the exact mechanism for zoom settle.
- Doctest seams: ComputeParams is a pure value type (test_field/test_seeds patterns); Workspace accepts
  a fake SessionFactory (test_editor.cpp dipoleTab/ThrowFactory) - recalc-on-zoom-settle is testable
  without a GUI. Rendered "no white ring" is verifiable via renderPng pixel sampling in tests_gui
  (test_render_ctx.cpp pattern) if the planner wants a pixel-level pin.

</code_context>

<specifics>
## Specific Ideas

- Screenshot proof for the success criterion: render the Dipole at ppu 800 (old zoom-8 case) with an
  adapted compute into tests_gui via renderSurface and assert pixels near the charge edge are
  non-background along a sampling ring; keep it tolerant (sampling, not exact bytes).
- Update the h25 docs (README zoom bullet, api.md view.zoom) to the new ceiling story.

</specifics>

<deferred>
## Deferred Ideas

None.

</deferred>
