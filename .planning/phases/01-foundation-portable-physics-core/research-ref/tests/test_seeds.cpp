#include "doctest/doctest.h"

#include <cmath>
#include <cstdlib>

#include "ef/method.hpp"
#include "ef/presets.hpp"
#include "ef/seeds.hpp"
#include "ef/tracer.hpp"

using namespace ef;

namespace {
int sourceSeedCount(const std::vector<Seed>& seeds, std::size_t chargeIndex) {
  int n = 0;
  for (const auto& s : seeds)
    if (s.chargeIndex == chargeIndex) ++n;
  return n;
}
}  // namespace

TEST_CASE("seeds are proportional to |q| * linesPerUnitCharge for every preset (sinks get exactly one)") {
  for (unsigned lines : {4u, 16u, 32u}) {
    for (bool equip : {true, false}) {
      ComputeParams p;
      p.linesPerUnitCharge = lines;
      p.calculateEquipotentials = equip;
      for (const auto& preset : presets()) {
        CAPTURE(preset.slug);
        CAPTURE(lines);
        CAPTURE(equip);
        const ChargeSystem field = makeSystem(preset);
        bool diff = false;
        const int total = field.totalCharge(diff);
        const auto seeds = planSeeds(field, p);
        int expectedAll = 0;
        for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
          const int q = field.charges[ci].charge;
          const bool source = sign(total) == sign(q);
          const int expected = source ? std::abs(q) * static_cast<int>(lines) : (equip ? 1 : 0);
          CHECK(sourceSeedCount(seeds, ci) == expected);
          expectedAll += expected;
        }
        CHECK(static_cast<int>(seeds.size()) == expectedAll);
        // seed geometry: every seed sits on the circle of radius chargeRadius/pixelsPerUnit (0.25) around its charge
        for (const auto& s : seeds) {
          const double r = (s.point - field.charges[s.chargeIndex].position).Length();
          CHECK(r == doctest::Approx(0.25).epsilon(1e-12));
        }
      }
    }
  }
}

TEST_CASE("expectedLineCount = sum(|q|*L) over sources + 1 per sink (negative totals no longer wrap)") {
  ComputeParams p;
  p.linesPerUnitCharge = 16;
  CHECK(expectedLineCount(makeSystem(presets()[0]), p) == 16 + 1);       // dipole
  CHECK(expectedLineCount(makeSystem(presets()[3]), p) == 32 + 1);       // +2 / -1
  CHECK(expectedLineCount(makeSystem(presets()[5]), p) == 4 * 16);       // four equal charges
  ChargeSystem negative;                                                  // all-negative: MFC wrapped unsigned here
  Charge c;
  c.charge = -2;
  negative.charges.push_back(c);
  CHECK(expectedLineCount(negative, p) == 32);
}

TEST_CASE("net-zero configurations: sign(0)==+1 makes the POSITIVE charges the sources") {
  ComputeParams p;
  for (const char* slug : {"dipole", "quadrupole", "two-dipoles-same-direction", "parallel-dipoles"}) {
    CAPTURE(slug);
    for (const auto& preset : presets()) {
      if (preset.slug != slug) continue;
      const ChargeSystem field = makeSystem(preset);
      bool diff = false;
      CHECK(field.totalCharge(diff) == 0);
      for (const auto& s : planSeeds(field, p)) {
        CHECK(s.isSource == (s.charge > 0));
        CHECK(s.totalCharge == 0);
      }
    }
  }
}

TEST_CASE("negative total charge flips the roles: negative charges are the sources") {
  ChargeSystem field;
  Charge a, b;
  a.charge = -2; a.position = Vec2(-2, 0);
  b.charge = 1;  b.position = Vec2(2, 0);
  field.charges = {a, b};
  ComputeParams p;
  const auto seeds = planSeeds(field, p);
  int nA = 0, nB = 0;
  for (const auto& s : seeds) (s.chargeIndex == 0 ? nA : nB)++;
  CHECK(nA == 32);
  CHECK(nB == 1);
}

TEST_CASE("seed angles: source starts at -pi - step/2, sink at +step/2; first seed is flagged") {
  ComputeParams p;
  p.linesPerUnitCharge = 16;
  const ChargeSystem field = makeSystem(presets()[0]);  // dipole
  const auto seeds = planSeeds(field, p);
  const double step = 2. * kPi / 16.;
  REQUIRE(seeds.size() == 17);
  CHECK(seeds[0].angle == doctest::Approx(-kPi - step / 2.).epsilon(1e-12));
  CHECK(seeds[0].first);
  CHECK_FALSE(seeds[1].first);
  CHECK(seeds[1].angle - seeds[0].angle == doctest::Approx(step).epsilon(1e-12));
  CHECK(seeds[16].chargeIndex == 1);
  CHECK(seeds[16].angle == doctest::Approx(2. * kPi / 16. / 2.).epsilon(1e-12));  // sink: step/2
  CHECK(seeds[16].first);
  CHECK_FALSE(seeds[16].isSource);
}

TEST_CASE("zero charges are skipped") {
  ChargeSystem field = makeSystem(presets()[0]);
  Charge z;
  z.charge = 0;
  z.position = Vec2(0, 5);
  field.charges.push_back(z);
  ComputeParams p;
  for (const auto& s : planSeeds(field, p)) CHECK(s.chargeIndex != 2);
}

TEST_CASE("every method traces a dipole field line from the + charge that ends on the - charge") {
  const ChargeSystem field = makeSystem(presets()[0]);
  for (const auto& m : kMethods) {
    CAPTURE(m.name);
    ComputeParams p;
    p.method = m.method;
    const auto seeds = planSeeds(field, p);
    // seed 4 is on the +x side half-way up: a typical "banana" line
    const Seed& s = seeds[4];
    REQUIRE(s.isSource);
    const FieldLine line = withSolver(m.method, [&](auto& solver) { return traceFieldLine(solver, field, p, s); });
    REQUIRE(line.points.size() > 10);
    const Vec2 last = line.points.back();
    CHECK((last - field.charges[1].position).Length() < 0.35);  // arrives next to the negative charge
    for (const auto& pt : line.points) CHECK(std::isfinite(pt.X + pt.Y));
  }
}

TEST_CASE("sink seeds record no points (they only start equipotentials)") {
  const ChargeSystem field = makeSystem(presets()[0]);
  ComputeParams p;
  const auto seeds = planSeeds(field, p);
  const FieldLine line = withSolver(Method::Euler, [&](auto& solver) { return traceFieldLine(solver, field, p, seeds.back(), 2000); });
  CHECK(line.points.empty());
}
