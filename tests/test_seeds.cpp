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

TEST_CASE("an empty charge system yields no seeds and zero expected lines") {
  const ChargeSystem field;
  const ComputeParams p;
  CHECK(planSeeds(field, p).empty());
  CHECK(expectedLineCount(field, p) == 0);
}

// v2.6 phase 2: seed/hit radii adapt to the view ONLY above kRefPpu; at/below it every value is
// exactly the legacy one, so the default view and ef-cli (which never sets viewScale) are bit-identical.
TEST_CASE("zoom-adaptive radii: legacy at/below kRefPpu, clamped shrink above (v2.6 phase 2)") {
  ComputeParams p;  // viewScale 0 = legacy
  CHECK(p.seedRadius() == doctest::Approx(0.25));
  CHECK(p.hitRadius() == doctest::Approx(25.0 / 1.5 / 100.0));
  p.viewScale = 100.0;  // exactly the reference ppu: still legacy
  CHECK(p.seedRadius() == doctest::Approx(0.25));
  CHECK(p.hitRadius() == doctest::Approx(25.0 / 1.5 / 100.0));
  CHECK(p.seedRadius() == doctest::Approx(p.chargeRadiusWorld()));

  p.viewScale = 150.0;  // just above the reference: ~2 px seed circle (2/150); hit is capped at the seed
  CHECK(p.seedRadius() == doctest::Approx(2.0 / 150.0));
  CHECK(p.hitRadius() == doctest::Approx(2.0 / 150.0));  // never above the seed: lines must escape their own charge

  p.viewScale = 800.0;  // 2/800 is below the kMinSeedWorld floor: the seed floored, the hit rides on it
  CHECK(p.seedRadius() == doctest::Approx(ComputeParams::kMinSeedWorld));
  CHECK(p.hitRadius() == doctest::Approx(ComputeParams::kMinSeedWorld));

  p.viewScale = 2000.0;  // both target radii are below the kMinSeedWorld floor: floored
  CHECK(p.seedRadius() == doctest::Approx(ComputeParams::kMinSeedWorld));
  CHECK(p.hitRadius() == doctest::Approx(ComputeParams::kMinSeedWorld));

  p.viewScale = 2500.0;  // the new ceiling: the floored seed circle is exactly the drawn dot on screen
  CHECK(p.seedRadius() == doctest::Approx(ComputeParams::kMinSeedWorld));
  CHECK(p.seedRadius() * 2500.0 == doctest::Approx(25.0));
  CHECK(p.hitRadius() == doctest::Approx(p.seedRadius()));

  // Escape invariant over the whole adapted range: seed >= hit, or every line would end on its own
  // charge at the first tracer step (found by the renderSurface proof, fixed as a deviation).
  for (const double v : {101.0, 120.0, 150.0, 200.0, 400.0, 800.0, 1500.0, 2500.0}) {
    p.viewScale = v;
    CHECK(p.seedRadius() >= p.hitRadius());
  }

  // planSeeds places the seeds on the adapted circle (planSeeds reads p.seedRadius()).
  const ChargeSystem single = [] {
    ChargeSystem f;
    f.charges.push_back(Charge{1, Vec2{0, 0}});
    return f;
  }();
  p.viewScale = 150.0;
  for (const auto& s : planSeeds(single, p))
    CHECK((s.point - single.charges[s.chargeIndex].position).Length() == doctest::Approx(2.0 / 150.0));
}

// 261020-ef1: the seed circle is each charge's OWN effective drawn dot (its sizePx, else the Options
// default). A charge with its own small size used to seed (and hit) on the Options-sized circle: the
// lines stopped short of the drawn dot leaving the untraced annulus the owner screenshotted.
TEST_CASE("per-charge seed placement and hit targets (261020-ef1)") {
  ChargeSystem field;
  field.charges.push_back(Charge{1, Vec2{0, 0}, 0, "", 4.f});  // a 4 px dot of its own
  field.charges.push_back(Charge{-1, Vec2{5, 0}});             // no size: the 25 px Options default
  ComputeParams p;                                             // legacy regime

  const auto seeds = planSeeds(field, p);
  REQUIRE(seeds.size() >= 17);  // 16 on the + charge (source), 1 on the - (sink, equipotentials on)
  for (const auto& s : seeds) {
    const double r = (s.point - field.charges[s.chargeIndex].position).Length();
    if (s.chargeIndex == 0)
      CHECK(r == doctest::Approx(0.04));  // born on the 4 px dot's edge (4 px / 100 ppu)
    else
      CHECK(r == doctest::Approx(0.25));  // the unsized charge keeps the Options-sized circle
  }

  // Hit targets follow the same dots: a point 0.1 world from the 4 px charge is OUTSIDE its
  // 4/1.5/100 = 0.0267 hit circle (the old shared 25 px radius killed lines there), while the
  // unsized partner still hits at its 25/1.5/100 circle.
  CHECK(field.HitCharge(Vec2{0.02, 0}, p));
  CHECK_FALSE(field.HitCharge(Vec2{0.1, 0}, p));
  CHECK(field.HitCharge(Vec2{5.0 + 0.16, 0}, p));
  CHECK_FALSE(field.HitCharge(Vec2{5.0 + 0.17, 0}, p));
}
