#pragma once
// The open-tab session file (v2.1, SES-01/02): which tabs are open, with which charges and calculation
// method, in a human-readable session.ini next to options.ini / configurations.ini. Std-only; the GUI
// resolves the file location and saves on graceful quit / restores at startup (never in headless modes).
//
// Format:
//   # Electric Field open tabs
//   [tab "Dipole 1"]
//   preset = 0          # index into ef::presets(), or "none" for user/empty/custom tabs
//   source = My Dipole  # optional (h22): the configuration the tab came from; absent = derive from the title on restore
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

// v2.6 REC-02: the recents cap (the same 10 the GUI's recents menu enforces).
inline constexpr std::size_t kMaxRecents = 10;

struct SavedTab {
  std::string title;
  std::size_t preset = kNoPreset;  // kNoPreset = "none"
  std::string method;              // a kMethods persistence name ("" = default)
  bool autoRecalc = false;         // v2.2 CALC-03: continuous calculation
  GridSettings grid;               // v2.3 GRID: snap grid settings
  std::vector<ef::PresetCharge> charges;
  std::string source{};            // h22: the configuration the tab came from; empty = derive from the title on restore (the pre-change convention)
};

// Sections `[tab "Title"]` with `preset`, `method` and `charge` lines (same grammar as the configuration
// charge lines). A section needs a non-empty title and at least one valid charge, else it is skipped
// (counted). Unknown keys and foreign sections are ignored. `collection = <name>` (v2.2) selects the
// collection the tabs belong to; files without it parse as the Original collection.
// 261012-h26: `recent = <path>` lines (UTF-8, v2.5 OPEN-01 paths) record the recently opened collection
// files, newest first - the pointer file (session.ini) carries them; a per-collection tab file never
// writes any, and parsing one there is harmless.
std::string serializeSession(const std::vector<SavedTab>& tabs, const std::string& collection = "Original");
std::string serializeSession(const std::vector<SavedTab>& tabs, const std::string& collection, const std::vector<std::string>& recents);

struct ParsedSession {
  std::string collection = "Original";  // v2.2: the collection the tabs belong to
  bool hasCollection = false;           // v2.6 REC-02: true only when the file carried a non-empty `collection =` value (the struct's "Original" default is NOT a stored line)
  std::vector<SavedTab> value;
  int skippedTabs = 0;
  int skippedCharges = 0;
  std::vector<std::string> recents;  // 261012-h26: `recent` lines, in file order (newest first)
};

ParsedSession parseSession(const std::string& text);

bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, std::string& error);
bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, const std::string& collection, std::string& error);
bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, const std::string& collection,
                 const std::vector<std::string>& recents, std::string& error);
std::vector<SavedTab> loadSession(const std::filesystem::path& file);

// Like loadSession, but also returns the collection tag (v2.2 COL-04) and the recents (261012-h26).
struct SavedSession {
  std::string collection = "Original";
  bool hasCollection = false;  // v2.6 REC-02: true only when the file carried a non-empty `collection =` value (the struct's "Original" default is NOT a stored line)
  std::vector<SavedTab> tabs;
  std::vector<std::string> recents;
};
SavedSession loadSessionFull(const std::filesystem::path& file);

// v2.6 REC-02: seed a session pointer's collection into the recents list - inserted at the front when
// absent (it is by definition the most recently opened), capped at kMaxRecents. `collectionKey` is the
// UTF-8 path of the collection file (recents entries are full paths), or empty when there is nothing
// to seed (no stored collection line / file missing).
std::vector<std::string> seedRecents(std::vector<std::string> recents, const std::string& collectionKey);

}  // namespace efapp
