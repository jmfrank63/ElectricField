#pragma once
// Port of FieldLinesCalculator::CalcThread::CalculateElectricFieldLine (FieldLinesCalculator.cpp:51-110)
// and FieldLine::AddPoint (FieldLine.h:18-29); direction functor = FunctorForE (FieldLinesCalculator.h:49-60), without the equipotential posting (Phase 2).
#include <cstddef>
#include <vector>

#include "ef/charge_system.hpp"
#include "ef/math.hpp"
#include "ef/seeds.hpp"

namespace ef {

struct FieldLine {
  std::vector<Vec2> points;

  // Thinning thresholds are in pixels of the 100 px/unit scale (dif) and in WORLD units (pt.Length()), as in the original.
  void addPoint(const Vec2& pt, double pixelsPerUnit) {
    if (points.empty()) {
      points.push_back(pt);
      return;
    }
    const Vec2& old = points.back();
    const double dif = (old - pt).Length() * pixelsPerUnit;
    const double len = pt.Length();
    if ((dif >= 256. && len >= 2000 && len < 3000) || (dif >= 64. && len > 1000 && len < 2000) || (dif >= 8. && len <= 1000))
      points.push_back(pt);
  }
};

namespace detail {
struct FieldDirection {  // FunctorForE
  const ChargeSystem* field;
  int chargeSign;
  Vec2 operator()(double /*t*/, const Vec2& pos) const {
    const Vec2 v = field->ENormalized(pos);
    return chargeSign > 0 ? v : -v;
  }
};
}  // namespace detail

template <typename Solver>
FieldLine traceFieldLine(Solver& solver, const ChargeSystem& field, const ComputeParams& p, const Seed& seed,
                         std::size_t maxStepsOverride = 0) {
  FieldLine line;
  detail::FieldDirection functorE{&field, sign(seed.charge)};
  Vec2 point = seed.point;

  std::size_t steps = 40000;
  double precision = 0.01;  // dummy values, as in the original
  double max_step = 0.1;
  if (seed.hasDifferentSigns && seed.totalCharge == 0)
    steps = 5000000;
  else if (seed.hasDifferentSigns)
    steps = 1000000;
  if (maxStepsOverride != 0 && maxStepsOverride < steps) steps = maxStepsOverride;

  double t = 0;
  double step = 0.001;
  double next_step = step;
  const double px = static_cast<double>(p.pixelsPerUnit);

  const bool record = sign(seed.totalCharge) == sign(seed.charge);
  if (record) line.addPoint(point, px);

  for (std::size_t i = 0; i < steps; ++i) {
    const double len = point.Length() * px;
    const bool needs_precision = (len < 3000);

    if (solver.IsAdaptive()) {
      precision = (needs_precision ? 0.001 : 0.1);
      max_step = (needs_precision ? 0.001 : 0.1);
    } else {
      step = (needs_precision ? 0.001 : 0.5);
    }

    point = solver.SolveStep(functorE, point, t, step, next_step, precision, max_step);

    if (record) line.addPoint(point, px);
    if (field.HitCharge(point, p)) break;

    t += step;
    if (solver.IsAdaptive()) step = next_step;
  }
  return line;
}

}  // namespace ef
