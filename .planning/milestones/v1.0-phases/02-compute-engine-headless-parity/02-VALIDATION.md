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

Task IDs are `<plan>-T<n>` and match the task order in each PLAN.md. "Automated Command" condenses the task's `<verify>`/`<automated>` fast path (the PLAN.md holds the exact command); "gate:" names the task's `<plan_gate>`. Threat IDs refer to the STRIDE registers in the PLAN.md files.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 02-01-T1 (tracer) | 02-01 | 1 | CORE-04, CORE-05 (D-01..D-05, D-07, D-08) | T-02-01, T-02-02 | workers always joined, never detached; published lines immutable | integration | `zig build test -Dgui=false -Doptimize=ReleaseFast --summary all && zig build test -Dgui=false --summary all` + build.zig source-list greps (interim `test_session.cpp`: 1 vs 7 threads on dipole and parallel-dipoles through the full pipeline) | ❌ W0 (`src/core/ef/{bezier,finalizer,session}*`, interim `tests/test_session.cpp`: created by this task) | ⬜ pending |
| 02-01-T2 | 02-01 | 1 | CORE-06, QA-02, QA-03 (1-vs-N, cancel latency), CORE-04, CORE-05 (D-02, D-03, D-05, D-15) | T-02-02, T-02-03, T-02-06 | invalid Method -> failed + error; 200 ms asserted in every non-TSan build | integration | ReleaseFast + Debug core suite + `kLatencyBudgetMs` / sweep-name / `methodFromName` greps (`test_session.cpp`: invariants, de-dup on Quadrupole/Parallel dipoles, poll exactly-once, heaviest-job cancel, cancel + destroy for all 9 presets x 11 methods, destroy for 11 methods, stress, empty system, invalid Method, numThreads 0) | ❌ W0 (`tests/test_params.hpp`: created by this task; `tests/test_session.cpp` replaced by the full suite) | ⬜ pending |
| 02-02-T1 (tracer) | 02-02 | 2 | QA-03 (local TSan), D-14, D-17, D-20, IN-05 | T-02-04, T-02-22, T-02-23 | zon hashes unchanged; `-Dgui=false` fetches only doctest (fresh cache); any race exits 66 | unit + build + TSan | ReleaseFast core suite + `.lazy = true` x2 / `lazyDependency` x2 / `sanitize_thread` / D-17 / IN-05 greps; post-commit: D-17 fail-first from zig-out, fresh-clone laziness proof, WSL `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` on `git archive HEAD` (`tsan_symbols` > 0) | ✅ (build files and both tests exist; modified) | ⬜ pending |
| 02-02-T2 | 02-02 | 2 | CORE-07, CORE-04 (de-dup boundary), QA-02, QA-03, D-05, D-06, D-07, D-09, D-16 | T-02-22, T-02-23 | self-copy regression proven fail-first by a temporary mutation | unit + TSan | ReleaseFast + Debug core suite, `git diff --quiet -- src/core/ef/tracer.hpp`, test-list and `computeStages` greps (`test_tracer.cpp`: job(job), abs, pi macro, FMA canary; `test_bezier.cpp`: divide-by-zero guard; `test_finalizer.cpp`: shortcut, grouping, boundary); gate: Debug suite, `zig build`, `zig build smoke`, WSL TSan run of the whole suite on HEAD | ❌ W0 (`tests/test_{tracer,bezier,finalizer}.cpp`: created by this task) | ⬜ pending |
| 02-03-T1 (tracer) | 02-03 | 3 | QA-04, CORE-04, D-10, D-11 (amended: surface-backed software renderer), D-14 | T-02-07, T-02-11 | ef-cli is a console exe; renderPng saves/restores the ImGui context | integration | `zig build -Doptimize=ReleaseFast` + `ef-cli --preset dipole --svg --png` into `zig-out/check/` (summary `charges=2 field_lines=16 equipotentials=75`, PNG signature, `viewBox="0 0 1024 768"`) + ReleaseFast core suite (`test_scene.cpp`) | ❌ W0 (`src/core/ef/{scene,svg}*`, `src/render/*`, `src/cli/main.cpp`, `tests/test_scene.cpp`: created by this task) | ⬜ pending |
| 02-03-T2 | 02-03 | 3 | QA-04, CORE-05 (threads boundary), D-12, D-13, D-14 | T-02-07, T-02-08, T-02-09, T-02-12 | argument bounds exit 2; no shell/process execution; read-only CSV parse | CLI + integration | `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv` (9 lines, 9 svg, 9 png), `--list` order == CSV order, `/parity-out/` ignored; tolerance boundary 77 ok / 78 fail recorded; gate: WSL Linux headless `--all` without DISPLAY with equal counts + `compare-mfc.sh` report (header + 9 rows, recall >= 0.98) | ❌ W0 (`tools/parity/*`: created by this task) | ⬜ pending |
| 02-04-T1 (tracer) | 02-04 | 4 | QA-03, QA-04, CORE-07 (3-OS warning-clean), D-12, D-20, IN-04 | T-02-13, T-02-14, T-02-15, T-02-16, T-02-17, T-02-18 | `contents: read`; 5 SHA-pinned actions; no secrets; no force push, no tags | CI | `gh run list` -> run for HEAD `success` with 4 successful jobs and artifacts `parity-linux-x64,parity-macos-arm64,parity-windows-x64`; HEAD == `origin/feature/cross-platform`; gate: `gh run watch RUN --exit-status --interval 30` (external, 10-20 min cold) | ✅ (`ci.yml` exists; replaced) | ⬜ pending |
| 02-04-T2 | 02-04 | 4 | D-18, D-19 (IN-03), close-out (QA-03/QA-04 final verdict) | T-02-17, T-02-20 | `-Exe` validated before any registry access | static + CI | PowerShell AST parse `parse ok`, D-18 / IN-03 greps, research-ref removed and unreferenced; gate: `zig build smoke` without imgui.ini, bad `-Exe` run, final `gh run watch` for HEAD | ✅ | ⬜ pending |

**Requirement cross-index:** CORE-04: 02-01-T1, 02-01-T2, 02-02-T2, 02-03-T1 · CORE-05: 02-01-T1, 02-01-T2, 02-03-T2 · CORE-06: 02-01-T2 · CORE-07: 02-02-T2, 02-04-T1 · QA-02: 02-01-T2, 02-02-T2 (+ existing `test_seeds.cpp` for `sign(0)==+1`) · QA-03: 02-01-T2, 02-02-T1, 02-02-T2, 02-04-T1 · QA-04: 02-03-T1, 02-03-T2, 02-04-T1, 02-04-T2 · D-14..D-18: 02-01-T2 (D-15), 02-02-T1 (D-14, D-17), 02-02-T2 (D-16), 02-04-T2 (D-18)

**Sampling continuity:** wave 1: 2/2 tasks automated · wave 2: 2/2 tasks automated · wave 3: 2/2 tasks automated · wave 4: 2/2 tasks automated (CI verdicts read through `gh`; no checkpoint tasks in this phase)

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
