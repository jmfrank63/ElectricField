---
phase: 02-compute-engine-headless-parity
fixed_at: 2026-10-10T23:00:00Z
review_path: .planning/phases/02-compute-engine-headless-parity/02-REVIEW.md
iteration: 2
findings_in_scope: 11
fixed: 11
skipped: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-10-10
**Source review:** .planning/phases/02-compute-engine-headless-parity/02-REVIEW.md
**Iteration:** 2 (combined state: iteration 1 fixed WR-01..WR-10, iteration 2 fixed WR-11 and bundled IN-07/IN-08)

**Summary:**
- Findings in scope: 11 (WR-01..WR-11; no Critical findings)
- Fixed: 11
- Skipped: 0
- Info items addressed in iteration 2 on request: IN-07 (tests), IN-08 (script) -- plus IN-01 bundled in iteration 1

Isolation: `workflow.use_worktrees` is `false` in `.planning/config.json`, so all edits, commits and verification ran in the
main checkout on branch `feature/cross-platform` (no worktree, no recovery sentinel). Nothing was pushed.

## Fixed Issues

### Iteration 2

#### WR-11: "A cancel() that lands before publication always wins" is not guaranteed by the implementation

**Files modified:** `src/core/ef/session.cpp`, `src/core/ef/session.hpp`
**Commit:** 2639cc5
**Applied fix:** The publication block of `ComputeSession::finish()` now re-evaluates `stop_` under `m_` (`haveR && !stop_.load()`), so a `cancel()` that has returned before the result is published always ends the session cancelled with no result. Failure-over-cancel priority is unchanged (`failed_` is checked first and forces `cancelled_ = false`). The `cancel()` comment in `session.hpp` now states the exact guarantee: a cancel that has returned before publication wins; `cancel()` sets the flag without taking the lock, so a cancel racing the publication itself may land on either side, but status and `takeResult()` always agree; a cancel after done is a no-op. Status: fixed, requires human verification (logic/contract change; the race itself cannot be forced deterministically without a test seam in production code).

#### IN-07: Cancel-during-finalization paths untested

**Files modified:** `tests/test_finalizer.cpp`, `tests/test_session.cpp`
**Commit:** 43d8e67 (`test(02)`)
**Applied fix:** Three new doctest cases, all free of wall-clock thresholds:
- `finalize honours a set stop flag` (test_finalizer.cpp): pre-set `std::atomic<bool>` gives a deterministic partial result (empty-line filter ran, de-dup skipped, Bezier fit skipped); an unset flag and `nullptr` give identical full results.
- `cancel() after the session is done is a no-op` (test_session.cpp): the result stays available, `cancel()` is idempotent, `takeResult` succeeds exactly once.
- `a cancel() issued once all jobs are complete ... cancelled XOR result` (test_session.cpp): spins on `poll()` until every known job has completed (session is finalizing or done), cancels, and asserts only interleaving-independent invariants (done, not failed, `cancelled == !takeResult`, empty output when cancelled). Observed 24/24 runs cancelled during finalization. Not covered: the `finish()` catch path (needs an allocation-failure seam); remains an open note on IN-07.

#### IN-08: Partial `reg export` file can block the next run with misleading advice

**Files modified:** `tools/baseline/capture-mfc-baseline.ps1`
**Commit:** 92e3012
**Applied fix:** On `reg export` failure the (possibly partial) backup file is removed before throwing; the registry key has not been touched at that point. PowerShell parser check passed; the script was not executed (it drives the MFC GUI and the registry).

### Iteration 1

#### WR-01: `finish()` runs outside any try/catch on a worker thread

**Files modified:** `src/core/ef/session.cpp`
**Commit:** c6be157
**Applied fix:** The whole body of `finish()` (move-out of `allE_`/`allP_`, sorts, reserve/copies, `finalize`) is inside try/catch(`std::exception`)/catch(...) converting to `fail()`; `done_` is always published and `doneCv_` notified afterwards. `allE_`/`allP_` are moved instead of copied. `fail()` also tolerates a `bad_alloc` while recording the message.

#### WR-02: Cancellation not honoured during finalization

