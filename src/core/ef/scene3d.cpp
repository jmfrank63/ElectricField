// The 3D view of a scene (FieldScene::space): everything is placed in space and drawn back to front.
//  - Balls are shaded spheres. A line segment lies either before or behind a ball's centre; drawing the
//    segments behind it first and the ones before it afterwards hides exactly what the sphere covers.
//  - Lines get thinner and paler with distance.
//  - The cut plane of the equipotentials is a tinted sheet with a unit grid; what lies behind it is
//    tinted too.
//  - An axis triad in the corner shows how the scene is turned.
// Lines are projected from their points in space on every draw, so the view turns without a calculation.
#include <algorithm>
#include <cmath>
#include <vector>

#include "ef/scene.hpp"
#include "ef/scene_detail.hpp"

namespace ef {

namespace {

Color mix(Color a, Color b, double t) {
  t = std::clamp(t, 0., 1.);
  const auto channel = [t](std::uint8_t x, std::uint8_t y) {
    return static_cast<std::uint8_t>(std::lround(static_cast<double>(x) + (static_cast<double>(y) - static_cast<double>(x)) * t));
  };
  return Color{channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), 255};
}

constexpr int kFadeSteps = 6;        // depth fade is drawn in this many steps (keeps the strokes long)
constexpr double kFadeFar = 0.6;     // how far the farthest lines fade into the background
constexpr double kBehindSheet = 0.4; // how far things behind the sheet take its tint

// The points on the curve of a path in the 2D layout (p0,[c1,c2,p3],...,+ line points), in the plane z = 0.
std::vector<Vec3> flatten(const std::vector<Vec2>& pts) {
  std::vector<Vec3> out;
  if (pts.empty()) return out;
  const std::size_t n = pts.size(), segs = (n - 1) / 3;
  out.push_back(Vec3(pts[0].X, pts[0].Y, 0));
  for (std::size_t s = 0; s < segs; ++s) {
    const Vec2& a = pts[3 * s];
    const Vec2& b = pts[3 * s + 1];
    const Vec2& c = pts[3 * s + 2];
    const Vec2& d = pts[3 * s + 3];
    for (int k = 1; k <= 6; ++k) {
      const double t = k / 6., u = 1. - t;
      const Vec2 p = a * (u * u * u) + b * (3. * u * u * t) + c * (3. * u * t * t) + d * (t * t * t);
      out.push_back(Vec3(p.X, p.Y, 0));
    }
  }
  for (std::size_t i = 3 * segs + 1; i < n; ++i) out.push_back(Vec3(pts[i].X, pts[i].Y, 0));
  return out;
}

// Two extra points per span on a Catmull-Rom curve through the recorded points: the recorded spacing
// (8 px at the default zoom) shows as corners on tight bends otherwise.
std::vector<Vec3> smooth(const std::vector<Vec3>& p, bool closed) {
  const std::size_t n = p.size();
  if (n < 3) return p;
  std::vector<Vec3> out;
  out.reserve(3 * n);
  const std::size_t last = n - 1;
  const bool wrap = closed && p.front() == p.back() && n > 3;
  const auto at = [&](std::ptrdiff_t i) -> Vec3 {
    if (i < 0) return wrap ? p[last - 1] : p[0] * 2. - p[1];
    if (i > static_cast<std::ptrdiff_t>(last)) return wrap ? p[1] : p[last] * 2. - p[last - 1];
    return p[static_cast<std::size_t>(i)];
  };
  for (std::size_t i = 0; i < last; ++i) {
    const std::ptrdiff_t k = static_cast<std::ptrdiff_t>(i);
    const Vec3 a = at(k - 1), b = at(k), c = at(k + 1), d = at(k + 2);
    out.push_back(b);
    for (const double t : {1. / 3., 2. / 3.}) {
      const double t2 = t * t, t3 = t2 * t;
      out.push_back((b * 2. + (c - a) * t + (a * 2. - b * 5. + c * 4. - d) * t2 + (b * 3. - a - c * 3. + d) * t3) * 0.5);
    }
  }
  out.push_back(p[last]);
  return out;
}

struct Ball {
  Vec3 centre;
  double depth = 0;
  float radiusPx = 0;
  Color color;
};

struct Stroke {
  std::vector<PointF> pts;
  Color color;
  float width = 1;
};

class SpaceRenderer {
 public:
  SpaceRenderer(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view)
      : painter_(painter), scene_(scene), style_(style), view_(view), cam_(scene.view), toViewer_(scene.view.towardsViewer()) {
    cx_ = view.widthPx / 2.0 - view.centre.X * view.pixelsPerUnit;
    cy_ = view.heightPx / 2.0 + view.centre.Y * view.pixelsPerUnit;
  }

