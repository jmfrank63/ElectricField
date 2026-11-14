# MCP server

`ef-mcp` lets an AI agent run the simulation through the
[Model Context Protocol](https://modelcontextprotocol.io). It is a headless stdio server: it needs no
display or GPU, and it uses the same physics core and renderers as the app and `ef-cli`, so the same
configuration gives the same output. Each request is self-contained, and the server touches no user
files, options or collections.

## Install

`ef-mcp` ships in every installer:

| Platform | Path |
|---|---|
| Linux (`.deb`) | `/usr/bin/ef-mcp` |
| macOS (`.dmg`) | `/Applications/Electric Field.app/Contents/MacOS/ef-mcp` |
| Windows (setup) | `%LOCALAPPDATA%\Programs\ElectricField\ef-mcp.exe` |

From source, `zig build -Doptimize=ReleaseFast` produces `zig-out/bin/ef-mcp`. With `-Dgui=false` it
is built without PNG support (SVG and PDF only).

## Connect a client

Claude Code:

```sh
claude mcp add electric-field -- /usr/bin/ef-mcp
```

Clients configured through JSON (replace the command with the path for your platform):

```json
{
  "mcpServers": {
    "electric-field": { "command": "/usr/bin/ef-mcp" }
  }
}
```

## Tools

| Tool | What it does |
|---|---|
| `list_presets` | The 9 built-in configurations: slug, name, charges |
| `list_methods` | The 11 integration methods: name, adaptive flag, order |
| `evaluate_field` | E vector, magnitude and potential at up to 10000 points |
| `compute_forces` | The force on every charge from the others, in space, and the interaction energy |
| `compute_field_lines` | Traces a configuration and returns line counts and, with `include_lines`, the geometry |
| `export_csv` | The numbers as a CSV table: the points of the lines, or the charges with forces |
| `render_field` | Computes and renders a PNG, SVG or PDF, inline or to a file |

Every tool that takes a configuration needs exactly one of:

- `preset`: a slug from `list_presets`, such as `"dipole"`
- `charges`: 1 to 64 charges at distinct positions, each `{"charge": 1, "x": -2, "y": 0}` with an
  optional `z` (default 0) and an optional drawn radius `size_px` (0, or 2 to 120)

`charge` is a non-zero integer (0 is accepted with `charge_model` `conductor`, for an uncharged ball of
a body or a neutral conductor). World coordinates have y pointing up. The field uses a unit Coulomb
constant: `E = sum q*r/|r|^3` and `V = sum q/|r|`.

`charge_radius` (default 0, at most 1000, world units) works with a preset or with `charges` and gives
every charge a physical size: each becomes a uniformly charged ball of that radius. Outside a ball the
field is unchanged; inside it is finite, `E = q*r/R^3` and `V = q*(3R^2 - r^2)/(2R^3)`, so
`evaluate_field` never reports a point as singular. The inside of a ball is not part of the traced field:
equipotentials stop at ball surfaces. Each ball keeps its uniform charge and overlapping charges add.
Balls whose centres are at most `2R` apart form one body, whatever their signs, and balls may share a
centre. Field lines are placed on a body's outer surface in proportion to the field flowing through it
(`lines_per_unit_charge` lines per unit of flux): with a total charge of 0 or more they start where the
field leaves a surface and are traced along it, with a negative total they start where it enters and are
traced against it. An isolated ball gets the same number of lines as a point charge; a neutral body with
both signs gets the lines that loop from its positive to its negative side.

`charge_model` (default `uniform`, only with `charge_radius` above 0) chooses how the charge of the
balls is distributed. With `conductor`, the charge of each body shifts over its outer surface until the
surface is an equipotential, so every field line ends at a right angle. Only the total charge of a body
is fixed, touching balls share their charge, the field inside a body is 0 and its potential constant.
`compute_forces` then reports the force on a body split over its balls (only the sum per body is
meaningful) and `potential_energy` is the total electrostatic energy, half the sum of charge times
potential over the bodies, which includes each body's own energy. The field is solved numerically
(sources inside the balls fitted to the surface); it is accurate to about 0.1%, also for balls that
almost touch (gaps down to 0.01 radii). The `bodies` CSV table reports the error as `surface_error`.

### 3D

A charge with `z` other than 0, or a turned view (`view_yaw`, `view_pitch`), makes the request 3D. z
points towards the viewer. The 3D pipeline differs from the planar one in three ways:

- Field lines start evenly spread over a sphere around each source charge (`|charge|` times
  `lines_per_unit_charge` lines) and are traced in space with the same integration rules.
- The view is orthographic and turns about the world origin: `view_yaw` about the vertical axis, then
  `view_pitch` about the screen's horizontal axis, both in degrees. Lines are returned and drawn as their
  projection onto the screen; nearer charges are drawn over farther ones.
- Equipotentials are surfaces in space. Drawn and returned is their cut with the plane parallel to the
  screen through the centre of the charges (the mean of their positions); in the lines CSV their points
  carry the coordinates in space.

Results carry `"dimensions": 2` or `3`. A planar request with the default view gives exactly the output
it always gave.

`evaluate_field` points take an optional `z`. When the point or any charge has one, each value also
carries `z` and `ez`, and `magnitude` is the length of the full vector.

`compute_forces` takes the configuration (`preset` or `charges`, and `charge_radius`) and returns, per
charge, `fx`, `fy`, `fz`, `magnitude` and `body` (the index of the melted body it belongs to), plus `potential_energy` for the whole system, with
`F = q1*q2/d^2` and `U = q1*q2/d`. With `charge_radius` above 0, balls whose centres are closer than
`2R` overlap: they are treated as rigid balls that pass through each other, which gives, with
`s = d/R`, `F = q1*q2/R^2 * (s - 9s^2/16 + s^4/32)` and
`U = q1*q2/R * (6/5 - s^2/2 + 3s^3/16 - s^5/160)`. The force goes to 0 as the centres meet and the
energy stays finite. The self energy of the balls is not included.

`export_csv` takes the same arguments as `compute_field_lines` plus `table` (`lines`, the default,
`charges` or `bodies`), and optionally `output_path` (absolute, ending in `.csv`) and `overwrite`. Without
`output_path` the CSV comes back inline as a second text block. The `lines` table has one row per point
of every line: `kind, line, point, x, y, z, screen_x, screen_y, potential, ex, ey, ez, magnitude,
azimuth_deg, elevation_deg, label`. The `charges` table has one row per charge: `index, name, charge,
x, y, z, radius, body, potential, ex, ey, ez, fx, fy, fz, force, screen_x, screen_y, view_yaw_deg,
view_pitch_deg, system_energy`, where potential and field are those of the other charges at its centre.

The `bodies` table has one row per body: `body, balls, charge, x, y, z, px, py, pz, dipole,
flux_out, flux_in, fx, fy, fz, force, model, potential, surface_error`, with the net charge, the mean of the ball centres, the dipole moment about it, and
the flux leaving and entering the outer surface in charge units (`flux_out - flux_in` is the net charge).

`compute_field_lines`, `export_csv` and `render_field` also accept:

| Argument | Default | Range |
|---|---|---|
| `method` | `euler` | a name from `list_methods` |
| `lines_per_unit_charge` | 16 | 4 to 128 |
| `equipotentials` | `true` | |
| `potential_interval` | 0.1 | 0.01 to 0.5 |
| `threads` | 4 | 1 to 128 |
| `timeout_ms` | 60000 | 100 to 600000 |
| `view_yaw` | 0 | -360 to 360 degrees |
| `view_pitch` | 0 | -90 to 90 degrees |

A request may need at most 10000 field lines in total (the sum of `|charge|` times
`lines_per_unit_charge`).

`compute_field_lines` adds `include_lines` (default `false`) and `max_points_per_line` (default 200,
2 to 5000). Geometry contains only points that lie on the curves, in world units; thinning keeps both
endpoints of each line.

`render_field` adds:

| Argument | Default | Notes |
|---|---|---|
| `format` | `png` | `png`, `svg` or `pdf` (`svg` is the default without PNG support) |
| `width`, `height` | 1024, 768 | 16 to 8192 px |
| `pixels_per_unit` | 100 | 1 to 10000 |
| `center_x`, `center_y` | 0, 0 | The world point at the image centre |
| `output_path` | none | Write to this file instead of returning the image inline |
| `overwrite` | `false` | Allow `output_path` to replace an existing file |

`output_path` must be absolute, in an existing directory, and end in the format's extension. It is
required for PDF. Inline results are limited to 8 MiB.

Invalid arguments and failed or timed-out computations are returned as tool errors (`isError: true`)
with a message that says what to change.

## Protocol

The server speaks newline-delimited JSON-RPC on stdin and stdout, and writes diagnostics only to
stderr. It supports MCP `2026-07-28`, including `server/discover` and per-request `_meta`, and clients
of `2025-11-25`, `2025-06-18`, `2025-03-26` and `2024-11-05` through `initialize`.

Requests run one at a time, in order. `notifications/cancelled` stops a running or queued request,
which then gets no response. When stdin closes, unfinished work gets 5 seconds and is then cancelled.

To try it from a shell:

```sh
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"list_methods","_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' | ef-mcp
```
