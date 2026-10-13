---
phase: 1
reviewed: 2026-10-13
depth: deep
status: findings
files_reviewed: 10
files_reviewed_list:
  - src/app/session_file.hpp
  - src/app/session_file.cpp
  - src/gui/app.cpp
  - src/gui/app.hpp
  - src/gui/ui.cpp
  - tests/test_session_file.cpp
  - tests/scripts/44-recents-seed.efscript
  - .github/workflows/ci.yml
  - README.md
  - docs/api.md
findings:
  critical: 0
  major: 1
  minor: 2
  info: 1
  nit: 2
  total: 6
verified:
  - "zig build test: 133 + 4 doctest cases, 213,988 assertions, 0 failures"
  - "script 44 end-to-end with CI-equivalent printf pre-seed: exit 0, both expectations pass, session.ini unchanged after run"
  - "old quit code re-read from 392f5e9^ to characterize the dump-branch regression precisely"
---

# Phase 1 (v2.6 collections-ux-fixes): Code Review Report

**Reviewed:** 2026-10-13
**Depth:** deep (per-file + cross-file tracing of the pointer write/read lifecycle, quit flow, chooser flow, CI sandbox pairing)
**Files Reviewed:** 10
**Commits:** 6c1fbd6, 392f5e9, a75babb, 0e5b873
**Status:** findings

## Summary

The phase delivers REC-02 (seed recents from the session pointer, never blank the pointer, rewrite it at quit) and COL-10 (visible chooser Remove... button). The core seams are solid: `hasCollection` is set only for a genuinely stored, non-empty `collection` line and is correctly threaded through `ParsedSession` → `SavedSession`; `seedRecents` is a clean pure function; the init seeding gates on `exists()`; the quit rework genuinely fixes the empty-recents-on-upgrade defect (collection tabs now go to `Sessions/<name>.ini` via `saveSessionNow`, keeping grid/autocalc the old inline dump dropped); the chooser button reuses the right-click confirm flow with a correct bounds guard. Docs match the implementation. Unit tests and script 44 pass locally; script 44 is idempotent on re-runs and its CI pre-seed cannot touch the 30/42/43 sandbox pairs.

One major finding: the "collection-less preset tabs migrate through the pointer" quit branch writes the tabs into the pointer and then destroys them one line later — dead code today (proved unreachable through every shipped path), but it contradicts its own comment, the commit message, and the quit prompt's promise, and it is a data-loss trap if ever resurrected. Two minor consistency/robustness issues (duplicated recents-cap constant; seeding caps only when inserting) and a few nits round it out.

Security: **clean.** Local ini files only; `collectionFile` sanitizes path separators before any path is built; the parser is bounds-checked (`parseIntBounded`); the CI pre-seed writes a fixed literal into its own sandbox directory; no secrets, no injection surface, no unsafe deserialization.

## Major

### MAJ-01: Quit-time pointer dump of collection-less tabs is immediately destroyed by `writeSessionPointer()`

**File:** `src/gui/app.cpp:541-545` (comment block `:520-526`; interplay with `writeSessionPointer` `:1251-1265` and the startup migration `:207-212`)
**Issue:** The quit persist block's middle branch writes the open tabs *into the pointer* (`saveSession(sessionPath_, saved, std::string(), recents_, err)`, `:543`), and the very next statement `writeSessionPointer()` (`:545`) rewrites the pointer with `{}` tabs unconditionally. Since the dump wrote a **blank** collection line, the read-modify-write in `writeSessionPointer` (`:1259-1262`) finds `prev.hasCollection == false`, keeps `collection == ""`, and saves `saveSession(sessionPath_, {}, "", recents_, err)` — the tabs written at `:543` are erased before the process exits. At the next start, `legacy.tabs.empty()` makes the migration at `:207-212` a no-op, so the tabs are nowhere on disk.

This contradicts three written claims:
- the code comment `:524-525`: *"Only collection-less preset tabs still migrate through the pointer"* — they don't; they are wiped;
- the commit message of 392f5e9 (same wording);
- the quit prompt `:492`: *"They are restored on the next start"* — with this branch they would not be.

**Why it matters:** latent silent data loss plus false documentation. It is **not** currently reachable — which is the only reason this is not critical: every tab-opening path requires a collection open (chooser `ui.cpp:958-959`, Recent Collections `ui.cpp:970`, command `tab.new-empty` gated at `app.cpp:1712`), `closeCollection()` closes all tabs (`app.cpp:1401`), no CLI flag opens tabs, and scripts exit via `runScript()` (`:2042`), which never runs this block. But the branch exists precisely as a defensive path for a state the code elsewhere insists cannot happen; if anything ever re-opens that state (e.g. preset tabs without a collection returning), the first quit destroys the user's tabs with no error anywhere.