  void run(const GridSettings* grid) {
    painter_.begin(view_.widthPx, view_.heightPx, style_.background);
    if (grid) drawGrid(painter_, *grid, view_);
    collectBalls();
    planSheet();
    if (sheet_) drawSheet();

    for (const ScenePath& l : scene_.potential) addPath(l, style_.potentialLine, style_.potentialThickness);
    for (const ScenePath& l : scene_.electric) addPath(l, style_.electricLine, style_.electricThickness);

    // back to front: the lines behind a ball, the ball, the lines before it, the next ball, ...
    for (std::size_t slab = 0; slab <= balls_.size(); ++slab) {
      for (const Stroke& s : strokes_[slab]) painter_.strokePath(s.pts.data(), s.pts.size(), 0, false, s.color, s.width);
      if (slab < balls_.size()) drawBall(balls_[slab]);
    }
    drawTriad();
    painter_.end();
  }

 private:
  PointF toScreen(const Vec3& p) const {
    const Vec2 flat = cam_.project(p);
    return PointF{static_cast<float>(cx_ + flat.X * view_.pixelsPerUnit), static_cast<float>(cy_ - flat.Y * view_.pixelsPerUnit)};
  }

  void collectBalls() {
    double nearest = 0, farthest = 0;
    bool any = false;
    for (const SceneCharge& c : scene_.charges) {
      Ball b;
      b.centre = c.centre;
      b.depth = cam_.depth(c.centre);
      b.radiusPx = scene_.chargeRadius > 0 ? static_cast<float>(scene_.chargeRadius * view_.pixelsPerUnit)
                                           : (c.sizePx > 0.f ? c.sizePx * view_.styleScale : style_.chargeRadiusPx);
      b.color = chargeColor(style_, c.charge);
      balls_.push_back(b);
      nearest = any ? std::max(nearest, b.depth) : b.depth;
      farthest = any ? std::min(farthest, b.depth) : b.depth;
      any = true;
    }
    std::stable_sort(balls_.begin(), balls_.end(), [](const Ball& a, const Ball& b) { return a.depth < b.depth; });
    strokes_.resize(balls_.size() + 1);
    // the depth range the fade runs over: the charges and two units around them
    far_ = farthest - 2.;
    near_ = nearest + 2.;
  }

  // The sheet: the cut plane of the equipotentials, a rectangle around the charges.
  void planSheet() {
    if (scene_.potential.empty() || scene_.charges.empty()) return;
    sheetZ_ = 0;
    for (const ScenePath& l : scene_.potential)
      if (!l.space.empty()) {
        sheetZ_ = l.space.front().Z;
        break;
      }
    double x0 = scene_.charges[0].centre.X, x1 = x0, y0 = scene_.charges[0].centre.Y, y1 = y0;
    for (const SceneCharge& c : scene_.charges) {
      x0 = std::min(x0, c.centre.X);
      x1 = std::max(x1, c.centre.X);
      y0 = std::min(y0, c.centre.Y);
      y1 = std::max(y1, c.centre.Y);
    }
    const double pad = std::max(2., 0.35 * std::max(x1 - x0, y1 - y0)) + scene_.chargeRadius;
    x0_ = std::floor(x0 - pad);
    x1_ = std::ceil(x1 + pad);
    y0_ = std::floor(y0 - pad);
    y1_ = std::ceil(y1 + pad);
    if (!(x1_ - x0_ < 1e4 && y1_ - y0_ < 1e4)) return;
    sheet_ = true;
    sheetColor_ = mix(style_.background, Color{90, 130, 220}, 0.13);
  }

  bool behindSheet(const Vec3& p) const {
    if (!sheet_ || std::abs(toViewer_.Z) < 1e-6) return false;
    const double t = (sheetZ_ - p.Z) / toViewer_.Z;  // along the line of sight towards the viewer
    if (t <= 1e-6) return false;
    const double x = p.X + toViewer_.X * t, y = p.Y + toViewer_.Y * t;
    return x >= x0_ && x <= x1_ && y >= y0_ && y <= y1_;
  }

