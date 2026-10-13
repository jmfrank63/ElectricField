// Portability regressions (CORE-07, D-09) and equipotential job posting.
#define _USE_MATH_DEFINES  // force the M_PI macro to exist on every libc: the core must not redefine it
#include <cmath>

#include "doctest/doctest.h"

#include <vector>

#include "ef/method.hpp"
#include "ef/presets.hpp"
#include "ef/tracer.hpp"

using namespace ef;

#ifdef M_PI
static_assert(kPi == M_PI, "kPi must equal the libc M_PI (and compile next to it: no redefinition)");
#endif

namespace {
struct CollectHooks {
  struct Child {
    Vec2 start;
    double label;
  };
  std::vector<Child> children;
  bool stop = false;
  bool cancelled() const { return stop; }
  void postEquipotential(const Vec2& p, double label) { children.push_back({p, label}); }
};

ChargeSystem preset(const char* slug) {
  for (const auto& p : presets())
    if (p.slug == slug) return makeSystem(p);
  return {};
}
}  // namespace

TEST_CASE("equipotential jobs are posted along the first line of a charge (abs(double) regression: |dV| >= interval must not truncate to 0)") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  const auto seeds = planSeeds(dipole, p);
  REQUIRE(seeds[0].first);
  CollectHooks h;
  rk::Euler<Vec2> solver;
  traceElectricLine(solver, dipole, p, seeds[0], h);
  REQUIRE(h.children.size() > 20);  // ~ (V(r=0.25) - 0) / 0.1 posts: measured 75 jobs for the whole dipole
  // labels advance by exactly one potentialInterval per post (MFC: old_potential += sign(...) * potentialInterval)
  for (std::size_t i = 1; i < h.children.size(); ++i)
    CHECK(std::abs(h.children[i].label - h.children[i - 1].label) == doctest::Approx(p.potentialInterval).epsilon(1e-9));
}

TEST_CASE("FieldLineJob job(job) regression (CORE-07): a posted job is a copy of the PARENT's current state, never uninitialised data") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  for (const auto& seed : planSeeds(dipole, p)) {
    if (!seed.first) continue;
    CollectHooks h;
    rk::Euler<Vec2> solver;
    traceElectricLine(solver, dipole, p, seed, h, nullptr, 20000);
    for (const auto& c : h.children) {
      CHECK(std::isfinite(c.start.X));
      CHECK(std::isfinite(c.start.Y));
      CHECK(std::isfinite(c.label));
      // the start point lies on the parent line: within the plane region the line can reach, and its potential matches
      // the label to within one interval (garbage from a self-copy would not)
      CHECK(c.start.Length() < 5000.);
      CHECK(std::abs(dipole.Potential(c.start) - c.label) <= p.potentialInterval + 1e-9);
      CHECK(sign(c.label) == sign(seed.charge));
    }
  }
}

TEST_CASE("no equipotential jobs when equipotentials are off or for non-first seeds") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  auto seeds = planSeeds(dipole, p);
  CollectHooks h;
  rk::Euler<Vec2> solver;
  traceElectricLine(solver, dipole, p, seeds[1], h);  // not the first seed of its charge
  CHECK(h.children.empty());
  p.calculateEquipotentials = false;
  seeds = planSeeds(dipole, p);
  traceElectricLine(solver, dipole, p, seeds[0], h);
  CHECK(h.children.empty());
}

// v2.6 phase 2 invariant: viewScale at/below kRefPpu posts EXACTLY the legacy jobs (same count, labels
// and points) - the default view and ef-cli (which never sets viewScale) stay bit-identical.
TEST_CASE("viewScale <= kRefPpu posts exactly the legacy equipotential jobs (ef-cli/parity invariant)") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams legacy;
  const auto seeds = planSeeds(dipole, legacy);
  REQUIRE(seeds[0].first);
  CollectHooks hLegacy;
  rk::Euler<Vec2> solver;
  traceElectricLine(solver, dipole, legacy, seeds[0], hLegacy);
  REQUIRE(hLegacy.children.size() > 20);

  ComputeParams atRef = legacy;
  atRef.viewScale = 100.0;  // exactly the reference ppu
  CollectHooks hRef;
  traceElectricLine(solver, dipole, atRef, seeds[0], hRef);
  REQUIRE(hRef.children.size() == hLegacy.children.size());
  for (std::size_t i = 0; i < hLegacy.children.size(); ++i) {
    CHECK(hRef.children[i].label == hLegacy.children[i].label);
    CHECK(hRef.children[i].start.X == hLegacy.children[i].start.X);
    CHECK(hRef.children[i].start.Y == hLegacy.children[i].start.Y);
  }
}

