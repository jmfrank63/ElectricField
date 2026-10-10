#pragma once
// Port of FieldLine::AdjustForBezier (FieldLine.cpp:183-236). Pure functions, no ASSERT, no MFC.
#include <cfloat>
#include <cmath>
#include <vector>

#include "ef/vec2.hpp"

namespace ef {

// Smallest |divi| accepted before the guarded fallback kicks in. The original only had ASSERT(abs(divi) > DBL_MIN)
// (debug-only, MFC macro) and then divided by it. Normal groups have divi ~ 0.05 (t1 ~ 1/4, t2 ~ 1/2).
inline constexpr double kBezierMinDivisor = 1e-9;

// Computes the two inner control points of one cubic group (pt0, [pt1, pt2], pt3) so that the curve passes through
// pt1 and pt2 at parameters t1, t2. Returns false (outputs untouched) for degenerate input (coincident points,
// non-finite result): the caller then keeps pt1/pt2 unchanged.
// PARITY QUIRK (D-07): t3 = t1 + t2 + |pt3-pt2| (t2 already contains t1), as in the original; do not "fix" it.
inline bool adjustForBezier(const Vec2& pt0, const Vec2& pt1, const Vec2& pt2, const Vec2& pt3, Vec2& o1, Vec2& o2) {
  double t1 = (pt1 - pt0).Length();
  double t2 = t1 + (pt2 - pt1).Length();
  const double t3 = t1 + t2 + (pt3 - pt2).Length();

  t1 /= t3;
  t2 /= t3;

  const double divi = 3. * t1 * t2 * (1. - t1) * (1. - t2) * (t2 - t1);
  if (!(std::abs(divi) > kBezierMinDivisor)) return false;  // also catches NaN (t3 == 0)

  double a = t2 * (1. - t2) * (pt1.X - (1. - t1) * (1. - t1) * (1. - t1) * pt0.X - t1 * t1 * t1 * pt3.X);
  double b = t1 * (1. - t1) * (pt2.X - (1. - t2) * (1. - t2) * (1. - t2) * pt0.X - t2 * t2 * t2 * pt3.X);
  const double xo1 = (t2 * a - t1 * b) / divi;
  const double xo2 = ((1. - t1) * b - (1. - t2) * a) / divi;

  a = t2 * (1. - t2) * (pt1.Y - (1. - t1) * (1. - t1) * (1. - t1) * pt0.Y - t1 * t1 * t1 * pt3.Y);
  b = t1 * (1. - t1) * (pt2.Y - (1. - t2) * (1. - t2) * (1. - t2) * pt0.Y - t2 * t2 * t2 * pt3.Y);
  const double yo1 = (t2 * a - t1 * b) / divi;
  const double yo2 = ((1. - t1) * b - (1. - t2) * a) / divi;

  if (!std::isfinite(xo1) || !std::isfinite(xo2) || !std::isfinite(yo1) || !std::isfinite(yo2)) return false;
  o1 = Vec2(xo1, yo1);
  o2 = Vec2(xo2, yo2);
  return true;
}

// In-place over a point list laid out p0,[p1,p2,p3],[p4,p5,p6],...: rewrites the inner control points of every
// complete group; a trailing incomplete group is left alone (drawn as line segments). Returns the number of groups
// that fell back to the unadjusted points.
inline std::size_t adjustForBezier(std::vector<Vec2>& points) {
  std::size_t fallbacks = 0;
  for (std::size_t i = 0; i + 3 < points.size(); i += 3) {
    Vec2 o1, o2;
    if (adjustForBezier(points[i], points[i + 1], points[i + 2], points[i + 3], o1, o2)) {
      points[i + 1] = o1;
      points[i + 2] = o2;
    } else {
      ++fallbacks;
    }
  }
  return fallbacks;
}

}  // namespace ef
