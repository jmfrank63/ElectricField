---
phase: 3
slug: gui-shell
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-10-11
---

# Phase 3 — Validation Strategy

> Source: `03-RESEARCH.md` § Validation Architecture (requirement → test map, sampling, manual-only items). Task IDs are filled in by the planner.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | doctest 2.5.3 + the app's headless `--smoke` harness |
| **Config file** | none — `build.zig` steps `test`, `test-render`, `smoke` |
| **Quick run command** | `zig build test -Dgui=false` |
| **Full suite command** | `zig build test && zig build test-render && zig build smoke -Doptimize=ReleaseFast` (+ Phase 2 parity: `ef-cli --all --out-dir parity-out --expect tools/parity/expected-counts.csv`) |
| **Estimated runtime** | quick: seconds (cached); full: ~1–2 min warm |

## Sampling Rate

- **After every task commit:** quick run; plus `zig build smoke` for any `src/gui`, `src/render`, `build.zig` change
- **After every plan wave:** full suite (Debug and ReleaseFast core tests as CI does)
- **Before `/gsd-verify-work`:** CI green on windows-x64, linux-x64, macos-arm64, tsan-linux (incl. parity), then manual-only items
- **Max feedback latency:** 60 s (quick path)

## Per-Task Verification Map

Task IDs are `<plan>-T<n>` and match the task order in each PLAN.md. "Automated Command" condenses the task's `<verify>`/`<automated>` fast path (the PLAN.md holds the exact command); "gate:" names the task's `<plan_gate>`. Threat IDs refer to the STRIDE registers in the PLAN.md files.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 03-01-T1 (tracer) | 03-01 | 1 | VIEW-01, VIEW-02, VIEW-03, VIEW-04 (D-09, D-10, D-11, D-17) | T-03-01, T-03-02, T-03-03 | offscreen render never touches the caller's ImGui context; IM_ASSERT non-interactive in every build mode; core/oracle/baselines unchanged | unit + integration | `zig build test -Dgui=false` (core 60 + app 9: titles/counters, cached scene, closeAll < 1000 ms, glyph, label id, overlay clamp, title/version, scale metrics, canvas view) + `zig build test-render` (4 context-rule cases) + unchanged-core guard; gate: ReleaseFast suites, `zig build test`, hello smoke, ef-cli parity oracle; context-rule mutant must fail | ❌ W0 (`src/app/*`, `tests/test_app.cpp`, `tests_gui/*`, `src/imgui_config/ef_imconfig.h`: created by this task) | ⬜ pending |
| 03-01-T2 | 03-01 | 1 | VIEW-02, VIEW-04 (D-10, D-11, D-12) | T-03-03, T-03-04 | every session branch ends in a defined tab state; cancel before destroy; no race | unit + TSan | `zig build test -Dgui=false` + counts of `frame pacer:` (>= 2) and `workspace:` (>= 6) cases + `git diff --quiet -- src/app`; gate: ReleaseFast suite + WSL `zig build test -Dgui=false -Dsanitize=thread -Doptimize=ReleaseSafe` on `git archive HEAD`; two mutants (pacer settle, closeAll cancel-first) must fail | ✅ (`tests/test_app.cpp` from T1; extended) | ⬜ pending |
| 03-02-T1 (tracer) | 03-02 | 2 | VIEW-01..VIEW-05 (D-01..D-08, D-11, D-14, D-15, D-16) | T-03-06, T-03-07, T-03-08, T-03-09, T-03-10, T-03-11, T-03-13 | fixed URLs only (1 SDL_OpenURL, 3 https literals); clipboard gets only that URL; no imgui.ini; no dialog in headless modes | smoke + build + real window | `zig build -Doptimize=ReleaseFast` + `zig build smoke -Doptimize=ReleaseFast` (`smoke ok frames=<N> tabs_opened=2 lines=16 closed=2`, 3 x `canvas check ... ok`, `keys=ok middle_click=ok failed_tab=ok overlay=ok`) + no imgui.ini + PE Subsystem == 2; gate: `zig build test`, parity oracle, `--screenshot` line, fatal start (`SDL_VIDEODRIVER=nosuch` -> exit 1); gate (PowerShell): title `Electric Field`, idle CPU < 2 % | ❌ W0 (`src/gui/{app,ui,icon}.*`, `assets/*`: created by this task) | ⬜ pending |
| 03-02-T2 | 03-02 | 2 | VIEW-05 (D-13), VIEW-01..VIEW-04 on Linux (Open Question 5) | T-03-10, T-03-12 | build never fails for lack of git; hash is display only | smoke + Linux pre-flight | after the commit, smoke prints `about: Version 0.1.0 (<git rev-parse --short=7 HEAD>)` (fresh hash without clearing caches) + `has_side_effects`/`runFallible`/`captureStdOut` greps; gate: WSL Linux from `git archive` (no git): test-render, full smoke, `about: Version 0.1.0 (unknown)`, fatal start exit 1 | ✅ (`build.zig`, `src/gui/app.cpp` from T1; modified) | ⬜ pending |
| 03-03-T1 (tracer) | 03-03 | 3 | VIEW-01..VIEW-05 on windows/linux/macos-arm64 (D-16, D-17) | T-03-14, T-03-15, T-03-16, T-03-17, T-03-18, T-03-19 | `contents: read`; 6 SHA-pinned actions; no secrets; no force push, no tags | CI | `gh run list` -> run for HEAD `success` with 4 successful jobs and artifacts `gui-*` + `parity-*` for 3 OSes; HEAD == `origin/feature/cross-platform`; gate: `gh run watch RUN --exit-status --interval 30` (external, 10-20 min cold) | ✅ (`ci.yml` exists; extended) | ⬜ pending |
| 03-03-T2 | 03-03 | 3 | close-out (final verdict for VIEW-01..VIEW-05) | T-03-18, T-03-19 | research-ref removed, nothing outside .planning references it | static + CI | research-ref gone and unreferenced, core/oracle/baselines/MFC unchanged since 8f8935a, no probes/ or shots2/ in the tree; gate: final `gh run watch` for HEAD + six artifacts | ✅ | ⬜ pending |

