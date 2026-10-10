---
phase: 02-compute-engine-headless-parity
reviewed: 2026-10-10T00:00:00Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - .github/workflows/ci.yml
  - .gitignore
  - build.zig
  - build.zig.zon
  - src/cli/main.cpp
  - src/core/ef/bezier.hpp
  - src/core/ef/finalizer.cpp
  - src/core/ef/finalizer.hpp
  - src/core/ef/method.hpp
  - src/core/ef/params.hpp
  - src/core/ef/rk.hpp
  - src/core/ef/scene.cpp
  - src/core/ef/scene.hpp
  - src/core/ef/session.cpp
  - src/core/ef/session.hpp
  - src/core/ef/svg.cpp
  - src/core/ef/svg.hpp
  - src/core/ef/tracer.hpp
  - src/gui/main.cpp
  - src/render/imgui_painter.hpp
  - src/render/png_export.cpp
  - src/render/png_export.hpp
  - tests/test_bezier.cpp
  - tests/test_finalizer.cpp
  - tests/test_params.hpp
  - tests/test_presets.cpp
  - tests/test_rk_order.cpp
  - tests/test_scene.cpp
  - tests/test_session.cpp
  - tests/test_tracer.cpp
  - tools/baseline/capture-mfc-baseline.ps1
  - tools/parity/compare-mfc.sh
  - tools/parity/expected-counts.csv
findings:
  critical: 0
  warning: 10
  info: 6
  total: 16
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-10-10
**Depth:** standard
**Files Reviewed:** 33
**Status:** issues_found

## Summary

`ComputeSession` locking/wakeup logic traced by hand: no lost wakeup, deadlock or data race (cancel/notify handshake, `inFlight_ == 0` exit predicate, `notify_one` on job post, constructor thread-creation failure path, destructor join, `activeWorkers_` hand-off to `finish()`, poll drain-and-swap). Port is faithful to MFC (`AddPoint`, `weightCenter`, `PostCalculateEquipotential`, `GetDataFromThreads`, `AdjustForBezier`); deviations are the intentional ones (stable sort, guarded Bezier, CORE-07 fix). Remaining: exception safety on the last-worker `finish()` path, cancel contract not covering finalization, unchecked I/O and return values in CLI/SVG/PNG, weak parity-oracle validation, wall-clock-based tests, and a data-loss path in the local capture script.

## Warnings

### WR-01: `finish()` runs outside any try/catch on a worker thread — OOM terminates the process (or hangs `wait()`)

**File:** `src/core/ef/session.cpp:94, 139-181`
**Issue:** `workerMain` calls `finish()` after its try/catch. Inside `finish()` only `finalize()` is guarded (and only against `std::exception`); copies of `allE_`/`allP_`, `std::sort`, `reserve`/`push_back` and the result assignment all allocate. Any throw escapes the thread entry → `std::terminate`; if it did not terminate, `done_` would never be set and `wait()` would block forever. The deep copy at 161-162 doubles peak memory.
**Fix:** Wrap the whole body in try/catch(`std::exception`)/catch(...) converting to `fail(...)`, and always set `done_` + `notify_all` afterwards; move lines out of `allE_`/`allP_` instead of copying.

### WR-02: Cancellation is not honoured during finalization; `cancel()` after the last job may still yield a result

**File:** `src/core/ef/session.cpp:139-181`, `src/core/ef/session.hpp:54`
**Issue:** `stop_` is checked once at line 144; sort/copy/de-dup/Bezier then run uncancellably on the last worker, and the destructor joins it — destruction blocks for the full finalize time, contradicting the header comment "bounded by the cancel stride". Outcome of a cancel racing the check is unspecified to callers.
**Fix:** Re-check `stop_` between finalize stages (after sort, after de-dup, per line in the Bezier loop) and take the cancelled path; update the `session.hpp:54` comment either way.

### WR-03: `appendNum` ignores `snprintf`'s return value → out-of-bounds read on long numbers

**File:** `src/core/ef/svg.cpp:10-20`
**Issue:** `n` is the would-be length; when `n >= 32` the buffer is truncated but `buf[len-1]` and `append(buf, len)` read past it. NaN/Inf print as invalid SVG; negative `n` indexes `buf[-1]`.
**Fix:** Map non-finite to 0, use a 64-byte buffer, clamp `n` to `sizeof buf - 1`, handle `n <= 0`; `#include <cmath>`.

### WR-04: ef-cli reports SVG write success before the stream is flushed

**File:** `src/cli/main.cpp:93-97`
**Issue:** `if (!f)` is checked before `close()`, so disk-full/I/O errors are missed and the CLI exits 0 with a truncated SVG (the parity artifact).
**Fix:** `f.close(); if (!f) { err = ...; return false; }`.

### WR-05: ef-cli has no top-level exception handling

