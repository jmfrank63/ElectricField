---
phase: 2
verified: 2026-10-13T18:05:00Z
status: passed  # human items auto-accepted per standing auto-mode instruction (recorded for owner UAT)
score: 4/5 must-haves verified
behavior_unverified: 1
overrides_applied: 0
covered_files: [".planning/phases/02-zoom-adaptive-calculation/02-01-PLAN.md", "README.md", "docs/api.md", "src/app/editor.cpp", "src/app/editor.hpp", "src/app/workspace.cpp", "src/app/workspace.hpp", "src/core/ef/params.hpp", "src/core/ef/tracer.hpp", "src/gui/app.cpp", "src/gui/ui.cpp", "tests/test_editor.cpp", "tests/test_seeds.cpp", "tests/test_tracer.cpp", "tests_gui/test_render_ctx.cpp"]
covered_digest: "v2:sha256:52dfd940457dbf4b5b7633c7a76cebf9ddc3a206e7db1f2ca23c2b8532171188"
behavior_unverified_items:
  - truth: "When the zoom settles, a zoomed GUI tab recomputes once with the adapted radii; zooming never recomputes while the wheel is still rolling (SC2, GUI arm->fire chain)"
    test: "Open a tab, let it finish, wheel-zoom into a charge (or run view.zoom 3 through the API), hold still ~0.5 s"
    expected: "Exactly one recomputation fires about 0.35 s after the last wheel tick (brief busy overlay), with lines now hugging the charge; rolling the wheel continuously never triggers one"
    why_human: "The workspace contract is behaviorally doctested, but the drawCanvas arming block (src/gui/ui.cpp:121-127) has no automated end-to-end assertion: smoke only zooms in and back out (the disarm path, app.cpp:810-816) and asserts nothing about the recalc; the doctests set pendingViewRecalcAt manually; the render proof bypasses the GUI. Deleting the arming block would leave every suite green."
coincidental_reliance_items:
  - truth: "The disarm regime guard (91f3b61): zooming back into the legacy regime disarms instead of arming — smoke passes repeatedly"
    reason: undeclared-precondition
    harden: "The guard's sameRegime check (src/gui/ui.cpp:122-124) disarms only when the settled ppu is <= kRefPpu exactly; a wheel factor whose double round-trip drifts above 1.0 (f*(1/f) != 1.0) would arm an adapted recalc at ppu 100+2e-14, silently dropping the innermost legacy equipotential rings (the gate would skip rings closer than ~0.35 world). All current factors (1.15/1.2/1.25/1.5/2/3) round-trip exactly, so behavior is correct today; harden by treating ppu <= kRefPpu*(1+1e-9) as legacy in sameRegime."
---

# Phase 2: Zoom-Adaptive Calculation Verification Report