**Requirement cross-index:** VIEW-01: 03-01-T1, 03-02-T1, 03-02-T2 (Linux), 03-03-T1 · VIEW-02: 03-01-T1, 03-01-T2, 03-02-T1, 03-03-T1 · VIEW-03: 03-01-T1, 03-02-T1, 03-02-T2 (Linux), 03-03-T1 · VIEW-04: 03-01-T1, 03-01-T2, 03-02-T1, 03-03-T1 · VIEW-05: 03-02-T1, 03-02-T2, 03-03-T1 · D-01..D-17: D-09/D-10/D-11/D-17 in 03-01, D-01..D-08/D-11/D-13..D-16 in 03-02, D-16/D-17 in 03-03, D-12 in 03-01-T2 and 03-02-T1

**Sampling continuity:** wave 1: 2/2 tasks automated · wave 2: 2/2 tasks automated · wave 3: 2/2 tasks automated (CI verdicts read through `gh`; no checkpoint tasks; human checks are end-of-phase per `human_verify_mode`)

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

## Wave 0 Requirements

- [ ] Copy `src/app/*` pure logic, `tests/test_app.cpp`, `tests_gui/*`, `src/imgui_config/ef_imconfig.h`, `src/render/png_export.*` from `research-ref/` (per RESEARCH delta list) — 03-01-T1
- [ ] Copy GUI modules `src/gui/*` and `assets/*` from `research-ref/` — 03-02-T1
- [ ] `FramePacer` unit tests and `Workspace` scripted-session tests (RESEARCH Wave 0 gaps) — 03-01-T2
- [ ] PE subsystem == 2: local gate in 03-02-T1, CI step (Windows) in 03-03-T1

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real-window interactive check on Windows | VIEW-01/02/04/05 | needs real window + pointer | double-click exe (no console), chooser at startup, File > New > each preset, several tabs, close a computing tab, middle-click, Ctrl+N/W/Q, wait cursor over busy canvas, About links open browser, title, resize |
| Real HiDPI display | VIEW-03 | no HiDPI hardware in automation | 125/150/200 % Windows display; move window between monitors; sharp lines, same physical canvas size |
| macOS / Linux real window | VIEW-03/05 | CI is headless | Retina sharpness, Cmd shortcuts, X11/Wayland |
| Fatal-startup behaviour | VIEW-05 | message box not testable headlessly | `SDL_VIDEODRIVER=nosuch` → exit 1, text on stderr |

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
