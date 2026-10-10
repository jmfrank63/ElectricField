#include "app/ui_logic.hpp"

#include <algorithm>
#include <cmath>

namespace efapp {

char busyGlyph(double seconds) {
  static const char kGlyphs[4] = {'|', '/', '-', '\\'};
  const long long step = static_cast<long long>(std::floor(seconds * 8.0));
  return kGlyphs[static_cast<std::size_t>(step & 3)];
}

std::string tabLabel(const Tab& t, double seconds) {
  std::string s;
  if (t.state == TabState::Computing) {
    s += busyGlyph(seconds);
    s += ' ';
  } else if (t.state == TabState::Failed) {
    s += "! ";
  }
  s += t.title;
  s += "###ef";
  s += std::to_string(t.serial);
  return s;
}

std::string overlayText(std::size_t linesReceived, int expectedLines) {
  const std::size_t m = expectedLines > 0 ? static_cast<std::size_t>(expectedLines) : 0;
  const std::size_t n = std::min(linesReceived, m);
  return "Computing... " + std::to_string(n) + " / " + std::to_string(m) + " lines";
}

std::string windowTitle(const Tab* active) {
  if (!active) return "Electric Field";
  return "Electric Field \xE2\x80\x94 " + active->title;
}

std::string versionLine(std::string_view semver, std::string_view hash) {
  std::string s = "Version ";
  s += semver;
  s += " (";
  s += hash.empty() ? std::string_view("unknown") : hash;
  s += ")";
  return s;
}

std::string shortcutLabel(const char* keyName, bool mac) { return std::string(mac ? "Cmd+" : "Ctrl+") + keyName; }

std::string failureDetail(const std::string& error) { return error.empty() ? std::string("Unknown error") : error; }

float clampScale(float k) {
  if (!std::isfinite(k) || k <= 0.f) return 1.f;
  return std::min(4.f, std::max(0.75f, k));
}

IntSize minimumWindowSize(float k) {
  k = clampScale(k);
  return {static_cast<int>(std::lround(640.0 * static_cast<double>(k))), static_cast<int>(std::lround(480.0 * static_cast<double>(k)))};
}

IntSize defaultWindowSize(float k, int usableW, int usableH) {
  k = clampScale(k);
  const double want_w = 1024.0 * static_cast<double>(k), want_h = 824.0 * static_cast<double>(k);
  const double cap_w = usableW > 0 ? 0.9 * usableW : want_w, cap_h = usableH > 0 ? 0.9 * usableH : want_h;
  return {static_cast<int>(std::lround(std::min(want_w, cap_w))), static_cast<int>(std::lround(std::min(want_h, cap_h)))};
}

ef::View canvasView(float availW, float availH, float k) {
  ef::View v;
  v.widthPx = std::max(1, static_cast<int>(std::floor(availW)));
  v.heightPx = std::max(1, static_cast<int>(std::floor(availH)));
  v.pixelsPerUnit = 100.0 * static_cast<double>(clampScale(k));
  return v;
}

ef::DrawStyle scaledStyle(float k) {
  ef::DrawStyle s;
  k = clampScale(k);
  s.electricThickness *= k;
  s.potentialThickness *= k;
  s.chargeRadiusPx *= k;
  return s;
}

}  // namespace efapp