  void drawSheet() {
    const Vec3 corners[4] = {Vec3(x0_, y0_, sheetZ_), Vec3(x1_, y0_, sheetZ_), Vec3(x1_, y1_, sheetZ_), Vec3(x0_, y1_, sheetZ_)};
    PointF pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = toScreen(corners[i]);
    painter_.fillPolygon(pts, 4, sheetColor_);

    const Color gridColor = mix(sheetColor_, Color{60, 90, 170}, 0.22);
    const Color edgeColor = mix(sheetColor_, Color{60, 90, 170}, 0.5);
    double step = 1;
    while ((x1_ - x0_) / step > 40 || (y1_ - y0_) / step > 40) step *= 2;
    const auto line = [this](const Vec3& a, const Vec3& b, Color color) {
      // in short pieces, so a ball hides only the part behind it
      const int pieces = std::max(1, static_cast<int>(std::ceil((b - a).Length() / 0.25)));
      std::vector<Vec3> p;
      p.reserve(static_cast<std::size_t>(pieces) + 1);
      for (int i = 0; i <= pieces; ++i) p.push_back(a + (b - a) * (static_cast<double>(i) / pieces));
      addPolyline(p, color, 1.f, false);
    };
    for (double x = x0_; x <= x1_ + 1e-9; x += step) {
      const bool edge = x == x0_ || x + step > x1_ + 1e-9;
      line(Vec3(x, y0_, sheetZ_), Vec3(x, y1_, sheetZ_), edge ? edgeColor : gridColor);
    }
    for (double y = y0_; y <= y1_ + 1e-9; y += step) {
      const bool edge = y == y0_ || y + step > y1_ + 1e-9;
      line(Vec3(x0_, y, sheetZ_), Vec3(x1_, y, sheetZ_), edge ? edgeColor : gridColor);
    }
    // where the sheet cuts a ball
    if (scene_.chargeRadius > 0)
      for (const SceneCharge& c : scene_.charges) {
        const double h = c.centre.Z - sheetZ_;
        if (std::abs(h) >= scene_.chargeRadius) continue;
        const double r = std::sqrt(scene_.chargeRadius * scene_.chargeRadius - h * h);
        std::vector<Vec3> ring;
        for (int k = 0; k <= 64; ++k) {
          const double a = 2. * kPi * k / 64.;
          ring.push_back(Vec3(c.centre.X + r * std::cos(a), c.centre.Y + r * std::sin(a), sheetZ_));
        }
        addPolyline(ring, mix(chargeColor(style_, c.charge), Color{0, 0, 0}, 0.45), 1.f, false);
      }
  }

  void addPath(const ScenePath& path, Color color, float width) {
    if (!path.space.empty())
      addPolyline(smooth(path.space, path.closed), color, width, true);
    else
      addPolyline(flatten(path.points), color, width, true);
  }

  // Cuts a polyline in space into strokes of one drawing slab and one look, and files them.
  void addPolyline(const std::vector<Vec3>& p, Color color, float width, bool fade) {
    if (p.size() < 2) return;
    const float w = std::max(1.f, width);
    Stroke run;
    std::size_t runSlab = 0;
    int runLook = -1;
    const auto flush = [&] {
      if (run.pts.size() >= 2) strokes_[runSlab].push_back(std::move(run));
      run = Stroke{};
    };
    PointF prev = toScreen(p[0]);
    for (std::size_t i = 0; i + 1 < p.size(); ++i) {
      const PointF next = toScreen(p[i + 1]);
      const Vec3 mid = (p[i] + p[i + 1]) * 0.5;
      const double depth = cam_.depth(mid);
      // on screen at all?
      const float margin = 10.f;
      const bool visible = !(std::max(prev.x, next.x) < -margin || std::min(prev.x, next.x) > static_cast<float>(view_.widthPx) + margin ||
                             std::max(prev.y, next.y) < -margin || std::min(prev.y, next.y) > static_cast<float>(view_.heightPx) + margin);
      if (!visible) {
        flush();
        runLook = -1;
        prev = next;
        continue;
      }
      std::size_t slab = 0;
      while (slab < balls_.size() && balls_[slab].depth < depth) ++slab;
      const double nearness = std::clamp((depth - far_) / (near_ - far_), 0., 1.);
      const int step = fade ? std::min(kFadeSteps - 1, static_cast<int>(nearness * kFadeSteps)) : kFadeSteps - 1;
      const bool behind = behindSheet(mid);
      const int look = step * 2 + (behind ? 1 : 0);
      if (look != runLook || slab != runSlab || run.pts.empty()) {
        flush();
        runSlab = slab;
        runLook = look;
        const double t = (step + 0.5) / kFadeSteps;
        Color c = fade ? mix(color, style_.background, kFadeFar * (1. - t)) : color;
        if (behind) c = mix(c, sheetColor_, kBehindSheet);
        run.color = c;
        run.width = fade ? w * static_cast<float>(0.75 + 1.0 * t) : w;
        run.width = std::max(run.width, 1.f);
        run.pts.push_back(prev);
      }
      run.pts.push_back(next);
      prev = next;
    }
    flush();
  }

