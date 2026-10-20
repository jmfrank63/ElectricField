# Electric Field command API

Every manual action (menu entry, editor operation, collection operation) is callable through a
text command interface. Commands run between frames of the real application loop, so what a script
does is exactly what a user clicking through the UI does.

## Running scripts

```sh
ElectricField --script <file> [--user-dir <dir>] [--out <results>] [--lenient]
```

- The runner is headless (dummy video driver, software renderer) and exits 0 when every command
  succeeded, 1 on the first failed command (unless `--lenient`), 2 on usage errors.
- `--user-dir` redirects ALL user files (options.ini, Collections/, Sessions/, session.ini) to a
  directory. Without it, scripts run in a throwaway sandbox under the temp folder - they never touch
  real user data.
- `--out` writes one result line per command (`[ok] <command>` / `[error] <command> -> <why>`);
  the default is stdout.
- Lines starting with `#` are comments. Names containing spaces are written in double quotes.

## The state oracle

`app.state` dumps the whole UI state; `app.expect <text>` asserts that the dump contains `<text>`
(the workhorse of the script tests in `tests/scripts/`):

```
collection=Test
collections=Original|Test
recents=/tmp/ef-sandbox/Collections/Test|/tmp/ef-sandbox/Collections/Original
configs=My Field
drawsize=25
tabs=1
tab serial=2 title=Empty Configuration 1 state=done method=euler autocalc=0 stale=1 dirty=1 selected=1 lines=16 zoom=1 centre=0,0 grid=1,1,0,0,0,0,0.5,15 charges=1,0.5,0,25,1,Charge 1;3,0,2,0,2,Charge 2
```

`grid=` is `active,visible,polar,intersectionsOnly,originX,originY,spacing,angleStepDeg`.

`charges=` is a `;`-separated list of `v,x,y,sizePx,id,name` per charge (v2.6 phase 3): the drawn
radius in px (0 = the Options default), the stable id, and the EFFECTIVE name (the stored name, or
the derived `Charge N`; names are comma-free on disk). Older dumps' `v,x,y` prefix is unchanged, so
`app.expect charges=1,0.5,0` keeps matching. Names are stored raw apart from ',', so a name
containing ';' or '=' makes the dump ambiguous for consumers that split on them - avoid such names
in scripts.

`drawsize=` is the drawn charge radius in px (v2.5 SIZE-01, valid range 1-120; the `chargeSize` key in options.ini).

`couple=` is the value-and-size coupling flag (261020-ef2, `coupleValueSize` in options.ini).

