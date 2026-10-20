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

// v2.6 (owner zoom report, replaces the phase-2 gate test): in the adapted regime the effective
// interval stretches so CONSECUTIVE posted rings keep >= kRingPx logical px of on-screen spacing - no
// mush (1-2 px rings merged into a solid gray field) and no dashed arcs (the old hard skip crossed
// mid-ring in an asymmetric field). Walk a probe outward like the tracer does and pin the spacing.
TEST_CASE("adapted equipotential posts keep kRingPx on-screen spacing; legacy unchanged") {
  ChargeSystem field;
  field.charges.push_back(Charge{1, Vec2{0, 0}});
  ComputeParams p;
  p.potentialInterval = 0.1;
  p.viewScale = 1000.0;
  Seed s;
  s.first = true;
  s.charge = 1;

  std::vector<double> postedR, postedV;
  double oldPotential = 0;
  for (double r = 0.05; r <= 3.0; r += 0.0005) {
    if (shouldPostEquipotential(field, p, s, Vec2{r, 0}, oldPotential)) {
      postedR.push_back(r);
      postedV.push_back(oldPotential);
    }
  }
  REQUIRE(postedR.size() >= 3);
  // Rings start right away (no empty hole around the charge) and the potential decreases outward.
  CHECK(postedR.front() == doctest::Approx(0.05).epsilon(0.02));
  for (std::size_t i = 1; i < postedR.size(); ++i) {
    const double dr = postedR[i] - postedR[i - 1];
    const double dV = postedV[i] - postedV[i - 1];
    CHECK(dV < 0.0);  // outward = falling potential for a + charge
    // The on-screen radial gap (dr * viewScale, dr = dV / |E|) stays near kRingPx - the step
    // quantization of the walk (and of the real trace) wobbles it by well under a pixel; what the
    // stretch must guarantee is DISTINCT rings, i.e. never anywhere near the old 1.25 px mush.
    CHECK(dr * p.viewScale >= ef::kRingPx * 0.7);
  }

  // Legacy (no view scale) never stretches: within the stretch region the plain interval posts far
  // denser (the region ends where |E| * kRingPx == interval * viewScale, r = sqrt(kRingPx/80) here).
  p.viewScale = 0;
  std::vector<double> legacyR;
  oldPotential = 0;
  for (double r = 0.05; r <= 3.0; r += 0.0005)
    if (shouldPostEquipotential(field, p, s, Vec2{r, 0}, oldPotential)) legacyR.push_back(r);
  std::size_t adaptedInside = 0, legacyInside = 0;
  const double rStretch = std::sqrt(ef::kRingPx / (0.1 * 1000.0));
  for (const double r : postedR)
    if (r <= rStretch) ++adaptedInside;
  for (const double r : legacyR)
    if (r <= rStretch) ++legacyInside;
  CHECK(legacyInside > adaptedInside * 3);
  CHECK(legacyR.front() == doctest::Approx(0.05).epsilon(0.02));  // posts immediately at the walk start
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

// v2.6 phase 2 (WR-05): the adapted ring ANCHOR density - a regression of recordPx (tracer.hpp) back to
// the legacy density passed every other suite (the sampling proof samples line ink, not ring shape).
// The ring PATH does not depend on the params (same solver, same steps), so the adapted trace is the
// same loop recorded ~8x denser: anchor count pins the scaling two-sided, and every recorded segment
// respects addPoint's >= 8 px thinning threshold AT THE TARGET VIEW.
TEST_CASE("adapted rings record anchors at the target view density (v2.6 phase 2 WR-05)") {
  const ChargeSystem dipole = preset("dipole");
  rk::Euler<Vec2> solver;
  CollectHooks h;
  const Vec2 start(-2 + 1.0, 0.0);  // a small closed loop around the + charge (as in the closure test)

  ComputeParams legacy;
  const PotentialLine a = traceEquipotential(solver, dipole, legacy, start, 0.7, h);
  REQUIRE(a.points.size() > 10);
  REQUIRE(a.points.front() == a.points.back());  // closed

  ComputeParams adapted;
  adapted.viewScale = 800.0;
  const PotentialLine b = traceEquipotential(solver, dipole, adapted, start, 0.7, h);
  REQUIRE(b.points.size() > 10);
  REQUIRE(b.points.front() == b.points.back());
  CHECK(b.potential == a.potential);
  // ~8x denser (8 px anchors at 800 vs 8 px at 100): strictly more, with slack for step quantisation
  CHECK(b.points.size() > 4 * a.points.size());
  // thinning contract at the target view: recorded segments are >= 8 target-view px (the loop closure
  // pushes startPoint again, so the last - closing - segment is exempt)
  for (std::size_t i = 1; i + 1 < b.points.size(); ++i)
    CHECK((b.points[i] - b.points[i - 1]).Length() * adapted.viewScale >= 8.0 - 1e-9);
}

// 261020-ef1 fan regression: the ring-closure test measured "within 6 px of the start" at the FIXED
// 100 px/unit reference. In the zoom-adapted regime that world threshold (0.06) exceeds whole ring
// diameters, so rings closed after a fraction of their circle with a straight chord up to 150
// target-view px long - the owner's screenshot: arcs bunching into near-straight jogged "blades" on
// one side, clean rings on the other. The close test now measures at the TARGET-view scale.
TEST_CASE("adapted equipotential rings close around the FULL circle (261020-ef1)") {
  ChargeSystem field;
  field.charges.push_back(Charge{-1, Vec2{0, 0}});  // the owner's scene: a single -1 charge
  CollectHooks h;
  rk::Euler<Vec2> solver;
  ComputeParams p;
  p.viewScale = 2500.0;                 // deep zoom: the ring sits at a few hundredths of a world unit
  const Vec2 start(-0.0304, 0.00605);   // r ~= 0.031 world = ~77 target-view px from the charge
  const PotentialLine line = traceEquipotential(solver, field, p, start, -30.0, h);
  REQUIRE(line.points.size() > 20);
  REQUIRE(line.points.front() == line.points.back());  // closed (startPoint pushed again)

  // Unwrap the recorded angles: the trace must sweep the whole circle (the old bug stopped after
  // ~147 degrees, when the oversized chord test fired near the antipode).
  double turn = 0.0, prev = 0.0, rmin = 1e9, rmax = 0.0;
  for (std::size_t i = 0; i + 1 < line.points.size(); ++i) {  // the last point repeats the first
    const double a = std::atan2(line.points[i].Y, line.points[i].X);
    if (i == 0) {
      prev = a;
      continue;
    }
    double d = a - prev;
    while (d > kPi) d -= 2.0 * kPi;
    while (d < -kPi) d += 2.0 * kPi;
    turn += d;
    prev = a;
    const double r = line.points[i].Length();
    rmin = std::min(rmin, r);
    rmax = std::max(rmax, r);
  }
  CHECK(std::abs(turn) > 2.0 * kPi * 0.95);
  // ... and stays a circle: the radius spread is a hair of the target-view spacing, not the old
  // 30-40 px eccentric wobbling.
  CHECK((rmax - rmin) * p.viewScale < 10.0);

  // Legacy regime unchanged: recordPx == px there, so the closure behaviour and the recorded points
  // are exactly what they were before the fix (a full-size legacy ring: a 0.03 world ring records
  // only ~3 anchors at the 100 px/unit legacy density).
  ChargeSystem dipole = preset("dipole");
  ComputeParams legacy;
  const Vec2 onAxis(-2.0 + 1.0, 0.0);  // a 1-world-unit ring around the + charge (as in the closure test)
  const PotentialLine baseline = traceEquipotential(solver, dipole, legacy, onAxis, 0.7, h);
  REQUIRE(baseline.points.size() > 20);
  CHECK(baseline.points.front() == baseline.points.back());
  const PotentialLine again = traceEquipotential(solver, dipole, legacy, onAxis, 0.7, h);
  REQUIRE(again.points.size() == baseline.points.size());
  for (std::size_t i = 0; i < baseline.points.size(); ++i)
    CHECK(again.points[i] == baseline.points[i]);
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

// v2.6 phase 2 regression for the adapted radii: a line born on the adapted seed circle (0.01 world,
// INSIDE the naive adapted hit radius) must escape its own charge and fly to the partner. With an
// uncorrected hitRadius > seedRadius every adapted trace died at its first step (n = 1, seed only).
TEST_CASE("adapted view scale: the field line escapes its own charge and ends on the partner (v2.6 phase 2)") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  p.viewScale = 800.0;
  REQUIRE(p.seedRadius() >= p.hitRadius());
  const auto seeds = planSeeds(dipole, p);
  REQUIRE(seeds[0].first);
  rk::Euler<Vec2> solver;
  const FieldLine line = traceFieldLine(solver, dipole, p, seeds[0]);
  REQUIRE(line.points.size() > 10);
  const Vec2 last = line.points.back();
  CHECK((last - dipole.charges[1].position).Length() < 0.1);  // ends within the adapted hit zone of the - charge
  for (const auto& pt : line.points) CHECK(std::isfinite(pt.X + pt.Y));
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

// v2.6 (review): the spacing contract on REAL traces of asymmetric fields - |E| varies along a ring
// there, which is exactly what dashed the old hard-skip gate mid-ring. Walk the actual posted jobs
// of traced field lines (the compute session's flow) and pin the on-screen spacing between
// consecutive jobs of the same parent line.
TEST_CASE("adapted equipotential jobs keep kRingPx spacing on real asymmetric-field traces") {
  rk::Euler<Vec2> solver;
  ComputeParams p;
  p.potentialInterval = 0.1;
  p.viewScale = 1000.0;
  const char* slugs[] = {"dipole", "two-unequal-charges-different-signs", "quadrupole"};
  for (const char* slug : slugs) {
    const ChargeSystem field = preset(slug);
    const auto seeds = planSeeds(field, p);
    REQUIRE(seeds.size() >= 2);
    CAPTURE(slug);
    // Only a charge's FIRST seed posts equipotentials (shouldPostEquipotential's contract); trace
    // the first seed of each of the first two charges.
    unsigned traced = 0;
    for (std::size_t si = 0; si < seeds.size() && traced < 2; ++si) {
      if (!seeds[si].first) continue;
      ++traced;
      CollectHooks h;
      traceElectricLine(solver, field, p, seeds[si], h);
      REQUIRE(h.children.size() >= 3);
      CAPTURE(si);
      CAPTURE(h.children.size());
      // Measured worst case across these fields: 4.0 px (the first rings right at the seed, where
      // |E| halves between consecutive rings and the stretch is evaluated one step late at the
      // detection point) - still 3.2x the old 1.25 px mush threshold, i.e. clearly distinct rings.
      // Chords also run slightly under the arc spacing on bent traces, hence 0.5, not 1.0.
      for (std::size_t i = 1; i < h.children.size(); ++i)
        CHECK((h.children[i].start - h.children[i - 1].start).Length() * p.viewScale >= ef::kRingPx * 0.5);
    }
    REQUIRE(traced == 2);
  }
}
