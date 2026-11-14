// 3D systems: charges with a z coordinate, the camera, lines traced in space, forces between charges.
#include "doctest/doctest.h"

#include <cmath>

#include "ef/charge_system.hpp"
#include "ef/cut_plane.hpp"
#include "ef/forces.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/seeds.hpp"
#include "ef/session.hpp"
#include "test_params.hpp"

using namespace ef;

namespace {
Charge at(int q, double x, double y, double z) {
  Charge c;
  c.charge = q;
  c.position = Vec2(x, y);
  c.z = z;
  return c;
}

FieldResult computeAll(const ChargeSystem& field, const ComputeParams& p) {
  ComputeSession session(field, p);
  REQUIRE_FALSE(session.wait().failed);
  FieldResult r;
  REQUIRE(session.takeResult(r));
  return r;
}
}  // namespace

TEST_CASE("3d: the default camera is the drawing plane, a turned one stays orthonormal") {
  const Camera plain;
  CHECK(plain.project(Vec3(1.5, -2.5, 7)) == Vec2(1.5, -2.5));
  CHECK(plain.depth(Vec3(1.5, -2.5, 7)) == 7.0);

  Camera cam;
  cam.yaw = 0.7;
  cam.pitch = -0.4;
  CHECK(cam.right() * cam.up() == doctest::Approx(0).epsilon(1e-12));
  CHECK(cam.right() * cam.towardsViewer() == doctest::Approx(0).epsilon(1e-12));
  CHECK(cam.up() * cam.towardsViewer() == doctest::Approx(0).epsilon(1e-12));
  CHECK(cam.right().Length() == doctest::Approx(1));
  CHECK(cam.up().Length() == doctest::Approx(1));
  const Vec3 w = cross(cam.right(), cam.up());  // right-handed: right x up points at the viewer
  CHECK((w - cam.towardsViewer()).Length() < 1e-12);

  Camera quarter;
  quarter.yaw = kPi / 2;  // the z axis now runs to the right of the screen
  CHECK(quarter.project(Vec3(0, 0, 3)).X == doctest::Approx(3));
  CHECK(std::abs(quarter.project(Vec3(3, 0, 0)).X) < 1e-12);
}

TEST_CASE("3d: in the plane the spatial field is the 2D field; a preset is not 3D") {
  ChargeSystem field = makeSystem(presets()[0]);
  CHECK_FALSE(field.is3D());
  for (const Vec2 p : {Vec2(0, 0), Vec2(-1.9, 0.05), Vec2(3, 4)}) {
    const Vec3 e = field.E(Vec3(p.X, p.Y, 0));
    CHECK(e.X == field.E(p).X);
    CHECK(e.Y == field.E(p).Y);
    CHECK(e.Z == 0.0);
    CHECK(field.Potential(Vec3(p.X, p.Y, 0)) == field.Potential(p));
  }
  // off the plane: Coulomb in space
  ChargeSystem one;
  one.charges.push_back(at(2, 1, 0, 0));
  const Vec3 e = one.E(Vec3(1, 0, 2));
  CHECK(e.Z == doctest::Approx(0.5));
  CHECK(e.X == 0.0);
  CHECK(one.Potential(Vec3(1, 0, 2)) == doctest::Approx(1.0));

  field.charges[0].z = 0.5;
  CHECK(field.is3D());
  ChargeSystem turned = makeSystem(presets()[0]);
  turned.camera.yaw = 0.1;
  CHECK(turned.is3D());
}

TEST_CASE("3d: seeds are spread over a sphere around the source charges only") {
  ChargeSystem field;
  field.charges.push_back(at(2, 0, 0, 1));
  field.charges.push_back(at(-1, 3, 0, 0));
  ComputeParams p = lightParams();
  const std::vector<Seed> seeds = planSeeds3(field, p);
  REQUIRE(seeds.size() == 8);  // |+2| * 4 lines; the sink gets none
  CHECK(expectedLineCount(field, p) == 8);
  Vec3 sum;
  for (const Seed& s : seeds) {
    CHECK(s.chargeIndex == 0);
    const Vec3 d = s.point3 - field.charges[0].position3();
    CHECK(d.Length() == doctest::Approx(p.seedRadiusFor(p.chargeRadiusPx)));
    sum += d;
  }
  CHECK(sum.Length() < 0.1 * p.seedRadiusFor(p.chargeRadiusPx) * 8);  // balanced over the sphere
}

