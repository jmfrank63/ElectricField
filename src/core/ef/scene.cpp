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

// 261012-h21: device-space gate for the zoom-adaptive refinement in drawPath. 2x the ~24 px default-view
// anchor spacing (the 8 px recorded thinning x 3): below it the legacy fitted cubics render sub-pixel
// smooth, above it the corner kinks between consecutive anchors become visible and refinement pays off.
constexpr double kRefineAnchorChordPx = 48.0;

// One cubic group in the Painter::strokePath layout (p0, c1, c2, p3).
struct CubicGroup {
  PointF p[4];
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

  // On-curve anchors (261012-h21): the fitted cubics pass through the group endpoints 0,3,6,...,3*segs plus the
  // trailing line points. A closed figure whose last anchor closes the loop (world front==back, guaranteed by
  // buildScene) drops that duplicate and wraps instead, so the loop needs no zero-length final group.
  std::vector<PointF> anchors;
  anchors.reserve(segs + 1 + tail);
  for (std::size_t k = 0; k <= segs; ++k) anchors.push_back(dev[3 * k]);
  for (std::size_t i = 1; i <= tail; ++i) anchors.push_back(dev[3 * segs + i]);
  bool periodic = false;
  if (path.closed && anchors.size() >= 2 && anchors.front().x == anchors.back().x && anchors.front().y == anchors.back().y) {
    anchors.pop_back();
    periodic = true;
  }

  // Gate: refine only when some anchor chord is big enough on screen for the joins to kink. Purely a draw-time
  // decision from view.pixelsPerUnit: zooming never triggers a recompute and the trace data is untouched.
  // (Squared comparison: identical decision, no sqrt.)
  bool refine = false;
  if (anchors.size() >= 2) {
    double maxChordSq = 0.0;
    for (std::size_t i = 0; i + 1 < anchors.size(); ++i) {
      const double dx = static_cast<double>(anchors[i + 1].x) - static_cast<double>(anchors[i].x);
      const double dy = static_cast<double>(anchors[i + 1].y) - static_cast<double>(anchors[i].y);
      maxChordSq = std::max(maxChordSq, dx * dx + dy * dy);
    }
    if (periodic) {  // the wrap pair is drawn too (it closes the loop)
      const double dx = static_cast<double>(anchors.front().x) - static_cast<double>(anchors.back().x);
      const double dy = static_cast<double>(anchors.front().y) - static_cast<double>(anchors.back().y);
      maxChordSq = std::max(maxChordSq, dx * dx + dy * dy);
    }
    refine = maxChordSq > kRefineAnchorChordPx * kRefineAnchorChordPx;
  }

