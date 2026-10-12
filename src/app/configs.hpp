#pragma once
// User charge configurations (v2 Phase 1, CFG-01..02): named charge sets persisted in a human-readable
// configurations.ini next to options.ini. Std-only, same strict flags as the core: unit-testable without
// SDL or ImGui; the GUI resolves the file location (SDL_GetPrefPath) and hands over a path.
#include <filesystem>
#include <string>
#include <vector>

#include "ef/presets.hpp"  // ef::PresetCharge: int value + x, y

namespace efapp {

struct UserConfig {
  std::string name;
  std::vector<ef::PresetCharge> charges;
};

// File format (D-01), mirroring the options.ini style:
//   # Electric Field user configurations
//   [configuration "My Dipole"]
//   charge = 1, -2, 0
//   charge = -1, 2, 0
// Coordinates are written with %.10g (round-trips through stod far below any meaningful world-unit
// precision). Double quotes inside names are stripped on save.
//
// Forward compatibility (v3+ plans 3D / magnetic fields / quantum behaviour): this format is intentionally
// minimal - no dead z, magnetic or quantum fields. Those features are structural changes to the math core
// (Vec3, Lorentz-force ODE, probabilistic engine), not extra columns. When one lands, this file gains a
// format marker and the charge line gains optional trailing tokens here - in this one parser - and
// `ef::PresetCharge` remains the single interchange type for the editor.
std::string serializeConfigs(const std::vector<UserConfig>& configs);

struct ParsedConfigs {
  std::vector<UserConfig> value;
  int skippedSections = 0;  // sections without a name or without a single valid charge line (D-02)
  int skippedCharges = 0;   // charge lines that failed to parse (also outside any section)
  int duplicateNames = 0;   // later duplicates ignored: the first occurrence wins (D-02)
};

// Parse rules (D-02): a `[configuration "Name"]` section needs a non-empty name and at least one valid
// charge line, else it is skipped; `charge = v, x, y` needs an integer and two finite decimals; order of
// the kept sections is preserved.
ParsedConfigs parseConfigs(const std::string& text);

bool saveConfigs(const std::filesystem::path& file, const std::vector<UserConfig>& configs, std::string& error);
std::vector<UserConfig> loadConfigs(const std::filesystem::path& file);

enum class UpsertResult { OK, EMPTY_NAME, RESERVED_NAME };

// Case-insensitive lookup by name; -1 when absent.
int findUserConfig(const std::vector<UserConfig>& configs, const std::string& name);

// Save As decision core (D-03/D-04): reserved names (the built-in preset display names) are rejected
// case-insensitively; an existing entry (also matched case-insensitively) is overwritten IN PLACE,
// preserving its position in the list; a new name is appended. The candidate name is trimmed.
UpsertResult upsertUserConfig(std::vector<UserConfig>& configs, const UserConfig& candidate,
                              const std::vector<std::string>& reservedNames);

enum class SaveIntoResult { OK, EMPTY_NAME, RESERVED_NAME, NAME_TAKEN };

// Save Configuration decision core (h22, v2.5 CFG-04): writes `candidate` - the active tab's charges
// named by the tab's CURRENT base title - into the collection on behalf of the tab's source entry
// `sourceName`. The tab's name is the user's configuration identity, so a renamed tab updates its own
// entry in place instead of stranding the charges under the old name:
//   - an empty/whitespace candidate name -> EMPTY_NAME;
//   - the candidate matching the tab's OWN source entry case-insensitively (the unrenamed save and
//     case-only respells) renames that entry in place - position, other entries and the reserved list
//     are irrelevant here, because the entry is a user configuration the tab verifiably came from;
//   - any other candidate name matching a DIFFERENT existing entry -> NAME_TAKEN: a save that would
//     silently overwrite a foreign configuration is refused; Save Configuration As owns confirmed
//     overwrites (and a vanished source never earns the right to clobber a foreign entry either);
//   - a candidate that is a reserved built-in preset name in every other case -> RESERVED_NAME (the
//     built-in presets are immutable; a preset-sourced tab with a fresh name escapes via the append);
//   - a vanished source (renamed away, or a legacy session restore) with a fresh candidate name
//     appends the candidate (Save-As creation semantics) -> OK.
// The candidate name is trimmed; all name matching is case-insensitive via findUserConfig.
SaveIntoResult saveIntoUserConfig(std::vector<UserConfig>& configs, const std::string& sourceName, const UserConfig& candidate,
                                  const std::vector<std::string>& reservedNames);

}  // namespace efapp
