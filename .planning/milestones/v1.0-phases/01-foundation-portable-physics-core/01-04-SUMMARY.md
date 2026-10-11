---
phase: 01-foundation-portable-physics-core
plan: 04
subsystem: infra
tags: [github-actions, ci, zig, sdl3, imgui, release-gate, d-12]
status: complete

requires:
  - phase: 01-01
    provides: "walking-skeleton build.zig, hello window, 3-OS ci.yml"
  - phase: 01-02
    provides: "MFC baseline images (QA-05)"
  - phase: 01-03
    provides: "seeds, tracer, integrator and preset oracles"
provides:
  - "research-ref scaffolding removed; nothing outside .planning referenced it"
  - "feature/cross-platform published to origin after explicit user consent (D-12)"
  - "Green 3-OS CI run for the pushed head d17dd2f: windows-x64, linux-x64, macos-arm64 (BUILD-04)"
  - "First real execution of the Zig + SDL3 + ImGui stack on macOS 26 arm64 and Ubuntu 24.04 (smoke prints presets=9)"
affects: [phase-2-physics-parity, phase-3-gui, phase-4-mfc-removal]

actuals:
  tokens: 16900
  tasks: 3
  commits: 1

tech-stack:
  added: []
  patterns: ["CI run as go/no-go gate keyed to the pushed head SHA", "fix at the source, never weaken the gate"]

key-files:
  created: []
  modified: []
  deleted:
    - ".planning/phases/01-foundation-portable-physics-core/research-ref/ (23 files, preserved at commit 3d6ac8e)"

key-decisions:
  - "D-12 consent recorded: user answered 'push' (no redaction); the push went out with the unmodified 26-commit history"
  - "D-12 go/no-go: GO. The Zig 0.17 + SDL3 + ImGui stack passes on all three OSes on the first CI run; the Qt 6 + CMake fallback is NOT triggered"

requirements-completed: [BUILD-01, BUILD-02, BUILD-03, BUILD-04, BUILD-05]

coverage:
  - id: D1
    description: "3-OS GitHub Actions run green for the pushed head commit"
    requirement: "BUILD-04"
    verification:
      - kind: integration
        ref: "gh run view 38040977802 (headSha d17dd2f, conclusion success, 3/3 jobs success)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Hello window starts headlessly on Linux and macOS arm64 and prints presets=9"
    requirement: "BUILD-01"
    verification:
      - kind: integration
        ref: "zig build smoke in CI jobs 114180991517 (linux) and 114180991385 (macOS)"
        status: pass
    human_judgment: false
  - id: D3
    description: "git status clean after MSBuild Release|x64 and after zig build; MFC sources untouched since fea046f"
    requirement: "BUILD-05"
    verification:
      - kind: other
        ref: "Task 1 local plan_gate; git diff --name-only fea046f HEAD -- ElectricField/ filtered prints nothing"
        status: pass
    human_judgment: false

duration: ~60min (of which ~45 min waiting for a queued macOS runner)
completed: 2026-10-10
---

# Phase 1 Plan 04: Phase Close-out and 3-OS CI Gate Summary

**Phase 1 go/no-go gate passed: the pushed head d17dd2f built, tested (Debug+UBSan and ReleaseFast core suites) and ran the headless SDL3+ImGui smoke start-up green on windows-x64, linux-x64 and macos-arm64 on the first CI run, with Zig 0.17.0 installed by mlugg/setup-zig on an empty cache.**

## Performance

- **Duration:** ~60 min wall clock; the macOS job sat queued for ~45 min waiting for a runner and then ran 4.5 min
- **Tasks:** 3 (Task 1 auto, Task 2 blocking-human decision, Task 3 auto)
- **Fix iterations needed:** 0 (no `fix(01-04)` commits; workflow, build.zig and src/ unchanged by this plan)

## Accomplishments

- **Task 1 (commit d17dd2f):** removed `research-ref/` (24 files changed, 1587 deletions); `git grep research-ref -- ':!.planning'` is empty. The reference content stays in git history (3d6ac8e) and in the repo code.
- **Pre-publication scan (clean):** 0 credential-pattern hits in the diff (GitHub tokens, PEM private keys, AWS AKIA, Slack xox); 0 `.reg/.pfx/.pem/.env` files in `origin/master..HEAD`.
- **Local phase gate (Windows):** full CI job body green, MSBuild Release|x64 with default directories and `zig build` each left `git status --porcelain --untracked-files=all -- . ':(exclude).planning'` empty (BUILD-05); MFC solution still builds (D-01).
- **D-01/D-02:** `git diff --name-only fea046f HEAD -- ElectricField/` filtered of `x64/` and the vcxproj entries prints nothing.
- **Task 2 (D-12 consent):** user answered **push**, explicitly choosing not to redact anything. Recorded here as the approval (T-01-17, T-01-18). This was a genuine user message, not an auto-approval.
- **Task 3:** `git push -u origin feature/cross-platform` (no force, no tags; remote had 0 branches of that name and 0 tags before, 0 tags after). CI run 38040977802 started for head `d17dd2f6d88f19e6c8a7b5f2f5d4f724eea7bb9e`, equal to `git rev-parse HEAD`.

## Published Commit List (first push, origin/master..HEAD, 26 commits)

First pushed SHA: `d17dd2f6d88f19e6c8a7b5f2f5d4f724eea7bb9e` (branch tip). Contents:

