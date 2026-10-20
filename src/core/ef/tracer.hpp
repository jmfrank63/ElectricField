#pragma once
// Port of FieldLinesCalculator::CalcThread::{CalculateElectricFieldLine, CalculateEquipotential, PostCalculateEquipotential}
// (FieldLinesCalculator.cpp:23-150) and FieldLine::AddPoint (FieldLine.h:18-29).
// Direction functors = FunctorForE / FunctorForV (FieldLinesCalculator.h:49-77).
#include <cmath>
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

struct PotentialLine : FieldLine {
  double potential = 0;  // the LABEL (job.old_potential), not the potential at the start point
  Vec2 weightCenter;
};

// Cooperative cancellation is polled every kCancelStride outer steps. D-02 (amended 2026-10-10) polls about every 1024 steps:
// the measured worst Debug+UBSan gap between polls is about 11 ms (Dormand-Prince); the relaxed atomic load costs nothing measurable.
inline constexpr std::size_t kCancelStride = 1024;

// v2.6 phase 2, revised after the owner's zoom report: in the adapted regime consecutive posted
// equipotential rings keep at least this many logical px of on-screen spacing (spacing dr = dV/|E|,
// on-screen dr * viewScale; viewScale is the GUI's zoom-relative logical scale). 1.25 was still dense
// enough that 1-px-wide rings merged into a solid gray field; 6 keeps them clearly distinct.
inline constexpr double kRingPx = 6.0;

// Hooks used by the tracers. Default = standalone (never cancelled, no equipotential jobs).
struct NoHooks {
  bool cancelled() const noexcept { return false; }
  void postEquipotential(const Vec2& /*startPoint*/, double /*potentialLabel*/) {}
};

namespace detail {
struct FieldDirection {  // FunctorForE: unit field, flipped for negative charges
  const ChargeSystem* field;
  int chargeSign;
  Vec2 operator()(double /*t*/, const Vec2& pos) const {
    const Vec2 v = field->ENormalized(pos);
    return chargeSign > 0 ? v : -v;
  }
};
struct PerpendicularDirection {  // FunctorForV: unit vector perpendicular to E
  const ChargeSystem* field;
  Vec2 operator()(double /*t*/, const Vec2& pos) const {
    Vec2 v = field->E(pos);
    const double temp = v.X;
    v.X = -v.Y;
    v.Y = temp;
    return v.Normalize();
  }
};
}  // namespace detail

// PostCalculateEquipotential (FieldLinesCalculator.cpp:23-49), minus the queue/thread restart. State `oldPotential`
// lives in the caller (it is job.old_potential in the original). Returns true when an equipotential job must be posted.
// FIX CORE-07: the original posted `FieldLineJob job(job)` (self-copy; MSVC legacy lookup silently copied the parameter,
// Clang/GCC/MSVC-/permissive- read an uninitialised object). The intended copy is of the PARENT job: the posted job
// carries the parent's current point and the UPDATED label.
inline bool shouldPostEquipotential(const ChargeSystem& field, const ComputeParams& p, const Seed& seed, const Vec2& point,
                                    double& oldPotential) {
  if (!seed.first || !p.calculateEquipotentials) return false;
  const double potential = field.Potential(point);
  // v2.6 (owner zoom report, replaces the phase-2 hard skip): in the adapted regime the effective
  // potential interval stretches so consecutive rings stay >= kRingPx logical px apart on screen
  // (spacing dr = dV/|E|; require dV >= |E| * kRingPx / viewScale). The hard skip this replaces left
  // two artifacts: rings posted at the 1-2 px boundary merged into a solid gray field, and in an
  // asymmetric field the |E| threshold crossed mid-ring, dashing arcs into moire stripes. Stretching
  // the interval instead spaces every posted ring by construction. |E| is exact for multi-charge
  // fields. Legacy (viewScale <= kRefPpu) uses the plain interval, unchanged.
  const double interval =
      p.adapts() ? std::max(p.potentialInterval, field.E(point).Length() * kRingPx / p.viewScale) : p.potentialInterval;
  if (sign(seed.charge) * sign(potential) > 0 && std::abs(oldPotential - potential) >= interval) {
    if (oldPotential == 0)
      oldPotential = std::floor(potential / interval) * interval;
    else
      oldPotential += sign(potential - oldPotential) * interval;
    return true;
  }
  return false;
}

