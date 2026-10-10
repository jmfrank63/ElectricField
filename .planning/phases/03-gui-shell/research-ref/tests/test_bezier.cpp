#include "doctest/doctest.h"

#include <cmath>
#include <vector>

#include "ef/bezier.hpp"

using namespace ef;

namespace {
Vec2 cubic(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, double t) {
  const double u = 1. - t;
  return p0 * (u * u * u) + p1 * (3. * u * u * t) + p2 * (3. * u * t * t) + p3 * (t * t * t);
}
bool finite(const Vec2& v) { return std::isfinite(v.X) && std::isfinite(v.Y); }
}  // namespace

TEST_CASE("AdjustForBezier: the fitted cubic passes through pt1 and pt2 at the chord-length parameters (MFC t3 quirk kept)") {
  const Vec2 p0(0, 0), p1(1, 0.5), p2(2.2, 0.4), p3(3, -0.3);
  Vec2 o1, o2;
  REQUIRE(adjustForBezier(p0, p1, p2, p3, o1, o2));
  // parameters exactly as the original computes them (t3 = t1 + t2 + |p3-p2|, NOT the total length)
  double t1 = (p1 - p0).Length();
  double t2 = t1 + (p2 - p1).Length();
  const double t3 = t1 + t2 + (p3 - p2).Length();
  t1 /= t3;
  t2 /= t3;
  const Vec2 a = cubic(p0, o1, o2, p3, t1);
  const Vec2 b = cubic(p0, o1, o2, p3, t2);
  CHECK(a.X == doctest::Approx(p1.X).epsilon(1e-9));
  CHECK(a.Y == doctest::Approx(p1.Y).epsilon(1e-9));
  CHECK(b.X == doctest::Approx(p2.X).epsilon(1e-9));
  CHECK(b.Y == doctest::Approx(p2.Y).epsilon(1e-9));
}

TEST_CASE("AdjustForBezier regression (CORE-07): coincident points no longer divide by zero") {
  const Vec2 a(0, 0), b(1, 1);
  Vec2 o1(-7, -7), o2(-7, -7);
  CHECK_FALSE(adjustForBezier(a, a, a, a, o1, o2));  // t3 == 0 -> NaN in the original
  CHECK_FALSE(adjustForBezier(a, a, b, b, o1, o2));  // t1 == 0 -> divi == 0
  CHECK_FALSE(adjustForBezier(a, b, b, b, o1, o2));  // t2 == t1 -> divi == 0
  CHECK(o1.X == -7);                                 // outputs untouched on fallback
  CHECK(o2.Y == -7);

  std::vector<Vec2> pts{a, a, a, a, b, b, b};  // two groups, both degenerate
  const std::vector<Vec2> before = pts;
  const std::size_t fallbacks = adjustForBezier(pts);
  CHECK(fallbacks >= 1);
  for (std::size_t i = 0; i < pts.size(); ++i) {
    CHECK(finite(pts[i]));
    CHECK(pts[i] == before[i]);  // fallback keeps the original polyline points
  }
}

TEST_CASE("adjustForBezier(list) touches only complete groups p0,[p1,p2],p3 and keeps shared endpoints") {
  std::vector<Vec2> pts;
  for (int i = 0; i < 9; ++i) pts.emplace_back(i * 1.0, (i % 2) * 0.3);  // 9 points = 2 groups + 1 trailing endpoint-less
  const std::vector<Vec2> before = pts;
  adjustForBezier(pts);
  for (std::size_t i = 0; i < pts.size(); i += 3) CHECK(pts[i] == before[i]);  // on-curve points never move
  CHECK_FALSE(pts[1] == before[1]);
  CHECK_FALSE(pts[4] == before[4]);

  std::vector<Vec2> three(before.begin(), before.begin() + 3);  // p0,p1,p2 only: incomplete group
  adjustForBezier(three);
  for (std::size_t i = 0; i < 3; ++i) CHECK(three[i] == before[i]);
}
