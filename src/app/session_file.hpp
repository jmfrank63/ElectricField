#pragma once
// The open-tab session file (v2.1, SES-01/02): which tabs are open, with which charges and calculation
// method, in a human-readable session.ini next to options.ini / configurations.ini. Std-only; the GUI
// resolves the file location and saves on graceful quit / restores at startup (never in headless modes).
//
// Format:
//   # Electric Field open tabs
//   [tab "Dipole 1"]
//   preset = 0          # index into ef::presets(), or "none" for user/empty/custom tabs
//   method = euler      # a kMethods name; unknown names fall back to the default on restore
//   charge = 1, -2, 0
//   charge = -1, 2, 0
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "app/workspace.hpp"  // efapp::kNoPreset
#include "ef/presets.hpp"     // ef::PresetCharge

namespace efapp {

struct SavedTab {
  std::string title;
  std::size_t preset = kNoPreset;  // kNoPreset = "none"
  std::string method;              // a kMethods persistence name ("" = default)
  std::vector<ef::PresetCharge> charges;
};

// Sections `[tab "Title"]` with `preset`, `method` and `charge` lines (same grammar as the configuration
// charge lines). A section needs a non-empty title and at least one valid charge, else it is skipped
// (counted). Unknown keys and foreign sections are ignored.
std::string serializeSession(const std::vector<SavedTab>& tabs);

struct ParsedSession {
  std::vector<SavedTab> value;
  int skippedTabs = 0;
  int skippedCharges = 0;
};

ParsedSession parseSession(const std::string& text);

bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, std::string& error);
std::vector<SavedTab> loadSession(const std::filesystem::path& file);

}  // namespace efapp
