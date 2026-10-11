#pragma once
// Shared text helpers for the ini-style files (options, configurations, session). Header-only; the
// per-file parsers keep their own grammars but share these primitives.
#include <cctype>
#include <algorithm>
#include <optional>
#include <string>

namespace efapp {
namespace ini {

inline std::string trim(const std::string& s) {
  const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t b = 0, e = s.size();
  while (b < e && isSpace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

inline std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Full-string parsers: trailing garbage counts as invalid. Bounded ints guard against absurd values.
inline bool parseIntBounded(const std::string& s, int lo, int hi, int& out) {
  try {
    std::size_t pos = 0;
    const long v = std::stol(s, &pos);
    if (pos != s.size() || v < lo || v > hi) return false;
    out = static_cast<int>(v);
    return true;
  } catch (...) {
    return false;
  }
}

inline bool parseDouble(const std::string& s, double& out) {
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

// Splits "a, b, c" (the part after '=') on commas into trimmed parts.
inline std::vector<std::string> splitCsv(const std::string& value) {
  std::vector<std::string> parts;
  std::istringstream vs(value);
  std::string part;
  while (std::getline(vs, part, ',')) parts.push_back(trim(part));
  return parts;
}

}  // namespace ini
}  // namespace efapp
