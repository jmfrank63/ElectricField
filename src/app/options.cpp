#include "app/options.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
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

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string colorHex(const ef::Color& c) {
  std::ostringstream os;
  os << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c.r) << std::setw(2) << static_cast<int>(c.g)
     << std::setw(2) << static_cast<int>(c.b);
  return os.str();
}

// Parses "#RRGGBB" (case-insensitive); false for anything else.
bool parseColor(const std::string& text, ef::Color& out) {
  if (text.size() != 7 || text[0] != '#') return false;
  int rgb[3];
  for (int i = 0; i < 3; ++i) {
    const int hi = hexDigit(text[static_cast<std::size_t>(1 + 2 * i)]);
    const int lo = hexDigit(text[static_cast<std::size_t>(2 + 2 * i)]);
    if (hi < 0 || lo < 0) return false;
    rgb[i] = hi * 16 + lo;
  }
  out.r = static_cast<std::uint8_t>(rgb[0]);
  out.g = static_cast<std::uint8_t>(rgb[1]);
  out.b = static_cast<std::uint8_t>(rgb[2]);
  out.a = 255;
  return true;
}

// Full-string numeric parsers: trailing garbage counts as invalid (no silent prefixes).
bool parseUll(const std::string& s, unsigned long long& out) {
  try {
    std::size_t pos = 0;
    const unsigned long long v = std::stoull(s, &pos);
    if (pos != s.size()) return false;
    out = v;
    return true;
  } catch (...) {
    return false;
  }
}

bool parseDouble(const std::string& s, double& out) {
  try {
    std::size_t pos = 0;
    const double v = std::stod(s, &pos);
    if (pos != s.size()) return false;
    out = v;
    return true;
  } catch (...) {
    return false;
  }
}

std::uint8_t toByte(float f) {
  f = std::min(1.f, std::max(0.f, f));
  return static_cast<std::uint8_t>(std::lround(f * 255.f));
}

float channel(std::uint8_t v) { return static_cast<float>(v) / 255.f; }

ef::Method methodAtIndex(int i) { return ef::kMethods[static_cast<std::size_t>(i < 0 || i >= static_cast<int>(ef::kMethodCount) ? 0 : i)].method; }

bool readFile(const std::filesystem::path& file, std::string& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  std::ostringstream os;
  os << in.rdbuf();
  out = os.str();
  return true;
}

}  // namespace

std::string validate(const Options& o) {
  if (o.compute.linesPerUnitCharge < kLinesMin || o.compute.linesPerUnitCharge > kLinesMax)
    return "Lines per unit charge must be between " + std::to_string(kLinesMin) + " and " + std::to_string(kLinesMax) + ".";
  if (o.compute.numThreads < kThreadsMin || o.compute.numThreads > kThreadsMax)
    return "Threads must be between " + std::to_string(kThreadsMin) + " and " + std::to_string(kThreadsMax) + ".";
  if (!(o.compute.potentialInterval >= kIntervalMin && o.compute.potentialInterval <= kIntervalMax))
    return "The potential interval must be between 0.01 and 0.50.";
  if (o.draw.electricThickness < static_cast<float>(kThicknessMin) || o.draw.electricThickness > static_cast<float>(kThicknessMax))
    return "The field line thickness must be between " + std::to_string(kThicknessMin) + " and " + std::to_string(kThicknessMax) + ".";
  if (o.draw.potentialThickness < static_cast<float>(kThicknessMin) || o.draw.potentialThickness > static_cast<float>(kThicknessMax))
    return "The equipotential thickness must be between " + std::to_string(kThicknessMin) + " and " + std::to_string(kThicknessMax) + ".";
  if (o.draw.chargeRadiusPx < static_cast<float>(kChargeSizeMin) || o.draw.chargeRadiusPx > static_cast<float>(kChargeSizeMax))  // v2.5 SIZE-01
    return "The charge size must be between " + std::to_string(kChargeSizeMin) + " and " + std::to_string(kChargeSizeMax) + " px.";
  return {};
}

