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
configs=My Field
tabs=1
tab serial=2 title=Empty Configuration 1 state=done method=euler autocalc=0 stale=1 dirty=1 selected=1 lines=16 zoom=1 centre=0,0 grid=1,1,0,0,0,0,0.5,15 charges=1,0.5,0;3,0,2
```

`grid=` is `active,visible,polar,intersectionsOnly,originX,originY,spacing,angleStepDeg`.

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
| `collection.new <name>` | create an empty collection (a file under Collections) and switch to it |
| `collection.save` | store edited open tabs into matching configurations, write the file |
| `collection.save-as <name\|path>` | save the collection's configurations to a new file; the tabs stay open and belong to it |
| `collection.close` | save and close the collection (afterwards no collection is open and the configuration menus grey out) |

### Configurations
| Command | Effect |
|---|---|
| `config.open <name>` | open a configuration of the current collection in a new tab |
| `config.save` | store the active tab's charges back into the configuration it came from (v2.5 CFG-04; fails for tabs never saved to a configuration) |
| `config.save-as <name>` | store the active tab's charges under the name (reserved preset names refused); the tab now saves there with `config.save` |
| `config.remove <name>` | remove a configuration from the collection (open tabs unaffected) |
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
| `charge.add <value> <x> <y>` | append a charge (snapped when the grid is active) and select it |
| `charge.select <index>` / `charge.move <i> <x> <y>` / `charge.value <i> <v>` / `charge.sign <i>` / `charge.remove <i>` | edit charges (index defaults to the selection where optional) |
| `editor.undo` / `editor.redo` | step through charge and method changes |

### View & grid
| Command | Effect |
|---|---|
| `view.zoom <factor>` / `view.pan <dpx> <dpy>` / `view.reset` | view transform (pan in device pixels) |
| `grid.set active\|visible\|polar\|intersections on\|off` | grid flags, independently settable; `intersections` = show only the lattice points (v2.5) |
| `grid.set origin <x> <y>` / `grid.set spacing <s>` / `grid.set angle <a>` | grid geometry |

### Options
| Command | Effect |
|---|---|
| `options.set lines\|threads\|interval\|equipotentials\|method\|charge-size <value>` | change computation/draw options (validated, persisted); `charge-size` = the drawn charge radius, 2-120 px (v2.5) |

## Menu parity

The File/Edit menus call the same actions: Open Configuration... and New Configuration... = the
chooser (= `config.open`), Open Collection... (Ctrl+O) and New Collection... = the native file
dialogs (the same actions as `collection.open`/`collection.new`), Close Tab = `tab.close`,
Close Collection = `collection.close`, Save Collection (Ctrl+S) = `collection.save`,
Save Collection As... = `collection.save-as`, Save Configuration = `config.save`,
Save Configuration As... = `config.save-as`, Calculate All = `tab.calculate-all`,
Export Configuration = the export dialog (PNG/SVG/PDF; = `export.configuration`'s render),
Export Collection... = `export.collection`, Edit > Undo/Redo = `editor.undo`/`editor.redo`,
tab right-click menu = Close/Rename/Save Configuration As/Calculate.

The configuration menu entries are greyed out while no collection is open (v2.5 CFG-04): a collection
is always opened first, then its configurations.
