# 01-02 Summary: Collections UI, zoom/pan, continuous calculation, leave prompt (GUI)

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** COL-01..04, EDIT-08, EDIT-09, EDIT-10, CALC-03, ZOOM-01/02

## What was built

- **Collections**: the menu bar hosts the collection switcher and a Calculate All button; File menu
  gains Calculate All and New Collection.... On first run the Original collection is seeded from the 9
  built-ins + any legacy configurations.ini (which stays untouched as a backup). The chooser lists the
  CURRENT collection's configurations plus "Empty Configuration"; Save As stores into the current
  collection file; switching saves the session, closes the tabs and loads the target collection.
  Calculate All closes the tabs and opens every configuration of the collection (each computes at once).
- **Always-visible New charge section** (EDIT-08): value + x + y + Add in the panel - no canvas needed,
  no collapsed header.
- **Sign flip** (EDIT-09): a "+/-" button beside the value field flips the selected charge's sign.
- **Zoom/pan** (ZOOM-01/02): the wheel zooms at the cursor, Zoom - / Zoom + / Reset view live in the
  panel, right-drag pans, and the arrow keys pan whenever no charge is selected (with a selection they
  keep nudging the charge). View changes never mark a tab stale or dirty.
- **Continuous calculation** (CALC-03): a per-tab checkbox; when on, every edit re-arms a 0.35 s
  debounce and the tab recomputes once the edits settle (one recompute per drag). The flag persists in
  the session file per tab.
- **Unsaved-changes prompt**: charge edits mark the tab dirty; quitting or switching collections with
  dirty tabs asks "leave anyway / cancel" first. Saved tabs are marked clean by Save As.
- Double-click adds / single-click selects for editing is pinned as EDIT-10 (already the behaviour).

## Verification

- `zig build test`: 113 + 4 cases green; `zig build smoke` green (`editor=ok` incl. wheel zoom round-trip
  and rename); canvas parity checks pass with the new panel and menu-bar widgets (the chrome-bleed
  backstop now scans the tab-bar band only, since the menu bar legitimately hosts the collection
  switcher and Calculate All).
- Known incident fixed during development: the chooser mis-classified rows for empty collections
  (crash), the smoke's double-click injection needed the double-click window widened, and the earlier
  "Add charge" section was hidden behind a collapsed header (the owner's report that started this
  milestone).
