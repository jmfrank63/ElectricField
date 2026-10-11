---
status: testing
phase: 04-options-export-mfc-removal
source: [04-VERIFICATION.md]
started: 2026-10-11T01:30:00Z
updated: 2026-10-11T01:30:00Z
---

## Current Test

number: 1
name: Options walkthrough in the real window
expected: |
  View > Options...: Computation tab shows 11 methods, equipotentials on, lines 16, threads 4, interval 0.10; typing 200 (or 0) in Lines/Threads and OK shows an error and keeps the dialog open; a changed field-line colour and thickness repaint every open tab at once; a changed thread count does not touch open tabs, but a newly opened tab uses it; after OK and an app restart the values are back; deleting %APPDATA%\ElectricField\ElectricField\options.ini resets to the defaults.
awaiting: user response

## Tests

### 1. Options walkthrough in the real window
expected: as above
result: [pending]

### 2. Export walkthrough in the real window
expected: File > Export is greyed while a tab computes; when done, PNG.../SVG... open a native save dialog; the saved PNG matches the tab exactly; cancelling does nothing; a non-ASCII folder name works
result: [pending]

### 3. Linux / macOS real desktop (optional)
expected: options, export and persistence behave the same on a real desktop (same scope as Phase 3's skipped item)
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps
