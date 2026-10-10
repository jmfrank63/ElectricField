---
phase: 2
slug: compute-engine-headless-parity
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-10-10
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Source: `02-RESEARCH.md` § Validation Architecture. Task IDs are filled in by the planner.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest 2.5.3 (existing, `tests/*.cpp`, wired in `build.zig`) |
| **Config file** | none — `build.zig` `test_sources` list; test cwd = repo root |
| **Quick run command** | `zig build test -Dgui=false -Doptimize=ReleaseFast` (~5 s warm); Debug `zig build test -Dgui=false` (~41 s) per task commit where numerics/threading change |
| **Full suite command** | `zig build test -Dgui=false && zig build test -Dgui=false -Doptimize=ReleaseFast && zig build && zig build test && zig build smoke && zig build -Doptimize=ReleaseFast && ./zig-out/bin/ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv`; Linux: `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` |
| **Estimated runtime** | quick ~5–41 s; full ~2–3 min on Windows (warm) |

---

## Sampling Rate

- **After every task commit:** quick run (ReleaseFast core suite); Debug core suite for threading/numerics tasks
- **After every plan wave:** full suite on Windows + TSan run (WSL Ubuntu or CI)
- **Before `/gsd-verify-work`:** CI green on all legs incl. `tsan-linux` and parity artifacts on windows/ubuntu/macos-arm64
- **Max feedback latency:** 60 seconds (quick path)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | CORE-04 | — | N/A | unit/integration | `zig build test -Dgui=false` (`test_tracer.cpp`, `test_finalizer.cpp`, `test_session.cpp` de-dup on Quadrupole/Parallel dipoles) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | CORE-05 | — | N/A | integration | `test_session.cpp` (1 vs N threads identical; poll drains each job once; empty system) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | CORE-06 | — | no detached threads / clean shutdown | integration | `test_session.cpp` (cancel < 200 ms; destructor joins for all 11 methods) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | CORE-07 | — | N/A | unit/compile | `test_tracer.cpp` (job(job) regression, abs/M_PI, numerics policy), `test_bezier.cpp` (divide-by-zero guard); strict `core_flags` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | QA-02 | — | N/A | integration | `test_session.cpp` per-preset invariants; `test_seeds.cpp` (`sign(0)==+1`) | ❌ W0 / ✅ | ⬜ pending |
| TBD | TBD | TBD | QA-03 | — | data-race freedom | CI (Linux) | `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | QA-04 | T supply chain (CI) | least-privilege CI | unit + CI | `test_scene.cpp`; `ef-cli --all --expect tools/parity/expected-counts.csv` on 3 OSes + artifacts | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | D-14..D-18 | — | N/A | unit/build | empty-cache `zig build -Dgui=false` fetches only doctest; `test_presets.cpp` fails when unreadable; invalid method rejected; capture-script parse + bad-path run | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_{tracer,bezier,finalizer,session,scene}.cpp` (+ shared test params header) — reference: `research-ref/tests/`
- [ ] `build.zig`: test list, `-Dsanitize=thread`, lazy deps, `ef-cli`; `build.zig.zon`: `.lazy = true` for sdl3/imgui
- [ ] `tools/parity/expected-counts.csv`, `tools/parity/compare-mfc.sh`; CI parity + TSan jobs
- [ ] Framework install: none (doctest exists)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| First macOS arm64 parity counts | QA-04 | No local Mac | Read CI artifact `parity-macos-arm64/summary.txt`; adjust tolerance/re-baseline per RESEARCH Open Question 2 if outside tolerance |
| Visual similarity to MFC baselines | QA-04 / core value | AA differs (informational) | `tools/parity/compare-mfc.sh` locally (ImageMagick); eyeball one preset side by side |
| Capture-script hardening end-to-end | D-18 | Needs the MFC exe + desktop | Parse check + bad-path run automated; optional full re-capture |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s (quick path)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
