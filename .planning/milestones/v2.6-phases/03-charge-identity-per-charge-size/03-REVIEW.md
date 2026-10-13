---
phase: 3
reviewed: 2026-10-14T06:30:00Z
depth: deep
files_reviewed: 21
files_reviewed_list:
  - src/core/ef/charge_system.hpp
  - src/core/ef/presets.hpp
  - src/core/ef/presets.cpp
  - src/core/ef/scene.hpp
  - src/core/ef/scene.cpp
  - src/app/editor.hpp
  - src/app/editor.cpp
  - src/app/workspace.hpp
  - src/app/workspace.cpp
  - src/app/configs.cpp
  - src/app/session_file.cpp
  - src/app/ini_text.hpp
  - src/app/ui_logic.cpp
  - src/gui/ui.hpp
  - src/gui/ui.cpp
  - src/gui/app.cpp
  - tests/test_editor.cpp
  - tests/test_scene.cpp
  - tests/test_configs.cpp
  - tests/test_session_file.cpp
  - tests/scripts/45-charge-identity.efscript
findings:
  critical: 2
  warning: 4
  info: 6
  total: 12
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-10-14 (commits fbc1526, 8701fc7, 67a1676, 47ef39e, 419e827, 2f7fbd2, 21546ba)
**Depth:** deep
**Files Reviewed:** 17 (+4 test files + 1 script)
**Status:** issues_found

## Summary

Reviewed the phase-3 charge-identity/per-charge-size implementation at deep depth: model fields and the carry-through sweep, both parsers/writers, the editor core, the GUI (canvas/panel/ring/tooltip), the API + dump, every exporter call site, and the new tests. Evidence gathered: `zig build test` green (157 + 6 cases), script `45-charge-identity.efscript` exit 0, on-disk artifacts inspected (`Sessions/Original.ini` and `Collections/Original.ini` round-trip id/name correctly).

The core is solid: the 3-or-6 grammar is symmetric and safely parsed (`parseIntBounded`/`parseDouble` are full-string, finite, overflow-safe), the id watermark survives undo/redo and session restore (water set in `spawn` from the assigned/persisted max — open/openUser/openEmpty/openRestored all funnel through it), `toPresetCharge` covers every save/merge/dump site, the panel/API/dump all use `chargeDisplayName`, and the doctests are real assertions with exact pins (not tautologies).

Two blockers surfaced. First, the F1 HiDPI defect is confirmed and is worse than "cosmetic": on any k=2 display (default macOS Retina) a GUI-added charge stores the Options default into `sizePx` and then renders at HALF its siblings' size — the phase's flagship feature misrenders in the default environment, and the active-tab exports reproduce it. Second, the sweep's grep gate missed the reverse direction: `exportCollectionTo` rebuilds `ef::Charge` positionally from `PresetCharge`, silently dropping id/name/sizePx, so whole-collection PDF export ignores per-charge sizes entirely. Four warnings cover parser/writer contract drift (unvalidated sizePx, the 1e9 id ceiling asymmetry, whitespace-name mutation, false-stale undo) and six info items cover smaller gaps.

Note: 03-VERIFICATION.md classified F1 as WARNING with a human fix-now/follow-up decision. This review classifies it **Critical**: per the rubric ("incorrect behavior … must be fixed before this code ships"), a feature that draws at half size on the default display configuration of the primary dev platform is incorrect behavior, not a quality degradation.

## Critical Issues

### CR-01: Per-charge sizePx is logical px but is consumed as device px in render, hit and ring — at k=2 every custom-sized charge draws/hits at half its intended size (verifier F1, confirmed and escalated)

**File:** `src/core/ef/scene.cpp:295` (render), `src/gui/ui.cpp:137` + `src/app/editor.cpp:65` (hit), `src/gui/ui.cpp:193` (ring)
**Issue:** The style passed to `renderScene` is device-scaled (`scaledStyle` multiplies `chargeRadiusPx` by k, `ui_logic.cpp:92`) and `canvasView` gives device ppu (`100 * k * zoom`, `ui_logic.cpp:83`, `ui.cpp:115`), but the per-charge branch uses raw `sizePx`:

