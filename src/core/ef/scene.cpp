#include "ef/scene.hpp"

#include <algorithm>
#include <cmath>

namespace ef {

FieldScene buildScene(const ChargeSystem& field, const FieldResult& result) {
  FieldScene s;
  s.electric.reserve(result.electric.size());
  for (const auto& l : result.electric) s.electric.push_back(ScenePath{l.points, false});
  s.potential.reserve(result.potential.size());
  for (const auto& l : result.potential)
    s.potential.push_back(ScenePath{l.points, l.points.size() > 1 && l.points.front() == l.points.back()});
  for (const auto& c : field.charges) s.charges.push_back(SceneCharge{c.position, c.charge});
  return s;
}

namespace {

constexpr float kMargin = 10.f;  // FieldLine::Draw: irect.InflateRect(10, 10)

struct Rect {
  float l, t, r, b;
  bool intersects(const PointF* p, std::size_t n) const {
    float minx = p[0].x, maxx = p[0].x, miny = p[0].y, maxy = p[0].y;
    for (std::size_t i = 1; i < n; ++i) {
      minx = std::min(minx, p[i].x);
      maxx = std::max(maxx, p[i].x);
      miny = std::min(miny, p[i].y);
      maxy = std::max(maxy, p[i].y);
    }
    return !(maxx < l || minx > r || maxy < t || miny > b);
  }
};

void drawPath(Painter& painter, const ScenePath& path, const View& view, const Rect& vis, Color color, float width) {
  const std::size_t n = path.points.size();
  if (n < 2) return;  // MFC: a single point draws nothing

  const double cx = view.widthPx / 2.0, cy = view.heightPx / 2.0;
  std::vector<PointF> dev(n);
  for (std::size_t i = 0; i < n; ++i) {
    dev[i].x = static_cast<float>(cx + path.points[i].X * view.pixelsPerUnit);
    dev[i].y = static_cast<float>(cy - path.points[i].Y * view.pixelsPerUnit);  // y up
  }

  const std::size_t segs = (n - 1) / 3;
  const std::size_t tail = (n - 1) % 3;  // trailing 1-2 points are drawn as straight lines (FieldLine::Draw morePoints)

  std::vector<PointF> run;       // device points of the current visible run
  std::size_t runSegs = 0;       // cubic segments at the front of `run`
  bool culledAny = false;
  auto flush = [&](bool closed) {
    if (run.size() >= 2) painter.strokePath(run.data(), run.size(), runSegs, closed, color, width);
    run.clear();
    runSegs = 0;
  };

  for (std::size_t k = 0; k < segs; ++k) {
    const PointF* p = &dev[3 * k];
    if (vis.intersects(p, 4)) {
      if (run.empty()) run.push_back(p[0]);
      run.push_back(p[1]);
      run.push_back(p[2]);
      run.push_back(p[3]);
      ++runSegs;
    } else {
      culledAny = true;
      flush(false);
    }
  }
  if (tail) {
    const PointF* p = &dev[3 * segs];
    if (vis.intersects(p, tail + 1)) {
      if (run.empty()) run.push_back(p[0]);
      for (std::size_t i = 1; i <= tail; ++i) run.push_back(p[i]);
    } else {
      culledAny = true;
      flush(false);
    }
  }
  flush(path.closed && !culledAny);
}

}  // namespace

void renderScene(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view) {
  painter.begin(view.widthPx, view.heightPx, style.background);
  const Rect vis{-kMargin, -kMargin, static_cast<float>(view.widthPx) + kMargin, static_cast<float>(view.heightPx) + kMargin};

  for (const auto& l : scene.electric) drawPath(painter, l, view, vis, style.electricLine, style.electricThickness);
  for (const auto& l : scene.potential) drawPath(painter, l, view, vis, style.potentialLine, style.potentialThickness);

  const double cx = view.widthPx / 2.0, cy = view.heightPx / 2.0;
  for (const auto& c : scene.charges) {
    const PointF center{static_cast<float>(cx + c.position.X * view.pixelsPerUnit), static_cast<float>(cy - c.position.Y * view.pixelsPerUnit)};
    painter.fillCircle(center, style.chargeRadiusPx, c.charge > 0 ? style.positiveCharge : style.negativeCharge);
  }
  painter.end();
}

}  // namespace ef
