---
phase: 1
fixed_at: 2026-10-13T22:40:00+07:00
review_path: .planning/phases/01-collections-ux-fixes/01-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 1 (v2.6 collections-ux-fixes): Code Review Fix Report

**Fixed at:** 2026-10-13T22:40:00+07:00
**Source review:** .planning/phases/01-collections-ux-fixes/01-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 5 (MAJ-01, MIN-01, MIN-02 incl. NIT-02's doctest cases, NIT-01, IN-01)
- Fixed: 5
- Skipped: 0

## Fixed Issues

### MAJ-01: Quit-time pointer dump of collection-less tabs is immediately destroyed

**Files modified:** `src/gui/app.cpp`
**Commit:** ccb89a3
**Status:** fixed: requires human verification (control-flow restructure; syntax + suites verified, semantics confirmed by reading)
**Applied fix:** `writeSessionPointer()` now runs only in the collection-open and no-tabs branches of the quit persist block; the collection-less tab-dump branch skips it (the dump already carries the recents, and the startup migration turns the dump back into `Sessions/<collection>.ini` + a pure pointer). Comment block rewritten to state the actual rule. No doctest added — `runInteractive` has no pure seam (GUI state + SDL loop); noted in the commit message.

### MIN-01: Recents cap existed as two independent constants

**Files modified:** `src/app/session_file.hpp`, `src/app/session_file.cpp`, `src/gui/app.hpp`, `src/gui/app.cpp`, `tests/test_session_file.cpp`
**Commit:** fa80206
**Applied fix:** single constant `efapp::kMaxRecentCollections = 10` in `session_file.hpp` (renamed from `kMaxRecents`); `seedRecents` and `efgui`'s `noteRecentCollection` both use it; the duplicate `gui/app.hpp` constant removed.

### MIN-02: seedRecents caps only when inserting (folded in NIT-02's doctest cases)

**Files modified:** `src/app/session_file.cpp`, `src/app/session_file.hpp`, `tests/test_session_file.cpp`
**Commit:** 0aae4be
**Status:** fixed: requires human verification (contract change; pinned by new doctests)
**Applied fix:** cap is unconditional — an over-cap pointer (parser admits 100 `recent` lines) comes back capped whether the key is absent, inserted, or already present. Header doc updated. New doctests: over-cap list with key inserted (capped with drop), over-cap list with key already present (order intact, tail dropped), and the trailing-space blank `collection =   ` parse (NIT-02a: `hasCollection` stays false).

### NIT-01: Never-blank pointer write blanks an unreadable session.ini

**Files modified:** `src/gui/app.cpp`
**Commit:** b1300cf
**Status:** fixed: requires human verification (error-path guard)
**Applied fix:** `writeSessionPointer()` probes readability before the read-modify-write; an existing-but-unreadable `session.ini` is left as-is (whole write skipped) instead of being rewritten with a blank collection line.

### IN-01: Name-only pointer resolution boundary

**Files modified:** `src/gui/app.cpp`
**Commit:** 05e7d9d
**Applied fix:** comment only, at the seeding site: name-only pointers (v2.2) resolve under `Collections\`; a same-stem collection opened from another path is not re-seeded, and a same-stem twin under `Collections\` is seeded instead.

## Skipped Issues

None — NIT-02 was folded into MIN-02's commit (its two unpinned cases are now doctests); no other finding skipped.

## Verification

Ran in the **main checkout** (`workflow.use_worktrees=false` — no isolated worktree, per project config):

- `zig build test`: both suites green — 133 + 4 doctest cases, 0 failures
- `zig build`: clean (-Werror gate)
- Script suite, CI-equivalent (fresh `/tmp/efp1fix` sandboxes; 41→40-session, 43→42-recents dir pairing; script 44 with its `printf 'collection = Original\n'` pre-seed): exit=0, all 10 scripts pass
- `zig build smoke`: ok on first run (keys, middle_click, failed_tab, overlay, options, user_configs, editor, idle, quit all ok)

Commits (oldest first): ccb89a3, fa80206, 0aae4be, b1300cf, 05e7d9d — one per finding, `.planning/` untouched.

---

_Fixed: 2026-10-13T22:40:00+07:00_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
