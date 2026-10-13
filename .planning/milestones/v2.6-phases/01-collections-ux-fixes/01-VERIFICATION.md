---
phase: 1-collections-ux-fixes
verified: 2026-10-13T14:57:27Z
status: passed  # human items auto-accepted per standing auto-mode instruction (recorded below for owner UAT)
score: 3/5 must-haves verified
covered_files:
  - .planning/phases/01-collections-ux-fixes/01-CONTEXT.md
  - .planning/phases/01-collections-ux-fixes/01-01-PLAN.md
  - src/app/session_file.hpp
  - src/app/session_file.cpp
  - src/gui/app.cpp
  - src/gui/ui.cpp
  - tests/test_session_file.cpp
  - tests/scripts/44-recents-seed.efscript
  - .github/workflows/ci.yml
  - README.md
  - docs/api.md
covered_digest: "v2:21fe5d4cf0cf0939d45a968951c8e5819adcc0ada7678a95f8d755ad4e660a1e"
behavior_unverified: 2
behavior_unverified_items:
  - truth: "The pointer file is rewritten on quit, so closing the app without switching collections still leaves the recents current on disk"
    test: "Interactive: start with a pre-upgrade session.ini (only `collection = Original`), open nothing, quit from the File menu; reopen and check File > Recent Collections"
    expected: "session.ini after quit carries `collection = Original` (not blanked) plus a `recent =` line for it; the next start shows Original under Recent Collections"
    why_human: "runInteractive's shutdown block (src/gui/app.cpp:527-546) is not reachable by any automated path: runSmoke (main.cpp:131) and runScriptMode (app.cpp:2014-2045) are separate shutdowns. Code is present+wired; the transition itself is unexercised"
  - truth: "In the chooser, the selected configuration can be removed with a visible Remove button (confirmation dialog, open tabs unaffected)"
    test: "Open a collection, File > New/Open Configuration..., select a configuration row -> Remove... -> confirm; also select Empty Configuration and check the button is disabled and skipped by keyboard nav"
    expected: "Click opens the existing 'Remove configuration?' dialog; confirming erases the entry from the collection file with open tabs unaffected; the button is greyed out for 'Empty Configuration'/no selection"
    why_human: "The click transition (ui.cpp:343-346) sets the same two fields as the shipped right-click path (ui.cpp:326-329), and removeConfigModal -> removeConfiguration is script-proven via config.remove (script 30), but no automated test clicks the button; enable/disable rendering and nav skipping are visual"
---

# Phase 1: Collections UX Fixes — Verification Report

