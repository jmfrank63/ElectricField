// Balls are seeded per body, in proportion to the field flowing through the body's outer surface.
#include "doctest/doctest.h"

#include <cmath>
#include <vector>

#include "ef/charge_system.hpp"
#include "ef/csv.hpp"
#include "ef/seeds.hpp"
#include "ef/session.hpp"
#include "test_params.hpp"

using namespace ef;

namespace {
Charge at(int q, double x, double y, double z = 0) {
  Charge c;
  c.charge = q;
  c.position = Vec2(x, y);
  c.z = z;
  return c;
}

ChargeSystem balls(double radius, std::initializer_list<Charge> charges) {
  ChargeSystem s;
  s.radius = radius;
  for (const Charge& c : charges) s.charges.push_back(c);
  return s;
}

FieldResult computeAll(const ChargeSystem& field, const ComputeParams& p) {
  ComputeSession session(field, p);
  REQUIRE_FALSE(session.wait().failed);
  FieldResult r;
  REQUIRE(session.takeResult(r));
  return r;
}

// The outward flux density at a seed: E . n on the ball the seed belongs to.
double fluxAt(const ChargeSystem& field, const Seed& s) {
  const Vec3 n = (s.point3 - field.charges[s.chargeIndex].position3()).Normalize();
  return field.E(s.point3) * n;
}
}  // namespace

TEST_CASE("flux: an isolated ball gets |q| * lines seeds, evenly spaced") {
  const ChargeSystem field = balls(1, {at(2, 0.5, -1)});
  ComputeParams p = lightParams();  // 4 lines per unit charge
  p.calculateEquipotentials = false;
  const std::vector<Seed> plane = planSeeds(field, p);
  REQUIRE(plane.size() == 8);
  for (std::size_t i = 1; i < plane.size(); ++i)
    CHECK(plane[i].angle - plane[i - 1].angle == doctest::Approx(2 * kPi / 8).epsilon(0.02));
  for (const Seed& s : plane) CHECK((s.point - field.charges[0].position).Length() == doctest::Approx(1.0));
  CHECK(planSeeds3(field, p).size() == 8);
  CHECK(expectedLineCount(field, p) == 8);
}

TEST_CASE("flux: what leaves a body minus what enters it is its net charge (Gauss), for any overlap") {
  // a mixed body (+3 overlapping -1), a separate -1 ball, and a neutral pair sharing one centre
  const ChargeSystem field = balls(1, {at(3, 0, 0), at(-1, 1.2, 0.3, 0.4), at(-1, 6, 0), at(1, -5, 4, 1), at(-1, -5, 4, 1)});
  CHECK(field.bodies() == std::vector<int>{0, 0, 1, 2, 2});
  const std::vector<BodyFlux> flux = bodyFlux(field);
  REQUIRE(flux.size() == 3);
  CHECK(flux[0].out - flux[0].in == doctest::Approx(2.0).epsilon(0.01));
  CHECK(flux[0].in > 0.01);  // the mixed body takes lines back in on its negative side
  CHECK(flux[1].out - flux[1].in == doctest::Approx(-1.0).epsilon(0.01));
  CHECK(std::abs(flux[2].out - flux[2].in) < 0.01);  // +1 and -1 on one centre: neutral
  // point charges: all out or all in
  const std::vector<BodyFlux> points = bodyFlux(balls(0, {at(2, 0, 0), at(-1, 3, 0)}));
  CHECK(points[0].out == 2.0);
  CHECK(points[1].in == 1.0);
}

TEST_CASE("flux: a mixed body is seeded where the field leaves it, and its lines end on a surface") {
  const ChargeSystem field = balls(1, {at(1, -0.6, 0), at(-1, 0.6, 0)});  // one neutral body
  ComputeParams p = lightParams();
  p.linesPerUnitCharge = 16;
  p.calculateEquipotentials = false;
  const std::vector<Seed> seeds = planSeeds(field, p);
  REQUIRE_FALSE(seeds.empty());
  for (const Seed& s : seeds) {
    CHECK(s.charge == 1);  // total charge 0: traced along the field
    CHECK(fluxAt(field, s) > 0);
    CHECK(s.point.X < 0.0);  // the positive side
  }
  const FieldResult r = computeAll(field, p);
  CHECK(r.electric.size() == seeds.size());
  for (const auto& line : r.electric) {
    const Vec2 end = line.points.back();
    CHECK((end - field.charges[1].position).Length() == doctest::Approx(1.0).epsilon(1e-6));  // on the negative ball
  }
}

TEST_CASE("flux: a negative total is seeded where the field enters and traced against it") {
  const ChargeSystem field = balls(1, {at(-2, 0, 0), at(1, 1.0, 0)});
  ComputeParams p = lightParams();
  p.calculateEquipotentials = false;
  const std::vector<Seed> seeds = planSeeds3(field, p);
  REQUIRE_FALSE(seeds.empty());
  for (const Seed& s : seeds) {
    CHECK(s.charge == -1);
    CHECK(fluxAt(field, s) < 0);
  }
  // about |net charge| * lines arrive from infinity, plus the few that loop from the positive side
  CHECK(seeds.size() >= 4);
  CHECK(seeds.size() <= 8);
}

TEST_CASE("flux: opposite balls sliding into each other lose their lines steadily, down to none") {
  ComputeParams p = lightParams();
  p.linesPerUnitCharge = 16;
  p.calculateEquipotentials = false;
  std::size_t previous = 1000;
  for (const double d : {1.9, 1.2, 0.6, 0.2, 0.0}) {
    const ChargeSystem field = balls(1, {at(1, 0, 0), at(-1, d, 0)});
    const std::size_t n = planSeeds3(field, p).size();
    CHECK(n <= previous);
    previous = n;
  }
  CHECK(previous == 0);  // the same centre: a neutral ball
  // and such a system still computes: no lines, no failure
  const FieldResult r = computeAll(balls(1, {at(1, 0, 0), at(-1, 0, 0)}), p);
  CHECK(r.electric.empty());
  // two like balls on one centre are one ball of the summed charge
  CHECK(planSeeds3(balls(1, {at(1, 0, 0), at(1, 0, 0)}), p).size() == 32);
}

TEST_CASE("flux: the bodies table lists charge, dipole moment and flux per body") {
  const ChargeSystem field = balls(1, {at(1, -0.6, 0), at(-1, 0.6, 0), at(2, 8, 0)});
  const std::string csv = bodiesCsv(field);
  CHECK(csv.rfind("body,balls,charge,x,y,z,px,py,pz,dipole,flux_out,flux_in\n", 0) == 0);
  CHECK(csv.find("\n0,2,0,0,0,0,-1.2,0,0,1.2,") != std::string::npos);  // neutral pair, dipole 1.2 towards -x
  CHECK(csv.find("\n1,1,2,8,0,0,0,0,0,0,") != std::string::npos);
}
