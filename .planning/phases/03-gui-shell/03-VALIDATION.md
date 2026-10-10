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

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | TBD | VIEW-01 | — | N/A | unit + smoke | `zig build test -Dgui=false` (tab title counters); `zig build smoke` (`keys=ok`) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VIEW-02 | — | no hang / no leaked session | unit + smoke | Workspace close/closeAll tests; smoke (`middle_click=ok`, `quit_ms`, opened == closed) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VIEW-03 | — | N/A | unit + smoke | `zig build test-render` (context rule); smoke canvas checks 640x480 / 1024x820 / 1280x1024; scale/metrics unit tests | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VIEW-04 | — | N/A | unit + smoke | busy glyph / stable id / overlay clamp tests; smoke (`overlay=ok failed_tab=ok idle_frames=`) | ❌ W0 | ⬜ pending |
| TBD | TBD | TBD | VIEW-05 | — | links open only fixed URLs | unit + smoke + CI | version/title tests; smoke renders About; PE subsystem == 2 check | ❌ W0 | ⬜ pending |

## Wave 0 Requirements

- [ ] Copy GUI modules, `src/app/*` pure logic, `tests/test_app.cpp`, `tests_gui/test_render_ctx.cpp`, `assets/*`, `src/imgui_config/ef_imconfig.h` from `research-ref/` (per RESEARCH delta list)
- [ ] `FramePacer` unit tests and `Workspace` fake-session tests (RESEARCH Wave 0 gaps)
- [ ] Optional CI step: PE subsystem == 2

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
