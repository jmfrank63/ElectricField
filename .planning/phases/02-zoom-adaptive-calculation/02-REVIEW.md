---
phase: 2
reviewed: 2026-10-13T18:13:05Z
depth: deep
files_reviewed: 20
files_reviewed_list:
  - src/core/ef/params.hpp
  - src/core/ef/tracer.hpp
  - src/core/ef/charge_system.hpp
  - src/core/ef/seeds.cpp
  - src/core/ef/finalizer.cpp
  - src/app/workspace.hpp
  - src/app/workspace.cpp
  - src/app/editor.hpp
  - src/app/editor.cpp
  - src/app/ui_logic.cpp
  - src/app/options.hpp
  - src/app/session_file.hpp
  - src/gui/ui.cpp
  - src/gui/ui.hpp
  - src/gui/app.cpp
  - src/cli/main.cpp
  - tests/test_seeds.cpp
  - tests/test_tracer.cpp
  - tests/test_editor.cpp
  - tests_gui/test_render_ctx.cpp
findings:
  critical: 0
  warning: 5
  info: 2
  total: 7
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-10-13T18:13:05Z
**Depth:** deep
**Files Reviewed:** 20
**Status:** issues_found

Commits reviewed: f6ad731, fa7ad01, c78a897, 4c59ead, 7495240, 91f3b61 (`git show` per commit) plus all surrounding code reached through the call chains (seeds, charge_system, finalizer, ui_logic, options, session_file, cli, render/export paths, and the new tests).

## Summary

The phase is solidly built. The adapted formulas in `params.hpp` are clamped correctly, every adapted branch is unreachable at viewScale 0/<=100 (verified through `adapts() = viewScale > kRefPpu` and confirmed by the bit-identical-posting doctest and the ef-cli parity oracle), and the view-recalc deadline protocol is *sound* — I specifically tried to break it with "a zoom that happens during a recalc" and could not: `params.viewScale` always equals `armedViewPpu` at the moment of a compute (both are written only together, on Done frames, in ui.cpp:125-126), so any post-compute ppu deviation re-arms and the tab self-heals. No Critical issues found.

The five warnings are hardening/consistency gaps, none of which breaks the k=1 default-view contract: an fp-round-trip reliance in the regime guard (verifier-flagged, agreed), a ceiling that silently assumes the default 25 px dot (breaks for small `charge-size` settings), a HiDPI regime flip on first recalc at k >= 1.25, zoom-history-dependent exports, and the two test gaps the verifier already flagged (with concrete cheap pins proposed below).

Verdicts on the six review-brief questions:

1. **Adapted formulas — correct.** Clamp order is safe (no `std::clamp` UB: the upper bound `chargeRadiusWorld()` is fixed at 0.25 because `ComputeParams::chargeRadiusPx` is never written from user options — the user-settable 2..120 px size lives only in `DrawStyle`). The hit<=seed cap is enforced by the outer `std::min(..., seedRadius())` and holds over the whole range (sweep doctest). Legacy short-circuits: `adapts()` is strict `>`, so viewScale 0 and 100 both return the legacy expressions verbatim; the gate's `p.adapts() && ...` and `recordPx = adapts() ? viewScale : px` are equally gated. Caveat: the hit-radius "shrinks continuously out of the legacy value" comment is false — see IN-01.
2. **Tracer gate — short-circuited, labels lost by design.** `field.E(point)` is evaluated only after `p.adapts()` passes AND the label condition holds (a ring would otherwise be posted) — not per step; legacy pays zero. When a post is SKIPPED, `oldPotential` has already advanced and the skipped label is never re-posted (it is lost, intentionally); label continuity is preserved and the next posted ring's label stays within one interval of the potential at the point it is drawn at — the same ±interval slop legacy has. The hot-loop note: in the adapted regime near charges the label advances nearly every step, so those steps pay two N-body evaluations (Potential + E) — bounded, correctness-neutral, performance out of scope for v1.
3. **viewRecalcTick — protocol consistent.** Same -1/-2/deadline markers as `pendingRecalcAt`; -2 converts exactly once (idle frames cannot re-arm); a deadline cannot survive into Computing (the only path to Computing is `calculate()`, which disarms `pendingViewRecalcAt` at workspace.cpp:96 before starting the session); fire-while-computing is double-guarded (`:127` skip + `:92` refuse); tab close destroys the deadline with the tab; explicit Calculate honours and disarms. "Missed zoom during a recalc": no — see Summary above; zooms during Computing are caught on the first Done frame because `armedViewPpu` is frozen at the pre-fire value and the ppu-equality check then fails. All touch points (`drawCanvas`, `App::frame`, ticks) are UI-thread; workers only get a params copy. Correctness verified against test_editor.cpp:239-320.
4. **Arming block — O(1) per frame** (two double compares + two stores on Done tabs), first-frame guard correct (`armedViewPpu != 0`), Done-only, correctly placed before `viewControls` so the same frame's wheel re-arms next frame. The regime guard's fp round-trip reliance is real but latent — WR-01.
5. **Ceiling — correct, one blind spot.** Degenerate inputs fall back to kZoomMax (40); kMaxPpu/base ∈ [6.25, 33.33] for all legal clamped scales, so kZoomMax only binds pre-first-frame (and the first frame re-clamps). No stale `kMinViewWorld` references remain; sessions do not persist zoom or viewScale (session_file.hpp:28-36), so no restored >20 zoom can leak in; `app.state` prints `zoom=` informationally with no script asserting the old 20. The blind spot: kMaxPpu=2500 hardcodes the default 25 px dot — WR-02.
6. **Tests — two gaps, cheapest pins proposed in WR-05.**

