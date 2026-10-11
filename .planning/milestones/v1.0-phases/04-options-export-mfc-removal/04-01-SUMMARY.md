# 04-01 Summary: Options model, persistence and the Options dialog

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** OPT-01, OPT-02, OPT-03, OPT-04

## What was built

- `src/app/options.{hpp,cpp}` (efapp, std-only, strict flags): `Options` (ComputeParams + DrawStyle),
  `validate` with the MFC ranges (lines 4-128, threads 1-128, interval 0.01-0.50, BOTH thicknesses 1-64 —
  fixing the MFC bug that never validated the equipotential thickness), `serialize`/`parse` for the
  portable ini (`#RRGGBB` colours, method by stable kMethods name, interval at 3 decimals), per-key
  fallback to defaults with rejected-key reporting, `loadOptions`/`saveOptions` over `std::filesystem::path`
  (non-ASCII paths survive on Windows), and the dialog edit state `OptionsEdit` (reset / validate / apply).
- `tests/test_options.cpp`: 8 test cases — range edges, round-trip, comments/unknown keys, per-key
  fallback (never clamped), format acceptance, file round-trip + missing-file defaults, edit-state rules.
- GUI: View ▸ Options… opens a tabbed modal (Computation: 11-method combo, equipotentials checkbox,
  lines/threads edits, interval slider 0.01-0.50 shown %.2f; Draw: 4 colour pickers, 2 thickness edits).
  OK validates (error stays in-dialog), Cancel/Esc discards. Accepting applies: the global DrawStyle is
  replaced (every open tab re-renders next frame), Workspace::setDefaults feeds new tabs only, and the
  file is saved immediately (MFC parity: saved on Apply).
- App: options load from SDL_GetPrefPath("ElectricField","ElectricField")/options.ini at startup;
  headless modes (smoke/screenshot) skip load AND save so CI never touches the developer's config.
- smoke extras gained `options=ok`: dialog opens, Esc closes, an accepted apply reaches the app
  (thickness + new-tab params), defaults restored afterwards.

## Verification

- `zig build test`: 86 cases / 213111 assertions pass (was 78), including `-Dgui=false` (no SDL/ImGui).
- `zig build smoke`: canvas checks ok at 3 sizes, extras `options=ok`, idle loop 2 frames/1.3 s.
- Core, parity oracle and baselines untouched (src/core, tools/parity, docs/baseline byte-identical).

## Notes / carry-over

- ui_logic gained `scaledStyle(base, k)`; `checkCanvas` compares against the user's style so the
  GUI-vs-headless guarantee survives option changes.
- The interval slider gives 3-decimal granularity (MFC slider range 10-500); display stays %.2f.