- Render (`scene.cpp:295`): `radius = c.sizePx > 0 ? c.sizePx : style.chargeRadiusPx` — inherited charge: `25·k` device px; custom `sizePx=40`: `40` device px. At k=2 a GUI-added charge (double-click `ui.cpp:141` / panel Add `ui.cpp:737-738` freeze the Options default 25 into `sizePx`) draws 25 device px = **12.5 logical px while its siblings draw 25 logical px** — the new dot visibly shrinks to half. The "Options size = default for new charges" equality (SC3) is violated on screen.
- Hit (`ui.cpp:137` passes device `view.pixelsPerUnit` + `dotPx * env.k`; bound `editor.cpp:65` divides `c.sizePx` raw): the default branch is consistent (25·2 / 200), the per-charge branch is not — a "40 px" charge has the world bound of a 20 px charge at k=2. It is self-consistent with what is *drawn* (both raw), but wrong relative to the user's intent and to inherited charges.
- Ring (`ui.cpp:193`): mixes raw `sizePx` with the k-scaled `style.chargeRadiusPx` and `3·env.k` — it tracks the (half-scale) drawn dot, inheriting the same defect.
- Exports: `App::runExport` (`app.cpp:1148-1156`) renders the same cached scene with `scaledStyle(k)` + `canvasView(k)`, so PNG/PDF/SVG of the active tab reproduce the mixed-scale dots at k≠1. (`exportConfigurationTo`/`exportCollectionTo` use the unscaled style with `fittedView`, i.e. the k=1 space, where raw sizePx is *correct* — only CR-02 mars those.)

Derived k=2 behaviour, exactly: inherited 25 → 50 device px (0.25 world at zoom 1); custom 40 → 40 device px (0.20 world) instead of 80 device px (0.40). Ratio between two charges with sizes 25 and 40 should be 1.6, renders as 1.25. All tests and scripts run at k=1, which is why CI is green — there is no test at k≠1.

**Fix:** The suggested fix direction is validated: carry the scale once and keep `sizePx` logical. Concretely:
1. Store the display scale on `ef::View` (e.g. `float styleScale = 1.f`, set by `canvasView`/`runExport` callers; defaults to 1 for every core/exporter caller — ef-cli, SVG, PDF, fitted exports stay untouched) and use `c.sizePx * view.styleScale` in the per-charge branch of `renderScene` (`scene.cpp:295`). A View-carried scale is the single-carry point because `runExport` reuses the same cached scene the canvas draws — scaling in the scene mirror would double-scale exports.
2. Hit site: pass the **logical** ppu and **logical** default — `hitCharge(t.field, mouseWorld, 100.0 * t.zoom, dotPx)` at `ui.cpp:137` (k-free, byte-identical to today at every k for the default branch, correct for per-charge). Same for the panel/zoom-ceiling site which already uses logical (`ui.cpp:720-721`).
3. Ring: `(sc.sizePx > 0 ? sc.sizePx * env.k : style.chargeRadiusPx) + 3.f * env.k`.
4. Add a doctest at scale 2 (renderScene with `View{..., styleScale=2}`) pinning `radius == 2 * sizePx`, so the k=1-only blind spot is closed.

### CR-02: `exportCollectionTo` rebuilds `ef::Charge` positionally from `PresetCharge` — drops id/name/sizePx; whole-collection PDF export ignores per-charge sizes (sweep miss)

**File:** `src/gui/app.cpp:1225`
**Issue:** `for (const ef::PresetCharge& ch : c.charges) field.charges.push_back(ef::Charge{ch.charge, ef::Vec2{ch.x, ch.y}});` — a positional 2-field initializer. The trailing NSDMIs make this compile silently while dropping `id`, `name` and `sizePx`, so every dot in the exported collection PDF renders at the style default and the per-charge identity is gone. This is exactly the "silently dropped fields" class the Task-1 sweep was completeness-critical about; the grep gate only searched the `PresetCharge{c.charge…}` direction and missed the `Charge{ch.charge…}` direction. The phase claim "all exporters funnel through renderScene [with per-charge sizes]" does not hold for File > Export Collection.
**Fix:**
```cpp
for (const ef::PresetCharge& ch : c.charges)
  field.charges.push_back(ef::Charge{ch.charge, ef::Vec2{ch.x, ch.y}, ch.id, ch.name, ch.sizePx});
```
(or add the mirror helper `ef::fromPresetCharge` next to `toPresetCharge` and use it at all three PresetCharge→Charge sites — `workspace.cpp:55/163` already carry the fields by hand). Add a sweep grep for `Charge{` + `PresetCharge` consumers in both directions.

