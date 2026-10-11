# Phase 1 (v2.4): Automation API & UX Repairs - Context

**Gathered:** 2026-10-11
**Status:** Ready for planning
**Mode:** autonomous (owner: auto-mode, recommended answers)

<domain>
## Phase Boundary

A command API + script runner covering every manual action; fixes for the invisible grid and the New Collection lockup; the collection-centric File menu (Save/Save As/Close Collection, per-collection sessions); tab right-click menu (Close/Rename/Save As/Calculate).

Not in this phase: remote/socket API (the script interface is the contract), undo of collection operations, collection deletion.

<decisions>
## Implementation Decisions

- **D-01 (API):** `App::command(line)` is the single dispatcher: parses a command line (whitespace-separated, quoted strings allowed), executes it, and returns "ok" or "error: <why>". Every menu action maps to a command (app/tab/config/charge/view/grid/options/editor/collection). A parity table in the plan lists menu entry -> command.
- **D-02 (script runner):** `ElectricField --script <file> [--user-dir <dir>] [--out <results>] [--lenient]` runs headless; commands execute between frames; `app.wait-done` pumps until a tab finishes; results (one line per command) go to stdout or --out. Strict by default (stop on first error, exit 1); `app.state` dumps the full UI state as the test oracle. Without --user-dir, scripts run in a temp sandbox (never the real user data).
- **D-03 (BUG-01):** the grid becomes a core render pass: `renderScene(..., const GridSettings*)` draws grid lines AFTER the background fill and BEFORE the paths/charges; the GUI stops drawing the grid itself. ef-cli/SVG export stay gridless (nullptr).
- **D-04 (BUG-02):** after the leave-confirm modal's answer is consumed, the modal must close: a `closePopups` flag is consumed by every modal handler (CloseCurrentPopup + state reset). Reproduced and pinned by a script regression (collection.new then app.state).
- **D-05 (COL-06..09):** File menu: New Configuration..., New Collection..., Close Tab, Close Collection, Save Collection (Ctrl+S), Save Collection As..., Save Configuration As... (tab-level), Calculate All, Export, Exit. Save Collection merges matching edited tabs (title base name = configuration name) into the collection file and marks them clean. Save Collection As writes a new collection file and switches WITHOUT closing tabs. Close Collection saves and switches to Original. Per-collection sessions: `<userDir>/Sessions/<Collection>.ini` (SavedTab grammar) + `session.ini` reduced to the pointer (`collection = <name>`); switching saves the current collection's session and restores the target's (tabs reopen); startup follows the pointer (legacy session.ini content migrates).
- **D-06 (TAB-02..04):** right-click on a TAB opens a menu: Close Tab, Rename..., Save Configuration As..., Calculate. Rename uses a small modal (input + OK). The panel name field stays (same action).
- **D-07:** smoke keeps its UI-path checks; script tests add the scenario layer (tests/scripts/*.efscript run by a `script-tests` build step in CI on all three OSes).

</decisions>

<code_context>
- The v2.2 leave-prompt consumption leaves the modal open (ghost) - the New Collection lockup.
- The GUI drawGrid draws before renderScene's background fill - invisible grid.
- saveAsApply/frame consumption and the configs/save session helpers are the building blocks the commands reuse.
</code_context>
