#include "ef/svg.hpp"

#include <cstdio>

namespace ef {

namespace {

// printf-style numbers are locale-dependent only if the program calls setlocale(); the core never does.
void appendNum(std::string& s, float v) {
  char buf[32];
  const int n = std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
  int len = n;
  // trim trailing zeros: "12.50" -> "12.5", "12.00" -> "12"
  if (len > 0) {
    while (len > 0 && buf[len - 1] == '0') --len;
    if (len > 0 && buf[len - 1] == '.') --len;
  }
  s.append(buf, static_cast<std::size_t>(len));
}

void appendPt(std::string& s, const PointF& p) {
  appendNum(s, p.x);
  s += ' ';
  appendNum(s, p.y);
}

void appendColor(std::string& s, Color c) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", c.r, c.g, c.b);
  s += buf;
}

}  // namespace

void SvgPainter::begin(int w, int h, Color bg) {
  out_.clear();
  out_ += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(w) + "\" height=\"" + std::to_string(h) +
          "\" viewBox=\"0 0 " + std::to_string(w) + ' ' + std::to_string(h) + "\">\n";
  out_ += "<rect width=\"100%\" height=\"100%\" fill=\"";
  appendColor(out_, bg);
  out_ += "\"/>\n";
}

void SvgPainter::strokePath(const PointF* pts, std::size_t n, std::size_t bezierSegments, bool closed, Color color, float width) {
  if (n < 2) return;
  out_ += "<path fill=\"none\" stroke=\"";
  appendColor(out_, color);
  out_ += "\" stroke-width=\"";
  appendNum(out_, width);
  out_ += "\" d=\"M";
  appendPt(out_, pts[0]);
  std::size_t i = 1;
  for (std::size_t s = 0; s < bezierSegments; ++s, i += 3) {
    out_ += s == 0 ? "C" : " ";
    appendPt(out_, pts[i]);
    out_ += ' ';
    appendPt(out_, pts[i + 1]);
    out_ += ' ';
    appendPt(out_, pts[i + 2]);
  }
  if (i < n) {
    out_ += 'L';
    for (bool first = true; i < n; ++i, first = false) {
      if (!first) out_ += ' ';
      appendPt(out_, pts[i]);
    }
  }
  if (closed) out_ += 'Z';
  out_ += "\"/>\n";
}

void SvgPainter::fillCircle(PointF c, float r, Color color) {
  out_ += "<circle cx=\"";
  appendNum(out_, c.x);
  out_ += "\" cy=\"";
  appendNum(out_, c.y);
  out_ += "\" r=\"";
  appendNum(out_, r);
  out_ += "\" fill=\"";
  appendColor(out_, color);
  out_ += "\"/>\n";
}

void SvgPainter::end() { out_ += "</svg>\n"; }

std::string renderSvg(const FieldScene& scene, const DrawStyle& style, const View& view) {
  SvgPainter p;
  renderScene(p, scene, style, view);
  return p.str();
}

}  // namespace ef
