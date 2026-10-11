# 01-02 Summary: Chooser integration and Save As

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** CFG-02, CFG-03

## What was built

- The App loads `configurations.ini` from the per-user config directory at startup (next to
  options.ini); headless modes skip load and save entirely. The master list is mirrored into
  `ui_.userConfigs` each frame.
- The preset chooser now lists the 9 built-ins, a disabled "User configurations" separator (only when
  any exist), one row per user configuration in file order, and "Empty Configuration" last. Keyboard
  Up/Down/Enter/double-click/Esc behaviour is unchanged (the separator is not focusable).
- File > Save As... (Ctrl+S, enabled with an active tab) opens a modal prefilled with the tab title
  minus the counter suffix. Validation runs through the same `upsertUserConfig` decision core the App
  uses for the real write: an empty name and a built-in preset name (case-insensitive) show inline
  errors; an existing user configuration name asks "Overwrite?" before writing. Success upserts the
  master list and rewrites `configurations.ini`.
- Smoke gains `user_configs=ok`: an in-memory user configuration and the empty entry open and compute
  to completion headlessly (no file touched in CI).

## Verification

- `zig build test`: 95 + 4 cases green.
- `zig build smoke`: all checks green including `user_configs=ok`.
- One crash found and fixed during smoke: the chooser mis-classified the "Empty Configuration" row as a
  user row when no user configurations existed (negative index); row kinds are now computed with
  explicit bounds.

## Notes / carry-over

- Manual walkthrough (human UAT): save a preset under a new name, reopen it from the chooser, restart to
  confirm persistence; renaming/deleting configs from the UI is deferred (hand-edit the file).
