// The Conductor charge model: every body's outer surface is an equipotential, lines end perpendicular.
#include "doctest/doctest.h"

#include <cmath>
#include <vector>

#include "ef/charge_system.hpp"
#include "ef/conductor.hpp"
#include "ef/forces.hpp"
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

ChargeSystem conductors(double radius, std::initializer_list<Charge> charges) {
  ChargeSystem s;
  s.radius = radius;
  s.model = ChargeModel::Conductor;
  for (const Charge& c : charges) s.charges.push_back(c);
  prepare(s);
  return s;
}

Vec3 direction(int i, int n) {
  const double h = 1. - (2. * i + 1.) / n;
  const double ring = std::sqrt(1. - h * h);
  return Vec3(ring * std::cos(2.4 * i), ring * std::sin(2.4 * i), h);
}

// Largest sine of the angle between the field and the surface normal over the outer surface of ball i.
double worstTilt(const ChargeSystem& field, std::size_t ball) {
  double worst = 0;
  for (int i = 0; i < 300; ++i) {
    const Vec3 n = direction(i, 300);
    const Vec3 p = field.charges[ball].position3() + n * (field.radius * 1.0001);
    if (field.BallAt(p) >= 0) continue;  // inside a neighbour: not outer surface
    const Vec3 e = field.E(p);
    if (e.Length() < 1e-3) continue;  // a crease or a neutral spot: no direction to speak of
    const Vec3 tangential = e - n * (e * n);
    worst = std::max(worst, tangential.Length() / e.Length());
  }
  return worst;
}
}  // namespace

TEST_CASE("conductor: only balls with the Conductor model get a surface solution") {
  ChargeSystem uniform;
  uniform.radius = 1;
  uniform.charges.push_back(at(1, 0, 0));
  prepare(uniform);
  CHECK(uniform.surface == nullptr);
  ChargeSystem points;
  points.model = ChargeModel::Conductor;  // radius 0: point charges ignore the model
  points.charges.push_back(at(1, 0, 0));
  prepare(points);
  CHECK(points.surface == nullptr);
  CHECK_FALSE(points.conductor());
}

TEST_CASE("conductor: an isolated ball is the point charge outside, with potential q / R and no field inside") {
  const ChargeSystem field = conductors(2, {at(3, 1, -1, 0.5)});
  REQUIRE(field.surface != nullptr);
  CHECK(field.surface->residual < 1e-3);
  CHECK(field.surface->bodyPotential[0] == doctest::Approx(1.5).epsilon(1e-6));
  const Vec3 p(5, 2, -1);
  const Vec3 d = p - field.charges[0].position3();
  CHECK(field.Potential(p) == doctest::Approx(3. / d.Length()).epsilon(1e-6));
  CHECK((field.E(p) - d * (3. / std::pow(d.Length(), 3))).Length() < 1e-6);
  CHECK(field.E(Vec3(1.5, -1, 0.5)).Length() == 0.0);
  CHECK(field.Potential(Vec3(1.5, -1, 0.5)) == doctest::Approx(1.5).epsilon(1e-6));
  CHECK(field.Potential(Vec3(60, 0, 0)) == doctest::Approx(3. / (Vec3(60, 0, 0) - field.charges[0].position3()).Length()).epsilon(1e-6));  // far sum
}

TEST_CASE("conductor: lines meet the surface at a right angle where the uniform model does not") {
  // two opposite balls, a gap of one radius between them
  ChargeSystem field = conductors(1, {at(1, -1.5, 0), at(-1, 1.5, 0)});
  CHECK(field.surface->residual < 0.01);
  CHECK(worstTilt(field, 0) < 0.03);
  CHECK(worstTilt(field, 1) < 0.03);
  // the charge is conserved: far away it is still a dipole, close by the balls attract more than points
  const std::vector<Vec3> f = forces(field);
  CHECK(f[0].X > 1.0 / 9);            // stronger than Coulomb between the centres (induction)
  CHECK(f[0].X == doctest::Approx(-f[1].X).epsilon(0.02));
  CHECK(std::abs(f[0].Y) < 1e-3);

  ChargeSystem uniform = field;
  uniform.model = ChargeModel::Uniform;
  prepare(uniform);
  CHECK(worstTilt(uniform, 0) > 0.1);  // the same balls with fixed uniform charge: oblique
}

TEST_CASE("conductor: touching and overlapping balls are one equipotential body with the summed charge") {
  // +3 overlapping -1: one body of charge +2; a far -2 ball
  const ChargeSystem field = conductors(1, {at(3, 0, 0), at(-1, 1.2, 0), at(-2, 9, 0)});
  REQUIRE(field.surface->bodyPotential.size() == 2);
  CHECK(field.surface->residual < 0.03);
  const std::vector<BodyFlux> flux = bodyFlux(field);
  CHECK(flux[0].out - flux[0].in == doctest::Approx(2.0).epsilon(0.02));
  CHECK(flux[1].out - flux[1].in == doctest::Approx(-2.0).epsilon(0.02));
  CHECK(worstTilt(field, 2) < 0.03);
  // +1 touching -1 with nothing else around neutralises completely: no field, no lines
  const ChargeSystem neutral = conductors(1, {at(1, -1, 0), at(-1, 1, 0)});
  CHECK(neutral.E(Vec3(0, 3, 0)).Length() < 1e-6);
  ComputeParams p = lightParams();
  p.calculateEquipotentials = false;
  CHECK(planSeeds3(neutral, p).empty());
  // balls on one centre add their charge
  const ChargeSystem stacked = conductors(1, {at(1, 0, 0), at(2, 0, 0)});
  CHECK(stacked.Potential(Vec3(4, 0, 0)) == doctest::Approx(0.75).epsilon(1e-6));
}

TEST_CASE("conductor: a session computes it; lines start and end on surfaces") {
  ChargeSystem field;
  field.radius = 1;
  field.model = ChargeModel::Conductor;  // not prepared: the session does that itself
  field.charges = {at(1, -2, 0), at(-1, 2, 0)};
  ComputeSession session(field, lightParams());
  REQUIRE_FALSE(session.wait().failed);
  FieldResult r;
  REQUIRE(session.takeResult(r));
  CHECK(r.electric.size() >= 3);
  for (const auto& line : r.electric) {
    CHECK((line.points.front() - field.charges[0].position).Length() == doctest::Approx(1.0).epsilon(1e-6));
    CHECK((line.points.back() - field.charges[1].position).Length() == doctest::Approx(1.0).epsilon(1e-6));
  }
  CHECK_FALSE(r.potential.empty());
  // the total energy of two far balls tends to the two self energies q^2 / (2R)
  const ChargeSystem far = conductors(1, {at(1, -50, 0), at(-1, 50, 0)});
  CHECK(potentialEnergy(far) == doctest::Approx(1.0 - 0.01).epsilon(1e-3));
}

TEST_CASE("conductor: the surface error stays small down to a gap of a fifth of the radius") {
  for (const double gap : {2.0, 0.5, 0.2}) {
    const ChargeSystem f = conductors(1, {at(1, -1 - gap / 2, 0), at(-1, 1 + gap / 2, 0)});
    CHECK(f.surface->residual < 0.01);
    CHECK(worstTilt(f, 0) < 0.01);
  }
}
