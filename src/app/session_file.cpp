#include "app/session_file.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "app/ini_text.hpp"

namespace efapp {
namespace {

bool readFile(const std::filesystem::path& file, std::string& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  std::ostringstream os;
  os << in.rdbuf();
  out = os.str();
  return true;
}

// `[tab "Title"]`
bool parseTabHeader(const std::string& line, std::string& title) {
  if (line.rfind("[tab ", 0) != 0 || line.back() != ']') return false;
  const std::string inner = ini::trim(line.substr(5, line.size() - 6));
  if (inner.size() < 2 || inner.front() != '"' || inner.back() != '"') return false;
  title = ini::trim(inner.substr(1, inner.size() - 2));
  return true;
}

}  // namespace

std::string serializeSession(const std::vector<SavedTab>& tabs, const std::string& collection) {
  return serializeSession(tabs, collection, {});
}

std::string serializeSession(const std::vector<SavedTab>& tabs, const std::string& collection, const std::vector<std::string>& recents) {
  std::ostringstream os;
  os << "# Electric Field open tabs\n";
  os << "collection = " << collection << '\n';
  // 261012-h26: the recently opened collection files, newest first (the session pointer's recents menu).
  for (const std::string& r : recents) os << "recent = " << r << '\n';
  for (const SavedTab& t : tabs) {
    std::string title = t.title;
    title.erase(std::remove(title.begin(), title.end(), '"'), title.end());
    os << "\n[tab \"" << title << "\"]\n";
    os << "preset = " << (t.preset == kNoPreset ? std::string("none") : std::to_string(t.preset)) << '\n';
    if (!t.source.empty()) {  // h22: the tab's configuration provenance; free-form names may contain quotes
      std::string source = t.source;
      source.erase(std::remove(source.begin(), source.end(), '"'), source.end());
      os << "source = " << source << '\n';
    }
    os << "method = " << (t.method.empty() ? std::string("euler") : t.method) << '\n';
    if (t.autoRecalc) os << "autocalc = true\n";
    char gx[32], gy[32], gs[32], ga[32];
    std::snprintf(gx, sizeof(gx), "%.10g", t.grid.origin.X);
    std::snprintf(gy, sizeof(gy), "%.10g", t.grid.origin.Y);
    std::snprintf(gs, sizeof(gs), "%.10g", t.grid.spacing);
    std::snprintf(ga, sizeof(ga), "%.10g", t.grid.angleStepDeg);
    os << "grid = " << (t.grid.active ? 1 : 0) << ',' << (t.grid.visible ? 1 : 0) << ',' << (t.grid.polar ? 1 : 0) << ',' << (t.grid.intersectionsOnly ? 1 : 0) << ',' << gx << ','
       << gy << ',' << gs << ',' << ga << '\n';
    for (const ef::PresetCharge& c : t.charges) {
      char x[32], y[32];
      std::snprintf(x, sizeof(x), "%.10g", c.x);
      std::snprintf(y, sizeof(y), "%.10g", c.y);
      os << "charge = " << c.charge << ", " << x << ", " << y << '\n';
    }
  }
  return os.str();
}

ParsedSession parseSession(const std::string& text) {
  ParsedSession r;
  std::istringstream is(text);
  std::string line;
  bool inTab = false;
  SavedTab cur;
  auto closeTab = [&]() {
    if (!inTab) return;
    inTab = false;
    if (cur.title.empty() || cur.charges.empty()) {
      ++r.skippedTabs;
      return;
    }
    r.value.push_back(std::move(cur));
  };
  while (std::getline(is, line)) {
    const std::string t = ini::trim(line);
    if (t.empty() || t[0] == '#') continue;
    std::string title;
    if (parseTabHeader(t, title)) {
      closeTab();
      inTab = true;
      cur = SavedTab{};
      cur.title = title;
      continue;
    }
    if (t[0] == '[') continue;  // foreign section
    const std::size_t eq = t.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = ini::trim(t.substr(0, eq));
    const std::string value = ini::trim(t.substr(eq + 1));

    if (key == "collection") {
      if (!value.empty()) r.collection = value;  // v2.2: which collection the tabs belong to
    } else if (key == "recent") {  // 261012-h26: a recently opened collection file (UTF-8 path, newest first)
      if (!value.empty() && r.recents.size() < 100) r.recents.push_back(value);
    } else if (key == "autocalc") {
      if (inTab && ini::toLower(value) == "true") cur.autoRecalc = true;
    } else if (key == "grid") {
      if (!inTab) continue;
      const std::vector<std::string> gp = ini::splitCsv(value);
      int a = 0, vis = 0, pol = 0;
      double ox = 0, oy = 0, sp = 0, ang = 0;
      // Pre-v2.5 lines: a,v,p,ox,oy,sp,ang (7 fields); v2.5+ lines add the intersections flag (8).
      const std::size_t geoShift = gp.size() >= 8 ? 1u : 0u;
      if (gp.size() >= 7 && ini::parseIntBounded(gp[0], 0, 1, a) && ini::parseIntBounded(gp[1], 0, 1, vis) &&
          ini::parseIntBounded(gp[2], 0, 1, pol) && ini::parseDouble(gp[3 + geoShift], ox) && ini::parseDouble(gp[4 + geoShift], oy) &&
          ini::parseDouble(gp[5 + geoShift], sp)) {
        cur.grid.active = a != 0;
        cur.grid.visible = vis != 0;
        cur.grid.polar = pol != 0;
        cur.grid.origin = ef::Vec2{ox, oy};
        cur.grid.spacing = sp;
        if (gp.size() >= 7 + geoShift && ini::parseDouble(gp[6 + geoShift], ang)) cur.grid.angleStepDeg = ang;
        if (gp.size() >= 8 && gp[3] == "1") cur.grid.intersectionsOnly = true;
      }
    } else if (key == "preset") {
      if (!inTab) continue;
      if (ini::toLower(value) == "none")
        cur.preset = kNoPreset;
      else {
        int idx = -1;
        if (ini::parseIntBounded(value, 0, 100000, idx))
          cur.preset = static_cast<std::size_t>(idx);
        else
          cur.preset = kNoPreset;  // an invalid index degrades to a plain named tab, not a lost one
      }
    } else if (key == "source") {
      if (inTab && !value.empty()) cur.source = value;  // h22: the configuration the tab came from
    } else if (key == "method") {
      if (inTab) cur.method = ini::toLower(value);
    } else if (key == "charge") {
      const std::vector<std::string> parts = ini::splitCsv(value);
      int v = 0;
      double x = 0, y = 0;
      if (inTab && parts.size() == 3 && ini::parseIntBounded(parts[0], -1000000, 1000000, v) && ini::parseDouble(parts[1], x) &&
          ini::parseDouble(parts[2], y))
        cur.charges.push_back(ef::PresetCharge{v, x, y});
      else if (inTab)
        ++r.skippedCharges;
    }
  }
  closeTab();
  return r;
}

bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, const std::string& collection, std::string& error) {
  return saveSession(file, tabs, collection, {}, error);
}

bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, const std::string& collection,
                 const std::vector<std::string>& recents, std::string& error) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "Could not write the session file.";
    return false;
  }
  out << serializeSession(tabs, collection, recents);
  out.flush();
  if (!out) {
    error = "Could not write the session file.";
    return false;
  }
  return true;
}

bool saveSession(const std::filesystem::path& file, const std::vector<SavedTab>& tabs, std::string& error) {
  return saveSession(file, tabs, "Original", error);
}

std::vector<SavedTab> loadSession(const std::filesystem::path& file) {
  std::string text;
  if (!readFile(file, text)) return {};
  return parseSession(text).value;
}

SavedSession loadSessionFull(const std::filesystem::path& file) {
  SavedSession s;
  std::string text;
  if (!readFile(file, text)) return s;
  const ParsedSession r = parseSession(text);
  s.collection = r.collection;
  s.tabs = r.value;
  s.recents = r.recents;
  return s;
}

}  // namespace efapp
