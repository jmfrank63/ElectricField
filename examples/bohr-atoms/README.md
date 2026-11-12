# Bohr-model atoms

`make_atoms.py` drives the `ef-mcp` MCP server over stdio and writes one PDF per atom to `pdf/`:

```sh
zig build -Doptimize=ReleaseFast
python3 examples/bohr-atoms/make_atoms.py            # or: make_atoms.py /path/to/ef-mcp /output/dir
```

Model: the nucleus is a +Z point charge. Shell n holds up to 2n² electrons (-1 each), spaced evenly on a
circle of radius n² (the Bohr radius scaling). Shells fill strictly in order (2, 8, 18, 32), the idealised
2n² rule rather than the real Aufbau order. Because 2n² electrons sit on a circle of circumference 2πn²,
neighbouring electrons in a full shell are always π apart.

Atoms: H, He, Li, C, O, Ne, Na, Ar (2, 8, 8), and Ni (Z = 28) and Nd (Z = 60) as the first atoms with three
and four full 2n² shells.

Rendering: RK4, 16 field lines per unit charge, equipotentials every 0.5. Field lines are traced in the
image plane only, so they do not divide in proportion to charge: inner shells catch nearly all of them and
outer electrons in Na, Ni and Nd receive few or none.
