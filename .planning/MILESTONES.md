# Milestones

## v2.4 Automation API and UX Repairs (Shipped: 2026-10-11)

**Phases completed:** 1 phase, 2 plans

**Key accomplishments:**

- The command API: every menu entry and editor/collection operation callable by text command; a headless script runner (`--script/--user-dir/--out/--lenient`) with the `app.state`/`app.expect` oracle; 5 scenario scripts running in CI on all three OSes; docs/api.md.
- BUG-01 fixed: the grid became a core render pass inside renderScene (bg -> grid -> lines -> charges), so the canvas background can never overwrite it again.
- BUG-02 fixed: the leave prompt closes after its answer (the ghost modal that locked the UI on New Collection); smoke regression through the real UI path.
- Collection-centric File menu: Save Collection (Ctrl+S), Save Collection As..., Close Collection, Save Configuration As...; per-collection Sessions files restore on switch and start.
- Tab context menu: Close / Rename / Save Configuration As / Calculate; restored tabs establish the active tab (CI-caught, fe96c4f).
- Verified: 114 unit tests + smoke + script suite green; CI run 38150402997 green on windows-x64, linux-x64, macos-arm64 and tsan-linux.

---

## v2.3 v2.3 (Shipped: 2026-10-11)

**Phases completed:** 1 phase, 2 plans

**Key accomplishments:**

- The window title shows the collection; configurations can be removed from a collection (chooser right-click + confirmation); the File menu shows New Configuration... / New Collection....
- Continuous calculation fixed (the GUI had re-armed the debounce every frame) and pinned by a smoke regression through the real canvas input path.
- Esc reverts uncalculated edits to the last calculated state; per-tab Undo/Redo (Ctrl+Z / Ctrl+Y) over charges and method.
- Snap grid: independently active and visible, Cartesian or polar, definable origin and spacing, persisted per tab in the session; snapping covers drag, double-click add and typed positions.
- Verified: 113 unit tests + full headless smoke (incl. the continuous-calculation regression), CI green on windows-x64, linux-x64, macos-arm64 and tsan-linux.

---
## v2.2 v2.2 (Shipped: 2026-10-11)

**Phases completed:** 1 phase, 2 plans

**Key accomplishments:**

- Collections: one human-readable file per collection under `Collections\`; the Original collection seeded from the built-ins + legacy configurations; menu-bar switcher; New Collection...; Calculate All opens every configuration of the collection in tabs.
- The session records the current collection and restores it with its tabs; Save As stores into the current collection; the unsaved-changes prompt guards quitting and switching.
- Editor refinements: always-visible New charge section (value + position + Add), "+/-" sign-flip button, double-click-add / single-click-edit contract pinned.
- Zoom and pan: wheel zoom at the cursor, Zoom - / Zoom + / Reset view buttons, right-drag pan, arrow-key pan when nothing is selected (the core View gained a centre, honoured by every backend including SVG/PNG export).
- Continuous calculation: a per-tab checkbox that recomputes (debounced 0.35 s) after every canvas change; persisted in the session.
- Verified: 113 unit tests + full headless smoke (incl. wheel zoom), CI green on windows-x64, linux-x64, macos-arm64 and tsan-linux.

---

## v2.1 v2.1 (Shipped: 2026-10-11)

**Phases completed:** 1 phase, 2 plans

**Key accomplishments:**

- Add-charge section in the editor panel (value, x, y + button) and arrow-glyph stepper buttons with press-and-hold repeat.
- Tab renaming from the editor panel (window title follows, empty names refused).
- Session persistence: open tabs - including each tab's calculation method and charge edits - stored in `session.ini` on quit and restored on start, with counter bumps preventing duplicate titles; Help > Open Configuration Folder.
- Verified: 107 unit tests + full headless smoke, CI green on windows-x64, linux-x64, macos-arm64 and tsan-linux.

---

## v1.0 v1.0 (Shipped: 2026-10-11)
**Phases completed:** 4 phases, 14 plans

**Key accomplishments:**

- The MFC application fully replaced by the cross-platform port: Zig 0.17 build (SDL3 + Dear ImGui), identical physics and visuals, options with persistence, PNG/SVG export, headless CLI + parity oracle, CI on three OSes plus ThreadSanitizer.

---

