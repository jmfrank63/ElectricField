---
phase: 3
verified: 2026-10-14T00:00:00Z
status: passed  # human items auto-accepted per standing auto-mode instruction (recorded for owner UAT)
score: 5/6 must-haves verified (1 present, behavior-unverified: canvas tooltip)
behavior_unverified: 1
covered_files: [".planning/phases/03-charge-identity-per-charge-size/03-01-PLAN.md", ".planning/phases/03-charge-identity-per-charge-size/03-CONTEXT.md", "src/core/ef/charge_system.hpp", "src/core/ef/presets.hpp", "src/core/ef/presets.cpp", "src/core/ef/scene.cpp", "src/core/ef/scene.hpp", "src/app/configs.cpp", "src/app/session_file.cpp", "src/app/editor.hpp", "src/app/editor.cpp", "src/app/workspace.cpp", "src/app/workspace.hpp", "src/gui/ui.cpp", "src/gui/ui.hpp", "src/gui/app.cpp", "tests/test_configs.cpp", "tests/test_session_file.cpp", "tests/test_editor.cpp", "tests/test_scene.cpp", "tests/scripts/45-charge-identity.efscript", "docs/api.md", "README.md"]
covered_digest: "v3:sha256:fed517dfcfa1f184ffc1adb838818585af7e6825cb83448c12e1349c46b03884"
behavior_unverified_items:
  - truth: "Hovering a charge on the canvas shows its name as a tooltip (SC4)"
    test: "Open a Done tab, hover a charge; hover while dragging; hover while computing"
    expected: "Tooltip shows the effective name (stored or 'Charge N'); no tooltip while mouse-dragging a charge or while the tab is computing"
    why_human: "Hover is an ImGui runtime state no headless script can produce; code path ui.cpp:137/183 is wired but no test exercises it"
human_verification:
  - test: "Hover a charge on a Done tab, then start dragging it, then press Calculate and hover during compute"
    expected: "Tooltip shows the effective name (stored name or derived 'Charge N'); it disappears while dragging and never shows while Computing"
    why_human: "ImGui hover state; not reachable from --script"
  - test: "Edit Name and Size rows in the editor panel with Continuous calculation on"
    expected: "Size change redraws the dot immediately; NO 'edited - press Calculate' recalc hint appears from name/size edits (dirty-only); derived name shows as disabled 'Charge N' context text"
    why_human: "Panel row runtime behaviour and the visual recalc hint are GUI-only; the dirty-only core contract is doctested but the row wiring is not"
  - test: "View > Options: check the Charge size help text and add a new charge after changing the size"
    expected: "Help text reads 'default size for new charges (2 - 120)'; the new charge takes the new default; existing charges without an override keep following it (sizePx 0 = fallback)"
    why_human: "Options modal text and new-charge visual are GUI-only"
  - test: "HiDPI check: run with --scale 2, set a per-charge size of 40 px on one charge and compare against an inherited (size 0) charge with the same Options default"
    expected: "Today (pre-fix, finding F1): the custom-sized charge renders/hits at HALF the intended logical size vs the inherited one. After the F1 fix: both draw the same visual radius for the same numeric value"
    why_human: "Requires visual comparison at a non-1 content scale; no test runs at k != 1"
---

# Phase 3: Charge Identity & Per-Charge Size — Verification Report

**Phase Goal:** Every charge carries a stable id, an editable name and its own drawn size; the Options charge size becomes the default for new charges only; hovering a charge shows its name as a tooltip; everything persists in collections and sessions and is API-callable.
**Verified:** 2026-10-14 (commits fbc1526..67a1676)
**Status:** human_needed (all 6 SCs implemented and test-pinned at k=1; tooltip is GUI-only; one HiDPI display-space defect found — F1 below)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (roadmap SCs)