std::string serialize(const Options& o) {
  std::ostringstream os;
  os << "# Electric Field options\n";
  os << "linesPerUnitCharge = " << o.compute.linesPerUnitCharge << '\n';
  os << "calculateEquipotentials = " << (o.compute.calculateEquipotentials ? "true" : "false") << '\n';
  char interval[32];
  std::snprintf(interval, sizeof(interval), "%.3f", o.compute.potentialInterval);
  os << "potentialInterval = " << interval << '\n';
  os << "numThreads = " << o.compute.numThreads << '\n';
  os << "method = " << ef::info(o.compute.method).name << '\n';
  os << "positiveChargeColor = " << colorHex(o.draw.positiveCharge) << '\n';
  os << "negativeChargeColor = " << colorHex(o.draw.negativeCharge) << '\n';
  os << "electricLineColor = " << colorHex(o.draw.electricLine) << '\n';
  os << "potentialLineColor = " << colorHex(o.draw.potentialLine) << '\n';
  os << "electricLineThickness = " << static_cast<long long>(o.draw.electricThickness) << '\n';
  os << "potentialLineThickness = " << static_cast<long long>(o.draw.potentialThickness) << '\n';
  os << "chargeSize = " << static_cast<long long>(o.draw.chargeRadiusPx) << '\n';  // v2.5 SIZE-01
  os << "coupleValueSize = " << (o.coupleValueSize ? "true" : "false") << '\n';    // 261020-ef2
  return os.str();
}

ParsedOptions parse(const std::string& text) {
  ParsedOptions r;  // the value starts at its defaults: a rejected key keeps its default (D-03)
  std::istringstream is(text);
  std::string line;
  while (std::getline(is, line)) {
    const std::string t = trim(line);
    if (t.empty() || t[0] == '#') continue;
    const std::size_t eq = t.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = trim(t.substr(0, eq));
    const std::string value = trim(t.substr(eq + 1));

    if (key == "linesPerUnitCharge") {
      unsigned long long v = 0;
      if (parseUll(value, v) && v >= kLinesMin && v <= kLinesMax)
        r.value.compute.linesPerUnitCharge = static_cast<unsigned>(v);
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "calculateEquipotentials") {
      const std::string v = toLower(value);
      if (v == "true" || v == "1")
        r.value.compute.calculateEquipotentials = true;
      else if (v == "false" || v == "0")
        r.value.compute.calculateEquipotentials = false;
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "potentialInterval") {
      double v = 0;
      if (parseDouble(value, v) && v >= kIntervalMin && v <= kIntervalMax)
        r.value.compute.potentialInterval = v;
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "numThreads") {
      unsigned long long v = 0;
      if (parseUll(value, v) && v >= kThreadsMin && v <= kThreadsMax)
        r.value.compute.numThreads = static_cast<unsigned>(v);
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "method") {
      const std::optional<ef::Method> m = ef::methodFromName(toLower(value));
      if (m)
        r.value.compute.method = *m;
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "positiveChargeColor") {
      if (!parseColor(value, r.value.draw.positiveCharge)) r.rejectedKeys.push_back(key);
    } else if (key == "negativeChargeColor") {
      if (!parseColor(value, r.value.draw.negativeCharge)) r.rejectedKeys.push_back(key);
    } else if (key == "electricLineColor") {
      if (!parseColor(value, r.value.draw.electricLine)) r.rejectedKeys.push_back(key);
    } else if (key == "potentialLineColor") {
      if (!parseColor(value, r.value.draw.potentialLine)) r.rejectedKeys.push_back(key);
    } else if (key == "electricLineThickness") {
      unsigned long long v = 0;
      if (parseUll(value, v) && v >= kThicknessMin && v <= kThicknessMax)
        r.value.draw.electricThickness = static_cast<float>(v);
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "potentialLineThickness") {
      unsigned long long v = 0;
      if (parseUll(value, v) && v >= kThicknessMin && v <= kThicknessMax)
        r.value.draw.potentialThickness = static_cast<float>(v);
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "chargeSize") {  // v2.5 SIZE-01
      unsigned long long v = 0;
      if (parseUll(value, v) && v >= kChargeSizeMin && v <= kChargeSizeMax)
        r.value.draw.chargeRadiusPx = static_cast<float>(v);
      else
        r.rejectedKeys.push_back(key);
    } else if (key == "coupleValueSize") {  // 261020-ef2
      const std::string v = toLower(value);
      if (v == "true" || v == "1")
        r.value.coupleValueSize = true;
      else if (v == "false" || v == "0")
        r.value.coupleValueSize = false;
      else
        r.rejectedKeys.push_back(key);
    }
    // Unknown keys are ignored (D-03).
  }
  return r;
}