## Warnings

### WR-01: Parsers accept any finite sizePx — no 0-or-2..120 validation, unlike every other mutator

**File:** `src/app/configs.cpp:122`, `src/app/session_file.cpp:170`
**Issue:** The 6-field branch validates only `parseDouble` finiteness for `sizePx`. A hand-edited `charge = 1, 0, 0, -5, 1, x` or `1e30` enters the model (the editor, API and `addCharge` all enforce 0 or 2..120). Negative/0<x<2 values silently degrade to the default at render/hit (the `> 0` guards), but `1e30` renders a screen-filling dot and gives `hitCharge` a planetary bound (`sizePx / ppu + slack`, `editor.cpp:65`), swallowing every click nearest-first. The out-of-range value also persists and dumps verbatim.
**Fix:** In both parsers, accept the extended field only when `sizePx == 0 || (sizePx >= 2 && sizePx <= 120)`; otherwise skip the charge (same counting as an arity failure) or normalize to 0 — matching `addCharge`'s degrade-to-0 rule is the least surprising.

### WR-02: id ceiling asymmetry — the writer can emit ids the parser rejects, silently dropping the charge on reload

**File:** `src/app/configs.cpp:122` and `src/app/session_file.cpp:170` (bound `[0, 1000000000]`) vs writers `configs.cpp:56` / `session_file.cpp:71` and `editor.cpp:45-50`
**Issue:** The parser accepts ids ≤ 1e9, but `nextChargeId` is max+1 with no ceiling. A file carrying `id = 1000000000` (parseable) yields `nextChargeId = 1000000001` for the next add; the writer emits it faithfully; the **next load rejects the bound and skips the entire charge** (counted as a skipped line, never surfaced to the user) — silent data loss on a round-trip.
**Fix:** Align the bounds. Either parse the id with a uint64-safe path (e.g. `std::stoull` + full-string check, reject only absurd magnitudes) or clamp in the writer. Cheapest correct fix: raise the parser bound to `4000000000`+ via `long long` and reject > `std::numeric_limits<std::uint64_t>::max()` — as long as the parse bound is ≥ any id the writer can produce from a previously parsed file, the round-trip is closed.

### WR-03: Names with leading/trailing whitespace are silently mutated by save→load (contradicts the raw-UTF-8-passthrough pin)

**File:** `src/app/ini_text.hpp:55` (`splitCsv` trims every field) with `configs.cpp:56` / `session_file.cpp:71` (writer emits `", " << name`) and `editor.cpp:189` (`setChargeName` does not trim)
**Issue:** `setChargeName` stores names verbatim (only ',' stripped), so the model can hold `" Pad"`, `"Pad "` or `"  "`. The writer emits them raw after `", "`, and `splitCsv` trims each field on read: `" Pad"` → `"Pad"`, and a whitespace-only name collapses to `""` — which then *displays as the derived "Charge N"* after reload. The round-trip doctests ("lossless") never exercise leading/trailing whitespace, so the claim is untested and false for this class.
**Fix:** Trim the name once at the entry point (`setChargeName`, mirroring the tab-rename path at `ui.cpp:712`) so the model can never hold trimmable-only differences, or escape/quote the name field on write. Also add a round-trip doctest for `" Pad"` to pin whichever behaviour is chosen.

### WR-04: undo()/redo() unconditionally mark the tab stale — undoing a name/size-only edit produces a false "press Calculate" state

**File:** `src/app/editor.cpp:220` (undo), `src/app/editor.cpp:235` (redo)
**Issue:** The phase's pinned decision is that name/size edits are dirty-only, "never stale, never pendingRecalcAt". But the undo/redo paths restore the snapshot and set `t.stale = true` unconditionally, so Ctrl+Z after a pure rename shows the "Edited - the lines show the previous calculation" hint and F5 recomputes although the physics inputs are byte-identical. The inconsistency is new in practice because phase 3 added mutators whose only delta is non-physical.
**Fix:** After restoring in undo/redo, set stale only when the physics slice actually differs from `t.computed` (compare charge count + positions + values + method; names/sizes/ids excluded), e.g. reuse a small `samePhysics(a, b)` helper. Alternatively accept and document it, but then the hint text over-promises.