**Phase Goal:** Zooming into a charge never shows the empty untraced disk: the tab's calculation adapts to the view — seed and hit radii shrink with the zoom so lines hug the charges on screen (~1 px seed circle), the tab recalculates automatically (debounced) when the zoom settles, and the zoom-in ceiling extends to the numerics limit.
**Verified:** 2026-10-13T18:05:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | SC1: viewScale=100 (and 0) gives exactly today's 0.25 / 25/1.5/100 radii and identical posting; ef-cli unchanged | ✓ VERIFIED | `src/core/ef/params.hpp:27,33-47` — `adapts()` is `viewScale > kRefPpu` (100 not > 100), so both radii return the legacy expressions verbatim; `src/core/ef/tracer.hpp:90` gate is `p.adapts() && …` and `tracer.hpp:166` `recordPx = adapts() ? viewScale : px` — legacy at/below 100. Doctests: `tests/test_seeds.cpp:161-168` (radii exact at 0 and 100), `tests/test_tracer.cpp:89-108` (bit-identical job count, labels, and start points at viewScale 100). ef-cli never sets viewScale (grep of `src/cli/`: no hits). Parity oracle RUN BY VERIFIER: `./zig-out/bin/ef-cli --all --out-dir /tmp/efverp2 --expect tools/parity/expected-counts.csv` → exit 0, all 9 presets match. |
| 2 | SC2: recalc fires once ~0.35 s after the zoom settles, never while the wheel rolls, never stale/dirty/undo | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Workspace contract behaviorally proven: `tests/test_editor.cpp:239-281` (marker -2 → deadline now+0.35 → fires once with viewScale stamped, no early fire at deadline-0.01, no stale/dirty/undo, explicit Calculate honours+disarms, never-armed tab untouched) and `:285-320` (real session completes the adapted computation back to Done, flags clean). Code: `src/app/workspace.cpp:124-136` (`viewRecalcTick`: skips live sessions; -2 → deadline; fires once, disarms), `src/gui/ui.cpp:121-127` (arming, Done tabs only, first-frame guard), `src/gui/app.cpp:297` (called every frame). **The GUI arm→fire composition has no automated assertion** — see behavior_unverified_items. |
| 3 | SC3: at high zoom the field lines visually start at the charge surface (no white annulus) | ✓ VERIFIED | In-suite behavioral render proof, `tests_gui/test_render_ctx.cpp:200-225`: real `ComputeSession` at viewScale 800 vs 0, rendered at ppu 800 centred on the + charge; 360-sample ring at 30 px (5 px outside the 25 px drawn dot). Two-sided pin: `legacyInk <= 4` (the ring is background without adaptation) AND `adaptedInk > legacyInk` AND `adaptedInk >= 4`. Run by verifier inside `zig build test`: **legacy=0, adapted=18** — cannot pass trivially (the legacy half proves the annulus exists pre-adaptation). Supporting invariants: floored seed 0.01 world = 8 px at ppu 800 < 25 px dot (`tests/test_seeds.cpp:174-185`), adapted line escapes its own charge and ends on the partner (`tests/test_tracer.cpp:169-182`). |
| 4 | SC4: the zoom-in ceiling rises, numerics-bounded (static cap may exceed 20) | ✓ VERIFIED | `src/app/editor.cpp:222-228` — `clamp(kMaxPpu / pixelsPerUnitBase, kZoomMin, kZoomMax)`, degenerate inputs → kZoomMax; `editor.hpp:59-65` kZoomMax=40, kMaxPpu=2500=25/0.01 (drawn dot covers the floored seed). Doctests `tests/test_editor.cpp:206-234`: 768/100→25, 1024/200→12.5 (HiDPI), canvas size no longer bounds, degenerate→40. Call sites compile to the intended values: `ui.cpp:111` (drawCanvas, base = canvasView ppu before zoom), `ui.cpp:716` (zoom buttons, base 100*k), `app.cpp:1877` (view.zoom, base 100*clampScale(k_)) — all 25x at k=1, and `ui.cpp:112` re-clamps the stored zoom every frame. |
| 5 | SC5: suites + scripts + smoke green; docs updated | ✓ VERIFIED | Run by verifier: `zig build test` → 139/139 core cases (215,892 assertions) + 5/5 GUI cases, SUCCESS. CI-style script loop (10 scripts, 44 pre-seeded with `printf 'collection = Original\n'`) → rc=0, 44 asserts `recents=Original`. `zig build smoke` → **3/3 passes** (the editor-click flake surface from 91f3b61 held every time). Docs: `README.md:63-67` (zoom bullet: lines hug charges, ~25x ceiling, settled zoom recalculates), `docs/api.md:104` (view.zoom row). |

**Score:** 4/5 truths verified (1 present, behavior-unverified)

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ----------- | ----------- | ------ | -------- |
| ZOOM-02 | ROADMAP Phase 2 (plan frontmatter carries no `requirements:` field; ROADMAP mapping is authoritative) | View-adapted seed/hit radius, debounced auto-recompute on settle, ceiling to the numerics limit, default view + ef-cli parity unchanged | ✓ SATISFIED | Truths 1-5 above; the seed circle at the ceiling is exactly 25 px on screen (`tests/test_seeds.cpp:184`), floored at 0.01 world = 10x the near-field step (h/r <= 0.1 — the numerics bound) `params.hpp:24-25` |