```
d17dd2f chore(01-04): remove research reference implementation (superseded by repo code)
132bc9e docs(01-03): complete seeds, tracer and integrator/preset oracles plan
7041336 test(01-03): preset table cross-checked against the MFC source (D-07)
e820172 test(01-03): integrator convergence-order and vector-math oracles (QA-01)
92ae444 feat(01-03): seed planning + single-line tracer, every method produces field lines (CORE-02, CORE-03)
c86d502 docs(01-02): complete MFC baseline capture plan
26eea3f docs(01-02): MFC baseline images of all 9 presets (QA-05)
09b7597 feat(01-02): MFC baseline capture script + dipole reference image (QA-05 tracer)
f4eeeaa docs(01-01): complete walking skeleton plan
a0ea3bf docs(01-01): add plan summary
ec46365 ci(01-01): 3-OS GitHub Actions gate (windows, ubuntu, macos-arm64)
d4ba5b6 chore(01-01): untrack MSVC build outputs and ignore MSVC + Zig artifacts
e21ef49 chore(01-01): switch MFC project to the v145 toolset (VS 18) for a reproducible baseline build
ae1033e feat(01-01): walking skeleton - zig build, std-only efcore, doctest, SDL3+ImGui hello window
e53c91c chore(01): run phase sequentially (use_worktrees=false) and set zig build/test gate commands
7c0d0a4 docs(01): create phase plan
fea046f docs(phase-1): add validation strategy
3d6ac8e docs(01): research phase 1 (foundation & portable physics core)
373fe02 docs(state): record phase 1 context session
b62dd1e docs(01): capture phase context
0d757c0 docs: create roadmap (4 phases)
6030267 docs: define v1 requirements
b22f215 docs: complete project research
df8409c docs: complete project research
b433891 docs: initialize project
6ca7807 chore: add project config
```

(Hashes copied from the git log at push time; see `git log --oneline origin/master..d17dd2f` for the authoritative list.) Publicly visible as a result: all `.planning/` documents, the 9 baseline PNGs, and two files containing the local path `C:\Users\jmfrank` (`.claude/CLAUDE.md` and `.planning/research/STACK.md`, both a spike directory reference). The user accepted this when choosing "push".

## CI Verdict (BUILD-04, D-11)

Run: https://github.com/jmfrank63/ElectricField/actions/runs/38040977802 (workflow CI, branch feature/cross-platform, headSha `d17dd2f6d88f19e6c8a7b5f2f5d4f724eea7bb9e`, conclusion **success**).

| Job | Job ID | Conclusion | Runtime | `zig version` | smoke output |
|-----|--------|------------|---------|----------------|--------------|
| windows-x64 | 114180991477 | success | 7m42s | 0.17.0 | `video=dummy renderer=software presets=9` / `smoke ok frames=2` |
| linux-x64 | 114180991517 | success | 5m11s | 0.17.0 | `video=dummy renderer=software presets=9` / `smoke ok frames=2` |
| macos-arm64 | 114180991385 | success | 4m34s (after ~45 min queue) | 0.17.0 | `video=dummy renderer=software presets=9` / `smoke ok frames=2` |

Every job: Core tests (Debug+UBSan) 5/5, Core tests (ReleaseFast) 5/5, Build everything 12/12, Tests (default) 5/5, smoke 11/11 steps succeeded, with a Zig cache miss (empty cache), so all three pinned dependencies were fetched cold with only Zig installed.

Acceptance checks:
- `git rev-parse HEAD` equals `git rev-parse origin/feature/cross-platform` after `git fetch` (both d17dd2f).
- `grep -c continue-on-error .github/workflows/ci.yml` = 0; matrix has exactly three entries; `"-Werror"` and `"-ffp-contract=off"` each appear once in build.zig. The gate was not touched.
- `git ls-remote --tags origin | wc -l` = 0 before and after the push.
- Research assumptions resolved: A1 (stack builds, links and runs on macOS 26 arm64) true; A2 (setup-zig v2.2.1 accepts 0.17.0) true; A3 (strict warnings stay clean on arm64) true; A4 (dummy-driver window + software renderer on macOS runners) true; A5 (cold runs fit 30 min) true.

## Deviations from Plan

None - plan executed exactly as written. No fix iterations were required. The one operational note is not a deviation: `macos-latest` (macOS 26 arm64) job queued about 45 minutes for runner capacity (queue time does not count against `timeout-minutes`); the pre-approved `macos-15` switch was deliberately not used because the image did not misbehave.

## Annotations (informational, not failures)

- mlugg/setup-zig is pinned to a Node 20 action that GitHub now forces to run on Node 24 (deprecation warning only).
- `ubuntu-latest` migrates to Ubuntu 26 from 2026-10-19; the stack needs no apt packages, so no change is expected, but the next CI run after that date is worth watching.

## Known Stubs

None. This plan changed no source code.

## Threat Flags

None. No new network endpoints, auth paths or trust-boundary file access; the workflow still requests only `contents: read` with no secrets (T-01-19).

## Issues Encountered

- The background `gh run watch` process hit its tool time limit while the macOS job was still queued; the verdict was read directly from the GitHub API instead (run status `completed`, conclusion `success`).

## Next Phase Readiness

Phase 1 is complete from a build and CI standpoint: Phase 2 (physics parity: GUI-independent threading and defect fixes, including the open Dormand-Prince error-weight decision from 01-03) can proceed on a trusted three-OS foundation. The Phase 1 blocker "Linux and macOS execution is unverified" is resolved.

## Self-Check: PASSED

- research-ref directory absent; D-01/D-02 MFC diff empty.
- Commit d17dd2f exists and equals origin/feature/cross-platform at the time of the CI verdict.
- CI run 38040977802: headSha == d17dd2f, conclusion success, 3/3 jobs success.