## Info

### IN-01: `charge.add` (API) does not pass the Options default — freeze-vs-track divergence from GUI-added charges (verifier F2)

**File:** `src/gui/app.cpp:1817`
**Issue:** Plan Decision 4 named `options_.draw.chargeRadiusPx` as the `charge.add` argument; the implementation uses the `defaultSizePx = 0` default. An API-added charge stores 0 and therefore *tracks* later Options changes, while GUI-added charges *freeze* the value at creation; the divergence is observable in `app.state` and on disk (`0` vs `25` for otherwise identical charges). Verifier judged this acceptable; recommend unifying (pass `static_cast<float>(options_.draw.chargeRadiusPx)`) or documenting the difference in api.md — script 45 currently pins the implementation, not the plan.

### IN-02: Panel name buffer goes stale after undo/redo or a scripted `charge.name`

**File:** `src/gui/ui.cpp:821-825`
**Issue:** `chargeNameBuf` resyncs only when `(tabSerial, selected)` changes. After `editor.undo` (name restored to empty/older value) or an API rename, the InputText still shows the old text; the next keystroke commits the stale text back over the fresher model (clobbering the undo). Same pre-existing pattern as `tabNameBuf` — new instance, low impact.
**Fix:** Re-sync the buffer when `c.name` no longer matches what the buffer was seeded from (store the seeded name or a dirty flag), or resync on undo/redo/`editor.undo` command paths.

### IN-03: 128-byte panel buffer silently truncates long API-set names

**File:** `src/gui/ui.hpp:75`, `src/gui/ui.cpp:827`
**Issue:** `charge.name` accepts unbounded names; the panel's `char[128]` InputText displays at most 127 bytes and the next panel edit persists the truncated form (mid-UTF-8 truncation can also split a code point). Fix: clamp name length in `setChargeName` (e.g. 120 bytes, rejecting or truncating consistently) or document the panel limit.

### IN-04: `app.state` dump format is ambiguous for names containing `;` or `=`

**File:** `src/gui/app.cpp:1624-1626` with `editor.cpp:189`
**Issue:** Only ',' is stripped from names, but api.md documents `charges=` as a `;`-separated list: a charge named `A;B` glues the following charge's fields into the name for any consumer that splits on ';'. The script suite passes only because `app.expect` is substring-based. Fix: strip ';' in `setChargeName` too (same one-line rule as ','), or document the escaping rule.

### IN-05: Script 45 never round-trips a non-zero sizePx through the collection file

**File:** `tests/scripts/45-charge-identity.efscript:16-24`
**Issue:** The 40 px size is set, then *undone* before `config.save-as`; the on-disk round-trip assertion pins `sizePx = 0` only. The non-zero-size file path is covered solely by in-memory serialize→parse doctests. Fix: `charge.size 0 40` again after the undo, before `config.save-as`, and expect `charges=1,0,0,40,1,My Charge` after `config.open`.

### IN-06: `fittedView`'s fixed 64 px margin is smaller than the maximum per-charge radius

**File:** `src/gui/app.cpp:1183-1184`
**Issue:** The deterministic PDF exports guarantee charge *centres* ≥ 64 px from the canvas edge; a charge with `sizePx` up to 120 at the bounding-box boundary now clips up to 56 px of dot in `exportConfigurationTo`/`exportCollectionTo`. Fix: `margin = std::max(64.0, maxEffectiveRadius(charges))` using `sizePx > 0 ? sizePx : style.chargeRadiusPx`.

---

_Positive notes (no action): the 3-or-6 grammar is all-or-nothing with a correct trailing-comma guard for empty names (verified byte-level on disk, incl. CRLF-safe `trim`); `parseIntBounded`/`parseDouble` are overflow- and garbage-safe; the id watermark is correctly outside the history snapshots and set from the restored max in every open path (pinned by `rt.chargeIdWater == 10`); `setChargeSize` validation is NaN/±inf-safe; the doctests are genuine exact-value assertions; `toPresetCharge` covers all five save/merge/dump sites in app.cpp/ui.cpp; makeSystem stays field-carrying for ef-cli parity._

_Reviewed: 2026-10-14T06:30:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
