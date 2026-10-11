# ElectricField

Electric field lines and equipotentials of point charges, computed with Runge-Kutta methods, including adaptive ones.

A detailed description of the physics is on the computational physics blog: https://compphys.go.ro/electric-field-lines/

![The Dipole preset in the cross-platform GUI](docs/images/gui-dipole.png)

This is the cross-platform port of the original MFC application: the same charge configurations produce
the same field lines and equipotentials, but the project now builds and runs on Windows, Linux and macOS
with nothing but [Zig](https://ziglang.org) and its package manager. The original Windows/MFC version is
preserved in history and on the `v1.0` tag.

## Features

- The 9 preset charge configurations of the original app, each opening in its own tab (they can be open side by side and compute in parallel):

| Preset | Charges (+ / -, position x y) |
|---|---|
| Dipole | +1 (-2, 0), -1 (2, 0) |
| Two Equal Charges | +1 (-2, 0), +1 (2, 0) |
| Two Unequal Charges Same Sign | +1 (-2, 0), +2 (2, 0) |
| Two Unequal Charges Different Signs | +2 (-2, 0), -1 (2, 0) |
| Quadrupole | +1 (-1.5, -1.5), -1 (1.5, -1.5), -1 (-1.5, 1.5), +1 (1.5, 1.5) |
| Four Charges Same Sign | +1 (-3, 0), +1 (-1, 0), +1 (1, 0), +1 (3, 0) |
| Two Dipoles Same Direction | +1 (-3, 0), -1 (-1, 0), +1 (1, 0), -1 (3, 0) |
| Two Dipoles Opposed Direction | +1 (-3, 0), -1 (-1, 0), -1 (1, 0), +1 (3, 0) |
| Parallel Dipoles | +1 (-1.5, -1.5), -1 (1.5, -1.5), +1 (-1.5, 1.5), -1 (1.5, 1.5) |

- 11 integration methods: Euler, Midpoint, Ralston, Heun, RK4, RK3/8 and the adaptive Heun-Euler,
  Bogacki-Shampine, Cash-Karp, Fehlberg and Dormand-Prince methods.
- Field lines per unit charge, equipotentials with an adjustable potential interval, a thread count and
  all draw colours/thicknesses in **View > Options...**; the settings persist between runs and draw
  changes repaint the open tabs immediately.
- **Collections**: organise your configurations into named collections (one file per collection under
  `Collections\` in the per-user config directory). The **Original** collection holds the built-in
  arrangements; **File > New Collection...** starts an empty one; the menu-bar switcher changes between
  them; **File > Save As... (Ctrl+S)** stores the current tab's charges into the current collection.
- **Charge editor**: click a charge to select it, drag it with the mouse, nudge it with the arrow keys
  (Shift = bigger steps) or type exact numbers into the editor panel's value/x/y fields with their arrow
  buttons (press and hold to repeat; **+/-** flips the sign). The **New charge** section creates a charge
  with any value and position, double-click empty canvas adds one, Delete removes the selected one.
  Rename the tab by typing over its name at the top of the panel. Charges move in realtime; the drawn
  lines keep showing the previous calculation (the tab is marked *edited*) until you press
  **Calculate (F5)** - with a per-tab integration method selector in the panel and an optional
  **Continuous calculation** checkbox that recomputes automatically after every edit.
- **Zoom and pan**: the mouse wheel zooms at the cursor, the panel's Zoom - / Zoom + / Reset view buttons
  are always at hand, right-drag pans, and the arrow keys pan whenever no charge is selected.
- **Your workspace comes back**: the open tabs - including each tab's calculation method, continuous
  calculation and any charge edits - are stored in `session.ini` when you quit and restored on the next
  start. If tabs have changes that are not saved to a collection, a prompt asks before quitting or
  switching collections. **Help > Open Configuration Folder** shows where `options.ini`,
  `configurations.ini`, `session.ini` and `Collections\` live.
- Export of the current configuration view to **PNG** and **SVG** (File > Export), matching what is on
  screen.
- Tabs can be closed while they compute; quitting mid-computation is safe. The computation is cancelled
  within a fraction of a second.

## Building

You need [Zig 0.17](https://ziglang.org/download/) - no other toolchain, no Visual Studio. SDL3 and Dear
ImGui are fetched automatically on the first build.

```sh
zig build          # build everything: the GUI app, ef-cli and the tests
zig build run      # build and start the application
zig build test     # run the unit tests (physics core, app logic, render paths)
zig build smoke    # headless start-up and tab-lifecycle check of the GUI (used by CI)
```

The core, the app logic and `ef-cli` are plain C++17 built with the Zig compiler; `-Dgui=false` builds
only that GUI-free part and fetches no GUI dependency at all.

### Headless command-line modes

```sh
ElectricField --smoke                 # headless GUI start-up check (dummy video driver)
ElectricField --screenshot <slug> <out.png> [--size WxH] [--scale k] [--modal about|chooser]
ElectricField --export <dir>          # export the Dipole as PNG + SVG (byte-equal to ef-cli's output)
ef-cli --list                         # the 9 preset slugs
ef-cli --preset quadrupole --png q.png --svg q.svg
ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv
```

`ef-cli` renders any preset without a display. CI uses it on Windows, Linux and macOS as a parity oracle:
the committed `tools/parity/expected-counts.csv` pins the number of charges, field lines and
equipotentials per preset, and the GUI's export is checked to be byte-identical to it. The
`docs/baseline/mfc/` images were rendered by the original MFC application and are the visual reference
for the port.

## Options and configuration files

Everything lives in the per-user config directory (`SDL_GetPrefPath("ElectricField", "ElectricField")`):

- Windows: `%APPDATA%\ElectricField\ElectricField\`
- Linux: `$XDG_CONFIG_HOME/ElectricField/ElectricField/` (usually `~/.config/...`)
- macOS: `~/Library/Application Support/ElectricField/ElectricField/`

- `options.ini` - computation and draw options; colours as `#RRGGBB`, the integration method by name
  (`euler`, `midpoint`, `ralston`, `heun`, `rk4`, `rk3per8`, `adaptive-heun-euler`,
  `adaptive-bogacki-shampine`, `adaptive-cash-karp`, `adaptive-fehlberg`, `adaptive-dormand-prince`).
  A missing or invalid file falls back to the defaults per setting.
- `configurations.ini` - the pre-collections user configurations (kept for reference; imported into the
  Original collection on first run of v2.2 or later).
- `Collections\<Name>.ini` - one file per collection, sections `[configuration "Name"]` with
  `charge = value, x, y` lines.
- `session.ini` - the open tabs of the current collection (title, preset source, per-tab method,
  continuous-calculation flag, charges), restored on the next start.

## Project layout

- `src/core/ef/` - the physics and scene core (std-only): field/potential, Runge-Kutta integrators,
  seeds and tracers, compute sessions, scene building, SVG backend.
- `src/app/` - GUI-free application logic: the tab model over compute sessions, pure UI decisions, the
  options/configuration/session models, the editor rules.
- `src/render/` - Dear ImGui painter and the offscreen PNG export shared by the GUI and `ef-cli`.
- `src/gui/` - the SDL3 + Dear ImGui application (window, menus, tabs, canvas, dialogs).
- `src/cli/` - `ef-cli`, the headless renderer and CI parity oracle.
- `tests/`, `tests_gui/` - doctest suites (the GUI-free ones run without any display or GUI dependency).
- `tools/parity/` - the committed line-count expectations.
- `assets/` - application icons.

## CI

Every push is built and tested on `windows-x64`, `linux-x64` and `macos-arm64`, plus a Linux
ThreadSanitizer job. The pipeline runs the unit tests, the headless GUI smoke, the 9-preset parity check,
a screenshot/export comparison against the `ef-cli` oracle and a guard that keeps the tree free of the
old MFC/Win32/GDI code.

## Credits

- Original application and physics: Adrian Roman (https://compphys.go.ro/electric-field-lines/)
- Cross-platform port: Johannes Maria Frank (https://github.com/jmfrank63/ElectricField)

Licensed under the GNU General Public License v3.0 - see [LICENSE](LICENSE).

## Editor details (v2.3)

- **Collections in the title**: the window title shows the collection you are working in, plus the active
  tab.
- **Removing configurations**: right-click a configuration in the New chooser to remove it from the
  collection (with confirmation; open tabs are not affected).
- **Esc reverts**: while a tab shows the *edited* marker, Esc restores the charges of the last
  calculation. **Edit > Undo (Ctrl+Z) / Redo (Ctrl+Y)** steps through charge and method changes.
- **Grid**: the panel's Grid section enables snapping and/or drawing independently, switches between
  Cartesian and polar grids, and lets you place the grid origin and set the spacing. With snapping on,
  dragged, added and typed positions land on the grid.
