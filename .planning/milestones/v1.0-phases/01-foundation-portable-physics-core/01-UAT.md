---
status: testing
phase: 01-foundation-portable-physics-core
source: [01-VERIFICATION.md]
started: 2026-10-10T10:24:12Z
updated: 2026-10-10T10:24:12Z
---

## Current Test

number: 1
name: Interactive hello window on Windows
expected: |
  `zig build run` opens a window showing the ImGui hello panel; no console window appears; closing the window exits cleanly.
awaiting: user response

## Tests

### 1. Interactive hello window on Windows
expected: `zig build run` opens a window showing the ImGui hello panel; no console window appears; closing the window exits cleanly.
result: [pending]

### 2. Visual review of the 9 MFC baseline images
expected: Each PNG in docs/baseline/mfc/ shows the charge layout and colours from the README preset table, with field lines (black) and equipotentials (grey) fully drawn across the 1024x768 view.
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
