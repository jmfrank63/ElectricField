# 01-01 Summary: User configuration model + Workspace entries

**Status:** Complete
**Date:** 2026-10-11
**Requirements:** CFG-01, CFG-02

## What was built

- `src/app/configs.{hpp,cpp}` (efapp, std-only, strict flags): `UserConfig` (name + `ef::PresetCharge`
  charges), `serializeConfigs`/`parseConfigs` for the `configurations.ini` format
  (`[configuration "Name"]` + `charge = value, x, y`, x/y at %.10g), skip-and-count parser rules
  (no-name/empty sections, invalid charge lines with non-finite coordinates, foreign sections ignored,
  duplicate names keep the first), `loadConfigs`/`saveConfigs` over `std::filesystem::path`, the
  `upsertUserConfig` Save As decision core (EMPTY_NAME / RESERVED_NAME case-insensitive / in-place
  overwrite / append) and `findUserConfig`.
- `Workspace::openUser(name, charges)` (title "<name> <N>", per-name counter never reused after close,
  `Tab::preset = kNoPreset`) and `Workspace::openEmpty()` ("Empty Configuration <N>", one +1 charge at
  the origin), both through a shared `spawn` tail that keeps the preset path byte-identical.

## Verification

- `zig build test -Dgui=false`: 95/95 cases (86 + 9 new: 6 config cases, 3 workspace cases incl. a real
  compute through the session path reaching the 16-line dipole count).
- Presets suite untouched and green.

## Notes

- Coordinates round-trip at %.10g - far below any meaningful world-unit precision.
- Names compare case-insensitively for dedupe and reservation (documented in the header).
