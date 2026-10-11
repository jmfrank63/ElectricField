---
phase: 1
slug: foundation-portable-physics-core
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-10-10
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Source: `01-RESEARCH.md` § Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest 2.5.3 (single header, hash-pinned via `build.zig.zon`) |
| **Config file** | none — `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in `tests/test_main.cpp`; wiring in `build.zig` (both created by 01-01-T1) |
| **Quick run command** | `zig build test -Dgui=false` |
| **Full suite command** | `zig build test -Dgui=false && zig build test -Dgui=false -Doptimize=ReleaseFast && zig build && zig build test && zig build smoke` (= CI job body) |
| **Estimated runtime** | ~4 s quick (warm cache); ~60–120 s full on Windows (warm); cold first build several minutes |

---

## Sampling Rate

- **After every task commit:** Run the task's `<automated>` fast path (static/git checks, plus `zig build test -Dgui=false` where the task touches code; warm cache, target under 30 s)
- **Before a task or plan is marked done:** Run the task's `<plan_gate>` (cold-capable builds, MSBuild, captures, or the CI run; minutes on a cold cache)
- **After every plan wave:** Run the full suite command on Windows (plus optional WSL Ubuntu run per RESEARCH.md recipe). The wave 1 gate is the 01-01-T3 plan gate; the wave 3 local gate is the 01-04-T1 plan gate
- **Before `/gsd-verify-work`:** Full suite green locally AND CI green on all three jobs (windows, ubuntu, macos-arm64) for the pushed head commit
- **Max feedback latency:** 30 seconds for per-task fast paths (warm); 120 seconds for the warm full suite; cold plan gates take minutes (first build fetches about 430 MB and builds libc++); CI gate latency is external (~10–20 min cold)

---

## Per-Task Verification Map

Task IDs are `<plan>-T<n>` and match the task order in each PLAN.md. "Automated Command" mirrors the task's `<verify>`/`<automated>` fast path (condensed; the PLAN.md holds the exact command). "gate:" names the task's `<plan_gate>` where one exists. Threat IDs refer to the STRIDE registers in the PLAN.md files.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-T1 (tracer) | 01-01 | 1 | BUILD-01, BUILD-02, BUILD-03, CORE-01, QA-01 (analytic field), D-07 (table) | T-01-01, T-01-02, T-01-07, T-01-SC | deps fetched only by url + content hash; SDL pinned to a commit; no local paths | static + unit | `grep -c '\.hash = ' build.zig.zon` == 3, `-ffp-contract=off` in build.zig, `sign` definition, CORE-01 purity grep empty, QA-01 analytic `TEST_CASE` names present in `tests/test_field.cpp` (`grep -qF`), `zig build test -Dgui=false --summary all`; gate: `zig build test -Dgui=false && zig build && zig build smoke` (log has `presets=9`, `smoke ok frames=2`: two complete frames, within amended D-10 "at least one"); GUI-subsystem PE check via `od` (no Python) | ❌ W0 (`build.zig`, `build.zig.zon`, `src/core/ef/*`, `tests/test_main.cpp`, `tests/test_field.cpp`, `src/gui/main.cpp`: created by this task) | ⬜ pending |
| 01-01-T2 | 01-01 | 1 | BUILD-05 | T-01-06 | no MSVC artifacts (local paths in .pdb/.ilk/.tlog) tracked in the public repo | shell (git) | `git ls-files` artifact grep empty, `git ls-files -ci --exclude-standard` empty, `git check-ignore -q` for Zig and MSVC outputs; gate: `zig build` then `git status --porcelain --untracked-files=all -- . ':(exclude).planning'` empty | ✅ (git); `.gitignore` rewritten by this task | ⬜ pending |
| 01-01-T3 | 01-01 | 1 | BUILD-04 (workflow definition) | T-01-03, T-01-04, T-01-05 | setup-zig pinned to a 40-char SHA; `permissions: contents: read`; no secrets; no pull_request_target | static (grep/awk) | Git Bash coreutils structure check on a CR-stripped copy (no tabs, 5 top-level keys, triggers, `contents: read`, fail-fast false, matrix order, 2 `uses` with 40-hex setup-zig SHA, 6 single-line runs with the 5 zig build runs in CI order, comment-stripped negatives) prints `ci.yml OK`; no Python/PyYAML; gate: the 5-command CI job body (wave 1 gate) | ❌ W0 (`.github/workflows/ci.yml`: created by this task) | ⬜ pending |
| 01-02-T1 (tracer) | 01-02 | 2 | QA-05 | T-01-08, T-01-09, T-01-10 | options key backed up outside the repo and restored in `finally`; only the script's own PID is stopped | file + git | `magick identify` dipole.png == `1024 x 768`, no `.reg` in docs/, scoped `git status` empty; gate: one `powershell.exe -Command` runs MSBuild Release\|x64 into `$env:TEMP`, a dipole re-capture into `$env:TEMP`, and `magick compare -metric AE -fuzz 10%` against that same `$env:TEMP` path (single temp root; no bash `TMPDIR`) | ❌ W0 (`tools/baseline/capture-mfc-baseline.ps1`: created by this task) | ⬜ pending |
| 01-02-T2 | 01-02 | 2 | QA-05 | T-01-09, T-01-11 | no `.reg` file committed; README has no user-profile paths | file + git | 9 PNGs each `1024 x 768`, README provenance strings, static grep of `capture-mfc-baseline.ps1` for the 9 chooser texts (incl. preset-8 `Oposed`) and 9 slugs in MFC order plus the throw-on-mismatch guard, README has 3 `- <slug>: AE fuzz 10% = 0; AE raw = <n>` lines, scoped `git status` empty; gate: one `powershell.exe -Command` re-captures presets `"4,5,6"` into `$env:TEMP` and runs `magick compare -metric AE -fuzz 10%` against the same `$env:TEMP` paths | ❌ (PNGs and README: created by this task) | ⬜ pending |
| 01-03-T1 (tracer) | 01-03 | 2 | CORE-02, CORE-03, CORE-01 | T-01-13, T-01-14 | N/A | unit | `zig build test -Dgui=false --summary all` + build.zig source-list greps + empty-system test name grep | ❌ W0 (`tests/test_seeds.cpp`, `src/core/ef/seeds.*`, `tracer.hpp`: created by this task) | ⬜ pending |
| 01-03-T2 | 01-03 | 2 | QA-01, CORE-02 (method order), BUILD-02 | T-01-14, T-01-15 | test tolerances unchanged from REF | unit | `zig build test -Dgui=false --summary all && zig build test -Dgui=false -Doptimize=ReleaseFast --summary all` + source-list greps (warm after 01-01-T3) | ❌ W0 (`tests/test_vec2.cpp`, `tests/test_rk_order.cpp`: created by this task) | ⬜ pending |
| 01-03-T3 | 01-03 | 2 | D-07 (presets), CORE-01 | T-01-15, T-01-16 | preset oracle proven fail-first by a temporary mutation | unit + grep | Debug + ReleaseFast core suite, `git diff --quiet -- src/core/ef/presets.cpp`, `test_presets.cpp` listed in build.zig; purity greps in acceptance | ❌ W0 (`tests/test_presets.cpp`: created by this task) | ⬜ pending |
| 01-04-T1 | 01-04 | 3 | BUILD-01, BUILD-02, BUILD-03, BUILD-05 | T-01-17 | no credentials and no `.reg/.pfx/.pem/.env` files in the publish range | shell + unit | research-ref removed and unreferenced, MFC tree unchanged since `fea046f`, no sensitive files in `origin/master..HEAD`, `zig build test -Dgui=false --summary all`; gate: 5-command CI job body + default-dir MSBuild Release\|x64 + `zig build`, `git status` empty after each (wave 3 local gate) | ✅ | ⬜ pending |
| 01-04-T2 (checkpoint:decision, blocking-human) | 01-04 | 3 | BUILD-04 (D-12 consent) | T-01-17, T-01-18 | publication only after explicit human consent; never auto-approved | manual | none (human decision; see Manual-Only Verifications) | n/a | ⬜ pending |
| 01-04-T3 | 01-04 | 3 | BUILD-04, BUILD-01 (Linux and macOS arm64) | T-01-19, T-01-20, T-01-21 | gate not weakened; `contents: read`; no force push, no tags | CI | `gh run list` finds the run for HEAD with conclusion `success` and 3 successful jobs; HEAD == `origin/feature/cross-platform`; gate: `gh run watch RUN --exit-status --interval 30` (external, ~10–20 min cold) | ✅ (`ci.yml` from 01-01-T3) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

**Requirement cross-index:** BUILD-01: 01-01-T1, 01-04-T1, 01-04-T3 · BUILD-02: 01-01-T1, 01-03-T2, 01-04-T1 · BUILD-03: 01-01-T1, 01-04-T1 · BUILD-04: 01-01-T3, 01-04-T2, 01-04-T3 · BUILD-05: 01-01-T2, 01-04-T1 · CORE-01: 01-01-T1, 01-03-T1, 01-03-T3 · CORE-02: 01-03-T1, 01-03-T2 · CORE-03: 01-03-T1 · QA-01: 01-01-T1, 01-03-T2 · QA-05: 01-02-T1, 01-02-T2 · D-07: 01-01-T1, 01-03-T3

**Sampling continuity:** wave 1: 3/3 tasks automated · wave 2: 5/5 tasks automated (01-02 and 01-03 run in parallel) · wave 3: 2/2 implementation tasks automated (01-04-T2 is a checkpoint)

---

## Wave 0 Requirements

There is no separate Wave 0 plan. Plan 01-01 (wave 1, the walking skeleton) creates the test infrastructure before any task that depends on it, and waves 2-3 start only after it. Each entry names the task that creates the file.

- [ ] `build.zig`, `build.zig.zon` (deps: imgui v1.92.9b-docking, castholm/SDL release-3.4.x pinned commit, doctest 2.5.3). Reference: `research-ref/`. Created by **01-01-T1**; the `core_sources`/`test_sources` lists are completed by 01-03-T1, T2 and T3
- [ ] `.gitignore` rewrite (reference: `research-ref/gitignore.proposed`). Created by **01-01-T2**
- [ ] `.github/workflows/ci.yml` (reference: `research-ref/ci.yml`). Created by **01-01-T3**
- [ ] `src/core/ef/{math,vec2,rk,method,params,charge_system,presets}.hpp` and `src/core/ef/presets.cpp`. Created by **01-01-T1**
- [ ] `src/core/ef/seeds.hpp`, `src/core/ef/seeds.cpp`, `src/core/ef/tracer.hpp`. Created by **01-03-T1**
- [ ] `src/gui/main.cpp` (hello window with `--smoke`). Created by **01-01-T1**
- [ ] `tests/test_main.cpp`, `tests/test_field.cpp`. Created by **01-01-T1**
- [ ] `tests/test_seeds.cpp`. Created by **01-03-T1**
- [ ] `tests/test_vec2.cpp`, `tests/test_rk_order.cpp`. Created by **01-03-T2**
- [ ] `tests/test_presets.cpp`. Created by **01-03-T3**
- [ ] `tools/baseline/capture-mfc-baseline.ps1` (reference: `research-ref/tools/baseline/`). Created by **01-02-T1**
- [ ] Framework install: none. doctest arrives through `build.zig.zon` (**01-01-T1**)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| First push of `feature/cross-platform` to origin | BUILD-04 | Outward-facing, publishes commits to a public repo (D-12) | Executor asks the user; on approval `git push -u origin feature/cross-platform`, then watch CI |
| Visual sanity of 9 MFC baseline PNGs | QA-05 | Automation guarantees capture, not correctness | Open each PNG: charges placed/coloured correctly, field lines complete, equipotentials grey |
| macOS run of hello window | BUILD-01 | No local Mac | CI `macos-latest` job log is the evidence (`zig build smoke` exit 0) |
| Interactive window on Windows | BUILD-01 | Real window, not `--smoke` | `zig build run` shows an ImGui window; close it |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 120s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
