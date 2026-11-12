#include "app/shapes_file.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "app/ini_text.hpp"

namespace efapp {

namespace {
std::string cleanName(std::string name) {
  name.erase(std::remove(name.begin(), name.end(), '"'), name.end());  // the section delimiter
  name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
  name.erase(std::remove(name.begin(), name.end(), '\r'), name.end());
  return ini::trim(name);
}

bool isBuiltin(const std::string& name) {
  const std::string key = ini::toLower(name);
  for (const ef::Shape& s : ef::builtinShapes())
    if (ini::toLower(s.name) == key) return true;
  return false;
}
}  // namespace

std::string serializeShapes(const std::vector<ef::Shape>& shapes) {
  std::ostringstream os;
  os << "# Electric Field shapes: ball centres in units of the ball radius\n";
  for (const ef::Shape& s : shapes) {
    os << "\n[shape \"" << cleanName(s.name) << "\"]\n";
    for (const ef::Vec3& b : s.balls) {
      char x[32], y[32], z[32];
      std::snprintf(x, sizeof(x), "%.10g", b.X);
      std::snprintf(y, sizeof(y), "%.10g", b.Y);
      std::snprintf(z, sizeof(z), "%.10g", b.Z);
      os << "ball = " << x << ", " << y << ", " << z << '\n';
    }
  }
  return os.str();
}

std::vector<ef::Shape> parseShapes(const std::string& text) {
  std::vector<ef::Shape> out;
  ef::Shape cur;
  bool inSection = false;
  const auto flush = [&] {
    if (inSection && !cur.name.empty() && !isBuiltin(cur.name) && ef::validShape(cur) && findShape(out, cur.name) < 0) out.push_back(cur);
    cur = ef::Shape{};
    inSection = false;
  };
  std::istringstream is(text);
  std::string line;
  while (std::getline(is, line)) {
    const std::string t = ini::trim(line);
    if (t.empty() || t[0] == '#' || t[0] == ';') continue;
    if (t[0] == '[') {
      flush();
      const std::size_t a = t.find('"'), b = t.rfind('"');
      if (t.rfind("[shape", 0) == 0 && a != std::string::npos && b > a) {
        cur.name = cleanName(t.substr(a + 1, b - a - 1));
        inSection = true;
      }
      continue;
    }
    const std::size_t eq = t.find('=');
    if (!inSection || eq == std::string::npos || ini::trim(t.substr(0, eq)) != "ball") continue;
    std::vector<double> v;
    if (ini::parseDoubleList(ini::trim(t.substr(eq + 1)), v) && v.size() == 3)
      cur.balls.push_back(ef::Vec3(v[0], v[1], v[2]));
    else
      cur.balls.push_back(ef::Vec3(std::nan(""), 0, 0));  // a bad line spoils the shape: it is skipped as a whole
  }
  flush();
  return out;
}

std::vector<ef::Shape> loadShapes(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return {};
  std::ostringstream text;
  text << in.rdbuf();
  return parseShapes(text.str());
}

bool saveShapes(const std::filesystem::path& file, const std::vector<ef::Shape>& shapes, std::string& error) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (out) {
    out << serializeShapes(shapes);
    out.flush();
  }
  if (!out) {
    error = "Could not write the shapes file.";
    return false;
  }
  return true;
}

int findShape(const std::vector<ef::Shape>& custom, const std::string& name) {
  const std::string key = ini::toLower(ini::trim(name));
  const std::vector<ef::Shape>& builtin = ef::builtinShapes();
  for (std::size_t i = 0; i < builtin.size(); ++i)
    if (ini::toLower(builtin[i].name) == key) return static_cast<int>(i);
  for (std::size_t i = 0; i < custom.size(); ++i)
    if (ini::toLower(custom[i].name) == key) return static_cast<int>(builtin.size() + i);
  return -1;
}

const ef::Shape& shapeAt(const std::vector<ef::Shape>& custom, int index) {
  const std::vector<ef::Shape>& builtin = ef::builtinShapes();
  const std::size_t i = static_cast<std::size_t>(index);
  return i < builtin.size() ? builtin[i] : custom[i - builtin.size()];
}

SaveShapeResult upsertShape(std::vector<ef::Shape>& custom, const ef::Shape& shape) {
  ef::Shape clean = shape;
  clean.name = cleanName(shape.name);
  if (clean.name.empty()) return SaveShapeResult::EMPTY_NAME;
  if (isBuiltin(clean.name)) return SaveShapeResult::RESERVED_NAME;
  if (!ef::validShape(clean)) return SaveShapeResult::INVALID_SHAPE;
  const std::string key = ini::toLower(clean.name);
  for (ef::Shape& s : custom)
    if (ini::toLower(s.name) == key) {
      s = clean;
      return SaveShapeResult::OK;
    }
  custom.push_back(clean);
  return SaveShapeResult::OK;
}

}  // namespace efapp
