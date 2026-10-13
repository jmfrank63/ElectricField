# Roadmap - Milestone v2.6: Collections UX, Adaptive Zoom & Charge Identity

**Created:** 2026-10-13
**Status:** Complete (archived to milestones/v2.6-*)
**Milestone goal:** As a user, I can pick up where I left off (Recent Collections always shows what I opened, even right after an update), delete configurations from a collection without hidden knowledge, zoom into charges without ever seeing empty white areas (the calculation adapts to the view), give every charge its own drawn size and an editable name with a stable id, and see that name as a tooltip on the canvas.

## Phases

### Phase 1: Collections UX Fixes
**Goal:** Recent Collections is populated from the session pointer even when starting fresh (pre-recents pointers seed their collection), the pointer is rewritten at quit, and removing a configuration from the collection is a visible action in the chooser (selected row -> Remove..., confirmation kept; right-click keeps working).
**Depends on**: -
**Requirements**: REC-02, COL-10
**Success Criteria** (what must be TRUE):

1. A session pointer written before the recents feature (only `collection = X`) yields a Recent Collections menu containing X on the next start; a script proves it (seed a legacy pointer in the sandbox, start, `app.expect` the recents)
2. Opening collections still dedupes to the front and caps at 10 (existing 30/42/43 scripts stay green)
3. The pointer file is rewritten on quit, so closing the app without switching collections still leaves the recents current on disk
4. In the chooser, the selected configuration can be removed with a visible Remove button (confirmation dialog, open tabs unaffected); right-click removal still works; `config.remove` still works
5. `zig build test` green, smoke green, script suite green; docs updated (api.md, README)

### Phase 2: Zoom-Adaptive Calculation
**Goal:** Zooming into a charge never shows the empty untraced disk: the tab's calculation adapts to the view — seed and hit radii shrink with the zoom so lines hug the charges on screen (~1 px seed circle), the tab recalculates automatically (debounced) when the zoom settles, and the zoom-in ceiling extends to the numerics limit.
**Depends on**: Phase 1
**Requirements**: ZOOM-02
**Success Criteria** (what must be TRUE):

1. ComputeParams carries a view-adapted seed/hit scale; at the default view (100 px/unit) the radii are exactly today's values (0.25 / 1/6 world units) — the default view is unchanged
2. When the zoom settles (debounce like continuous calculation, ~0.35 s), a zoomed tab recomputes once with the adapted radii; zooming never recomputes while the wheel is still rolling
3. At high zoom the field lines visually start at the charge surface (no white annulus): a screenshot at the previous ceiling's zoom shows lines within a few pixels of the drawn charge
4. The zoom-in ceiling rises (numerics-bounded; the static cap may exceed 20 now) while the seed circle on screen stays >= ~1 px and the view still shows at least the old minimum world extent OR the seed-scale rule, whichever the planner derives
5. ef-cli parity unchanged (the CLI uses fixed default params); `zig build test` green incl. new doctests for the radius adaptation; smoke green; docs updated

### Phase 3: Charge Identity & Per-Charge Size
**Goal:** Every charge carries a stable id, an editable name and its own drawn size; the Options charge size becomes the default for new charges only; hovering a charge shows its name as a tooltip; everything persists in collections and sessions and is API-callable.
**Depends on**: Phase 2 (rebases cleanly on the adapted-compute core)
**Requirements**: CHARGE-01, CHARGE-02
**Success Criteria** (what must be TRUE):

1. ef::Charge carries id (stable integer), name (string) and size (drawn radius px, 0 = inherit the default); presets and new charges get sensible defaults ("Charge N", default size)
2. Collection files and session files persist id/name/size in a backward-compatible way (old 3-field `charge = v, x, y` lines still parse; new fields are optional)
3. The editor panel edits the selected charge's name and size; View > Options' charge size applies to NEW charges only (and stays the fallback for charges without an override)
4. Hovering a charge on the canvas shows its name as a tooltip
5. API: `charge.name <i> <name>`, `charge.size <i> <px>`; `app.state` shows per-charge name/size
6. `zig build test` green (incl. round-trip doctests for the extended formats), smoke green, script suite extended and green; docs updated (api.md, README)

## Notes

- Owner instruction: GSD auto-mode, always take the recommended answer (UAT gate auto-accepted and recorded); implement with subagents.
- Design choices: the seed/hit adaptation is GUI-side (ComputeParams gains a scale; ef-cli keeps fixed defaults so parity is untouched); per-charge size is a DRAW property (the physics hit radius keeps using the effective radius: charge hit radius = max(default, per-charge)? — planner decides with parity in mind); charge ids are persisted integers assigned max-id+1 per collection; the name tooltip is drawn by the ImGui layer from the scene-charge index.