`recents=` is the File > Recent Collections menu, newest first (261012-h26; the opened collection
files' names, persisted as `recent` path lines in session.ini). v2.6 REC-02: at startup the list is
seeded from the pointer's `collection` value when it names an existing collection file that is
missing from the list (a legacy pointer carries only that line), and session.ini is rewritten on
quit - the list stays current even when the app closes without a collection switch.

## Commands

### Application
| Command | Effect |
|---|---|
| `app.state` | dump the state (above) |
| `app.expect <text>` | fail unless the state dump contains the text |
| `app.expect-no <text>` | fail if the state dump contains the text (v2.5) |
| `app.expect-error <text> <cmd> [args...]` | run the command, fail unless it errors with the text (v2.5) |
| `app.quit [force]` | quit; refuses when tabs have unsaved edits unless `force` |
| `app.frames <n>` | run n frames of the loop |
| `app.wait-done [serial\|active] [ms]` | pump frames until the tab finished (default 60 s) |
| `app.open-config-folder` | open the configuration folder (desktop only) |

### Collections
| Command | Effect |
|---|---|
| `collection.open <name\|path>` / `collection.switch <name\|path>` | open a collection FILE (v2.5 OPEN-01): a bare name resolves to `Collections\<name>.ini`, a path opens the file from anywhere; saves this collection's session, closes it, restores the target's tabs |
| `collection.open-recent <n>` | open the n-th entry (1-based, newest first) of the recents list (261012-h26; = File > Recent Collections) |
| `collection.new <name>` | create an empty collection (a file under Collections) and switch to it |
| `collection.save` | store edited open tabs into matching configurations, write the file |
| `collection.save-as <name\|path>` | save the collection's configurations to a new file; the tabs stay open and belong to it |
| `collection.close` | save and close the collection (afterwards no collection is open and the configuration menus grey out) |

The session pointer (session.ini) never loses its stored collection name: with no collection open its
`collection` line is preserved instead of blanked, so the startup seeding above keeps working (v2.6
REC-02).

### Configurations
| Command | Effect |
|---|---|
| `config.open <name>` | open a configuration of the current collection in a new tab |
| `config.save` | store the active tab's charges into the configuration it came from, renaming that entry in place to the tab's current name when the tab was renamed (v2.5 CFG-04); a name owned by a different configuration is refused — use `config.save-as` to overwrite with confirmation; preset-sourced tabs append a new configuration under the current name; fails for tabs never saved to a configuration |
| `config.save-as <name>` | store the active tab's charges under the name (reserved preset names refused); the tab takes the name and now saves there with `config.save` |
| `config.remove <name>` | remove a configuration from the collection (open tabs unaffected); in the UI also the chooser's visible Remove... button, with the same confirmation (v2.6 COL-10) |
| `export.configuration <path.pdf>` | export the active configuration as a PDF with a fitted view (v2.5 EXP-03) |
| `export.collection <path.pdf>` | export every configuration of the collection - one PDF page each (v2.5 EXP-03) |

### Tabs
| Command | Effect |
|---|---|
| `tab.new-empty` | open the "Empty Configuration" workbench tab (needs an open collection, like the menu) |
| `tab.close [serial]` | close a tab (default: the active one) |
| `tab.select <serial>` / `tab.rename <name>` / `tab.calculate [serial]` | selection, rename, recompute |
| `tab.calculate-all` | close the tabs and open every configuration of the collection |
| `tab.esc` | revert the active tab's uncalculated edits (Esc behaviour) |

### Editor
| Command | Effect |
|---|---|
| `charge.add <value> <x> <y>` | append a charge (snapped when the grid is active) and select it; a value of 0 is refused (261020-ef2) |
| `charge.select <index>` / `charge.move <i> <x> <y>` / `charge.value <i> <v>` / `charge.sign <i>` / `charge.remove <i>` | edit charges (index defaults to the selection where optional); `charge.value` refuses 0 (`error: the charge value must not be 0`) |
| `charge.name <i> <name...>` | set the charge's name (v2.6 phase 3): remaining tokens joined with single spaces, quoted names arrive as one token, ',' is stripped, empty resets to the derived "Charge N"; marks the tab dirty without invalidating the drawn lines |
| `charge.size <i> <px>` | set the charge's own drawn radius (v2.6 phase 3): 0 = the Options default, else 1-120 px (`error: charge size must be 0 or 1-120`); marks the tab for recalculation (the per-charge seed/hit circles follow the size); with `options.set couple-value-size on` a resize also sets the charge's value to the size (sign preserved) |
| `editor.undo` / `editor.redo` | step through charge and method changes |

### View & grid
| Command | Effect |
|---|---|
| `view.zoom <factor>` / `view.pan <dpx> <dpy>` / `view.reset` | view transform (pan in device pixels); zoom-in is capped like in the UI by the adapted-calculation limit, about 26x at 100% scale - the calculation adapts to the zoom (lines hug the charges) and a settled zoom recalculates automatically, including a zoom made through this command |
| `grid.set active\|visible\|polar\|intersections on\|off` | grid flags, independently settable; `intersections` = show only the lattice points (v2.5) |
| `grid.set origin <x> <y>` / `grid.set spacing <s>` / `grid.set angle <a>` | grid geometry |

### Options
| Command | Effect |
|---|---|
| `options.set lines\|threads\|interval\|equipotentials\|method\|charge-size\|couple-value-size <value>` | change computation/draw options (validated, persisted); `charge-size` = the drawn charge radius, 1-120 px (v2.5, minimum 1 since 261020-ef2) - since v2.6 phase 3 it is the DEFAULT size for NEW charges and the fallback for charges without their own size; `couple-value-size on\|off` = the panel's value-and-size coupling (261020-ef2) |

## Menu parity

The File/Edit menus call the same actions: Open Configuration... and New Configuration... = the
chooser (= `config.open`), Open Collection... (Ctrl+O) and New Collection... = the native file
dialogs (the same actions as `collection.open`/`collection.new`), Recent Collections =
`collection.open-recent <n>` (261012-h26), Close Tab = `tab.close`, Close Collection = `collection.close`,
Save Collection (Ctrl+S) = `collection.save`, Save Collection As... = `collection.save-as`,
Save Configuration = `config.save`, Save Configuration As... = `config.save-as`,
Calculate All = `tab.calculate-all`, Export Configuration = the export dialog (PNG/SVG/PDF;
= `export.configuration`'s render), Export Collection... = `export.collection`,
Edit > Undo/Redo = `editor.undo`/`editor.redo`, tab right-click menu =
Close/Rename/Save Configuration As/Calculate.

The configuration menu entries are greyed out while no collection is open (v2.5 CFG-04): a collection
is always opened first, then its configurations.
