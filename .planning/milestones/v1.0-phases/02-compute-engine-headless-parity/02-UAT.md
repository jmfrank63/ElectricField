---
status: complete
phase: 02-compute-engine-headless-parity
source: [02-VERIFICATION.md]
started: 2026-10-10T23:30:00Z
updated: 2026-10-11T00:00:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Visual side-by-side of the headless PNG output against the MFC baselines
expected: For all 9 presets, charge positions/colours, line density and equipotential spacing are recognisably the same as docs/baseline/mfc/<slug>.png (anti-aliasing differs slightly, which is expected). Side-by-side montage: zig-out/check/part1.png, zig-out/check/part2.png (regenerate with `ef-cli --all --out-dir parity-out` + ImageMagick).
result: pass — user approved the side-by-side of all 9 presets ("All good — continue", 2026-10-10)

### 2. Optional: hardened MFC capture script against the real MFC exe
expected: tools/baseline/capture-mfc-baseline.ps1 captures one preset (e.g. dipole) successfully; PrintWindow-failure and alpha-255 dark-pixel paths behave.
result: skipped — optional; parse check and bad-path run automated (D-18)

## Summary

total: 2
passed: 1
issues: 0
pending: 0
skipped: 1
blocked: 0

## Gaps
