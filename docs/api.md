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
tab serial=2 title=Empty Configuration 1 state=done method=euler autocalc=0 stale=1 dirty=1 selected=1 lines=16 zoom=1 centre=0,0 grid=1,1,0,0,0,0.5,15 charges=1,0.5,0;3,0,2
```

## Commands

### Application
| Command | Effect |
|---|---|
| `app.state` | dump the state (above) |
| `app.expect <text>` | fail unless the state dump contains the text |
| `app.quit [force]` | quit; refuses when tabs have unsaved edits unless `force` |
| `app.frames <n>` | run n frames of the loop |
| `app.wait-done [serial\|active] [ms]` | pump frames until the tab finished (default 60 s) |
| `app.open-config-folder` | open the configuration folder (desktop only) |

### Collections
| Command | Effect |
|---|---|
| `collection.new <name>` | create an empty collection and switch to it |
| `collection.switch <name>` | save this collection's session, switch, restore the target's tabs |
| `collection.save` | store edited open tabs into matching configurations, write the file |
| `collection.save-as <name>` | save under a new name; tabs stay open |
| `collection.close` | save and switch to Original |

### Configurations
| Command | Effect |
|---|---|
| `config.open <name>` | open a configuration of the current collection in a new tab |
| `config.save-as <name>` | store the active tab's charges under the name (reserved preset names refused) |
| `config.remove <name>` | remove a configuration from the collection (open tabs unaffected) |

### Tabs
| Command | Effect |
|---|---|
| `tab.new-empty` | open the "Empty Configuration" workbench tab |
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
| `grid.set active\|visible\|polar on\|off` | grid flags, independently settable |
| `grid.set origin <x> <y>` / `grid.set spacing <s>` / `grid.set angle <a>` | grid geometry |

### Options
| Command | Effect |
|---|---|
| `options.set lines\|threads\|interval\|equipotentials\|method <value>` | change computation options (validated, persisted) |

## Menu parity

The File/Edit menus call the same actions: New Configuration... = the chooser (= `config.open`),
New Collection... = `collection.new`, Close Tab = `tab.close`, Close Collection = `collection.close`,
Save Collection (Ctrl+S) = `collection.save`, Save Collection As... = `collection.save-as`,
Save Configuration As... = `config.save-as`, Calculate All = `tab.calculate-all`, Export = the export
dialog, Edit > Undo/Redo = `editor.undo`/`editor.redo`, tab right-click menu = Close/Rename/
Save Configuration As/Calculate.
