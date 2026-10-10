---
status: testing
phase: 03-gui-shell
source: [03-VERIFICATION.md]
started: 2026-10-11T00:00:00Z
updated: 2026-10-11T00:00:00Z
---

## Current Test

number: 1
name: Windows real-window walkthrough
expected: |
  Double-click zig-out/bin/ElectricField.exe: no console; chooser at startup; File > New > each of the 9 presets opens in its own tab; several tabs; close a computing tab (x, middle-click, Ctrl+W) without hang; wait cursor over a busy canvas; About links open the browser; title "Electric Field — <Name> <N>"; Ctrl+Q while tabs compute exits cleanly; idle CPU low.
awaiting: user response

## Tests

### 1. Windows real-window walkthrough
expected: as above
result: [pending]

### 2. HiDPI on a real 125–200% display (and dragging between monitors)
expected: sharp 1 px lines, canvas same physical size as at 100%, rescales when moved between monitors
result: [pending]

### 3. Linux / macOS real window
expected: app runs with a real window; Cmd shortcuts on macOS; Retina sharp
result: [pending]

### 4. Visual confirmation of a finished tab vs MFC baseline
expected: zig-out/check/phase3-gui-vs-mfc.png — same charges, line layout, colours (AA differences only)
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Gaps