**Phase Goal:** Recent Collections is populated from the session pointer even when starting fresh (pre-recents pointers seed their collection), the pointer is rewritten at quit, and removing a configuration from the collection is a visible action in the chooser (selected row -> Remove..., confirmation kept; right-click keeps working).
**Verified:** 2026-10-13T14:57:27Z (working tree = 0e5b873, the phase's last commit)
**Status:** human_needed (no gaps; 2 behavior-dependent truths await interactive confirmation)
**Re-verification:** No — initial verification

All commands below were run by the verifier in this checkout (zig 0.17.0, macOS arm64).

| Check | Command | Result |
|-------|---------|--------|
| Unit + doctests | `zig build test --summary all` | 133 + 4 test cases, **0 failed**, 0 skipped; 17/17 steps |
| Script suite | 10x `--script tests/scripts/*.efscript --user-dir <fresh>` with the CI pairings (41->40, 43->42) and the `printf 'collection = Original\n'` pre-seed for 44 | all exit 0, no `[error]` lines |
| Script 44 | exactly as CI (ci.yml:101): fresh sandbox, legacy pointer pre-seeded | `[ok] app.expect recents=Original`, `[ok] app.expect collections=Original` |
| Smoke | `zig build smoke -Doptimize=ReleaseFast` | `smoke ok frames=294`, extras all ok; 18/18 steps |
| Never-blank probe | script: `collection.open Original` + `collection.close` + quit in a legacy-seeded sandbox | session.ini afterwards: `collection = Original` + `recent = .../Original.ini` (not blanked) |
| Anti-pattern scan | TBD/FIXME/XXX/TODO/PLACEHOLDER/stub greps over all 9 touched files | clean |

## Goal Achievement

### Observable Truths (ROADMAP success criteria are the contract; the PLAN adds no additional must_haves)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | SC1: a pre-recents pointer (`collection = X`, no recents) yields Recent Collections containing X on the next start; a script proves it | ✓ VERIFIED | Full path: `App::init` (src/gui/app.cpp:226-232) loads the pointer once, gates on `hasCollection` (src/app/session_file.cpp:102-108), resolves `collectionFile(pointer.collection)` + `exists`, seeds via `seedRecents` (front-insert, cap: src/app/session_file.cpp:200-208); mirrored to the menu each frame (app.cpp:297 -> ui.cpp:968 `BeginMenu("Recent Collections")`) and to the `app.state recents=` oracle (app.cpp:1571-1573). Script 44 + ci.yml:101 pre-seed ran green end-to-end: `[ok] app.expect recents=Original` |
| 2 | SC2: opening collections still dedupes to the front and caps at 10 (30/42/43 stay green) | ✓ VERIFIED | All three ran green in the CI-identical loop. Script 42 asserts re-ordering-by-open + 12 collections -> exactly 10 entries (`collection.open-recent 10` = R3) + `open-recent 11` errors; script 43 restores R4 as the 10th from the pointer; script 30 asserts `recents=Original|Test` newest-first. Sandbox inspection after the paired run: exactly 10 `recent =` lines, R4 newest. Cap constants agree (`kMaxRecents = 10` session_file.hpp:24; `kMaxRecentCollections = 10` app.hpp:37); `noteRecentCollection` (app.cpp:1269-1274) is untouched |
| 3 | SC3: the pointer file is rewritten on quit, so closing without switching still leaves recents current on disk | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Code present + wired: `runInteractive` shutdown (app.cpp:527-546) -> always `writeSessionPointer()`; `writeSessionPointer` never blanks (read-modify-write, app.cpp:1258-1262; first-ever write stays blank via the exists gate). The never-blank branch is behaviorally probed green (collection.open + collection.close -> pointer still `collection = Original`); doctests pin its inputs (`hasCollection` parse + round-trip, tests/test_session_file.cpp:114-138). The quit-time transition itself is only reachable interactively -> Human Verification #1 |
| 4 | SC4: chooser Remove... button, enabled only for configuration rows, confirmation kept; right-click + `config.remove` still work | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Button present and correctly gated: src/gui/ui.cpp:341-347 — `cfgSelected = chooserSel >= 0 && < cfgCount` (last row is "Empty Configuration"), real `BeginDisabled`. Click sets `removeConfigName`/`removeConfigOpen` — the exact two fields of the shipped right-click path (ui.cpp:326-329); `removeConfigModal` (ui.cpp:371) -> `removeConfigApply` -> `removeConfiguration` (app.cpp:438-441, 1411-1420: erases from the collection file only; no `ws_` touch, open tabs unaffected). `config.remove` (app.cpp:1691) is script-proven (script 30 line 39-41, green). The button's click-through + disabled rendering are interactive-only -> Human Verification #2 |
| 5 | SC5: `zig build test` green, smoke green, script suite green; docs updated (api.md, README) | ✓ VERIFIED | 133+4 cases 0 failed; `smoke ok frames=294`; 10/10 scripts exit 0 in fresh sandboxes mirroring CI. README Collections bullet (README.md:37-48) documents seeding, quit-time rewrite, and the Remove... button; docs/api.md:41-46 + 71-73 document the `recents=` seeding/rewrite and COL-10 in `config.remove` |

**Score:** 3/5 truths verified (2 present + wired, transitions not machine-exercisable — routed to human verification)

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | -------- | ------ | ------- |
| `src/app/session_file.hpp` | `hasCollection` on both structs, `kMaxRecents`, `seedRecents` decl | ✓ VERIFIED | lines 24, 49, 67, 74-77; documented |
| `src/app/session_file.cpp` | flag set on real parse only; `loadSessionFull` copies it; `seedRecents` impl | ✓ VERIFIED | lines 102-108, 197, 200-208; empty key / already-present / front-insert / truncate |
| `src/gui/app.cpp` | init seeding; never-blank `writeSessionPointer`; quit rewrite | ✓ VERIFIED (wiring) | lines 226-232, 1251-1265, 527-546 |
| `src/gui/ui.cpp` | Remove... button in `chooserModal`, wired to the confirm flow | ✓ VERIFIED (wiring) | lines 337-347 |
| `tests/test_session_file.cpp` | doctests: flag stored/blank/absent + round-trip; all six seed cases | ✓ VERIFIED | lines 114-138, 221-245; all green |
| `tests/scripts/44-recents-seed.efscript` | REC-02 script proof | ✓ VERIFIED | green exactly as CI runs it |
| `.github/workflows/ci.yml` | 44 sandbox pre-seeding | ✓ VERIFIED | line 101, inside the existing loop |
| `README.md`, `docs/api.md` | user + API docs of the new behaviour | ✓ VERIFIED | README.md:37-48; api.md:41-46, 71-73, 78 |

All artifacts are wired: the seam is consumed by app.cpp; the button sets the fields the shipped modal consumes; the script is consumed by CI. No orphans.

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | -- | --- | ------ | ------- |
| `App::init` (app.cpp:226) | `efapp::seedRecents` | `pointer.hasCollection` + `collectionFile` + `exists` gate | WIRED | seeded list assigned to `recents_` |
| `recents_` | File > Recent Collections menu | `frame()` mirror app.cpp:297 -> ui.cpp:968 | WIRED | menu is empty exactly when the list is (`!ui.recentCollections.empty()`) |
| `recents_` | `app.state recents=` oracle | `recentMenus(recents_)` app.cpp:1571-1573 | WIRED | what script 44 asserts |
| quit block (app.cpp:545) | `writeSessionPointer` | unconditional call after the collection-open / dump branch | WIRED | pointer rewritten in every quit shape |
| `writeSessionPointer` (app.cpp:1259-1261) | stored collection preserved | `loadSessionFull` read-modify-write gated on `hasCollection` | WIRED | behaviorally probed via `collection.close` |
| chooser button (ui.cpp:343-346) | confirm flow | `removeConfigName`/`removeConfigOpen`, identical to right-click (ui.cpp:326-329) | WIRED | same consumer `removeConfigModal` -> `removeConfigApply` (app.cpp:438) |
| ci.yml:101 | script 44 sandbox | `printf 'collection = Original\n'` pre-seed | WIRED | ran green in the replicated loop |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| Recent Collections menu | `recents_` | session.ini pointer parsed at init (+ live updates on open/drop) | yes — real file I/O | ✓ FLOWING |
| `app.state recents=` | `recents_` | same list through `recentMenus` | yes | ✓ FLOWING |
| Chooser rows + Remove... | `ui.userConfigs` | `userConfigs_` from the open collection file (app.cpp:1286, 1418) | yes | ✓ FLOWING |

No static fallbacks, no hardcoded menus, no hollow props.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | -------- | ------ |
| SC1 end-to-end | script 44 with CI-identical pre-seed (`printf 'collection = Original\n' > $dir/session.ini`) | `[ok] app.expect recents=Original`, `[ok] app.expect collections=Original` | ✓ PASS |
| SC2 dedupe/cap/restore | scripts 30, 42, 43 in CI-shaped sandboxes | all `[ok]`, 42's sandbox pointer holds exactly 10 `recent` lines, R4 first | ✓ PASS |
| Never-blank pointer | probe script: `collection.open Original` + `collection.close` + `app.quit force` in a legacy-seeded sandbox | session.ini keeps `collection = Original` + `recent` line | ✓ PASS |
| Tabs survive quit-with-collection | scripts 40/41 pairing (script-mode quit -> `saveSessionNow` -> restore) | 41 restores 2 tabs incl. "My Dipole" | ✓ PASS |
| Suite regression | full 10-script loop, CI-identical | exit 0, zero `[error]` | ✓ PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` probes exist for this phase and none are declared in PLAN/SUMMARY. The phase's declared probe surface (script 44 + CI seeding) is covered by the Behavioral Spot-Checks above, run from the repository root exactly as CI does.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ---------- | ----------- | ------ | -------- |
| REC-02 | 01-01-PLAN (objective; REQUIREMENTS.md:314) | Recents seeded from a legacy pointer at startup; pointer rewritten at quit | ✓ SATISFIED | SC1 + SC3 evidence above |
| COL-10 | 01-01-PLAN (objective; REQUIREMENTS.md:315) | Visible Remove action in the chooser with the existing confirmation; right-click and `config.remove` unchanged | ✓ SATISFIED | SC4 evidence above |

No orphaned requirements: REQUIREMENTS.md maps exactly REC-02, COL-10 to phase 1, and the plan claims both.

### Decision Coverage

All 4 `<decisions>` entries of 01-CONTEXT.md are honored in the shipped code: (1) seed at init after loading, gated on `hasCollection` + existing file, front-insert — app.cpp:226-232; (2) persist at quit + `writeSessionPointer` never blanks (read-modify-write, exists gate) — app.cpp:545, 1258-1262; (3) `Remove...` under the list, enabled only for `chooserSel < cfgCount`, sets the existing confirm flow — ui.cpp:341-347; (4) right-click removal and `config.remove` kept exactly as they are — ui.cpp:326-329 and app.cpp:1691 are byte-identical to pre-phase (verified in the 392f5e9/a75babb diffs: no hunks touch them). Honored 4/4.

### Regression Sweep (quit-path rework)

| Concern | Verdict | Evidence |
| ------- | ------- | -------- |
| Dirty tabs + collection open at quit: prompt still precedes the save | ✓ SAFE | The leave-confirm (app.cpp:491-499) resets `quit_`/`requestQuit` until confirmed, so `iterate()` keeps looping; `runInteractive`'s shutdown block (527-546) only runs after the loop exits. `saveSessionNow` (1327-1344) writes tabs to `Sessions/<name>.ini` regardless of dirty — the prompt's promise ("not saved to a collection [file]") is unchanged |
| Legacy pre-v2.4 session.ini with tabs still migrates once | ✓ SAFE | Migration runs at init (app.cpp:205-212) BEFORE the new seeding (226-232), writes `Sessions/<collection>.ini` then rewrites the pointer preserving `collection` + `recents`; the rewritten pointer then seeds correctly. Tabs cannot be re-migrated (pointer rewritten tab-less) |
| Collection-less preset tabs quit shape (app.cpp:541-543) | ✓ SAFE (unreachable) | Tabs cannot exist without a collection in the interactive app: the chooser is opened only from collection-gated menu entries (ui.cpp:956-957), `collection.close` does `saveSessionNow()` + `ws_.closeAll()` (app.cpp:1400-1401), and there are no preset-tab menu entries. The branch is defensive dead code; its 4-arg dump (blank collection + recents preserved) is strictly better than the pre-phase 3-arg dump that wrote the `Original` default |
| `writeSessionPointer` after the dump erases tabs (PLAN Risks warning) | ✓ NOT VIOLATED for reachable shapes | Shape A (collection open): `saveSessionNow` writes tabs to `Sessions/`, pointer write (545) writes `{}` tabs to the pointer — session.ini is SUPPOSED to be tab-less (COL-09). Shape B (no collection, no tabs): nothing to erase. Only the unreachable Shape C would dump-then-erase |
| Script-mode shutdown untouched | ✓ CONFIRMED | `runScriptMode` ends `if (!quit_) ws_.closeAll(); saveSessionNow();` (app.cpp:2041-2043) — no pointer write, exactly the pre-phase code; `app.quit force` semantics unchanged (script 30/42/43/44 green) |
| Recents drop on vanished file | ✓ SAFE | `collection.open-recent` drop path still persists via `writeSessionPointer` (app.cpp:420) — now never-blank, so it can no longer blank a stored collection |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| (none) | — | — | — | No TBD/FIXME/XXX/TODO/HACK/PLACEHOLDER markers, no stub returns, no hardcoded empty data in any of the 9 touched files |

### Test Quality Audit

| Test File | Linked Req | Active | Skipped | Circular | Assertion Level | Verdict |
| --------- | ---------- | ------ | ------- | -------- | --------------- | ------- |
| tests/test_session_file.cpp | REC-02 | 2 extended TEST_CASEs | 0 | no (fixed-vector oracle) | Value (exact vector equality, flag round-trips) | VALID |
| tests/scripts/44-recents-seed.efscript | REC-02 | 3 asserts | 0 | no (app.state oracle) | Behavioral (seeded state after start) | VALID |
| tests/scripts/30/42/43 | REC-02 (regression) | 39+24+5 asserts | 0 | no | Behavioral | VALID |

Disabled tests on requirements: 0. Circular patterns: 0. Insufficient assertions: 0.

## Human Verification Required

### 1. Quit-time pointer rewrite (SC3 transition)

**Test:** In the real app with a per-user config dir carrying a pre-upgrade `session.ini` (only `collection = <name>`): start, do NOT switch collections, quit via File > Exit. Inspect `session.ini`, then start again.
**Expected:** After quit the file carries the stored `collection = <name>` line (NOT blanked) plus `recent =` lines including that collection; on restart File > Recent Collections lists it. Repeat once after opening + closing a collection: the list must stay current without any collection switch.
**Why human:** `runInteractive`'s shutdown block (src/gui/app.cpp:527-546) has no headless path — smoke and script mode use different shutdowns. Everything up to the transition is machine-verified; only this final write-then-restart needs eyes.

### 2. First-ever pointer write stays blank (never-blank exists gate)

**Test:** On a brand-new config dir (no session.ini), start the app, open no collection, quit.
**Expected:** `session.ini` is written with an empty `collection =` line and no invented `Original`; the next start seeds nothing (recents empty unless files were opened).
**Why human:** The exists gate (app.cpp:1259) fires only on the first interactive quit; no automated run reaches it.

### 3. Chooser Remove... button in the real GUI (SC4 click-through)

**Test:** Open a collection, File > New/Open Configuration...: select a configuration row -> the `Remove...` button (bottom row, left of Open/Close) is enabled -> click -> confirm. Then select "Empty Configuration" and tab/arrow around.
**Expected:** The "Remove configuration?" dialog opens; confirming removes the entry from the collection file with open tabs unaffected; the button is visibly disabled for "Empty Configuration"/no selection and keyboard nav skips it (list rows keep default focus).
**Why human:** The click handler (ui.cpp:343-346) is structurally identical to the shipped right-click path and its consumer is script-proven, but button rendering, disabled state, and nav skipping are visual/interactive.

### 4. Quit with dirty tabs while a collection is open (prompt ordering after the rework)

**Test:** Open a collection, edit a tab (dirty), quit; confirm "Quit anyway".
**Expected:** The prompt appears before shutdown; after quit the tabs are in `Sessions/<name>.ini` (restored when the collection is re-opened) and session.ini is a clean pointer with current recents.
**Why human:** The prompt is exercised by script 35/36-style checks in the harness only for the prompt itself; the prompt-then-new-shutdown ordering is an interactive sequence.

## Gaps Summary

None. Every deliverable of the plan exists, is substantive, and is wired; the 5 roadmap success criteria are satisfied by code + machine evidence except the final interactive transitions of SC3/SC4, which the plan itself scoped as interactive-only (PLAN Risks: "Quit-time rewrite is not script-reachable... SC3 is covered interactively (UAT)"). The context's out-of-scope guarantees hold: recents cap/order semantics unchanged (constants and `noteRecentCollection` untouched), `config.remove` semantics unchanged (byte-identical handler), collection file format untouched (no diff to `saveConfigs`/config parsing).

---

_Verified: 2026-10-13T14:57:27Z_
_Verifier: the agent (gsd-verifier)_