  // A lit sphere out of flat discs: a dark rim, then discs that shrink towards the highlight (upper left).
  void drawBall(const Ball& b) const {
    const PointF c = toScreen(b.centre);
    const float r = b.radiusPx;
    if (!(r > 0.f) || c.x + r < 0 || c.x - r > static_cast<float>(view_.widthPx) || c.y + r < 0 || c.y - r > static_cast<float>(view_.heightPx)) return;
    Color base = b.color;
    if (behindSheet(b.centre)) base = mix(base, sheetColor_, kBehindSheet);
    const Color rim = mix(base, Color{0, 0, 0}, 0.5);
    const Color light = mix(base, Color{255, 255, 255}, 0.8);
    painter_.fillCircle(c, r, rim);
    const int layers = r < 6.f ? 4 : r < 40.f ? 14 : 28;
    for (int i = 1; i <= layers; ++i) {
      const double f = static_cast<double>(i) / layers;
      const Color color = f < 0.4 ? mix(rim, base, f / 0.4) : mix(base, light, (f - 0.4) / 0.6);
      const float shift = static_cast<float>(0.36 * f) * r;
      painter_.fillCircle(PointF{c.x - shift, c.y - shift}, r * static_cast<float>(1. - 0.94 * f), color);
    }
  }

  // The world axes in the lower left corner, the nearest drawn last, each with its letter.
  void drawTriad() const {
    const float k = view_.styleScale > 0 ? view_.styleScale : 1.f;
    const float len = 30.f * k;
    const PointF o{46.f * k, static_cast<float>(view_.heightPx) - 46.f * k};
    struct Axis {
      Vec3 dir;
      Color color;
      char letter;
    };
    Axis axes[3] = {{Vec3(1, 0, 0), Color{200, 50, 50}, 'x'}, {Vec3(0, 1, 0), Color{40, 150, 60}, 'y'}, {Vec3(0, 0, 1), Color{50, 90, 220}, 'z'}};
    std::stable_sort(std::begin(axes), std::end(axes), [this](const Axis& a, const Axis& b) { return cam_.depth(a.dir) < cam_.depth(b.dir); });
    for (const Axis& a : axes) {
      const Vec2 flat = cam_.project(a.dir);
      const PointF tip{o.x + static_cast<float>(flat.X) * len, o.y - static_cast<float>(flat.Y) * len};
      const PointF shaft[2] = {o, tip};
      painter_.strokePath(shaft, 2, 0, false, a.color, 2.f * k);
      const double l = flat.Length();
      const float ux = l > 0.15 ? static_cast<float>(flat.X / l) : 0.7f, uy = l > 0.15 ? static_cast<float>(-flat.Y / l) : -0.7f;
      const PointF at{tip.x + ux * 10.f * k, tip.y + uy * 10.f * k};
      const float s = 4.f * k;
      const auto stroke = [&](std::initializer_list<PointF> pts) {
        std::vector<PointF> v;
        for (const PointF& p : pts) v.push_back(PointF{at.x + p.x * s, at.y + p.y * s});
        painter_.strokePath(v.data(), v.size(), 0, false, a.color, 1.5f * k);
      };
      if (a.letter == 'x') {
        stroke({{-1, -1}, {1, 1}});
        stroke({{-1, 1}, {1, -1}});
      } else if (a.letter == 'y') {
        stroke({{-1, -1}, {0, 0}, {1, -1}});
        stroke({{0, 0}, {0, 1}});
      } else {
        stroke({{-1, -1}, {1, -1}, {-1, 1}, {1, 1}});
      }
    }
  }

  Painter& painter_;
  const FieldScene& scene_;
  const DrawStyle& style_;
  const View& view_;
  Camera cam_;
  Vec3 toViewer_;
  double cx_ = 0, cy_ = 0;
  std::vector<Ball> balls_;                   // far to near
  std::vector<std::vector<Stroke>> strokes_;  // [i]: behind ball i (and before ball i - 1)
  double far_ = 0, near_ = 1;
  bool sheet_ = false;
  double sheetZ_ = 0, x0_ = 0, x1_ = 0, y0_ = 0, y1_ = 0;
  Color sheetColor_;
};

}  // namespace

void renderSceneSpace(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view, const GridSettings* grid) {
  SpaceRenderer(painter, scene, style, view).run(grid);
}

}  // namespace ef
