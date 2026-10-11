#include "app/configs.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace efapp {
namespace {

std::string trim(const std::string& s) {
  const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t b = 0, e = s.size();
  while (b < e && isSpace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Whole-string numeric parsers: trailing garbage counts as invalid (same contract as options.cpp).
bool parseInt(const std::string& s, int& out) {
  try {
    std::size_t pos = 0;
    const long v = std::stol(s, &pos);
    if (pos != s.size() || v < -1000000 || v > 1000000) return false;
    out = static_cast<int>(v);
    return true;
  } catch (...) {
    return false;
  }
}

bool parseDouble(const std::string& s, double& out) {
  try {
    std::size_t pos = 0;
    const double v = std::stod(s, &pos);
    if (pos != s.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
  } catch (...) {
    return false;
  }
}

bool readFile(const std::filesystem::path& file, std::string& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  std::ostringstream os;
  os << in.rdbuf();
  out = os.str();
  return true;
}

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

}  // namespace

std::string serializeConfigs(const std::vector<UserConfig>& configs) {
  std::ostringstream os;
  os << "# Electric Field user configurations\n";
  for (const UserConfig& c : configs) {
    std::string name = c.name;
    name.erase(std::remove(name.begin(), name.end(), '"'), name.end());  // names cannot contain the delimiter
    os << "\n[configuration \"" << name << "\"]\n";
    for (const ef::PresetCharge& ch : c.charges) {
      char x[32], y[32];
      std::snprintf(x, sizeof(x), "%.10g", ch.x);
      std::snprintf(y, sizeof(y), "%.10g", ch.y);
      os << "charge = " << ch.charge << ", " << x << ", " << y << '\n';
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
      // split the value part on commas: value, x, y
      std::string v = t.substr(eq + 1);
      std::vector<std::string> parts;
      std::istringstream vs(v);
      std::string part;
      while (std::getline(vs, part, ',')) parts.push_back(trim(part));
      int value = 0;
      double x = 0, y = 0;
      if (inSection && parts.size() == 3 && parseInt(parts[0], value) && parseDouble(parts[1], x) && parseDouble(parts[2], y)) {
        cur.charges.push_back(ef::PresetCharge{value, x, y});
      } else {
        ++r.skippedCharges;
      }
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

}  // namespace efapp