**Fix:** make the dump branch self-consistent. Minimal change — the dump already carries `recents_`, and the startup migration (`:211`) rewrites the pointer preserving recents, so simply skip the redundant rewrite when the dump ran:

```cpp
if (!currentCollection_.empty()) {
  saveSessionNow();
  writeSessionPointer();                       // pure pointer: collection + recents
} else if (!saved.empty()) {
  std::string err;
  // Tabs go through the pointer (legacy shape); the startup migration turns this into
  // Sessions/<collection>.ini + a pure pointer, recents preserved.
  if (!efapp::saveSession(sessionPath_, saved, std::string(), recents_, err))
    std::fprintf(stderr, "session: %s\n", err.c_str());
} else {
  writeSessionPointer();                       // nothing to dump: just refresh the pointer
}
```

Alternatively (arguably better): dump to `Sessions/Original.ini`-equivalent instead of the pointer and keep `writeSessionPointer()` unconditional. While there, note the rebuilt `saved` vector (`:528-538`) is a third near-copy of `saveSessionNow`'s body that still omits `autoRecalc`/`grid` — folding the dump onto one helper would remove the drift risk this finding exposed. Add a doctest/script-level pin for whatever shape is chosen, since no test currently exercises this branch.

## Minor

### MIN-01: The recents cap exists as two independent constants that must stay equal by convention

**File:** `src/app/session_file.hpp:25` (`kMaxRecents = 10`) vs `src/gui/app.hpp:37` (`kMaxRecentCollections = 10`)
**Issue:** The new header comment promises *"the same 10 the GUI's recents menu enforces"*, but nothing enforces it: `seedRecents` caps at `kMaxRecents` (`session_file.cpp:207`) while `noteRecentCollection` caps at `kMaxRecentCollections` (`app.cpp:1273`). Two duplicated magic 10s in different layers; changing one silently desynchronizes the seeding cap from the menu cap (e.g. a pointer seeded to 15 entries immediately re-capped to 10 on the next open — harmless but confusing, or worse, the reverse).
**Why it matters:** the whole point of extracting `kMaxRecents` was single-sourcing the cap; the extraction stopped one step short.
**Fix:** single-source it — in `src/gui/app.hpp` (or delete the GUI constant and use it directly in `noteRecentCollection`):

```cpp
#include "app/session_file.hpp"                      // if not already pulled in transitively
inline constexpr std::size_t kMaxRecentCollections = efapp::kMaxRecents;
```

### MIN-02: `seedRecents` caps only when inserting, contradicting its documented contract

**File:** `src/app/session_file.cpp:203-209`; parser accepts up to 100 `recent` lines at `session_file.cpp:110`
**Issue:** The header doc says the result is *"capped at kMaxRecents"*, but the cap fires only on the insert path. A pointer carrying more than 10 `recent` lines (parser permits 100 — reachable via hand-editing or a corrupt-but-parseable file) passes through **uncapped** when `collectionKey` is empty or already present, so `recents_`, the File menu, and every subsequent pointer write preserve the over-long list indefinitely. Not reachable from app-written files (every writer caps at 10), so this is a robustness/doc-conformance gap, not a live bug. (All consumers stay memory-safe: `collection.open-recent` bounds-checks against `recents_.size()` at `app.cpp:1673`.)
**Why it matters:** the invariant "recents ≤ 10" is what the menu rendering and the docs lean on; one unexpected writer breaks it everywhere at once.
**Fix:** cap unconditionally:

```cpp
std::vector<std::string> seedRecents(std::vector<std::string> recents, const std::string& collectionKey) {
  if (!collectionKey.empty() && std::find(recents.begin(), recents.end(), collectionKey) == recents.end())
    recents.insert(recents.begin(), collectionKey);  // the pointer's collection is the most recently opened
  if (recents.size() > kMaxRecents) recents.resize(kMaxRecents);
  return recents;
}
```

## Info

### IN-01: Startup seeding resolves the pointer's collection name only under `Collections\` — same-stem external collections can seed the wrong file

