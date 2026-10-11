---
phase: 02-compute-engine-headless-parity
verified: 2026-10-10T14:30:00Z
status: passed
score: 4/5 must-haves verified
behavior_unverified: 1
overrides_applied: 0
behavior_unverified_items:
  - truth: "A cancel() that has returned before the result is published always wins (session ends cancelled, no result); status and takeResult() always agree (WR-11 contract, session.hpp)"
    test: "Force a cancel() into the window between the last job completing and the result publication in ComputeSession::finish() (needs a test seam or a deliberate stress harness), many iterations, also under TSan"
    expected: "Every run ends either cancelled with no result, or done with a result; never cancelled with a result, never done without one; no hang in the destructor join"
    why_human: "The publication branch (haveR && !stop_.load() evaluated under m_) cannot be forced deterministically. The existing test (test_session.cpp:369) is interleaving-safe and asserts cancelled XOR result, and observed 24/24 cancelled during finalization, but it does not provably reach the publication window (REVIEW IN-09). The code reads correct, but the invariant is not exercised on that exact path."
human_verification:
  - test: "Visual side-by-side of the headless PNG/SVG output against the MFC baselines"
    expected: "For all 9 presets, charge positions/colours, line density and equipotential spacing are recognisably the same as docs/baseline/mfc/<slug>.png (anti-aliasing differs slightly, which is expected)"
    why_human: "Visual similarity is a judgement call. tools/parity/compare-mfc.sh (ImageMagick, informational, never in CI) gives metrics, but the 'recognisably the same' core-value call is human. I viewed parity quadrupole.png myself: 4 charges (blue/red alternating), dense grey equipotentials, black field lines, y-up, centred, 1024x768, plausible. I did not compare it with the MFC baseline."
  - test: "Optional: run tools/baseline/capture-mfc-baseline.ps1 against a real MFC exe on one preset and compare with docs/baseline/mfc/dipole.png"
    expected: "Capture succeeds; the hardened PrintWindow-failure and alpha-255 dark-pixel paths behave"
    why_human: "Needs the MFC executable and an interactive desktop (D-18, VALIDATION manual-only row). The parse check and the bad-path (-Exe not found) run were automated."
overrides: []
gaps: []
deferred: []
---

# Phase 2: Compute Engine & Headless Parity Verification Report

