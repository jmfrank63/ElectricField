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
| `compute_field_lines` | Traces a configuration and returns line counts and, with `include_lines`, the geometry |
| `render_field` | Computes and renders a PNG, SVG or PDF, inline or to a file |

Every tool that takes a configuration needs exactly one of:

- `preset`: a slug from `list_presets`, such as `"dipole"`
- `charges`: 1 to 64 charges at distinct positions, each `{"charge": 1, "x": -2, "y": 0}` with an
  optional drawn radius `size_px` (0, or 2 to 120)

`charge` is a non-zero integer. World coordinates have y pointing up. The field uses a unit Coulomb
constant: `E = sum q*r/|r|^3` and `V = sum q/|r|`.

`charge_radius` (default 0, at most 1000, world units) works with a preset or with `charges` and gives
every charge a physical size: each becomes a uniformly charged ball of that radius. Outside a ball the
field is unchanged; inside it is finite, `E = q*r/R^3` and `V = q*(3R^2 - r^2)/(2R^3)`, so
`evaluate_field` never reports a point as singular. Field lines may pass through a ball and end on the
field null inside the ball they flow into, which is not necessarily its centre.

`compute_field_lines` and `render_field` also accept:

| Argument | Default | Range |
|---|---|---|
| `method` | `euler` | a name from `list_methods` |
| `lines_per_unit_charge` | 16 | 4 to 128 |
| `equipotentials` | `true` | |
| `potential_interval` | 0.1 | 0.01 to 0.5 |
| `threads` | 4 | 1 to 128 |
| `timeout_ms` | 60000 | 100 to 600000 |

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