// v2.6 phase 2 ring gate: with |E| = 1/r^2 (single +1 charge at the origin), the on-screen spacing is
// kRingPx px at r = sqrt(kRingPx / (interval * viewScale)) - no posts below it, posts allowed beyond.
TEST_CASE("ring gate: sub-pixel equipotential spacing is not posted above kRefPpu (v2.6 phase 2)") {
  ChargeSystem field;
  field.charges.push_back(Charge{1, Vec2{0, 0}});
  ComputeParams p;
  p.potentialInterval = 0.1;
  p.viewScale = 1000.0;
  const double rGate = std::sqrt(ef::kRingPx / (p.potentialInterval * p.viewScale));
  const double rBlock = 0.5 * rGate;  // well inside: |E| * kRingPx > interval * viewScale
  const double rAllow = 2.0 * rGate;  // well outside: |E| * kRingPx < interval * viewScale

  Seed s;
  s.first = true;
  s.charge = 1;
  double oldPotential = 0;
  CHECK_FALSE(shouldPostEquipotential(field, p, s, Vec2{rBlock, 0}, oldPotential));
  CHECK(shouldPostEquipotential(field, p, s, Vec2{rAllow, 0}, oldPotential));

  // Legacy never gates: the same blocked point posts once the label condition holds.
  p.viewScale = 0;
  oldPotential = 0;
  CHECK(shouldPostEquipotential(field, p, s, Vec2{rBlock, 0}, oldPotential));
}

TEST_CASE("equipotential trace closes on itself and records the weight centre (CalculateEquipotential)") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  CollectHooks h;
  rk::Euler<Vec2> solver;
  // a point at distance 1 from the + charge: a small closed loop around it
  const Vec2 start(-2 + 1.0, 0.0);
  const PotentialLine line = traceEquipotential(solver, dipole, p, start, 0.7, h);
  REQUIRE(line.points.size() > 10);
  CHECK(line.points.front() == line.points.back());  // closed: startPoint pushed again
  CHECK(line.potential == 0.7);                       // label, not V(start)
  CHECK(std::isfinite(line.weightCenter.X));
  CHECK(std::isfinite(line.weightCenter.Y));
  // all points of the loop are at (nearly) the same potential
  for (const auto& pt : line.points) CHECK(std::abs(dipole.Potential(pt) - dipole.Potential(start)) < 0.05);
}

TEST_CASE("cancel flag is polled: a stopped tracer returns after at most one stride and reports cancelled") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  const auto seeds = planSeeds(dipole, p);
  CollectHooks h;
  h.stop = true;
  TraceStatus st;
  rk::Euler<Vec2> solver;
  const FieldLine line = traceElectricLine(solver, dipole, p, seeds[0], h, &st);
  CHECK(st.cancelled);
  CHECK(line.points.size() <= 1);  // only the seed point
}

// Numerics policy (CORE-07): -ffp-contract=off and no fast-math. With contraction on, `x * y - 1.0` becomes one fused
// multiply-add on aarch64 (always) and on x86-64 with FMA (Zig compiles for the native CPU) and yields -2^-60 instead of 0.
#ifdef __FAST_MATH__
#error "the core must not be built with -ffast-math / -Ofast"
#endif
TEST_CASE("numerics policy: no fused multiply-add contraction, no fast-math (-ffp-contract=off)") {
  volatile double x = 1.0 + 0x1p-30;  // volatile: the compiler cannot fold the expression at compile time
  volatile double y = 1.0 - 0x1p-30;
  const double r = x * y - 1.0;  // exact product is 1 - 2^-60; rounded product is 1.0
  CHECK(r == 0.0);               // fused would give -2^-60
}