## Narrative Findings (AI reviewer)

### Warnings

#### WR-01: Regime guard relies on exact IEEE round-trip of the zoom factor

**File:** `src/gui/ui.cpp:122-124`
**Issue:** `sameRegime` disarms only when `view.pixelsPerUnit <= kRefPpu` *exactly*. If a wheel factor's round-trip ever drifts above 1.0 (`f*(1/f) != 1.0` — true for some factor/value combinations even though all six shipped factors round-trip exactly at zoom 1, per the verifier's check), a zoom settling back to the default view lands at ppu `100 + ε`, fails the guard, and arms an adapted recalc at ~100 ppu: `adapts()` is true, so the seed circle collapses 0.25 → ~0.02 world and the ring gate drops innermost rings (|E| > ~8) relative to the legacy computation the user expects at the default view. Deterministic today, but one factor change away from a silent rendering regression.
**Fix:** Treat a hair above the reference as legacy, consistently with `adapts()`:
```cpp
inline constexpr double kRefPpuEps = kRefPpu * (1.0 + 1e-9);
const bool sameRegime = view.pixelsPerUnit <= kRefPpuEps &&
                        (t.params.viewScale == 0 || t.params.viewScale <= kRefPpuEps);
```
(Keep `adapts()` strict — the epsilon only needs to cover the *guard*, since a ppu in `(100, 100*(1+1e-9)]` computes bit-near-legacy anyway; alternatively snap `viewPpu` down to `kRefPpu` when it is within the band before stamping.)

#### WR-02: kMaxPpu = 2500 assumes the default 25 px dot; small `charge-size` settings re-expose the white annulus

**File:** `src/app/editor.hpp:65` (also `src/core/ef/params.hpp:32-35`, `src/app/editor.cpp:227`)
**Issue:** The ceiling is justified as "the drawn dot (25 px) must cover the floored seed circle (0.01 world)". But the drawn dot is user-settable: v2.5 SIZE-01 allows `charge-size` 2..120 px (`options.hpp:49`), and it lives in `DrawStyle` only — the physics seed floor stays 0.01 world unconditionally. The floored seed circle on screen is `0.01 * ppu` device px; the dot is `s * k` px, and ppu = `100*k*zoom`, so the annulus appears whenever `zoom > s` (independent of k). With the option's minimum (s = 2, "from a dot" is an advertised setting), every zoom above 2x — i.e. the entire adapted range up to the ceiling 25x — shows an untraced white ring around a 2 px dot, exactly the artifact this phase exists to kill, plus adapted line *endpoints* at 0.01 world (up to 25 px) floating in space, uncovered by the dot.
**Fix:** Derive the ceiling from the applied dot size, e.g. clamp the effective max ppu to `dotPx / kMinSeedWorld`:
```cpp
// editor.cpp — new overload or replaced parameter (the dead minCanvasPx slot is free, see IN-02):
double zoomCeiling(double dotPx, double pixelsPerUnitBase) {
  const double maxPpu = !(dotPx > 0.0) ? kMaxPpu : std::min(kMaxPpu, dotPx / ef::kMinSeedWorld);
  ...
  return std::clamp(maxPpu / pixelsPerUnitBase, kZoomMin, kZoomMax);
}
```
with call sites passing `options_.draw.chargeRadiusPx * k` (app.cpp already has `k_`/`env.k` in all three call sites). Alternatively, document the small-dot limitation in README if a ceiling that shrinks with a draw option is deemed surprising.

