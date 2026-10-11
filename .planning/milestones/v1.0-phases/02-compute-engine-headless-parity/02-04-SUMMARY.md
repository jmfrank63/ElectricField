---
phase: 02-compute-engine-headless-parity
plan: 04
subsystem: ci-and-closeout
tags: [github-actions, parity-gate, threadsanitizer, macos-arm64, capture-script, phase-closeout]
status: complete

requires:
  - phase: 02-compute-engine-headless-parity
    provides: "ef-cli --all/--expect and tools/parity/expected-counts.csv (02-03); -Dsanitize=thread and the race-checked session (02-02); ComputeSession (02-01)"
provides:
  - "CI job build-test (windows-x64, linux-x64, macos-arm64): Debug and ReleaseFast core gates, full build, smoke, headless parity against the committed oracle, parity-<os> artifacts (14 days, also on failure)"
  - "CI job tsan-linux: full core suite and a threaded ef-cli run (parallel-dipoles, 8 threads) under ThreadSanitizer, no race"
  - "Hardened tools/baseline/capture-mfc-baseline.ps1 (D-18)"
  - "zig build smoke no longer writes imgui.ini (IN-03)"
affects: [03-gui]

actuals:
  tokens: 1500
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns: ["every third-party action pinned to a 40-hex commit SHA", "parity gate = exact charges/field lines, bounded equipotential tolerance, uploaded images as evidence"]

key-files:
  created: []
  modified:
    - .github/workflows/ci.yml
    - tools/baseline/capture-mfc-baseline.ps1
    - src/gui/main.cpp
  deleted:
    - .planning/phases/02-compute-engine-headless-parity/research-ref/

key-decisions:
  - "No tolerance or oracle change was needed: macOS arm64 produced counts identical to Windows and Linux on the first run, so RESEARCH assumption A1 holds and the exact/+-max(2,3%) check stays as committed"
  - "Docs-only follow-up pushes re-trigger CI; the code head 101e1b0 is the gated head recorded below"

requirements-completed: [QA-03, QA-04, CORE-07]

metrics:
  completed: 2026-10-10
---

# Phase 2 Plan 04: CI parity gate, ThreadSanitizer job and phase close-out Summary

**CI renders all 9 presets headlessly on windows, ubuntu and macos-arm64 against the committed count oracle, runs the engine under ThreadSanitizer on Linux, and is green on all four jobs for both pushed heads.**

## What was done

### Task 1 (tracer): CI end to end - `b1f738c`

- `.github/workflows/ci.yml` now has the 3-OS `build-test` matrix with the headless parity step
  (`ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv | tee parity-out/summary.txt`),
  artifact upload `parity-<os>` (`if: always()`, 14 days), and the `tsan-linux` job (vm.mmap_rnd_bits=28
  precaution, TSan core suite, threaded `ef-cli` run).
- Deltas against the research reference: the duplicate default-configuration `zig build test` step was dropped
  (RESEARCH Open Question 3), and `actions/checkout` is pinned to `3d3c42e5...` (v7.0.1) in both jobs (IN-04).
  All 5 `uses:` are pinned to 40-hex SHAs; permissions stay `contents: read`; no secrets,
  no `continue-on-error`, no `pull_request_target`; push triggers are `feature/cross-platform` and `master`.
- Local dry runs before pushing: build-test job body exit 0 (Git Bash), tsan-linux job body exit 0 (WSL Ubuntu,
  parallel-dipoles at 8 threads, 1599 ms).
- Pre-push scan: no credential patterns and no .reg/.pfx/.pem/.env files in the pushed range
  (the only grep hit was the plan text that describes the patterns); remote tag count 0 before and after.
- Run **38054538292** (head `b1f738c`): https://github.com/jmfrank63/ElectricField/actions/runs/38054538292 - success.

| Job | Conclusion | Duration |
|-----|------------|----------|
| windows-x64 | success | 5m08s |
| linux-x64 | success | 4m54s |
| macos-arm64 | success | 5m59s |
| tsan-linux | success | 2m15s |

Artifacts: exactly `parity-linux-x64`, `parity-macos-arm64`, `parity-windows-x64`; each has 9 `preset=` lines,
9 SVG and 9 PNG.