bool saveOptions(const std::filesystem::path& file, const Options& o, std::string& error) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "Could not write the options file.";
    return false;
  }
  out << serialize(o);
  out.flush();
  if (!out) {
    error = "Could not write the options file.";
    return false;
  }
  return true;
}

Options loadOptions(const std::filesystem::path& file) {
  std::string text;
  if (!readFile(file, text)) return Options{};  // missing or unreadable: defaults (D-03)
  return parse(text).value;
}

void OptionsEdit::reset(const Options& current) {
  lines = static_cast<int>(current.compute.linesPerUnitCharge);
  threads = static_cast<int>(current.compute.numThreads);
  electricThickness = static_cast<int>(current.draw.electricThickness);
  potentialThickness = static_cast<int>(current.draw.potentialThickness);
  chargeSize = static_cast<double>(current.draw.chargeRadiusPx);  // v2.5 SIZE-01
  interval = current.compute.potentialInterval;
  methodIndex = static_cast<int>(current.compute.method);  // enum order == kMethods order (method.hpp)
  positive[0] = channel(current.draw.positiveCharge.r);
  positive[1] = channel(current.draw.positiveCharge.g);
  positive[2] = channel(current.draw.positiveCharge.b);
  negative[0] = channel(current.draw.negativeCharge.r);
  negative[1] = channel(current.draw.negativeCharge.g);
  negative[2] = channel(current.draw.negativeCharge.b);
  electric[0] = channel(current.draw.electricLine.r);
  electric[1] = channel(current.draw.electricLine.g);
  electric[2] = channel(current.draw.electricLine.b);
  potential[0] = channel(current.draw.potentialLine.r);
  potential[1] = channel(current.draw.potentialLine.g);
  potential[2] = channel(current.draw.potentialLine.b);
  equipotentials = current.compute.calculateEquipotentials;
  coupleValueSize = current.coupleValueSize;  // 261020-ef2: round-tripped (the panel owns the edit)
  error.clear();
}

bool OptionsEdit::validate() {
  Options o;
  o.compute.linesPerUnitCharge = static_cast<unsigned>(lines);
  o.compute.numThreads = static_cast<unsigned>(threads);
  o.compute.potentialInterval = interval;
  o.compute.method = methodAtIndex(methodIndex);
  o.compute.calculateEquipotentials = equipotentials;
  o.draw.electricThickness = static_cast<float>(electricThickness);
  o.draw.potentialThickness = static_cast<float>(potentialThickness);
  o.draw.chargeRadiusPx = static_cast<float>(chargeSize);  // v2.5 SIZE-01
  error = efapp::validate(o);  // qualified: the member hides the free function
  return error.empty();
}

Options OptionsEdit::apply() const {
  Options o;
  o.compute.linesPerUnitCharge = static_cast<unsigned>(lines);
  o.compute.numThreads = static_cast<unsigned>(threads);
  o.compute.potentialInterval = interval;
  o.compute.method = methodAtIndex(methodIndex);
  o.compute.calculateEquipotentials = equipotentials;
  o.draw.positiveCharge = ef::Color{toByte(positive[0]), toByte(positive[1]), toByte(positive[2]), 255};
  o.draw.negativeCharge = ef::Color{toByte(negative[0]), toByte(negative[1]), toByte(negative[2]), 255};
  o.draw.electricLine = ef::Color{toByte(electric[0]), toByte(electric[1]), toByte(electric[2]), 255};
  o.draw.potentialLine = ef::Color{toByte(potential[0]), toByte(potential[1]), toByte(potential[2]), 255};
  o.draw.electricThickness = static_cast<float>(electricThickness);
  o.draw.potentialThickness = static_cast<float>(potentialThickness);
  o.draw.chargeRadiusPx = static_cast<float>(chargeSize);  // v2.5 SIZE-01
  o.coupleValueSize = coupleValueSize;  // 261020-ef2: never reset by a dialog OK
  return o;
}

}  // namespace efapp