#### WR-03: At HiDPI k >= 1.25 the default view flips to the adapted regime on the first recalc — same view, two renderings depending on history

**File:** `src/app/workspace.cpp:95` (with `src/gui/ui.cpp:121-127`)
**Issue:** `calculate()` stamps `viewScale = viewPpu` whenever `armedViewPpu != 0`, and the arming block sets `armedViewPpu` on the *first* Done frame — no zoom required. At display scale k = 2 the default view's ppu is already 200 > kRefPpu, so a plain F5, an edit + autoRecalc, or a zoom wiggle-and-back recomputes the tab *adapted* (2 px seeds, ring gate at |E| > 16) while the same freshly-opened tab renders legacy. One configuration, one unchanged view, two different renderings before and after the first recalc — innermost equipotential rings visibly drop out on k >= 1.25 displays with no zoom action by the user. k = 1 is unaffected (ppu 100 computes legacy-identically), which is why every suite, script, and smoke run is green.
**Fix (pick one):** (a) accept and document — state in README that on HiDPI displays the default view already adapts once the tab recomputes; (b) make the opened state consistent by pre-stamping `viewScale` in `spawn()`/`openRestored()` from the live base ppu once `env.k` is known (needs the App to re-stamp on display-scale change); or (c) define the regime by zoom-relative scale (`100 * zoom`, k-independent) so the default view is always legacy — this changes the device-space story and must be a deliberate decision, not a drive-by.

#### WR-04: Interactive export renders the adapted cached scene at a fixed legacy view — exports become zoom-history-dependent

**File:** `src/gui/app.cpp:1138-1155`
**Issue:** `App::runExport` renders the *cached* scene through a fixed `canvasView(...)` (no zoom, no pan). Before this phase the cached scene was always a legacy computation, so a configuration exported twice — fresh vs. after user activity — produced identical output. Now a zoom-settle recalc (or any stamped recalc, see WR-03) leaves the scene adapted: innermost rings gated away, lines born under the dot. Exporting after zooming yields a PNG/SVG/PDF that silently lacks rings the same configuration exports with on a fresh open. For a codebase that maintains byte-level parity oracles and "deterministic exports" as a stated property (EXP-03), this is a determinism regression on the interactive path.
**Fix:** Cheapest: document that exports reflect the tab's current computation. Stronger: in `runExport`, when `t->params.viewScale > ef::ComputeParams::kRefPpu`, run a synchronous legacy recomputation of `t->field` with `viewScale = 0` into a temporary `FieldScene` for the export (the legacy compute is the cheap common case; exports are already user-visible-latency operations), leaving the tab's adapted scene untouched.

#### WR-05: Two verifier-flagged behaviors still have no automated pin (adapted-ring anchor density; GUI arm→fire composition)

