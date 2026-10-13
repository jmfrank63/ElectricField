#include "app/configs.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "app/ini_text.hpp"

namespace efapp {
namespace {

using ini::splitCsv;
using ini::toLower;
using ini::trim;

// The section header is `[configuration "Name"]`; anything else in brackets is a foreign section and
// ignored entirely (a foreign file must not produce garbage configurations).
bool parseSectionHeader(const std::string& line, std::string& name) {
  if (line.rfind("[configuration ", 0) != 0) return false;
  if (line.back() != ']') return false;
  const std::string inner = trim(line.substr(14, line.size() - 15));  // between "[configuration " and "]"
  if (inner.size() < 2 || inner.front() != '"' || inner.back() != '"') return false;
  name = trim(inner.substr(1, inner.size() - 2));
  return true;
}

bool readFile(const std::filesystem::path& file, std::string& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  std::ostringstream os;
  os << in.rdbuf();
  out = os.str();
  return true;
}

}  // namespace

std::string serializeConfigs(const std::vector<UserConfig>& configs) {
  std::ostringstream os;
  os << "# Electric Field user configurations\n";
  for (const UserConfig& c : configs) {
    std::string name = c.name;
    name.erase(std::remove(name.begin(), name.end(), '"'), name.end());  // names cannot contain the delimiter
    os << "\n[configuration \"" << name << "\"]\n";
    for (const ef::PresetCharge& ch : c.charges) {
      char x[32], y[32], s[32];
      std::snprintf(x, sizeof(x), "%.10g", ch.x);
      std::snprintf(y, sizeof(y), "%.10g", ch.y);
      std::snprintf(s, sizeof(s), "%.10g", static_cast<double>(ch.sizePx));
      // v2.6 phase 3: charge = value, x, y, sizePx, id, name. Names are comma-free (the ',' is the field
      // separator) and stored raw UTF-8; the derived "Charge N" display name is never persisted.
      std::string chargeName = ch.name;
      chargeName.erase(std::remove(chargeName.begin(), chargeName.end(), ','), chargeName.end());
      os << "charge = " << ch.charge << ", " << x << ", " << y << ", " << s << ", " << ch.id << ", " << chargeName << '\n';
    }
  }
  return os.str();
}

ParsedConfigs parseConfigs(const std::string& text) {
  ParsedConfigs r;
  std::istringstream is(text);
  std::string line;
  bool inSection = false;
  UserConfig cur;
  auto closeSection = [&]() {
    if (!inSection) return;
    inSection = false;
    const std::string key = toLower(cur.name);
    bool duplicate = false;
    for (const UserConfig& c : r.value)
      if (toLower(c.name) == key) duplicate = true;
    if (duplicate) {
      ++r.duplicateNames;
      return;
    }
    if (cur.name.empty() || cur.charges.empty()) {
      ++r.skippedSections;
      return;
    }
    r.value.push_back(std::move(cur));
  };
  while (std::getline(is, line)) {
    const std::string t = trim(line);
    if (t.empty() || t[0] == '#') continue;
    std::string name;
    if (parseSectionHeader(t, name)) {
      closeSection();
      inSection = true;
      cur = UserConfig{};
      cur.name = name;
      continue;
    }
    if (t[0] == '[') continue;  // foreign section: ignored
    if (t.rfind("charge", 0) == 0) {
      const std::size_t eq = t.find('=');
      if (eq == std::string::npos) {
        if (inSection) ++r.skippedCharges;
        continue;
      }
      // split the value part on commas: value, x, y (legacy) or value, x, y, sizePx, id, name (v2.6 phase 3);
      // exactly 3 or exactly 6 fields - anything else is a skipped charge, never fatal
      const std::string v = t.substr(eq + 1);
      const std::vector<std::string> parts = splitCsv(v);
      // The writer emits an empty name as a trailing empty field ("..., 3, "); the line trim swallows
      // its space, so an explicit trailing comma re-adds the empty final field (still 3-or-6 arity).
      std::vector<std::string> fields = parts;
      if (!v.empty() && v.back() == ',') fields.push_back(std::string());
      int value = 0;
      double x = 0, y = 0;
      bool parsed = false;
      if (inSection && (fields.size() == 3 || fields.size() == 6) && ini::parseIntBounded(fields[0], -1000000, 1000000, value) &&
          ini::parseDouble(fields[1], x) && ini::parseDouble(fields[2], y)) {
        if (fields.size() == 3) {
          cur.charges.push_back(ef::PresetCharge{value, x, y});  // legacy: sizePx 0, id 0, derived name
          parsed = true;
        } else {
          double sizePx = 0;
          int id = 0;
          // WR-01: the extended size field obeys the same contract as every mutator - 0 (inherit)
          // or 2..120 px. Anything else (1e30, -5, 0<x<2) is a malformed line: skipped and counted
          // like an arity failure, never silently degraded into the model.
          if (ini::parseDouble(fields[3], sizePx) && (sizePx == 0 || (sizePx >= 2 && sizePx <= 120)) &&
              ini::parseIntBounded(fields[4], 0, 1000000000, id)) {
            cur.charges.push_back(ef::PresetCharge{value, x, y, static_cast<float>(sizePx), static_cast<std::uint64_t>(id), fields[5]});
            parsed = true;
          }
        }
      }
      if (!parsed) ++r.skippedCharges;
      continue;
    }
    // any other line outside the known grammar is ignored
  }
  closeSection();
  return r;
}

bool saveConfigs(const std::filesystem::path& file, const std::vector<UserConfig>& configs, std::string& error) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "Could not write the configurations file.";
    return false;
  }
  out << serializeConfigs(configs);
  out.flush();
  if (!out) {
    error = "Could not write the configurations file.";
    return false;
  }
  return true;
}

