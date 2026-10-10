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
| **Config file** | none — `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in `tests/test_main.cpp`; wiring in `build.zig` (Wave 0 installs) |
| **Quick run command** | `zig build test -Dgui=false` |
| **Full suite command** | `zig build test -Dgui=false && zig build test -Dgui=false -Doptimize=ReleaseFast && zig build && zig build test && zig build smoke` (= CI job body) |
| **Estimated runtime** | ~4 s quick (warm cache); ~60–120 s full on Windows (warm); cold first build several minutes |

---

## Sampling Rate

- **After every task commit:** Run `zig build test -Dgui=false` (core/tests tasks) or `zig build` (build/GUI tasks); `git status --porcelain` for hygiene tasks
- **After every plan wave:** Run the full suite command on Windows (plus optional WSL Ubuntu run per RESEARCH.md recipe)
- **Before `/gsd-verify-work`:** Full suite green locally AND CI green on all three jobs (windows, ubuntu, macos-arm64) for the pushed head commit
- **Max feedback latency:** 120 seconds (local); CI gate latency is external (~10–20 min cold)

---

## Per-Task Verification Map

Task IDs are assigned by the planner; this map is keyed by requirement and must be mirrored in each PLAN.md task's `<verify>`/`<automated>` block.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | BUILD-01 | — | N/A | build | `zig build --summary all` | ❌ W0 (`build.zig`) | ⬜ pending |
| TBD | TBD | TBD | BUILD-02 | — | N/A | build/test | `zig build test --summary all` | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | BUILD-03 | T-1 supply chain | deps fetched only by hash-pinned URLs | static | `grep -c '\.hash = ' build.zig.zon` == 3 and no `.path =` under dependencies | ❌ W0 (`build.zig.zon`) | ⬜ pending |
| TBD | TBD | TBD | BUILD-04 | T-1 supply chain | third-party actions pinned (SHA/tag) | CI | `gh run list --branch feature/cross-platform --workflow ci.yml --limit 1 --json conclusion,headSha` → `success`; `gh run view <id> --json jobs` shows 3 successes | ❌ W0 (`.github/workflows/ci.yml`) | ⬜ pending |
| TBD | TBD | TBD | BUILD-05 | — | N/A | shell | `git ls-files \| grep -E "(^\|/)x64/\|(^\|/)\.vs/\|\.vcxproj\.user$"` empty; after `zig build` + MSBuild: `git status --porcelain` empty | ✅ (git) | ⬜ pending |
| TBD | TBD | TBD | CORE-01 | — | N/A | build + grep | `zig build test -Dgui=false`; `grep -rEn "afx\|theApp\|windows\.h\|SDL\|imgui\|CString\|COLORREF" src/core` empty | ❌ W0 (`src/core/ef/*`) | ⬜ pending |
| TBD | TBD | TBD | CORE-02 | — | N/A | unit | `zig build test -Dgui=false` (method table order, every method traces a dipole line) | ❌ W0 (`tests/test_rk_order.cpp`, `tests/test_seeds.cpp`) | ⬜ pending |
| TBD | TBD | TBD | CORE-03 | — | N/A | unit | `zig build test -Dgui=false` (seeds = \|q\|·L, sinks 1, `sign(0)==+1`, seed angles) | ❌ W0 (`tests/test_seeds.cpp`) | ⬜ pending |
| TBD | TBD | TBD | QA-01 | — | N/A | unit | `zig build test -Dgui=false` (`test_vec2.cpp`, `test_field.cpp`, `test_rk_order.cpp`) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | QA-05 | — | N/A | script + file check | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\baseline\capture-mfc-baseline.ps1 ...`; `ls docs/baseline/mfc/*.png \| wc -l` == 9; each 1024x768; README present | ❌ W0 (script) | ⬜ pending |
| TBD | TBD | TBD | D-07 (presets) | — | N/A | unit | `zig build test -Dgui=false` (`test_presets.cpp` parses `ElectricField/ConfigDocs.cpp`) | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `build.zig`, `build.zig.zon` (deps: imgui v1.92.9b-docking, castholm/SDL release-3.4.x pinned commit, doctest 2.5.3) — reference: `research-ref/`
- [ ] `.gitignore` rewrite (reference: `research-ref/gitignore.proposed`)
- [ ] `.github/workflows/ci.yml` (reference: `research-ref/ci.yml`)
- [ ] `src/core/ef/*`, `src/gui/main.cpp`, `tests/test_main.cpp` + test files (reference: `research-ref/`)
- [ ] `tools/baseline/capture-mfc-baseline.ps1` (reference: `research-ref/tools/baseline/`)
- [ ] Framework install: none — doctest arrives through `build.zig.zon`

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