**Phase Goal:** As the developer, I want a cancellable threaded compute engine with headless SVG/PNG output, so that CI can verify every preset on all OSes without a display.
**Verified:** 2026-10-10 (HEAD 1eb877e, after the code-review fixes)
**Status:** human_needed (no gaps; automated evidence is complete, remaining items need a human)
**Re-verification:** No, initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria are the contract)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Any preset computes on the configured number of worker threads without blocking the caller; 1 vs N threads equivalent within tolerance; field lines terminate; equipotentials at the configured interval with de-dup (Quadrupole, Parallel dipoles) | VERIFIED | `src/core/ef/session.cpp` is a real condition-variable worker pool (the constructor starts `numThreads` workers and returns, `wait()`/`poll()`/`takeResult()`). Tests run by me locally (`zig build test -Dgui=false`, 60/60 test cases, 212831 assertions): `1 thread and N threads give the same result` (1e-12), `field-line and equipotential invariants per preset`, `de-duplication on Quadrupole ... and Parallel dipoles`, `numThreads 0 is clamped`, `poll() drains each completed line exactly once`. De-dup is in core: `ef::dedupEquipotentials` and `ef::finalize` in `finalizer.cpp`. Same 60/60 on all 4 CI jobs at HEAD. |
| 2 | Cancel returns within ~200 ms with no crash/leak/detached thread; Linux TSan reports no data races | VERIFIED | `cancel()`/destructor join only (`session.cpp`: `t.join()` at lines 57 and 65; `grep detach src` finds no use, only a comment). `kCancelStride = 1024` polled in `tracer.hpp`. Local run: 99 preset x method runs, worst cancel 6.68 ms, worst destroy 8.80 ms against a 200 ms budget (`kLatencyBudgetMs` is 200, relaxed to 1000 only under `__SANITIZE_THREAD__`, test_session.cpp:31-43). CI `tsan-linux` job at HEAD: success (core suite 60/60 under `-Dsanitize=thread -Doptimize=ReleaseSafe`, plus threaded ef-cli parallel-dipoles at 8 threads); no `ThreadSanitizer:` report in the job log. Caveat: the TSan latency budget is 1000 ms (documented as carried to Phase 3). |
| 3 | Portability defects (abs(double), M_PI, FieldLineJob job(job), AdjustForBezier divide-by-zero) don't reproduce, each with a regression test; core warning-clean with -ffp-contract=off, no fast-math | VERIFIED | `tests/test_tracer.cpp:38` (abs(double): `std::abs` with the `>= potentialInterval` test at `tracer.hpp:77`), `:52` (`FieldLineJob job(job)`), `:122` (FMA canary, `__FAST_MATH__` undefined); `tests/test_bezier.cpp:36` (coincident points, `kBezierMinDivisor` guard in `bezier.hpp:28`, MFC t3 kept); `math.hpp` never uses or redefines `M_PI` and `grep M_PI src/core` finds only the comment. `build.zig` core flags: `-std=c++17 -ffp-contract=off -Wall -Wextra -Wshadow -Wabsolute-value -Wconversion -Wsign-conversion -Wuninitialized -pedantic -Werror`; no fast-math anywhere. The core built and tested warning-clean locally (Windows) and on CI Linux and macOS arm64 under `-Werror`. |
| 4 | Headless CLI renders all 9 presets to SVG and PNG without a display; CI runs it on windows, ubuntu, macos-arm64 as a parity check agreeing in charge and line counts within tolerance | VERIFIED | Ran `./zig-out/bin/ef-cli --all --out-dir <dir> --expect tools/parity/expected-counts.csv` locally: exit 0, 18 files (9 SVG + 9 PNG, 80-240 KB each, no window). CI run 38058493194 (HEAD 1eb877e): windows-x64, linux-x64, macos-arm64, tsan-linux all success; the `parity-windows-x64`, `parity-linux-x64`, `parity-macos-arm64` artifacts are uploaded. All three OSes plus my local run print identical counts for every preset (e.g. dipole 2/16/75, quadrupole 4/32/142, parallel-dipoles 4/32/136, four-charges-same-sign 4/64/128), equal to `tools/parity/expected-counts.csv`. Viewed `quadrupole.png`: a real, coherent rendering, not a blank or stub. |
| 5 | (Plan 02-01 / review WR-11) A cancel() that has returned before publication wins; status and takeResult() agree | PRESENT_BEHAVIOR_UNVERIFIED | Code present and correct on reading (`finish()` re-checks `stop_` under `m_`); tests assert the interleaving-independent XOR invariant and pass, but cannot force the publication window. See `behavior_unverified_items`. Not a gap: no evidence of a defect, and a stress-based test plus TSan exercise it. |

**Score:** 4/5 truths verified (1 present, behavior-unverified)

### Additional plan-level must-haves checked

| Plan truth | Status | Evidence |
|---|---|---|
| 02-01: empty ChargeSystem finishes done/empty; invalid Method fails with error; numThreads 0 clamped | VERIFIED | `test_session.cpp:248`, `:270` pass |
| 02-02: abs/pi/job-copy/Bezier regressions; D-05/D-07/D-06/D-08 MFC quirks pinned by characterisation tests; `-Dgui=false` fetches no GUI stack; fail-not-skip preset cross-check | VERIFIED | `test_finalizer.cpp:26` (D-05 shortcut), `:71` (2*chargeRadius strict boundary), `test_bezier.cpp:18` (t3 kept), `test_presets.cpp` cross-check, `.lazy` deps and `lazyDependency` in `build.zig`; Debug/ReleaseFast core gates pass on CI |
| 02-03: ef-cli flags, `--expect` semantics, exit codes, SVG-only `-Dgui=false` build, scene layer shared by SVG/ImGui painters | VERIFIED | `src/cli/main.cpp`, `src/core/ef/scene.cpp`, `svg.cpp`, `src/render/{imgui_painter.hpp,png_export.cpp}` exist and are wired (`renderOne` builds a `ComputeSession`; `renderPng` under `EF_CLI_PNG`); `--all` exit 0 observed. Edge-case exits (2/1) are covered by the plan's own tests and WR-06/WR-07 review fixes; I did not re-run each edge case by hand. |
| 02-04: CI 4 jobs green at pushed head; artifacts uploaded also on failure; actions pinned to SHA; `permissions: contents: read`; no `continue-on-error`; `research-ref/` removed; `ElectricField/` unchanged since 7281ffb | VERIFIED | `gh run view 38058493194`: all four jobs success; `ci.yml` has `if: always()` on upload, no `continue-on-error`/allow flags; `research-ref` absent; `git diff 7281ffb HEAD -- ElectricField` is empty |