**File:** `tests/test_tracer.cpp` (no adapted `traceEquipotential` test), `tests_gui/test_render_ctx.cpp:200-225` and `tests/test_editor.cpp:239-320` (composition gap)
**Issue:** (a) No test drives `traceEquipotential` with `adapts()` params, so a regression of `recordPx` (tracer.hpp:166) back to the legacy density would keep every suite green — the sampling proof samples line ink, not ring shape (verifier's Deviation Audit agrees: "a recordPx regression would NOT fail any suite"). (b) Deleting the entire drawCanvas arming block leaves every suite green (verifier's behavior_unverified item): nothing asserts the GUI-side arm→`viewRecalcTick` composition.
**Fix:** Cheapest pins, both pure doctests:
```cpp
// tests/test_tracer.cpp — anchor density scales with the view (pins recordPx both ways)
TEST_CASE("adapted rings record anchors at the target view density (v2.6 phase 2)") {
  const ChargeSystem dipole = preset("dipole");
  rk::Euler<Vec2> solver;
  const Vec2 start(-1.0, 0.0);  // small closed loop around the + charge (as in the legacy closure test)
  ComputeParams legacy;
  const auto a = traceEquipotential(solver, dipole, legacy, start, 0.7, hooks);
  ComputeParams adapted; adapted.viewScale = 800.0;
  const auto b = traceEquipotential(solver, dipole, adapted, start, 0.7, hooks);
  CHECK(b.points.size() > 4 * a.points.size());   // ~8x denser: 8 px @800 vs 8 px @100
  for (std::size_t i = 1; i + 1 < b.points.size(); ++i)          // thinning contract at the target view
    CHECK((b.points[i] - b.points[i - 1]).Length() * 800.0 >= 8.0 - 1e-9);
}
```
For (b), extract the three arming lines into a std-only helper in `src/app` (e.g. `editor::armViewRecalc(Tab&, double ppu)` called from drawCanvas) and doctest it: ppu change arms -2 / identical ppu is inert / first call (armedViewPpu 0) never arms / legacy→legacy change disarms. That moves the branch the verification called "deleting it passes everything" under test without an ImGui dependency.

### Info

#### IN-01: hitRadius's second clamp tier is unreachable and its "continuously" comment is false

**File:** `src/core/ef/params.hpp:37-46`
**Issue:** With the defaults, the adapted hit target is `16.67/viewScale` and the seed target is `2/viewScale` — the seed is smaller at every viewScale, so the outer `std::min(..., seedRadius())` reduces the whole adapted branch to `return seedRadius()`. The elaborate `std::clamp(chargeRadiusPx / 1.5 / viewScale, kMinSeedWorld, legacy)` never binds, and the comment's claim that the adapted radius "shrinks continuously out of" the legacy value never happens: hit jumps 0.1667 → 0.02 exactly at the regime boundary (as the seed does — that part is the intended regime switch, but the comment describes a continuity that does not exist).
**Fix:** Either simplify to `return std::min(legacy, seedRadius());` (equivalent, self-documenting: endpoints stay on/inside the seed circle), or keep the defensive clamp and correct the comment to say the seed cap is the binding constraint for the default constants. Also consider naming the implicit `kHitPx = chargeRadiusPx / 1.5` if the constant stays.

#### IN-02: zoomCeiling's minCanvasPx parameter is dead

**File:** `src/app/editor.hpp:69-71`, `src/app/editor.cpp:222-228`
**Issue:** The h25 rule is superseded and the parameter no longer affects the result, yet all three call sites still compute and pass `min(avail.x, avail.y)` / `minSide`. The header documents the decision ("keeps its parameter slot"), so this is deliberate API freeze — but every caller now carries a dead computation, and the parameter invites misuse (it *looks* load-bearing).
**Fix:** In the next milestone that touches this API, drop the parameter (or repurpose it per WR-02's fix) and simplify the three call sites; until then the documentation is sufficient.

---

**Verified non-issues** (checked, no finding): legacy short-circuit unreachability at viewScale 0/100 (strict `>` in `adapts()`, pinned by tests/test_tracer.cpp:89-108 and the parity oracle); no `std::clamp` UB (`chargeRadiusPx` fixed at 25, `pixelsPerUnit` fixed at 100); first-step self-hit at hit == seed (HitCharge checked after an outward step, `<` strict, pinned by tests/test_tracer.cpp:169-182); ring-gate label bookkeeping (advance-then-skip keeps labels consecutive; skipped labels intentionally lost, never re-posted); viewRecalcTick fire-while-computing, stale-during-recalc, cancellation-on-close, and the -2 marker protocol; interaction with autoRecalcTick (one calculate per frame, each fire disarms both deadlines); session files persist neither zoom nor viewScale; ef-cli and script exports keep legacy params; smoke's zoom-in/out race is genuinely fixed by 91f3b61 (zoom 1.15 → back to exactly 1.0 disarm).

---

_Reviewed: 2026-10-13T18:13:05Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