**File:** `src/cli/main.cpp:143-181`
**Issue:** `makeSystem`, `ComputeSession` ctor (`std::system_error` from `std::thread`), `buildScene`, `renderSvg` and allocations can throw → `std::terminate`/crash dialog instead of documented exit codes.
**Fix:** Move the body into `run(...)` and wrap in try/catch returning 1 with a `ef-cli: fatal: ...` message.

### WR-06: ef-cli argument parsing wraps/UB on bad numbers and silently ignores contradictory flags

**File:** `src/cli/main.cpp:60-62, 65-69, 143-179`
**Issue:** `--threads` via `strtoul` cast to `unsigned` wraps (`4294967297` → 1) and accepts `4x`; `--width/--height` via `atoi` is UB on overflow and yields 0 for garbage; `--all` silently ignores `--preset/--svg/--png`, `--expect` ignored without `--all`.
**Fix:** Parse with `std::from_chars`/`strtol`+endptr+errno into a wide type, range-check before narrowing, reject trailing characters; reject incompatible flag combinations.

### WR-07: `--expect` parity check is weaker than documented

**File:** `src/cli/main.cpp:113-139`
**Issue:** Duplicate slug rows can mask a missing preset (`seen` counter only); malformed/short rows parse as 0 via `atoi`; dead `err` parameter; `std::max` without `<algorithm>`.
**Fix:** Track seen slugs in a set and compare with the result keys; require exactly four fully-parsed fields per row; use or drop `err`.

### WR-08: PNG export releases ImGui context/backend manually — a throw leaks both and leaves the wrong context current

**File:** `src/render/png_export.cpp:28-65`
**Issue:** `ImGui_ImplSDLRenderer3_Shutdown`/`DestroyContext`/restoring the previous context are straight-line code; a `bad_alloc` in `renderScene` skips them, leaving the GUI's context swapped out — a real hazard once the GUI calls export (Phase 4). Clip push/pop in `ImGuiPainter::begin/end` is also unbalanced on throw.
**Fix:** RAII scope guards (`CtxGuard` restoring `prev`, `BackendGuard` created only after successful Init, destroyed first), or try/catch with the same cleanup.

### WR-09: Cancel-latency tests assert hard wall-clock budgets ~200× on shared CI runners

**File:** `tests/test_session.cpp:161-198, 257-308` (budget at 19-35)
**Issue:** 200 ms hard cap per run across 99 preset×method combinations (twice) and 4-thread sessions; a single scheduler hiccup on a 2-3 vCPU runner (Debug+UBSan, workers > cores) fails CI.
**Fix:** Assert on best-of-N or median per combination, or a ~1 s hard cap on CI with the 200 ms figure reported as a MESSAGE; keep the deterministic `cancelled == true` / no-result assertions.

### WR-10: Baseline capture script can destroy the user's only backup of their MFC options

**File:** `tools/baseline/capture-mfc-baseline.ps1:116-129, 183-193`
**Issue:** After a hard abort of a run (key deleted, `.reg` backup is the only copy), the next run with `-ResetOptions` deletes the existing backup first and writes none (key absent) → original options lost.
**Fix:** If the backup file exists, refuse to proceed (tell the user to `reg import` or move it), or use a unique timestamped backup name per run.

## Info

### IN-01: `std::max` used in `src/cli/main.cpp:128` without `#include <algorithm>`.

### IN-02: `tools/parity/compare-mfc.sh:20-31` leaks temp dirs on failure (no trap), writes junk into the CSV on magick errors, and prints 0.000 on a blank baseline (`m_a == 0`).
**Fix:** `trap 'rm -rf "$d"' EXIT`, validate `ae` is numeric, check `m_a`/`m_b` non-zero.

### IN-03: `src/gui/main.cpp` ignores `ImGui_ImplSDLRenderer3_Init`'s result, leaks on early-return paths, busy-loops without vsync (Phase 3 replaces this file).

### IN-04: `.github/workflows/ci.yml:5-16` runs twice per PR push (push + pull_request, different concurrency groups); `cancel-in-progress: true` also cancels master builds.
**Fix:** `cancel-in-progress: ${{ github.ref != 'refs/heads/master' }}`; optionally drop the branch push trigger once a PR exists.

### IN-05: `Painter::strokePath` contract (`n >= 1 + 3*bezierSegments`) is not defended in `src/core/ef/svg.cpp:45-71` or `src/render/imgui_painter.hpp:20-27`.
**Fix:** assert in both backends or clamp `bezierSegments`.

### IN-06: ef-cli path arguments are ANSI code page on Windows while `SDL_SavePNG` expects UTF-8 (`src/cli/main.cpp:94,100`, `src/render/png_export.cpp:58`); non-ASCII output dirs can diverge.
**Fix:** convert argv via `wmain`/`GetCommandLineW` to UTF-8 and open via `std::filesystem::u8path`, or document ASCII-only paths.

---

_Reviewed: 2026-10-10_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