### Prohibitions (must-NOT, ADR-550 D3)

| Prohibition | Tier | Status | Evidence |
|---|---|---|---|
| MFC parity quirks (DP low weights, AdjustForBezier t3, pixel/world thinning, unsorted two-opposite-charge shortcut, unfiltered last equipotential group) must not be corrected | test | VERIFIED (enforcement wired) | Pinned by characterisation tests that pass: `test_finalizer.cpp:26`, `test_bezier.cpp:18`, `test_rk_order.cpp` (Phase 1 DP characterisation re-verified unchanged after the `computeStages` refactor) |
| CI parity/TSan gate must not be weakened (no job dropped, no allow-failure, no skipped test, tolerance unchanged without per-OS evidence) | test | VERIFIED | `ci.yml` read in full: 3-OS matrix plus tsan job intact, `fail-fast: false` only, no `continue-on-error`, `-Werror` and `-ffp-contract=off` untouched; `expected-counts.csv` touched in one commit only (8caf46b) |
| Only normal fast-forward pushes of feature/cross-platform; no force-push, no tag, no other branch | test | VERIFIED | `git ls-remote --tags --heads origin`: no tags, only `feature/cross-platform` and `master` (master at de06f5f); GitHub activity API returns no force_push events |

No prohibition is flagged unverified.

### Required Artifacts

| Artifact | Status | Details |
|---|---|---|
| `src/core/ef/session.{hpp,cpp}` | VERIFIED | Substantive worker pool, join-only lifecycle, WR-01/02/11 hardening; used by CLI and tests |
| `src/core/ef/{tracer,bezier,finalizer,method}.*` | VERIFIED | Guarded, ported logic (kCancelStride, kBezierMinDivisor, dedup, methodFromName); covered by tests |
| `src/core/ef/{scene,svg}.*`, `src/render/{imgui_painter.hpp,png_export.*}` | VERIFIED | Wired through `ef-cli`; rendered output inspected |
| `src/cli/main.cpp` | VERIFIED | Runs; `--all --expect` exit 0 |
| `tools/parity/expected-counts.csv`, `compare-mfc.sh` | VERIFIED | Oracle matches all three OSes; script is informational, not in CI |
| `.github/workflows/ci.yml` | VERIFIED | 3-OS + tsan; green at HEAD |
| `tools/baseline/capture-mfc-baseline.ps1` | VERIFIED (static) | Hardened per D-18; end-to-end run is a human item |
| `tests/test_{session,tracer,bezier,finalizer,presets}.cpp`, `test_params.hpp` | VERIFIED | 60 test cases, all pass |

### Key Link Verification

| From | To | Status |
|---|---|---|
| session.cpp -> tracer.hpp (`traceElectricLine`, hooks) | WIRED | worker calls present |
| session.cpp -> finalizer (`finish()` -> `ef::finalize`) | WIRED | last worker finalizes; stop plumbed through |
| cli/main.cpp -> session.hpp / png_export | WIRED | `renderOne` -> `ComputeSession`; PNG under `EF_CLI_PNG` |
| ci.yml -> expected-counts.csv / `-Dsanitize=thread` | WIRED | exact commands present and executed in CI logs |
| build.zig `sanitize_thread` -> tests | WIRED | TSan job runs the same 60 tests instrumented |

### Data-Flow Trace (Level 4)

CLI output flows from real computation: `ComputeSession` -> `FieldResult` -> `buildScene` -> SVG/PNG. Counts vary per preset and match the oracle on three OSes; the rendered quadrupole image shows real lines. Status: FLOWING.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| Core test suite | `zig build test -Dgui=false --summary all` | 60/60 cases, 212831 assertions pass; worst cancel 6.7 ms | PASS |
| Full build (core, tests, GUI, ef-cli) | `zig build -Doptimize=ReleaseFast` | success | PASS |
| Headless render + parity | `ef-cli --all --out-dir D --expect tools/parity/expected-counts.csv` | exit 0, 9 SVG + 9 PNG | PASS |
| CI at HEAD | `gh run view 38058493194` | 4/4 jobs success, 3 parity artifacts | PASS |