TEST_CASE("3d: a dipole along the view axis computes; every line ends on the sink charge") {
  ChargeSystem field;
  field.charges.push_back(at(1, 0, 0, -2));
  field.charges.push_back(at(-1, 0, 0, 2));
  ComputeParams p = lightParams();
  const FieldResult r = computeAll(field, p);
  CHECK(r.electric.size() == 4);
  CHECK(r.potential.empty());  // the plane between the two charges is at potential 0: no level crosses it
  for (const auto& line : r.electric) {
    REQUIRE(line.points.size() >= 2);
    CHECK(line.points.back().Length() < p.hitRadiusFor(p.chargeRadiusPx));  // both charges project onto the origin
  }
  const FieldScene scene = buildScene(field, r);
  CHECK(scene.charges[0].depth == -2.0);
  CHECK(scene.charges[1].depth == 2.0);
  CHECK(scene.charges[0].position == Vec2(0, 0));
}

TEST_CASE("3d: turning the camera on a planar system projects charges and lines") {
  ChargeSystem field = makeSystem(presets()[0]);  // +1 at (-2, 0), -1 at (2, 0)
  field.camera.pitch = kPi / 2;                   // looking down the y axis: x stays, the screen's up is -z
  const FieldResult r = computeAll(field, lightParams());
  CHECK(r.electric.size() == 4);
  const FieldScene scene = buildScene(field, r);
  CHECK(scene.charges[0].position.X == doctest::Approx(-2));
  CHECK(std::abs(scene.charges[0].position.Y) < 1e-12);
  for (const auto& line : r.electric) CHECK((line.points.back() - Vec2(2, 0)).Length() < 0.2);
}

TEST_CASE("3d: balls in space end their lines on the surface of the sink ball") {
  ChargeSystem field;
  field.radius = 1;
  field.charges.push_back(at(1, 0, 0, -2));
  field.charges.push_back(at(-1, 0, 3, 2));
  const FieldResult r = computeAll(field, lightParams());
  CHECK(r.electric.size() == 4);
  for (const auto& line : r.electric) CHECK((line.points.back() - Vec2(0, 3)).Length() <= 1.0 + 1e-9);  // projected surface point
}

TEST_CASE("forces: Coulomb between points, equal and opposite, in space") {
  ChargeSystem field;
  field.charges.push_back(at(2, 0, 0, 0));
  field.charges.push_back(at(-3, 0, 0, 2));
  field.charges.push_back(at(1, 4, 1, -1));
  const std::vector<Vec3> f = forces(field);
  REQUIRE(f.size() == 3);
  CHECK((f[0] + f[1] + f[2]).Length() < 1e-12);
  ChargeSystem pair;
  pair.charges.push_back(at(2, 0, 0, 0));
  pair.charges.push_back(at(-3, 0, 0, 2));
  const std::vector<Vec3> g = forces(pair);
  CHECK(g[0].Z == doctest::Approx(1.5));  // attracted towards +z with |q1 q2| / d^2 = 6 / 4
  CHECK(g[0].X == 0.0);
  CHECK(potentialEnergy(pair) == doctest::Approx(-3.0));
}