### Per-OS parity counts (assumption A1 resolved)

Windows x64, Linux x64 and macOS arm64 agree on every number (the committed oracle matches all three, so the
tolerance was not touched and charges/field lines remain compared exactly):

| Preset | charges | field lines | equipotentials (win = linux = macOS) |
|--------|---------|-------------|--------------------------------------|
| dipole | 2 | 16 | 75 |
| two-equal-charges | 2 | 32 | 74 |
| two-unequal-charges-same-sign | 2 | 48 | 113 |
| two-unequal-charges-different-signs | 2 | 32 | 113 |
| quadrupole | 4 | 32 | 142 |
| four-charges-same-sign | 4 | 64 | 128 |
| two-dipoles-same-direction | 4 | 32 | 138 |
| two-dipoles-opposed-direction | 4 | 32 | 130 |
| parallel-dipoles | 4 | 32 | 136 |

macOS arm64 (libm sin/cos/pow, `-ffp-contract=off`, `-Werror`) built warning-clean and reproduced the
Windows/Linux counts exactly: A1 holds and CORE-07 is evidenced on all three targets.

### Task 2: phase close-out

- `6f5b826` - `tools/baseline/capture-mfc-baseline.ps1` (D-18): `-Exe` validated first (`Exe not found: <path>`,
  before `Add-Type` and any registry access); `Grab` stores the `PrintWindow` result and returns `$null` on
  failure; the polling loop sleeps and retries on `$null`, the final capture throws
  `PrintWindow failed for <slug>`; `W.Dark` counts only pixels with alpha 255. AST parse ok; a run with a
  nonexistent `-Exe` exits 1 with `Exe not found`.
  `src/gui/main.cpp` (IN-03): `IniFilename = nullptr` in smoke mode; `zig build smoke` prints `smoke ok frames=2` and
  leaves no `imgui.ini`. The GUI main loop is otherwise unchanged (WR-02 stays in Phase 3 per D-19).
- `101e1b0` - `research-ref/` removed (still in history at `fdfe2a2`); no reference outside `.planning`.
- Final local gate passed: build-test job body exit 0; no untracked/modified files outside `.planning`;
  `ElectricField/` unchanged since the phase base `7281ffb`.
- Run **38055102426** (head `101e1b0`, the final code head): https://github.com/jmfrank63/ElectricField/actions/runs/38055102426 - success.

| Job | Conclusion | Duration |
|-----|------------|----------|
| windows-x64 | success | 7m39s |
| linux-x64 | success | 5m16s |
| macos-arm64 | success | 4m25s |
| tsan-linux | success | 2m31s |

Artifacts: `parity-linux-x64`, `parity-macos-arm64`, `parity-windows-x64`.

## Deviations from Plan

None - plan executed exactly as written. No CI failure occurred, so the step 8 playbook (tolerance or
re-baseline, TSan fixes, arm64 warning fixes) and its 3-iteration budget were not used.

## Auth gates

None.

## Known Stubs

None.

## Threat Flags

None. The workflow requests only `contents: read`, uses no secrets, and uploads only `parity-out`.

## Open items

- Optional human check (needs the MFC exe and a desktop): run the hardened capture script on one preset and
  compare with `docs/baseline/mfc/dipole.png`. The hardened code paths only trigger against the real MFC window.
- Node.js 20 deprecation annotation for `mlugg/setup-zig` and the `ubuntu-latest` move to Ubuntu 26 (2026-10-19)
  are informational; a red Linux job after that date is to be investigated, not suppressed.
- The cancel-latency budget under TSan (1000 ms) and WR-02 (GUI busy loop) are carried to Phase 3 as before.

## Self-Check: PASSED

- Commits `b1f738c`, `6f5b826`, `101e1b0` exist in `git log` and on `origin/feature/cross-platform`.
- Runs 38054538292 and 38055102426 concluded success with all four jobs green and the three parity artifacts.
- `.github/workflows/ci.yml`, `tools/baseline/capture-mfc-baseline.ps1`, `src/gui/main.cpp` exist; `research-ref/` is gone.