### Probe Execution

No `probe-*.sh` scripts are declared or present. Skipped.

### Requirements Coverage

IDs declared in plan frontmatter: 02-01 [CORE-04, CORE-05, CORE-06, QA-02, QA-03]; 02-02 [CORE-07, QA-03, CORE-04, QA-02]; 02-03 [QA-04, CORE-04, CORE-05]; 02-04 [QA-03, QA-04, CORE-07]. Union: CORE-04, CORE-05, CORE-06, CORE-07, QA-02, QA-03, QA-04, which equals the 7 IDs REQUIREMENTS.md maps to Phase 2. No orphaned requirements.

| Requirement | Source Plans | Status | Evidence |
|---|---|---|---|
| CORE-04 equipotentials at interval + de-dup in core | 01, 02, 03 | SATISFIED | `finalizer.cpp`; tests incl. 2*chargeRadius boundary; counts oracle |
| CORE-05 configurable worker threads, non-blocking | 01, 03 | SATISFIED | `ComputeSession`; 1-vs-N test; `--threads 1..128` |
| CORE-06 cancel within ~200 ms, no crash/leak/detached thread | 01 | SATISFIED | 99-run sweep worst 6.7 ms; join-only; TSan clean. (WR-11 publication-window invariant is behavior_unverified, see above.) |
| CORE-07 portability defects + warning-clean, no fast-math | 02, 04 | SATISFIED | regression tests; build flags; clean on 3 OSes |
| QA-02 line invariants per preset, tolerances | 01, 02 | SATISFIED | `test_session.cpp:103`, `test_seeds.cpp`, `test_finalizer.cpp` |
| QA-03 1-vs-N, cancel latency, TSan on Linux CI | 01, 02, 04 | SATISFIED | tests + `tsan-linux` job green |
| QA-04 headless CLI + 3-OS CI parity | 03, 04 | SATISFIED | ef-cli, ci.yml, artifacts |

REQUIREMENTS.md already marks all seven `[x]` / Complete, consistent with the evidence.

### Anti-Patterns Found

| Scope | Finding | Severity |
|---|---|---|
| `TBD/FIXME/XXX` scan over `src tests tools .github build.zig` | none | clean |
| Open review Info items IN-02..IN-06, IN-09 (02-REVIEW.md, status clean, 0 critical / 0 warning) | compare-mfc.sh temp-dir trap; `src/gui/main.cpp` busy loop (Phase 3 replaces); CI cancel-in-progress also cancels master builds; `Painter::strokePath` contract not defended; ANSI vs UTF-8 paths on Windows; WR-11 test statistical | Info, none block the goal |

### Notes

- The TSan cancel-latency budget is 1000 ms rather than 200 ms (instrumentation slowdown). The roadmap's ~200 ms is enforced in all non-TSan builds (measured worst 6.7 ms locally and about 1.5 ms on CI), so criterion 2 holds.
- Pushed docs-only heads re-trigger CI; the head 1eb877e run is green and includes the 3 tests added after the earlier code head (57 -> 60 cases).
- QA-05 (MFC visual baseline) belongs to Phase 1 and is already recorded in `docs/baseline/mfc/`.

### Human Verification Required

1. **Visual side-by-side vs MFC baselines.** Compare the CI artifacts `parity-<os>` (or local `ef-cli --all` output) with `docs/baseline/mfc/*.png` for all 9 presets (optionally run `tools/parity/compare-mfc.sh`). Expected: recognisably the same configuration; AA differences only.
2. **WR-11 publication-window cancel race (behavior-unverified).** Optional deeper stress or a test seam, as described in the frontmatter.
3. **Optional capture-script end-to-end run** against the MFC exe (needs a desktop).

### Gaps Summary

None. All four roadmap success criteria are met with direct evidence (local test and CLI runs, plus a fully green four-job CI run at HEAD 1eb877e with parity artifacts). The status is `human_needed` only because the visual comparison to MFC output and the exact WR-11 publication race are judgement or non-deterministic items that automated checks cannot close.

---

_Verified: 2026-10-10_
_Verifier: Claude (gsd-verifier)_