| # | Truth (SC) | Status | Evidence |
|---|-----------|--------|----------|
| 1 | SC1 model: `Charge` carries id (0=unassigned), name (""=derived), sizePx (0=inherit); presets 1..N; ids max+1, never reused, incl. through undo/redo; `makeSystem` untouched | ✓ VERIFIED | charge_system.hpp:16-21 (trailing NSDMI fields); presets.cpp:21-31 (`makeSystem` field-carrying, no ids — ef-cli parity green, PARITY_EXIT=0); workspace.cpp:43 (`assignChargeIds` on open → 1..N); editor.cpp:45-55 (`nextChargeId` max+1, `assignChargeIds` id==0-only monotonic); watermark `chargeIdWater` on Tab OUTSIDE `HistoryEntry` (workspace.hpp:62-65, 84-88) so undo/redo cannot resurrect ids — pinned by doctests test_editor.cpp:568 (no reuse after remove), :589 (watermark survives undo+redo, ids 3/4/5 all retired), :616 (openUser/openRestored preserve persisted ids, fill only 0) |
| 2 | SC2 persistence: collections AND sessions store id/name/size backward-compatibly (3-field legacy parses; new fields optional) | ✓ VERIFIED | configs.cpp:47-57 + session_file.cpp:63-72 write 6 fields (`value, x, y, sizePx, id, name`), comma-stripped names, raw UTF-8; configs.cpp:103-128 + session_file.cpp:152-176 parse EXACTLY-3-or-EXACTLY-6 symmetrically (incl. identical trailing-comma re-add for empty names); pinned by round-trip doctests: test_configs.cpp:100 (6-field lossless, "Größerer Test" byte-passthrough, "Has, Comma"→"Has Comma", idempotent second write), :127 (empty name → trailing empty field), :139 (legacy 3-field parses zeros; 4/5-field skipped+counted); mirror set in test_session_file.cpp:87/:111/:143. On-disk proof: /tmp/.../45-charge-identity/Collections/Identity Test line `charge = 1, 0, 0, 0, 1, My Charge` |
| 3 | SC3 panel edits name+size; Options charge size = default for NEW charges only, fallback otherwise | ✓ VERIFIED (core) | ui.cpp:821-842 Name/Size rows wired to `setChargeName`/`setChargeSize` (resync keyed on serial+selected; derived-name `TextDisabled` at :832; clamp 0..120 at :837); editor.cpp:186-206 dirty-only (never `stale`, never `pendingRecalcAt`), comma strip, identical no-op without history, `syncScene` immediate redraw — pinned by test_editor.cpp:656/:686; Options help text ui.cpp:598; new-charge default passed at both GUI sites (double-click ui.cpp:141, panel Add ui.cpp:737-738) with 0-or-2..120 validation (test_editor.cpp:638). Deviation 7 judged below (API `charge.add` passes 0 — acceptable). Row UX → Human Verification |
| 4 | SC4 tooltip: hovering a charge shows its name | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | ui.cpp:137 single `hitCharge` per frame reused by double-click guard (:140), click-select (:145) and tooltip (:183); guard `hoverIdx >= 0 && !IsMouseDragging(0) && !dragActive && state != Computing`; name via `chargeDisplayName`. Wired correctly by inspection, but hover is unscriptable — no test exercises it → Human Verification |
| 5 | SC5 API: `charge.name <i> <name>`, `charge.size <i> <px>`; `app.state` shows per-charge name/size | ✓ VERIFIED | app.cpp:1892-1905 (index required, tokens joined, delegates to `setChargeName`), :1906-1917 (`error: charge size must be 0 or 2-120`, exact convention); dump app.cpp:1624-1627 `charges=v,x,y,sizePx,id,displayName` — substring-compatible prefix untouched (old scripts 40-44 green); pinned end-to-end by tests/scripts/45-charge-identity.efscript (all 8 expectations incl. error text, id-not-reused 2→3, undo snapshot restore, collection-file round-trip) — SUITE STATUS=0 |
| 6 | SC6 `zig build test` green (round-trip doctests), smoke green, script suite extended + green, docs updated | ✓ VERIFIED | `zig build test` exit 0 (all 16 suites incl. new v2.6 phase 3 TEST_CASEs); `zig build smoke` exit 0 (canvas checks 640/1024/1280, editor=ok with the new hitCharge signature); 11/11 scripts green under the canonical CI loop (41 in 40's dir, 43 in 42's, 44 printf pre-seed); api.md:39-42 (extended `charges=` format), :104-105 (both commands), :118 (Options = default-for-new + fallback); README.md:53-56 (Charge size bullet), :60-64 (charge editor bullet: Name/Size rows, tooltip, ring, id stability), :162-164 (collection file 6-field format + legacy note) |

**Score:** 5/6 truths verified (1 present, behavior-unverified: canvas tooltip)

### Findings

**F1 (WARNING — the k≠1 display-space defect, judged):** `scaledStyle` multiplies ONLY `style.chargeRadiusPx` by the content scale k (ui_logic.cpp:92), while the per-charge branch uses raw `sizePx` in all three device-space consumption sites:
- render: scene.cpp:295 `radius = c.sizePx > 0 ? c.sizePx : style.chargeRadiusPx`
- hit: ui.cpp:137 passes `dotPx * env.k` (scaled) but `hitCharge` uses `c.sizePx` raw (editor.cpp:65)
- ring: ui.cpp:193 `(sc.sizePx > 0 ? sc.sizePx : style.chargeRadiusPx) + 3*k`

**Derived behaviour:** at k=1 everything is exact (all tests/scripts run here — why CI is green). At k=2 the world extent of a per-charge dot is `sizePx/(100·k·zoom)` = HALF the extent of an inherited dot with the same numeric Options value (25 → 0.125 vs 0.25 world units). Because both GUI add sites store the Options default into `sizePx`, a new charge on a HiDPI display VISUALLY SHRINKS relative to inherited charges — the SC3 "default for new charges" equality is violated on screen at k≠1. Mitigations: it is fully self-consistent (hit bound and ring use the same raw value, so selection still lands on the drawn dot), physics/persistence/API/undo are untouched (raw px stored is the correct portable semantic), and it is invisible at k=1.