**File:** `src/gui/app.cpp:228-231` (`collectionFile(pointer.collection)` + `exists` gate)
**Issue:** The pointer stores a collection *name* (v2.2 format, the file's stem), and seeding resolves it to `<userDir>/Collections/<stem>.ini`. Two boundary effects: (a) a collection opened from an arbitrary path whose recents entry aged out of the top 10 is not re-seeded at startup (its `Collections\` twin doesn't exist); (b) if a *different* document with the same stem does live under `Collections\`, that twin is seeded instead of the file actually open at the previous quit. Inherent to the name-only pointer format and narrow in practice (needs a same-stem collision plus recents eviction); the phase's stated target — legacy pointers whose collection is the `Collections\`-resident `Original` — works correctly.
**Fix:** none required for this phase; document the boundary in `docs/api.md` (one sentence: seeding resolves the name under `Collections\` only), or store the collection's full path alongside the name in a future pointer-format revision.

## Nits

### NIT-01: "Never blank" read-modify-write silently blanks when `session.ini` exists but is unreadable

**File:** `src/gui/app.cpp:1259-1262`
**Issue:** If `session.ini` exists but `readFile` fails (ACL/locked-file edge cases on network shares), `loadSessionFull` returns defaults (`hasCollection == false`) and a successful subsequent write loses the stored name — the one case the "never blank" guarantee doesn't cover. Exotic; today the same conditions usually make the write fail too (stderr note only).
**Fix:** treat "exists but unreadable" as skip-don't-rewrite: check the read outcome (e.g. compare against a `readFile` failure signal or a `hasCollection && collection.empty()`-style sentinel) and leave the file untouched rather than overwriting with a blank line.

### NIT-02: Doctest gaps — trailing-space blank value and the present-key/over-cap case are unpinned

**File:** `tests/test_session_file.cpp:119-124, 225-243`
**Issue:** (a) The blank-value parse test uses `"collection =\n"` only; the trailing-whitespace form `"collection =   \n"` (the `ini::trim` path at `session_file.cpp:102`) is untested. (b) No test pins that a key that *is* present in an over-cap list is returned uncapped — which is exactly the behavior MIN-02 proposes to change, so pinning it (before/after) keeps the change intentional.
**Fix:** add `parseSession("collection =   \n[tab ...")` → `CHECK_FALSE(hasCollection)`, and a `seedRecents(overCapListWithKey, key)` case stating the expected shape.

## Verified clean (adversarial pass, no findings)

- **`hasCollection` semantics** (`session_file.cpp:104-108`): set only for a non-empty post-trim value; blank line, `collection =` with trailing spaces, and fully absent line all parse as `Original` with the flag false; the parser's `"Original"` default stays distinguishable. Multiple `collection` lines: last non-empty wins (benign). Hand-quoted values (`collection = "X"`) are not quote-stripped, but no writer produces them and `collectionFile`'s bad-char sanitization (`app.cpp:1225`, `"` → `_`) plus the `exists` gate degrades them safely to "no seed".
- **`seedRecents`** (`session_file.cpp:203-209`): empty list, already-present (order intact), empty key, insert-at-front, and cap-with-drop are all correct and pinned by doctests (`test_session_file.cpp:225-243`); pure function, value-taken param enables move-in; matches the file's style.
- **Read-modify-write** (`app.cpp:1251-1265`): first-ever write stays blank (`exists` gate, no invented collection); blank/whitespace stored value is correctly treated as "nothing to preserve"; a preserved name survives pointer-only writes (recent drop `:420`, collection-less quit).
- **Quit rework order**: prompt-first is preserved — `iterate()` (`:490-499`) intercepts the first quit with the leave-confirm modal and only lets the loop exit on confirmation, so the persist block (`:527-546`) runs strictly after the prompt. Collection-open quit: `saveSessionNow()` (keeps grid/autocalc — an actual repair vs the old inline dump) then pointer rewritten with `collection = <name>` + recents. The empty-recents-on-upgrade defect is genuinely fixed.
- **Chooser Remove...** (`ui.cpp:341-347`): `cfgSelected` bounds-checks `chooserSel` before the `userConfigs` index; `BeginDisabled` keeps keyboard nav off; flow is byte-identical to right-click (`removeConfigName`/`removeConfigOpen` → confirm → `removeConfigApply` → `removeConfiguration`); post-removal stale `chooserSel` is re-guarded everywhere (`:357`) and self-heals. No OOB in any state.
- **CI (44 + printf)**: the pre-seed writes only `script-out/44-recents-seed/session.ini` — the 41→40 / 43→42 dir pairings and scripts 30/42/43 are untouched; on a dirty-sandbox re-run the `printf` overwrites deterministically, and the run itself never modifies the pointer (`runScript` calls only `saveSessionNow`, a no-op with no collection) — verified live. `app.expect recents=Original` is a true positive: it cannot match the `collection=`/`collections=` lines, and it fails loudly (`recents=` empty) if seeding regresses. A failed `mkdir -p` skips the pre-seed silently, but 44 then fails its expectation, so the failure still surfaces.
- **Docs**: README bullet and `docs/api.md` REC-02/COL-10 text match the implemented behavior (seeding gate, quit rewrite, never-blank pointer, Remove... with confirmation).
- **Consistency**: new code follows house conventions throughout — `ini::trim`, `u8string`/`u8path` for recents keys, `std::fprintf(stderr, ...)` for session-write failures, version-tagged comments.
- **Security**: clean (see Summary).

---

_Reviewed: 2026-10-13_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
