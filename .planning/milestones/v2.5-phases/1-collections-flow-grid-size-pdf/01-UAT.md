# Plan 01-01 UAT: Collections Flow, Grid/Size Options & PDF Export

**Auto-accepted (recorded):** the owner runs GSD auto-mode with the standing instruction "always
take the recommended answer"; the UAT gate is therefore answered with the recommended result on the
owner's behalf, and each criterion below is backed by an automated check that was actually run.

| # | Criterion (what the owner asked for) | Automated evidence | Result |
|---|--------------------------------------|--------------------|--------|
| 1 | Open Collection opens a file picker; a collection is a file with configurations; open from anywhere; same for Save Collection As | Script probes: path-form `collection.open <abs path>` opens and restores its session; `collection.save-as <path>` writes the chosen file; SDL dialogs wired for the menu (kinds 10-12) | pass |
| 2 | Intersections is a checkbox above Cartesian | Panel layout: Checkbox("Intersections") precedes the Cartesian/Polar radios (ui.cpp) | pass |
| 3 | Open Configuration and Save Configuration exist and are grouped | File menu order: New/Open/Save/Save As Configuration together (ui.cpp); script 10-editor proves `config.save` round-trip | pass |
| 4 | Configuration options greyed without a collection | Menu enablement + shortcuts gated on `currentCollection`; scripts assert `app.expect-no collection=Original` at start and `app.expect-error "no configuration named" config.open Dipole`, `tab.new-empty` errors without a collection | pass |
| 5 | PDF export for a configuration and for a collection (all configurations at once) | 10-editor runs `export.configuration` + `export.collection` (10 pages for Original+MyEditor); CI --export writes dipole.pdf; PDF xref/structure unit-tested | pass |

Nothing in this plan changed physics; the port parity contracts (PNG/SVG byte equality, preset line
counts) stay pinned by CI.
