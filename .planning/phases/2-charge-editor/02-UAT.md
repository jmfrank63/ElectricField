---
status: testing
phase: 2-charge-editor
source: [02-VERIFICATION.md]
started: 2026-10-11T02:50:00Z
updated: 2026-10-11T02:50:00Z
---

## Current Test

number: 1
name: Editor walkthrough in the real window
expected: |
  Open any configuration: clicking a charge selects it (blue ring), dragging moves the glyph in realtime, arrow keys nudge (Shift = larger), double-click empty canvas adds a charge, Delete removes the selected one but never the last. The editor panel's -/+ buttons move the charge while held, and typing exact numbers into Value/X/Y works immediately. After any edit the panel shows the "Edited" hint; Calculate (F5) recomputes and clears it. The Method combo applies at the next Calculate. Save As stores the edited charges.
awaiting: user response

## Tests

### 1. Editor walkthrough in the real window
expected: as above
result: [pending]

## Summary

total: 1
passed: 0
issues: 0
pending: 1
skipped: 0
blocked: 0

## Gaps
