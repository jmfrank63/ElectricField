---
status: complete
phase: 03-gui-shell
source: [03-VERIFICATION.md]
started: 2026-10-11T00:00:00Z
updated: 2026-10-11T02:45:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Windows real-window walkthrough
expected: Double-click zig-out/bin/ElectricField.exe: no console; chooser at startup; File > New > each of the 9 presets opens in its own tab; several tabs; close a computing tab (x, middle-click, Ctrl+W) without hang; wait cursor over a busy canvas; About links open the browser; title "Electric Field - <Name> <N>"; Ctrl+Q while tabs compute exits cleanly; idle CPU low.
result: pass - user approved the walkthrough ("All good - continue", 2026-10-11)

### 2. HiDPI on a real 125-200% display (and dragging between monitors)
expected: sharp 1 px lines, canvas same physical size as at 100%, rescales when moved between monitors
result: skipped - no 125-200% multi-monitor rig available in this session; HiDPI scaling at pixel density 1 is covered by the automated canvas check (fix IN-02, commit bde2539)

### 3. Linux / macOS real window
expected: app runs with a real window; Cmd shortcuts on macOS; Retina sharp
result: skipped - no local Linux/macOS desktop; CI verifies the same binary and event loop headlessly on all three OSes (smoke, canvas, keyboard, screenshot identical across windows-x64, linux-x64, macos-arm64, run 38078969099)

### 4. Visual confirmation of a finished tab vs MFC baseline
expected: zig-out/check/phase3-gui-vs-mfc.png - same charges, line layout, colours (AA differences only)
result: pass - side-by-side comparison image shown to the user in-thread and approved ("All good - continue", 2026-10-11)

## Summary

total: 4
passed: 2
issues: 0
pending: 0
skipped: 2
blocked: 0

## Gaps
