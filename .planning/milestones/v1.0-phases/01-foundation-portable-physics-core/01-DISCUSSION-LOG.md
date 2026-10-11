# Phase 1: Foundation & Portable Physics Core - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-10
**Phase:** 1-Foundation & Portable Physics Core
**Mode:** --auto (recommended option auto-selected for each question)
**Areas discussed:** Source layout, Numerics policy, Presets, Build & deps, CI gate, Git hygiene, MFC baseline

---

## Source layout

| Option | Description | Selected |
|--------|-------------|----------|
| New `src/` tree at root, MFC untouched until Phase 4 | Keeps MFC buildable for baseline/comparison | ✓ |
| Refactor MFC files in place | Breaks the MFC build early | |

[auto] Source layout — Q: "Where does the new core live?" → Selected: "New src/ tree, MFC untouched" (recommended default)

---

## Numerics policy

| Option | Description | Selected |
|--------|-------------|----------|
| Tolerance/invariant tests, `-ffp-contract=off`, strict warnings | Robust across compilers/arches | ✓ |
| Bit-exact golden outputs | Breaks across compilers/arches | |

[auto] Numerics — Q: "How strictly to test numerics?" → Selected: "Tolerance/invariant tests" (recommended default)

---

## Presets

| Option | Description | Selected |
|--------|-------------|----------|
| Data table copied exactly from ConfigDocs.cpp + `.rc` names | Parity, testable | ✓ |
| Keep per-preset classes | MFC-shaped, no value | |

[auto] Presets → Selected: "Data table" (recommended default)

---

## Build & deps

| Option | Description | Selected |
|--------|-------------|----------|
| Zig 0.17 Module API, hash-pinned castholm/SDL, ImGui docking, doctest | Verified by Windows spike | ✓ |
| CMake + Qt 6 | Documented fallback only | |

[auto] Build → Selected: "Zig 0.17 per STACK.md" (recommended default)

---

## CI gate

| Option | Description | Selected |
|--------|-------------|----------|
| GitHub Actions 3-OS matrix, smoke run with SDL dummy driver; ask before first push | Real gate | ✓ |
| Local-only builds | Cannot verify Linux/macOS | |

[auto] CI → Selected: "GitHub Actions matrix" (recommended default)

---

## Git hygiene

| Option | Description | Selected |
|--------|-------------|----------|
| Untrack artifacts in a dedicated commit, unanchored `.gitignore` | Clean, non-destructive | ✓ |
| Rewrite history to purge binaries | Destructive, unnecessary | |

[auto] Git hygiene → Selected: "Untrack in dedicated commit" (recommended default)

---

## MFC baseline

| Option | Description | Selected |
|--------|-------------|----------|
| Automated capture of 9 presets at 1024×768 into `docs/baseline/mfc/` | Reproducible parity oracle | ✓ |
| Skip baseline | Loses the parity oracle | |

[auto] Baseline → Selected: "Automated capture (human-assisted fallback)" (recommended default)

---

## Amendments

- **2026-10-10, D-10 (smoke frames):** "render one ImGui frame" → "render at least one complete ImGui frame (two allowed)". Raised by plan-checker iteration 2 (plan 01-01 renders two frames so the first frame's font-atlas upload is exercised). D-10 was an auto-selected default, so the auto-mode orchestrator amended it; intent unchanged.

---

## Claude's Discretion

- Core file split and class names, test organisation, CI caching details, `Vector2D` templating.

## Deferred Ideas

- None raised.