std::vector<UserConfig> loadConfigs(const std::filesystem::path& file) {
  std::string text;
  if (!readFile(file, text)) return {};  // missing or unreadable: no user configurations (D-02)
  return parseConfigs(text).value;
}

int findUserConfig(const std::vector<UserConfig>& configs, const std::string& name) {
  const std::string key = toLower(trim(name));
  for (std::size_t i = 0; i < configs.size(); ++i)
    if (toLower(configs[i].name) == key) return static_cast<int>(i);
  return -1;
}

UpsertResult upsertUserConfig(std::vector<UserConfig>& configs, const UserConfig& candidate,
                              const std::vector<std::string>& reservedNames) {
  const std::string name = trim(candidate.name);
  if (name.empty()) return UpsertResult::EMPTY_NAME;
  const std::string key = toLower(name);
  for (const std::string& r : reservedNames)
    if (toLower(r) == key) return UpsertResult::RESERVED_NAME;
  const int at = findUserConfig(configs, name);
  if (at >= 0) {
    configs[static_cast<std::size_t>(at)].name = name;  // keep the list position, adopt the new spelling
    configs[static_cast<std::size_t>(at)].charges = candidate.charges;
    return UpsertResult::OK;
  }
  configs.push_back(candidate);
  configs.back().name = name;
  return UpsertResult::OK;
}

SaveIntoResult saveIntoUserConfig(std::vector<UserConfig>& configs, const std::string& sourceName, const UserConfig& candidate,
                                  const std::vector<std::string>& reservedNames) {
  const std::string name = trim(candidate.name);
  if (name.empty()) return SaveIntoResult::EMPTY_NAME;
  const int at = findUserConfig(configs, sourceName);
  const int target = findUserConfig(configs, name);
  const bool ownEntry = at >= 0 && target == at;  // the unrenamed save and case-only respells
  if (target >= 0 && !ownEntry) return SaveIntoResult::NAME_TAKEN;  // a foreign entry owns the name
  if (!ownEntry) {
    const std::string key = toLower(name);
    for (const std::string& r : reservedNames)
      if (toLower(r) == key) return SaveIntoResult::RESERVED_NAME;  // built-in presets are immutable
  }
  if (at >= 0) {  // the tab's own entry (unrenamed or renamed): in place - position and other entries kept
    configs[static_cast<std::size_t>(at)].name = name;  // keep the list position, adopt the new spelling
    configs[static_cast<std::size_t>(at)].charges = candidate.charges;
    return SaveIntoResult::OK;
  }
  configs.push_back(candidate);  // vanished source with a fresh name: Save-As creation semantics
  configs.back().name = name;
  return SaveIntoResult::OK;
}

}  // namespace efapp