**Severity judgement: NOT merely cosmetic, but not goal-blocking.** It is a real functional defect on HiDPI (macOS Retina default is k=2): custom-sized charges render at half the intended logical size and new charges visibly mismatch inherited ones. It does not break any SC at the model/persistence/API level and does not corrupt selection. Classified WARNING + human decision (fix now vs. follow-up), since the phase's own deliverable (renderScene per-charge branch) introduced it.

**Concrete fix suggestion:** carry the display scale once and apply it at the same boundary the style is scaled. Simplest zero-risk shape: pass `env.k` (or store a `styleScale` on `ef::View`, set by every `canvasView` caller) and use `c.sizePx * scale` in the three sites — renderScene's per-charge branch (via `view.styleScale`), `hitCharge`'s per-charge bound (new `scale` parameter, applied to BOTH the default and the per-charge branch so ui.cpp passes plain `dotPx` again), and the ring. Model/persisted `sizePx` stays logical px — only the device-space reads change.

**F2 (INFO — executor deviation 7, judged):** `charge.add` (app.cpp:1817) calls `addCharge` WITHOUT the default-size argument (→ sizePx 0), where plan Decision 4 said to pass `options_.draw.chargeRadiusPx`. Judgement: **consistent with SC3 and acceptable.** An API-added charge gets the Options size applied through the live fallback (0 = inherit) — visually identical at creation, and closer to the SC3 wording "default ... only" (no snapshot). The residual asymmetry: GUI-added charges freeze the default at creation while API-added charges track later Options changes — they diverge only after an Options change (and at k≠1 via F1). Recommend either unifying (pass the default in `charge.add`) or documenting the freeze-vs-track difference in api.md.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|---|---|---|---|---|
| CHARGE-01 | 03-01-PLAN | Per-charge drawn size; Options size = default for new charges; panel-editable; persists; `charge.size` API | ✓ SATISFIED | SC1+SC2+SC3+SC5 evidence above |
| CHARGE-02 | 03-01-PLAN | Stable id + editable name ("Charge N" default); panel edits; hover tooltip; ids/names persist | ✓ SATISFIED (tooltip wired, hover → Human Verification) | SC1+SC2+SC4+SC5 evidence above |

No orphaned requirements: REQUIREMENTS.md maps exactly CHARGE-01, CHARGE-02 → phase 3 (line 332).

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| Full test workspace | `zig build test` | exit 0, all suites SUCCESS | ✓ PASS |
| Smoke (GUI, incl. editor clicks on new hitCharge) | `zig build smoke` | exit 0, "smoke ok frames=288", editor=ok | ✓ PASS |
| ef-cli parity (makeSystem untouched) | `./zig-out/bin/ef-cli --all --out-dir /tmp/efverp3 --expect tools/parity/expected-counts.csv` | exit 0, 9 presets match expected counts | ✓ PASS |
| Script suite (canonical CI loop) | 11 scripts, 41→40's dir, 43→42's, 44 printf pre-seed | STATUS=0 (a naive loop without the CI dir-sharing fails 41/43/44 — those need cross-script state, not a phase defect) | ✓ PASS |
| On-disk 6-field line | grep 45-sandbox Collections | `charge = 1, 0, 0, 0, 1, My Charge` | ✓ PASS |

### Undo/Redo (check 8)

HistoryEntry snapshots the whole `ChargeSystem` (workspace.hpp:84-88) — id/name/sizePx ride inside `field`. Pinned by: test_editor.cpp:723-741 (name+size+id restored through undo AND redo), :714-720 (size undo restores scene mirror too), :589-614 (id watermark survives undo/redo), and 45-charge-identity.efscript:15-16 (`editor.undo` → `charges=1,0,0,0,1,My Charge`, size edit reverted through the snapshot). ✓ VERIFIED

### Anti-Patterns Found

None. Debt-marker scan (TBD/FIXME/XXX/HACK/PLACEHOLDER/not-yet-implemented) over all 13 phase-modified source files: 0 hits. No stub returns, no hardcoded-empty flows; `kEditorHitRadius` fully deleted (0 references).

### Human Verification Required

See `human_verification` frontmatter: (1) canvas tooltip incl. drag/compute suppression; (2) panel Name/Size rows dirty-only behaviour (no recalc hint, immediate redraw); (3) Options help text + new-charge default; (4) HiDPI comparison at `--scale 2` (documents finding F1's expected pre-fix symptom).

### Gaps Summary

No gaps. All 6 success criteria are implemented, wired, and pinned by passing tests/scripts at the default display scale. Status is human_needed solely because SC4's hover surface is GUI-only (accepted in plan Decision 10) plus the panel/Options GUI behaviours, and because finding F1 (per-charge size interpreted in raw ImGui units instead of logical px at k≠1) needs a product decision: fix in a follow-up round (mechanical, suggested above) or accept the HiDPI distortion for now.

---

_Verified: 2026-10-14_
_Verifier: the agent (gsd-verifier)_
