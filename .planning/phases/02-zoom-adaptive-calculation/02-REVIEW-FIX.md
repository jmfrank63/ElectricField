---
phase: 2
fixed_at: 2026-10-14T00:00:00Z
review_path: .planning/phases/02-zoom-adaptive-calculation/02-REVIEW.md
iteration: 1
findings_in_scope: 7
fixed: 7
skipped: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-10-14
**Source review:** `.planning/phases/02-zoom-adaptive-calculation/02-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 7 (5 warnings + 2 info, per the fix brief's numbering)
- Fixed: 7
- Skipped: 0

All fixes applied in the main checkout (`workflow.use_worktrees: false`); verification ran there (the tree the commits live in), so the numbers below are reproducible from `master` directly.

## Fixed Issues

### WR-03: viewScale must be LOGICAL (zoom-relative), not device

**Files modified:** `src/core/ef/params.hpp`, `src/gui/ui.cpp`, `src/core/ef/tracer.hpp`
**Commit:** af3cf32
**Applied fix:** The arming block stamps `100.0 * t.zoom` (logical, k-free) instead of `view.pixelsPerUnit` (100·k·zoom). `kRefPpu = 100` now identifies the default view at every DPI; kSeedPx / the 1.5x hit target / kRingPx are logical px, consistent with the logical dot radius. params/tracer comments updated to the logical semantics; the render-proof test passes viewScale directly (unchanged, numbers still valid); README/api.md said nothing wrong about viewScale ("device pixels" in api.md refers to `view.pan`, which is genuinely device px).

### WR-02 + IN-02: zoom ceiling from the applied dot; dead parameter dropped

**Files modified:** `src/app/editor.hpp`, `src/app/editor.cpp`, `src/gui/ui.cpp`, `src/gui/app.cpp`, `tests/test_editor.cpp`, `README.md`
**Commit:** 3ac8da5
**Applied fix:** `zoomCeiling(ppuBase, dotRadiusPx) = clamp(dotRadiusPx / kMinSeedWorld / ppuBase, kZoomMin, kZoomMax)` — the floored seed circle must fit inside the user's actual dot. `kMaxPpu` deleted; the dead `minCanvasPx` slot (IN-02) removed; all three call sites pass base 100 and the unscaled `options.draw.chargeRadiusPx` (drawCanvas / zoom buttons via `env.options`, view.zoom via `options_.draw`), making the ceiling display-scale-independent (README's "12.5x on a 2x HiDPI display" note updated). Doctests: dot 25 → 25, dot 2 → 2, base 200 → 12.5, degenerate → kZoomMax, huge dot → kZoomMax.

### WR-01: regime-guard epsilon

**Files modified:** `src/core/ef/params.hpp`, `src/gui/ui.cpp`
**Commit:** 36c4b79
**Applied fix:** Chosen mechanism: epsilon **band + snap**. A settled ppu in `(kRefPpu, kRefPpu*(1+1e-9)]` (`kRefPpuEps`, new constant in params.hpp) snaps down to exactly `kRefPpu` before the guard comparison AND before stamping, so the guard, a zoom-settle recalc, an edit recalc and F5 are all regime-stable (the guard-epsilon alone would have left a band-valued stamp that `adapts()` treats as adapted); the `params.viewScale` side of the guard also compares against the epsilon edge. `adapts()` stays strict. Note: a guard-only epsilon (the review's primary snippet) would still let an edit-recalc stamp a band value and adapt at the default view — the snap closes that hole, which is why it was picked.

### WR-04: deterministic command/script exports

**Files modified:** `src/gui/app.cpp`, `tests_gui/test_render_ctx.cpp`
**Commit:** be49188
**Applied fix:** `exportConfigurationTo` recomputes synchronously from the tab's charges with the tab's params and `viewScale = 0` (method/interval/line count preserved; `wait()` fine offline) instead of rendering the possibly adapted cached scene. `exportCollectionTo` already recomputed fresh — the default view scale is now spelled out defensively. `runExport` keeps the cached scene BY DESIGN (exports what is on screen) with a comment saying so. Script byte-comparison not cheap (no file-compare verb in the script framework) → per the fix brief's fallback, the contract is pinned at the compute level: new doctest proves two fresh legacy computes are bit-identical regardless of history and that the zoomed cache really differs (tests_gui suite 5 → 6).

### WR-05: pins for the arming composition and the ring anchor density

**Files modified:** `src/app/editor.hpp`, `src/app/editor.cpp`, `src/gui/ui.cpp`, `tests/test_editor.cpp`, `tests/test_tracer.cpp`
**Commit:** ec0514e
**Applied fix:** (a) `editor::armViewRecalc(Tab&, double logicalPpu)` — std-only, encapsulates Done-only, first-frame guard, WR-01 band/snap, regime disarm; ui.cpp calls it with `100.0 * t.zoom`. Doctests: not-Done never arms/stamps; first frame never arms; identical re-frame inert; zoom change arms -2; arm → viewRecalcTick fires exactly once with the stamped scale (no stale/dirty/undo, deadline intact across redraws); zoom back into the legacy regime disarms; band value snaps to exactly kRefPpu; genuinely-above-band arms. (b) New tracer doctest: same loop traced legacy vs viewScale 800 — anchor count > 4x legacy and every recorded segment ≥ 8 target-view px (thinning contract), closing the recordPx regression gap.

### IN-01: hitRadius simplification

**Files modified:** `src/core/ef/params.hpp`
**Commit:** e18a961
**Applied fix:** Adapted branch reduced to `return std::min(legacy, seedRadius());` (the inner clamp tier was unreachable) and the comment corrected: the seed circle is the binding constraint at every adapted viewScale and the regime switch is intentionally a jump, not continuous shrinkage.

## Verification (all run after ALL fixes, in the main checkout)

| Check | Result |
|---|---|
| `zig build test` | 142/142 core cases + 6/6 GUI cases, SUCCESS (was 139 + 5; +3 arming doctests, +1 tracer doctest, +1 export-recompute doctest) |
| `zig build` | clean, rc=0 |
| Parity oracle (`ef-cli --all --expect tools/parity/expected-counts.csv`) | exit 0, 9/9 presets match, no PARITY FAIL |
| Script suite (CI loop, fresh sandboxes, 44 printf pre-seed) | exit=0, 191 `[ok]` assertions |
| `zig build smoke` ×2 | both ok (frames=288, tabs_opened=2, editor extras ok) |

## Deliberately not done

- **Script-level export byte-comparison** (WR-04): the script framework has no file-compare verb; adding one is out of scope for a review fix. Covered by the helper-level doctest instead, as the fix brief allows.
- **`git commit --amend` of a typo** in the WR-05 commit message ("a pppu"): commit messages are not rewritten after the fact.

---

_Fixed: 2026-10-14_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