No orphaned requirements: REQUIREMENTS.md maps only ZOOM-02 to Phase 2 and it is claimed.

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | -------- | ------ | ------- |
| `src/core/ef/params.hpp` | viewScale + adapted seedRadius/hitRadius | ✓ VERIFIED | 50 lines; substantive (clamp math, two-tier hit clamp capped at seed); wired (consumed by seeds/field/tracer via existing p.seedRadius()/p.hitRadius() reads) |
| `src/core/ef/tracer.hpp` | sub-pixel ring gate + adapted ring anchor density | ✓ VERIFIED | `:43,77-94,163-166`; gate wired into both call sites (`:148` and via traceEquipotential); recordPx feeds `addPoint` thinning |
| `src/app/workspace.{hpp,cpp}` | Tab view-recalc fields + viewRecalcTick + calculate stamps/disarms | ✓ VERIFIED | `workspace.hpp:70-76,127-130`; `workspace.cpp:90-96,124-136`; called from `app.cpp:297`; data flows from the real canvas view (not a mock) |
| `src/gui/ui.cpp` | drawCanvas arming block | ✓ VERIFIED | `:107-127` — arms from the live `view.pixelsPerUnit` (100*k*zoom), Done-only, first-frame guard, regime disarm; executes every frame (smoke drives it) |
| `src/app/editor.{hpp,cpp}` | kMaxPpu=2500, kZoomMax=40, new zoomCeiling | ✓ VERIFIED | `editor.hpp:59-71`, `editor.cpp:222-228`; three call sites pass base 100*clampScale(k) |
| `tests/test_seeds.cpp`, `tests/test_tracer.cpp`, `tests/test_editor.cpp` | doctests for radii, gate, workspace contract, ceiling | ✓ VERIFIED | All pass in the suite run; see per-SC evidence |
| `tests_gui/test_render_ctx.cpp` | sampling proof | ✓ VERIFIED | `:200-225`, ran green with legacy=0 / adapted=18 |
| `README.md`, `docs/api.md` | updated zoom story | ✓ VERIFIED | README:63-67, api.md:104 |

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | --- | --- | ------ | ------- |
| ui.cpp drawCanvas | Tab pendingViewRecalcAt/viewPpu/armedViewPpu | arming block ui.cpp:121-127 | WIRED | sets -2 + stamps viewPpu on ppu change; regime guard disarms |
| app.cpp frame | Workspace::viewRecalcTick | app.cpp:297 | WIRED | runs after pollAll + autoRecalcTick, before the UI pass of the same frame |
| viewRecalcTick | Workspace::calculate | workspace.cpp:133 | WIRED | calculate() refuses live sessions (`:92`) and stamps viewScale (`:95`) — double guard on the fire path |
| calculate | ComputeSession factory | workspace.cpp:105 | WIRED | params (with viewScale) reach the real session; asserted via the fake factory receiving viewScale 800/900 (test_editor.cpp:261,273) |
| session params | seeds/tracer | p.seedRadius()/p.hitRadius()/gate | WIRED | planSeeds + HitCharge + shouldPostEquipotential read the adapted values (test_seeds.cpp:194-202 pins seed placement on the adapted circle) |
| view.zoom API | zoomCeiling | app.cpp:1877 | WIRED | same formula as the UI; docs claim matches |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| arming block | view.pixelsPerUnit | canvasView(100*clampScale(k)) * t.zoom (ui.cpp:107-113) | yes — live view state | ✓ FLOWING |
| viewRecalcTick fire | t.viewPpu | armedViewPpu-stamped live ppu (ui.cpp:125) | yes | ✓ FLOWING |
| render proof | viewScale 0 vs 800 | real ComputeSession + renderSurface | yes — real compute, no mocks | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Full core suite incl. radii/gate/contract/ceiling doctests | `zig build test` | 139/139 cases, 215,892 assertions, SUCCESS | ✓ PASS |
| Sampling proof (SC3) | `zig build test` (tests_gui) | ring ink legacy=0 adapted=18 (MESSAGE, test_render_ctx.cpp:218) | ✓ PASS |
| ef-cli parity oracle | `ef-cli --all --out-dir /tmp/efverp2 --expect tools/parity/expected-counts.csv` | exit 0, 9/9 presets match | ✓ PASS |
| Script suite (CI loop, 44 printf pre-seed) | loop over tests/scripts/*.efscript | rc=0; 44 asserts `[ok] app.expect recents=Original` | ✓ PASS |
| Smoke (editor-click flake retry twice) | `zig build smoke` x3 | 3/3 ok, frames=288 tabs_opened=2 | ✓ PASS |
| Regime-guard determinism | `1.15*(1/1.15)` etc. in IEEE doubles | exactly 1.0 for 1.15/1.2/1.25/1.5/2/3 → ppu==100 → disarm is deterministic, not timing luck | ✓ PASS |

### Probe Execution

| Probe | Command | Result | Status |
| ----- | ------- | ------ | ------ |
| (none declared) | — | phase PLAN/SUMMARY declare no probe-*.sh; parity oracle + smoke + script loop run above cover the runnable checks | n/a |

### Interaction Sweep (request #7)

- **view-scale recalc x autoRecalc:** both deadlines funnel into `Workspace::calculate`, which refuses while a session is live (workspace.cpp:92) and disarms BOTH deadlines (pendingViewRecalcAt `:96`, pendingRecalcAt `:103`). Frame order (app.cpp:295-297) is pollAll → autoRecalcTick → viewRecalcTick, so at most one calculate per tab per frame; whichever fires first disarms the other. An edit-recalc carries the stamped viewScale (F5-equivalent, `:95`); a view-recalc computes the CURRENT (edited) charges and clears stale — no lost edits, dirty untouched. No double-recalc, no cancellation fight.
- **unsaved-changes prompt:** `calculate()` (workspace.cpp:90-115) never writes `dirty` — only editor mutators do; doctests assert `CHECK_FALSE(t.dirty)` on both view-recalc paths (test_editor.cpp:264,308,316). ✓
- **fire while previous session computing:** `viewRecalcTick` skips tabs with a live session (workspace.cpp:127) AND `calculate()` refuses (`:92`) — double-guarded. A deadline cannot survive into Computing: every session start on an existing tab goes through calculate(), which always disarms (open/spawn create fresh tabs with pendingViewRecalcAt=-1).

### Deviation Audit (4 Rule-1 fixes)

| Fix | Sound? | Tested? | Legacy regression risk |
| --- | ------ | ------- | ---------------------- |
| hit capped at seed (params.hpp:46) | Yes — a line born on the seed circle would die on its own charge at step 1 if hit > seed | Yes — hit==seed at 4 scales + 8-point escape invariant (test_seeds.cpp:170-192) + behavioral escape at 800 (test_tracer.cpp:169-182) | None — the min sits inside the `adapts()` branch |
| adapted rings record anchors ~8 device px (tracer.hpp:166) | Plausible by inspection (sparse anchors → 64 px chords at ppu 800 → bezier swoops; 8 px chords stay in the h21 Catmull-Rom branch) | **No dedicated test** — no doctest drives traceEquipotential with adapts() params; the sampling proof samples a ring where field lines (not rings) provide the ink, so a recordPx regression would NOT fail any suite | None (legacy keeps `px`) — but the smoothness claim is human-verified only |
| calculate() stamps+disarms (workspace.cpp:95-96) | Yes — F5/API after a zoom uses the live view; no double compute | Yes — test_editor.cpp:268-280 (stamps 900, disarms; never-armed tab keeps legacy 0) | None — gated on armedViewPpu != 0; a never-zoomed Done tab stamps viewScale=100 which is legacy-identical |
| disarm regime guard (ui.cpp:122-124) | Yes — a legacy↔legacy settle would be bit-identical, so disarming is correct; zoom-back from adapted correctly still arms (restores legacy radii) | Smoke-only (deterministic — fp round-trip is exact; 3/3 green here); no unit test (ImGui dependency) | None; see coincidental_reliance_items for the epsilon hardening suggestion |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| (none) | — | No TBD/FIXME/XXX/HACK/PLACEHOLDER markers, no stub returns in any phase-modified file | — | — |

Note: `02-01-PLAN.md` is untracked in git (the executor's commits start at f6ad731); the orchestrator should bundle it.

### Human Verification Required

### 1. Wheel-zoom end-to-end (SC2 GUI chain + SC3 live look)

**Test:** Open the Dipole, wait for Done, wheel-zoom into the + charge continuously, then stop and hold ~0.5 s; repeat with the Zoom + button and `view.zoom 3` via the API.
**Expected:** Nothing recomputes while the wheel rolls; exactly one recomputation (brief busy overlay) fires ~0.35 s after the last zoom input; afterwards the lines hug the charge with no white annulus at every zoom up to ~25x; zooming back out recomputes once to the legacy look; F5 right after a zoom uses the new scale.
**Why human:** The workspace contract is doctested but the drawCanvas arming → fired-recalc composition has no automated assertion (deleting the arming block passes every suite); timing/feel and the on-screen look are not grep-able.

### 2. Adapted equipotential ring rendering

**Test:** Zoom to ~10-25x on a preset and inspect the equipotential rings near a charge.
**Expected:** Rings render as smooth closed curves (no polygonal chords or "wild swoops"); ring spacing on screen stays >= ~1 px.
**Why human:** The ~8-device-px anchor density change (tracer.hpp:166) is untested at unit level and the sampling proof does not assert ring shape.

### 3. Ceiling feel across scale factors

**Test:** Zoom in via wheel and buttons to the ceiling at 100% and 200% display scale.
**Expected:** Zoom-in stops at ~25x (k=1) / ~12.5x (k=2) with the floored seed circle still hidden behind the drawn dot (no white dot-ring at max zoom).
**Why human:** HiDPI ceiling interplay and the visual dot-coverage at the limit are display-dependent.

### Gaps Summary

None blocking. All five success criteria are implemented, wired, and — except the GUI arm→fire composition of SC2 — behaviorally proven by tests run in this verification (full suite, sampling proof, parity oracle, 10-script loop, 3x smoke). The single behavior-unverified truth and the untested ring-anchor rendering route to the human items above; the fp-exactness reliance of the regime guard is recorded with a one-line hardening suggestion (epsilon in sameRegime). Phase 3's dependency on "the adapted-compute core" is satisfied.

---

_Verified: 2026-10-13T18:05:00Z_
_Verifier: the agent (gsd-verifier)_
