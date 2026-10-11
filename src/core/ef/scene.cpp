#include <ef/grid.hpp>
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

  const double cx = view.widthPx / 2.0 - view.centre.X * view.pixelsPerUnit;
  const double cy = view.heightPx / 2.0 + view.centre.Y * view.pixelsPerUnit;
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
  renderScene(painter, scene, style, view, nullptr);
}

// v2.4 BUG-01: the grid pass. Drawn after the background fill and before the paths/charges, in the same
// view transform as the scene; light grey, thin. Line counts are bounded so a pathological spacing can
// never stall the frame.
void drawGrid(Painter& painter, const GridSettings& g, const View& view) {
  if (!g.visible || !(g.spacing > 0.0)) return;
  const Color gridColor{120, 210, 120, 255};        // v2.5 GRID-05: light green
  const Color dotColor{217, 119, 6, 255};           // v2.5 GRID-06: lattice points in a distinct colour (orange)
  const double ppu = view.pixelsPerUnit > 0 ? view.pixelsPerUnit : 1.0;
  const double cx = view.widthPx / 2.0 - view.centre.X * ppu;
  const double cy = view.heightPx / 2.0 + view.centre.Y * ppu;
  auto toScreen = [&](double wx, double wy) { return PointF{static_cast<float>(cx + wx * ppu), static_cast<float>(cy - wy * ppu)}; };

  if (!g.polar) {
    const double wx0 = (-cx) / ppu, wx1 = (view.widthPx - cx) / ppu;
    const double wy0 = (cy - view.heightPx) / ppu, wy1 = cy / ppu;
    const long nx0 = static_cast<long>(std::ceil((wx0 - g.origin.X) / g.spacing));
    const long nx1 = static_cast<long>(std::floor((wx1 - g.origin.X) / g.spacing));
    const long ny0 = static_cast<long>(std::ceil((wy0 - g.origin.Y) / g.spacing));
    const long ny1 = static_cast<long>(std::floor((wy1 - g.origin.Y) / g.spacing));
    if (nx1 - nx0 > 2000 || ny1 - ny0 > 2000) return;
    if (g.intersectionsOnly) {  // v2.5 GRID-06: only the lattice points, in the distinct colour
      if (double((nx1 - nx0 + 1)) * double(ny1 - ny0 + 1) > 20000.0) return;
      const float dotR = std::max(2.0f, static_cast<float>(0.03 * g.spacing * ppu));
      for (long i = nx0; i <= nx1; ++i)
        for (long j = ny0; j <= ny1; ++j)
          painter.fillCircle(toScreen(g.origin.X + static_cast<double>(i) * g.spacing, g.origin.Y + static_cast<double>(j) * g.spacing), dotR, dotColor);
      return;
    }
    for (long i = nx0; i <= nx1; ++i) {
      const double wx = g.origin.X + static_cast<double>(i) * g.spacing;
      const PointF a = toScreen(wx, wy0), b = toScreen(wx, wy1);
      const PointF pts[2] = {a, b};
      painter.strokePath(pts, 2, 0, false, gridColor, 1.f);
    }
    for (long j = ny0; j <= ny1; ++j) {
      const double wy = g.origin.Y + static_cast<double>(j) * g.spacing;
      const PointF a = toScreen(wx0, wy), b = toScreen(wx1, wy);
      const PointF pts[2] = {a, b};
      painter.strokePath(pts, 2, 0, false, gridColor, 1.f);
    }
  } else {
    const PointF oc = toScreen(g.origin.X, g.origin.Y);
    const double farX = std::max(std::abs(-cx), std::abs(view.widthPx - cx));
    const double farY = std::max(std::abs(cy - view.heightPx), std::abs(cy));
    const double maxR = std::sqrt(farX * farX + farY * farY) / ppu;
    const long nMax = static_cast<long>(maxR / g.spacing) + 1;
    const int rays = static_cast<int>(std::round(360.0 / std::clamp(g.angleStepDeg, 1.0, 180.0)));
    const double stepRad = g.angleStepDeg * 3.1415926535897932346 / 180.0;
    if (g.intersectionsOnly) {  // v2.5 GRID-06: the ring x ray lattice points, distinct colour
      if (nMax > 200 || double(rays) * double(std::min(nMax, 200L)) > 20000.0) return;
      const float dotR = std::max(2.0f, static_cast<float>(0.03 * g.spacing * ppu));
      for (long i = 1; i <= nMax; ++i)
        for (int j = 0; j < rays; ++j) {
          const double ang = static_cast<double>(j) * stepRad;
          const double r = static_cast<double>(i) * g.spacing;
          painter.fillCircle(toScreen(g.origin.X + r * std::cos(ang), g.origin.Y + r * std::sin(ang)), dotR, dotColor);
        }
      return;
    }
    if (nMax > 2000) return;
    for (long i = 1; i <= nMax; ++i) {  // circle as a dense polyline through the Painter contract
      const std::size_t N = 96;
      std::vector<PointF> pts(N + 1);
      const double r = static_cast<double>(i) * g.spacing;
      for (std::size_t k = 0; k <= N; ++k) {
        const double a = static_cast<double>(k) * 6.283185307179586 / static_cast<double>(N);
        const PointF s = toScreen(g.origin.X + r * std::cos(a), g.origin.Y + r * std::sin(a));
        pts[k] = s;
      }
      painter.strokePath(pts.data(), pts.size(), 0, false, gridColor, 1.f);
    }
    for (int a = 0; a < rays; ++a) {
      const double ang = static_cast<double>(a) * stepRad;
      const double r = maxR * 1.5;
      const PointF b = toScreen(g.origin.X + r * std::cos(ang), g.origin.Y + r * std::sin(ang));
      const PointF pts[2] = {oc, b};
      painter.strokePath(pts, 2, 0, false, gridColor, 1.f);
    }
  }
}

void renderScene(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view, const GridSettings* grid) {
  painter.begin(view.widthPx, view.heightPx, style.background);
  const Rect vis{-kMargin, -kMargin, static_cast<float>(view.widthPx) + kMargin, static_cast<float>(view.heightPx) + kMargin};

  if (grid) drawGrid(painter, *grid, view);  // v2.4 BUG-01: behind the lines, after the background
  for (const auto& l : scene.electric) drawPath(painter, l, view, vis, style.electricLine, style.electricThickness);
  for (const auto& l : scene.potential) drawPath(painter, l, view, vis, style.potentialLine, style.potentialThickness);

  const double cx = view.widthPx / 2.0 - view.centre.X * view.pixelsPerUnit;
  const double cy = view.heightPx / 2.0 + view.centre.Y * view.pixelsPerUnit;
  for (const auto& c : scene.charges) {
    const PointF center{static_cast<float>(cx + c.position.X * view.pixelsPerUnit), static_cast<float>(cy - c.position.Y * view.pixelsPerUnit)};
    painter.fillCircle(center, style.chargeRadiusPx, c.charge > 0 ? style.positiveCharge : style.negativeCharge);
  }
  painter.end();
}

}  // namespace ef