TEST_CASE("forces: overlapping balls follow the rigid-ball rule and join Coulomb at d = 2R") {
  const double R = 1.5;
  CHECK(pairForce(2 * R, R) == doctest::Approx(1. / (4 * R * R)));
  CHECK(pairForce(2 * R * (1 - 1e-9), R) == doctest::Approx(1. / (4 * R * R)).epsilon(1e-7));
  CHECK(pairEnergy(2 * R * (1 - 1e-9), R) == doctest::Approx(1. / (2 * R)).epsilon(1e-7));
  CHECK(pairEnergy(0, R) == doctest::Approx(6. / (5 * R)));
  CHECK(pairForce(0, R) == 0.0);
  // F = -dU/dd
  for (const double d : {0.2, 0.9, 1.7, 2.6, 4.0}) {
    const double h = 1e-6;
    CHECK(pairForce(d, R) == doctest::Approx(-(pairEnergy(d + h, R) - pairEnergy(d - h, R)) / (2 * h)).epsilon(1e-6));
  }
  // Against the definition: the field of ball 1 (at the origin) summed over the charge of ball 2.
  for (const double d : {0.6, 1.8, 2.7}) {
    Charge one;
    const int n = 60;
    const double cell = 2 * R / n;
    double sum = 0;
    long count = 0;
    for (int i = 0; i < n; ++i)
      for (int j = 0; j < n; ++j)
        for (int k = 0; k < n; ++k) {
          const Vec3 local(-R + (i + 0.5) * cell, -R + (j + 0.5) * cell, -R + (k + 0.5) * cell);
          if (local * local >= R * R) continue;
          sum += one.E(local + Vec3(d, 0, 0), R).X;
          ++count;
        }
    CHECK(pairForce(d, R) == doctest::Approx(sum / static_cast<double>(count)).epsilon(0.01));
  }
}

// Equipotentials of a 3D system: the cut of the equipotential surfaces with the plane parallel to the
// screen through the centre of the charges.
TEST_CASE("3d equipotentials: one charge in a turned view gives circles of radius q / level") {
  ChargeSystem field;
  field.charges.push_back(at(2, 1, 0.5, 0.25));
  field.camera.yaw = 0.6;
  field.camera.pitch = -0.3;
  const ComputeParams p = lightParams();
  const FieldResult r = computeAll(field, p);
  REQUIRE(r.potential.size() >= 5);
  const Vec2 centre = field.camera.project(field.charges[0].position3());
  for (const auto& line : r.potential) {
    CHECK(line.potential > 0);
    REQUIRE(line.space.size() >= 8);
    for (const Vec3& at3 : line.space) {
      CHECK(field.Potential(at3) == doctest::Approx(line.potential).epsilon(0.01));
      CHECK((field.camera.project(at3) - centre).Length() == doctest::Approx(2. / line.potential).epsilon(0.01));
      CHECK(field.camera.depth(at3) == doctest::Approx(CutPlane(field).depth));
    }
  }
}

TEST_CASE("3d equipotentials: charges off the cut plane; every line stays on its level, both signs appear") {
  ChargeSystem field;
  field.charges.push_back(at(1, -1, 0, 0.5));
  field.charges.push_back(at(-1, 1, 0, -0.5));
  field.charges.push_back(at(1, 0, 1.5, 0.3));
  ComputeParams p = lightParams();
  p.potentialInterval = 0.25;
  const FieldResult r = computeAll(field, p);
  const CutPlane cut(field);
  CHECK(cut.depth == doctest::Approx(0.1));
  bool positive = false, negative = false;
  for (const auto& line : r.potential) {
    (line.potential > 0 ? positive : negative) = true;
    REQUIRE(line.space.size() >= 4);
    for (const Vec3& at3 : line.space) {
      CHECK(field.camera.depth(at3) == doctest::Approx(cut.depth));
      CHECK(std::abs(field.Potential(at3) - line.potential) < 0.02);
    }
  }
  CHECK(positive);
  CHECK(negative);

  p.calculateEquipotentials = false;
  CHECK(computeAll(field, p).potential.empty());
}

TEST_CASE("3d equipotentials: with balls the lines stay outside and end on the cut circles") {
  ChargeSystem field;
  field.radius = 0.5;
  field.model = ChargeModel::Conductor;
  field.charges.push_back(at(1, -1.2, 0, 0.3));
  field.charges.push_back(at(-1, 1.2, 0, -0.3));
  field.camera.yaw = 0.3;
  ComputeParams p = lightParams();
  p.potentialInterval = 0.25;
  const FieldResult r = computeAll(field, p);
  REQUIRE_FALSE(r.potential.empty());
  for (const auto& line : r.potential)
    for (const Vec3& at3 : line.space)
      for (const Charge& c : field.charges) CHECK((at3 - c.position3()).Length() >= field.radius - 1e-6);
}