struct TraceStatus {
  bool cancelled = false;
};

template <typename Solver, typename Hooks>
FieldLine traceElectricLine(Solver& solver, const ChargeSystem& field, const ComputeParams& p, const Seed& seed, Hooks& hooks,
                            TraceStatus* status = nullptr, std::size_t maxStepsOverride = 0) {
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
  double oldPotential = 0;  // job.old_potential
  const double px = static_cast<double>(p.pixelsPerUnit);

  const bool record = sign(seed.totalCharge) == sign(seed.charge);
  if (record) line.addPoint(point, px);

  for (std::size_t i = 0; i < steps; ++i) {
    if ((i % kCancelStride) == 0 && hooks.cancelled()) {
      if (status) status->cancelled = true;
      break;
    }
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

    if (shouldPostEquipotential(field, p, seed, point, oldPotential)) hooks.postEquipotential(point, oldPotential);
  }
  return line;
}

// CalculateEquipotential (FieldLinesCalculator.cpp:113-150).
template <typename Solver, typename Hooks>
PotentialLine traceEquipotential(Solver& solver, const ChargeSystem& field, const ComputeParams& p, const Vec2& startPoint,
                                 double potentialLabel, Hooks& hooks, TraceStatus* status = nullptr,
                                 std::size_t maxStepsOverride = 0) {
  PotentialLine line;
  line.potential = potentialLabel;
  detail::PerpendicularDirection functorV{&field};
  Vec2 point = startPoint;
  const double px = static_cast<double>(p.pixelsPerUnit);
  // v2.6 phase 2: record ring anchors every ~8 logical px at the TARGET view (legacy keeps the 100 px
  // scale; viewScale is the GUI's zoom-relative logical scale). The near-charge adapted rings are
  // small on screen; at the legacy recording density they store 4-5 anchors and the bezier fit renders
  // them as wild swoops instead of circles.
  const double recordPx = p.adapts() ? p.viewScale : px;

  double t = 0;
  std::size_t numSteps = solver.IsAdaptive() ? 800000 : 1500000;
  if (maxStepsOverride != 0 && maxStepsOverride < numSteps) numSteps = maxStepsOverride;
  double step = solver.IsAdaptive() ? 0.001 : 0.0001;
  double next_step = step;

  line.addPoint(startPoint, recordPx);
  line.weightCenter = startPoint;

  for (std::size_t i = 0; i < numSteps; ++i) {
    if ((i % kCancelStride) == 0 && hooks.cancelled()) {
      if (status) status->cancelled = true;
      break;
    }
    point = solver.SolveStep(functorV, point, t, step, next_step, 1E-3, 0.01);

    line.addPoint(point, recordPx);
    line.weightCenter += point * step;  // 'step' plays the role of the portion of the curve 'weight'

    t += step;
    if (solver.IsAdaptive()) step = next_step;

    const double dist = (startPoint - point).Length();
    // 261020-ef1: the "within 6 px of the start" close test must be measured at the scale the ring is
    // drawn at. It used the FIXED 100 px/unit reference (px), so in the zoom-adapted regime
    // (recordPx = viewScale > 100) the world threshold 6/100 was larger than whole ring diameters:
    // rings closed after a fraction of their circle with a straight chord up to 6/100*viewScale px
    // long (the "fan of blades" the owner reported: arcs bunching into near-straight jogged lines on
    // one side, clean rings on the other). recordPx is exactly px in the legacy regime, so ef-cli /
    // MFC-parity output is bit-identical.
    if (dist * recordPx < 6. && t > 2. * dist) {
      line.points.push_back(startPoint);  // close the loop
      line.weightCenter /= t;             // divide by the whole 'weight' of the curve
      break;
    }
  }
  return line;
}

// Phase 1 entry point kept for the existing tests: standalone single-line trace, no cancellation, no equipotential jobs.
template <typename Solver>
FieldLine traceFieldLine(Solver& solver, const ChargeSystem& field, const ComputeParams& p, const Seed& seed,
                         std::size_t maxStepsOverride = 0) {
  NoHooks hooks;
  return traceElectricLine(solver, field, p, seed, hooks, nullptr, maxStepsOverride);
}

}  // namespace ef
