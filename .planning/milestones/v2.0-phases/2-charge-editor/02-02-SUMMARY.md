# 02-02 Summary: Editor panel, canvas interactions, smoke, README

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** EDIT-01..05, CALC-01, CALC-02

## What was built

- Tab content split into canvas (left) + 230-logical-px editor panel (right). Panel: Value (InputInt,
  realtime per keystroke, with -/+ buttons), X and Y (InputDouble %.3f with -/+ buttons stepping 0.1
  world units, press-and-hold repeat 300 ms delay then 60 ms), Remove (disabled at one charge),
  Method combo (the 11 display names; edits the tab's captured params.method and marks stale),
  Calculate (F5), and the orange stale hint on edited Done tabs. Without a selection: the how-to hint.
  While computing the panel is disabled.
- Canvas: click selects the nearest charge within 0.30 world units (or deselects); drag keeps moving the
  charge under the mouse (edge-safe while the button is held); double-click on empty canvas adds a +1
  charge; Delete removes the selected one; arrow keys nudge 0.1 (Shift 0.5) with key repeat. The selected
  charge gets a blue ring drawn over the scene. Every mutation routes through efapp::editor.
- F5 (no modal open) raises `calculateActive`, consumed by drawUi via `Workspace::calculate` on the
  active tab; the panel button raises the same request.
- Smoke gains `editor=ok`: the full loop headlessly through the injected input queue - open the empty
  configuration, click-select the origin charge, drag it 60 px, double-click to add a second charge,
  Delete back to one, verify the stale marker, F5-calculate to completion with fresh lines.
- README: new "Your own charge configurations" + "Charge editor" feature paragraphs.

## Verification

- `zig build test`: 102 + 4 cases green; `zig build smoke`: all green incl. `editor=ok`.
- Canvas at 1280x1024 is 1050 px wide (panel subtracted); the checkCanvas parity and export views follow
  the canvas rect automatically, so GUI-vs-oracle equality is preserved.

## Notes / carry-over

- ImGui computes double-clicks from per-frame button STATE, so two click cycles injected into one frame
  collapse to nothing, and two clicks 3 software frames apart (~300 ms) exceed the real 0.3 s window.
  The smoke therefore widens `MouseDoubleClickTime` (with `MouseSingleClickDelay` per the IM_ASSERT
  invariant) - same spirit as the existing KeyRepeatDelay trick; real mice are unaffected.
