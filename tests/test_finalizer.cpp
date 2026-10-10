#include "doctest/doctest.h"

#include <vector>

#include "ef/finalizer.hpp"
#include "ef/presets.hpp"

using namespace ef;

namespace {
PotentialLine eq(double potential, double cx, double cy) {
  PotentialLine l;
  l.potential = potential;
  l.weightCenter = Vec2(cx, cy);
  l.points = {Vec2(cx, cy), Vec2(cx + 1, cy)};
  return l;
}
ChargeSystem preset(const char* slug) {
  for (const auto& p : presets())
    if (p.slug == slug) return makeSystem(p);
  return {};
}
}  // namespace

TEST_CASE("de-dup shortcut: exactly two charges of different sign keep every raw line, in the given order (D-05)") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  std::vector<PotentialLine> raw{eq(0.5, 0, 0), eq(0.5, 0, 0), eq(0.1, 3, 3)};
  const auto out = dedupEquipotentials(raw, dipole, p);
  REQUIRE(out.size() == 3);
  CHECK(out[0].potential == 0.5);
  CHECK(out[2].potential == 0.1);
}

TEST_CASE("de-dup: same potential (within interval/3) and weight centres closer than 2*chargeRadius are duplicates") {
  const ChargeSystem four = preset("four-charges-same-sign");
  ComputeParams p;  // interval 0.1, chargeRadius 25 px => 0.5 world units
  // group A at 0.4: three copies of one curve (centres 0.1 apart) + a distinct curve 5 units away; group B at 0.8 (final group is NOT de-duplicated: MFC quirk)
  std::vector<PotentialLine> raw{eq(0.4, 0.0, 0), eq(0.4, 0.1, 0), eq(0.401, 0.2, 0), eq(0.4, 5.0, 0), eq(0.8, 0, 0), eq(0.8, 0, 0)};
  const auto out = dedupEquipotentials(raw, four, p);
  // group A -> 2 survivors (duplicates merged), group B appended as is (2 lines, duplicates kept)
  CHECK(out.size() == 4);
}

TEST_CASE("de-dup is deterministic: independent of the order of equal-potential input lines is NOT required, but a fixed input gives a fixed output") {
  const ChargeSystem four = preset("four-charges-same-sign");
  ComputeParams p;
  std::vector<PotentialLine> raw{eq(0.4, 1, 1), eq(0.4, -1, 1), eq(0.4, 1, -1), eq(0.8, 0, 0)};
  const auto a = dedupEquipotentials(raw, four, p);
  const auto b = dedupEquipotentials(raw, four, p);
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) CHECK(a[i].weightCenter == b[i].weightCenter);
}

TEST_CASE("finalize drops empty sink lines and applies the Bezier fit") {
  const ChargeSystem dipole = preset("dipole");
  ComputeParams p;
  FieldLine empty;
  FieldLine real;
  for (int i = 0; i < 7; ++i) real.points.emplace_back(i * 1.0, (i % 2) * 0.2);
  const auto r = finalize({real, empty}, {}, dipole, p);
  REQUIRE(r.electric.size() == 1);
  CHECK(r.electric[0].points[0] == real.points[0]);
  CHECK_FALSE(r.electric[0].points[1] == real.points[1]);  // inner control point rewritten
}

// ElectricFieldDoc.cpp:211 keeps a curve when the distance between weight centres is STRICTLY greater than
// 2 * chargeRadius, so exactly 0.5 world units apart (25 px at the default scale, twice: 2 * 0.25) means duplicate.
// The trailing 0.8 line flushes the 0.4 group; the last potential group is never filtered (D-05).
TEST_CASE("de-dup boundary: weight centres exactly 2*chargeRadius apart are duplicates, just beyond are kept (strict >)") {
  const ChargeSystem four = preset("four-charges-same-sign");
  ComputeParams p;  // interval 0.1, chargeRadius 25 px = 0.5 world units
  {
    std::vector<PotentialLine> raw{eq(0.4, 0, 0), eq(0.4, 0.5, 0), eq(0.8, 0, 0)};
    CHECK(dedupEquipotentials(raw, four, p).size() == 2);
  }
  {
    std::vector<PotentialLine> raw{eq(0.4, 0, 0), eq(0.4, 0.5000001, 0), eq(0.8, 0, 0)};
    CHECK(dedupEquipotentials(raw, four, p).size() == 3);
  }
}