**Files modified:** `src/core/ef/session.cpp`, `src/core/ef/session.hpp`, `src/core/ef/finalizer.cpp`, `src/core/ef/finalizer.hpp`
**Commit:** 04645fc
**Applied fix:** `finalize()` gained an optional `const std::atomic<bool>* stop` (default `nullptr`) polled between stages and per line in the Bezier loops. `finish()` re-checks `stop_` after the sort and per element in the copy loops and discards a partial result. Contract wording tightened in iteration 2 (WR-11). Status: fixed, requires human verification (logic change; now covered by the IN-07 tests).

#### WR-03: `appendNum` ignores `snprintf`'s return value

**Files modified:** `src/core/ef/svg.cpp`
**Commit:** ee4ced2
**Applied fix:** Non-finite values map to 0, buffer enlarged to 64 bytes, `n <= 0` emits `0`, the would-be length is clamped to `sizeof buf - 1`.

#### WR-04: ef-cli reports SVG write success before the stream is flushed

**Files modified:** `src/cli/main.cpp`
**Commit:** ec27510
**Applied fix:** Open failure is reported separately; `f.close()` precedes the `if (!f)` check so flush errors are detected.

#### WR-05: ef-cli has no top-level exception handling

**Files modified:** `src/cli/main.cpp`
**Commit:** 51660e5
**Applied fix:** Body moved into `run()`; `main()` wraps it in try/catch(`std::exception`)/catch(...) printing `ef-cli: fatal: ...` and returning 1.

#### WR-06: ef-cli argument parsing wraps/UB on bad numbers and ignores contradictory flags

**Files modified:** `src/cli/main.cpp`
**Commit:** bd70a02
**Applied fix:** Strict `parseInt` (whole string, errno/ERANGE, range-checked before narrowing) for `--threads` (1..128), `--width`/`--height` (16..8192); contradictory flag combinations exit 2. Status: fixed, requires human verification (new CLI contract).

#### WR-07: `--expect` parity check weaker than documented

**Files modified:** `src/cli/main.cpp`
**Commit:** a734514
**Applied fix:** Exactly four fully parsed fields per row, duplicates and unknown slugs fail, set comparison of rendered vs expected presets, `err` printed by the caller. Bundled IN-01 (`<algorithm>`). Status: fixed, requires human verification (stricter oracle semantics).

#### WR-08: PNG export releases ImGui context/backend manually

**Files modified:** `src/render/png_export.cpp`, `src/render/imgui_painter.hpp`
**Commit:** 2b013fd
**Applied fix:** `CtxGuard` and `BackendGuard` RAII guards (backend shut down before the context is destroyed and the caller's context restored); `ImGuiPainter` pops its clip rect in the destructor if `end()` was not reached and is non-copyable.

#### WR-09: Cancel-latency tests assert hard wall-clock budgets

**Files modified:** `tests/test_session.cpp`
**Commit:** 328de05
**Applied fix:** 200 ms contract kept (1000 ms under TSan); the hard assertion is on the best of up to 3 fresh attempts via `bestLatencyMs()`, extra attempts only after a miss; deterministic assertions evaluated on every attempt; destroy latency measures only `reset()`.

#### WR-10: Baseline capture script can destroy the user's only backup

**Files modified:** `tools/baseline/capture-mfc-baseline.ps1`
**Commit:** 234b2be
**Applied fix:** An existing backup aborts the run with `reg import` instructions instead of being deleted; the backup is removed only after a successful restore.

## Skipped Issues

None.

## Verification

Where it ran: the main checkout (`use_worktrees=false`) on Windows 11, Zig 0.17.0 (`x86_64-windows-gnu`); the TSan run used a fresh `git archive HEAD` copy of the tree in WSL Ubuntu (`~/zig17`, Zig 0.17.0), removed afterwards. Results below are from iteration 2 (HEAD 92e3012).

| Gate | Result |
|------|--------|
| `zig build test -Dgui=false` | PASS, 60/60 test cases |
| `zig build test -Dgui=false -Doptimize=ReleaseFast` | PASS, 60/60 |
| `zig build` | PASS |
| `zig build test` | PASS, 60/60 |
| `zig build smoke` | PASS (`smoke ok frames=2`) |
| `zig build -Doptimize=ReleaseFast` + `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` | PASS, exit 0, all 9 presets match |
| WSL `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` | PASS, 60/60, no ThreadSanitizer reports |

Not addressed (out of scope, Info): IN-02..IN-06; IN-07 sub-item "finish() catch path" (needs an allocation-failure seam in production code).

---

_Fixed: 2026-10-10_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 2_
