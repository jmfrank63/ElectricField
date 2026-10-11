# Plan 01-01 Summary: Collections Flow, Grid/Size Options & PDF Export

**Status:** Completed 2026-10-12

## What was built

- **Collections as documents (OPEN-01)**: the app starts with no collection open; File >
  Open Collection... (Ctrl+O) opens any collection file through the native file picker (SDL
  ShowOpenFileDialog, default dir Collections\); New Collection... and Save Collection As... use the
  native save dialog; Close Collection leaves nothing open (no Original fallback). Sessions stay
  keyed by file stem (`Sessions\<stem>.ini`) and restore when the file is opened again. The in-app
  collection list chooser was removed. API: `collection.open|switch|save-as <name|path>` (a bare
  name resolves to Collections\<name>.ini, which is kept verbatim when it already carries .ini).
- **Configuration group + greyout (CFG-04)**: File menu groups New/Open/Save/Save As Configuration +
  Close Tab; all greyed while no collection is open; Ctrl+N/Ctrl+S gated the same way. Tabs track
  `sourceConfig` (set by open/openUser/openRestored/save-as); Save Configuration writes the active
  tab's charges back into its configuration in place (`config.save`).
- **Grid (GRID-05/06)**: lines light green RGB(120,210,120); intersections-only mode draws the
  lattice points (Cartesian + polar) in orange RGB(217,119,6) with bounded counts; the panel has an
  Intersections checkbox above the Cartesian/Polar radios; `grid.set intersections on|off`; the
  session grid CSV grew to 8 fields with a backward-compatible parser.
- **Charge size (SIZE-01)**: `chargeSize` 2-120 px in options.ini (validate/serialize/parse,
  round-trip via operator==), the Options dialog Draw tab, and `options.set charge-size`; the drawn
  radius only - physics untouched.
- **PDF export (EXP-03)**: `PdfPainter` (font-free vector PDF 1.4, one renderScene = one page, exact
  xref, outline bookmarks with PDFDocEncoding titles); Export Configuration > PDF... (canvas view,
  like PNG/SVG); Export Collection... renders every configuration synchronously into one multi-page
  PDF with fitted views; `export.configuration <pdf>` / `export.collection <pdf>` commands;
  `--export` mode writes dipole.pdf on all three OSes.
- **Script suite**: all five scenarios rewritten for the empty start (each opens a collection
  first); new `app.expect-no` and `app.expect-error` assertions; 10-editor proves CFG-04 round-trip
  and the PDF exports; 30-collections proves open-by-name/path, save-as, close-to-empty.

## Commits

- 43432f9 feat(v2.5): collections as document files (native pickers), config save group, PDF
  export, light-green grid with intersections mode, charge size
- 3de6bc6 fix(v2.5): API guards without a collection (tab.new-empty), .ini-suffixed collection
  names kept verbatim

## Verification

119 + 4 unit tests pass; smoke green (276 frames); 5 script tests green; --export produces
dipole.pdf/png/svg; path-form collection.open + session restore probed; CI run on windows-x64,
linux-x64, macos-arm64 and tsan-linux.