  // The 4-point cubic groups + trailing straight-line points of the selected branch; both are consumed by ONE
  // shared culling/flush loop below (behavior preserved exactly).
  std::vector<CubicGroup> groups;
  std::vector<PointF> tailPts;  // legacy branch only: connect point + 1-2 trailing line points
  if (!refine) {
    groups.reserve(segs);
    for (std::size_t k = 0; k < segs; ++k) {
      CubicGroup g;
      for (std::size_t j = 0; j < 4; ++j) g.p[j] = dev[3 * k + j];
      groups.push_back(g);
    }
    if (tail) {
      tailPts.push_back(dev[3 * segs]);
      for (std::size_t i = 1; i <= tail; ++i) tailPts.push_back(dev[3 * segs + i]);
    }
  } else {
    // Refined emission (261012-h21): uniform Catmull-Rom through the anchors, emitted in the same 4-point group
    // layout so the Painter contract and every backend stay unchanged. Control points
    //   c1 = A[i] + (A[i+1] - A[i-1]) / 6,   c2 = A[i+1] - (A[i+2] - A[i]) / 6
    // (the uniform Catmull-Rom -> Bezier conversion; tangent-continuous at every join by construction). Periodic
    // paths wrap; open paths mirror the ends (2*A0 - A1 / 2*A[m-1] - A[m-2]). Pure double arithmetic, division by
    // the constant 6 only: no degeneracy fallbacks and no new NaN paths; coincident-anchor groups fall out
    // naturally and are harmless in every backend (the same class of degenerate groups the legacy path already
    // emits after adjustForBezier fallbacks).
    const std::size_t m = anchors.size();
    const auto prevOf = [&](std::size_t k) -> PointF {  // neighbour before anchor k
      if (k == 0) {
        if (periodic) return anchors[m - 1];
        return PointF{static_cast<float>(2.0 * static_cast<double>(anchors[0].x) - static_cast<double>(anchors[1].x)),
                      static_cast<float>(2.0 * static_cast<double>(anchors[0].y) - static_cast<double>(anchors[1].y))};
      }
      return anchors[k - 1];
    };
    const auto nextOf = [&](std::size_t k) -> PointF {  // neighbour after anchor k
      if (k == m - 1) {
        if (periodic) return anchors[0];
        return PointF{static_cast<float>(2.0 * static_cast<double>(anchors[m - 1].x) - static_cast<double>(anchors[m - 2].x)),
                      static_cast<float>(2.0 * static_cast<double>(anchors[m - 1].y) - static_cast<double>(anchors[m - 2].y))};
      }
      return anchors[k + 1];
    };
    const std::size_t outSegs = periodic ? m : m - 1;
    groups.reserve(outSegs);
    for (std::size_t i = 0; i < outSegs; ++i) {
      const std::size_t j = periodic ? (i + 1) % m : i + 1;  // destination anchor of the segment
      const PointF pm = prevOf(i);
      const PointF pn = nextOf(j);
      const double x0 = pm.x, y0 = pm.y;
      const double x1 = anchors[i].x, y1 = anchors[i].y;
      const double x2 = anchors[j].x, y2 = anchors[j].y;
      const double x3 = pn.x, y3 = pn.y;
      CubicGroup g;
      g.p[0] = anchors[i];
      g.p[1] = PointF{static_cast<float>(x1 + (x2 - x0) / 6.0), static_cast<float>(y1 + (y2 - y0) / 6.0)};
      g.p[2] = PointF{static_cast<float>(x2 - (x3 - x1) / 6.0), static_cast<float>(y2 - (y3 - y1) / 6.0)};
      g.p[3] = anchors[j];
      groups.push_back(g);
    }
  }

  std::vector<PointF> run;       // device points of the current visible run
  std::size_t runSegs = 0;       // cubic segments at the front of `run`
  bool culledAny = false;
  auto flush = [&](bool closed) {
    if (run.size() >= 2) painter.strokePath(run.data(), run.size(), runSegs, closed, color, width);
    run.clear();
    runSegs = 0;
  };

  for (const CubicGroup& g : groups) {
    if (vis.intersects(g.p, 4)) {
      if (run.empty()) run.push_back(g.p[0]);
      run.push_back(g.p[1]);
      run.push_back(g.p[2]);
      run.push_back(g.p[3]);
      ++runSegs;
    } else {
      culledAny = true;
      flush(false);
    }
  }
  if (!tailPts.empty()) {
    if (vis.intersects(tailPts.data(), tailPts.size())) {
      if (run.empty()) run.push_back(tailPts[0]);
      for (std::size_t i = 1; i < tailPts.size(); ++i) run.push_back(tailPts[i]);
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
      // 261012-h21: each ring's segment count adapts to its on-screen radius so grid circles stay smooth
      // when zoomed in: the chord sagitta is s = r_px * (2*pi/N)^2 / 8, so s <= 1 px means
      // N >= 2*pi*sqrt(r_px/8) = 2.2214*sqrt(r_px) (2.2214 = 2*pi/sqrt(8)). The 96 floor keeps every ring
      // with an on-screen radius under ~1,866 px exactly as before; the 2048 cap bounds the worst case.
      // Total emitted points stay bounded: zooming in shrinks the visible world radius maxR (proportional
      // to 1/ppu) so few big rings exist when rings are big, and zooming out trips the nMax <= 2000 guard.
      const double rPx = static_cast<double>(i) * g.spacing * ppu;
      const std::size_t N = static_cast<std::size_t>(std::clamp(std::ceil(2.2214 * std::sqrt(rPx)), 96.0, 2048.0));
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
