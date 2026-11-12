#!/usr/bin/env python3
"""Render Bohr-model atoms as PDFs by driving the ef-mcp MCP server over stdio.

Shell n holds at most 2n^2 electrons and has radius n^2 (Bohr: r_n ~ n^2). Shells are filled strictly in
order (2, 8, 18, 32), which is the idealised 2n^2 model, not the real Aufbau order. The nucleus carries +Z,
each electron -1, placed evenly on its shell. Usage: make_atoms.py [path/to/ef-mcp] [output_dir]
"""
import json, math, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "zig-out", "bin", "ef-mcp")
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "pdf"))

ATOMS = [(1, "H", "hydrogen"), (2, "He", "helium"), (3, "Li", "lithium"), (6, "C", "carbon"),
         (8, "O", "oxygen"), (10, "Ne", "neon"), (11, "Na", "sodium"), (18, "Ar", "argon"),
         (28, "Ni", "nickel"), (60, "Nd", "neodymium")]

def shells(z):
    out, n = [], 1
    while z > 0:
        k = min(z, 2 * n * n)
        out.append(k); z -= k; n += 1
    return out

def charges(z):
    cs = [{"charge": z, "x": 0, "y": 0, "size_px": 10}]
    for n, count in enumerate(shells(z), start=1):
        r = n * n
        offset = math.pi / 2 + (math.pi / count if n % 2 == 0 else 0)  # stagger alternate shells
        for i in range(count):
            a = offset + 2 * math.pi * i / count
            cs.append({"charge": -1, "x": round(r * math.cos(a), 9), "y": round(r * math.sin(a), 9), "size_px": 4})
    return cs

META = {"io.modelcontextprotocol/protocolVersion": "2026-07-28", "io.modelcontextprotocol/clientCapabilities": {}}

class Client:
    def __init__(self, cmd):
        self.p = subprocess.Popen([cmd], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.next_id = 0
    def call(self, method, params=None):
        self.next_id += 1
        msg = {"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": dict(params or {}, _meta=META)}
        self.p.stdin.write(json.dumps(msg) + "\n"); self.p.stdin.flush()
        reply = json.loads(self.p.stdout.readline())
        if "error" in reply: raise RuntimeError(reply["error"])
        return reply["result"]
    def close(self):
        self.p.stdin.close(); self.p.wait()

def main():
    os.makedirs(OUT, exist_ok=True)
    c = Client(SERVER)
    info = c.call("server/discover")["_meta"]["io.modelcontextprotocol/serverInfo"]
    print(f"server {info['name']} {info['version']}")
    for z, sym, name in ATOMS:
        n_max = len(shells(z))
        size = 1000
        args = {"charges": charges(z), "format": "pdf", "width": size, "height": size,
                "pixels_per_unit": 0.4 * size / (n_max * n_max), "method": "rk4",
                "lines_per_unit_charge": 16, "potential_interval": 0.5, "threads": 8, "timeout_ms": 600000,
                "output_path": os.path.join(OUT, f"{z:02d}-{sym}-{name}.pdf"), "overwrite": True}
        t = time.time()
        res = c.call("tools/call", {"name": "render_field", "arguments": args})
        data = res.get("structuredContent", {})
        if res.get("isError"): raise RuntimeError(res["content"][0]["text"])
        print(f"Z={z:2d} {sym:2s} shells={shells(z)} {time.time()-t:5.1f}s {json.dumps(data)}")
    c.close()

if __name__ == "__main__":
    main()
