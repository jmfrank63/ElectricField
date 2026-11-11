// Finite charge radius (ChargeSystem::radius): uniformly charged balls, Coulomb outside, finite inside.
#include "doctest/doctest.h"

#include <cmath>

#include "app/configs.hpp"
#include "app/session_file.hpp"
#include "ef/charge_system.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"
#include "test_params.hpp"

using namespace ef;

namespace {
ChargeSystem ballSystem(double radius, std::initializer_list<Charge> charges) {
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
}  // namespace

TEST_CASE("radius: outside a ball the field is exactly Coulomb, inside it is the analytic ball field") {
  Charge q;
  q.charge = 3;
  q.position = Vec2(1, -1);
  const double R = 2;
  // outside (|r| = 5): identical to the point charge
  const Vec2 out(4, 3);
  CHECK(q.Potential(out, R) == q.Potential(out));
  CHECK(q.E(out, R).X == q.E(out).X);
  CHECK(q.E(out, R).Y == q.E(out).Y);
  // inside (r = (0.6, 0.8), |r| = 1): E = q r / R^3, V = q (3R^2 - r^2) / (2R^3)
  const Vec2 in(1.6, -0.2);
  CHECK(q.E(in, R).X == doctest::Approx(3.0 * 0.6 / 8.0).epsilon(1e-12));
  CHECK(q.E(in, R).Y == doctest::Approx(3.0 * 0.8 / 8.0).epsilon(1e-12));
  CHECK(q.Potential(in, R) == doctest::Approx(3.0 * (12.0 - 1.0) / 16.0).epsilon(1e-12));
  // finite at the centre: E = 0, V = 3q / (2R)
  CHECK(q.E(q.position, R).Length() == 0.0);
  CHECK(q.Potential(q.position, R) == doctest::Approx(9.0 / 4.0));
}

TEST_CASE("radius: E and V are continuous across the ball surface") {
  Charge q;
  q.charge = -2;
  const double R = 1.5;
  for (double angle = 0; angle < 6.28; angle += 0.7) {
    const Vec2 dir(std::cos(angle), std::sin(angle));
    const Vec2 justIn = dir * (R * (1 - 1e-9)), justOut = dir * (R * (1 + 1e-9));
    CHECK(q.Potential(justIn, R) == doctest::Approx(q.Potential(justOut, R)).epsilon(1e-7));
    CHECK(q.E(justIn, R).X == doctest::Approx(q.E(justOut, R).X).epsilon(1e-7));
    CHECK(q.E(justIn, R).Y == doctest::Approx(q.E(justOut, R).Y).epsilon(1e-7));
  }
}

TEST_CASE("radius: 0 keeps the point-charge system bit-identical") {
  ChargeSystem point = makeSystem(presets()[0]);
  ChargeSystem zero = makeSystem(presets()[0]);
  zero.radius = 0;
  for (const Vec2 p : {Vec2(0, 0), Vec2(-1.9, 0.05), Vec2(3, 4)}) {
    CHECK(point.E(p).X == zero.E(p).X);
    CHECK(point.Potential(p) == zero.Potential(p));
  }
  CHECK_FALSE(zero.AtBallNull(Vec2(2, 0), 1.0));
}

TEST_CASE("radius: a dipole of balls computes and its lines end in the sink ball") {
  ChargeSystem field = makeSystem(presets()[0]);  // +1 at (-2, 0), -1 at (2, 0)
  field.radius = 1;
  const FieldResult r = computeAll(field, lightParams());
  CHECK(r.electric.size() == 4);
  for (const auto& line : r.electric) {
    REQUIRE(line.points.size() > 2);
    CHECK((line.points.back() - field.charges[1].position).Length() < 1.0);
  }
  CHECK_FALSE(r.potential.empty());
  // the scene carries the radius so the renderers outline the balls
  CHECK(buildScene(field, r).chargeRadius == 1.0);
}

TEST_CASE("radius: a line flowing into a ball ends on the field null inside it, not at the centre") {
  // A -1 ball in the strong field of a +20 ball: inside the -1 ball the outside field wins over most of
  // the volume, and the null where the two cancel sits well off the centre (about 0.56 towards +x).
  const ChargeSystem field = ballSystem(1, {Charge{20, Vec2(-6, 0)}, Charge{-1, Vec2(0, 0)}});
  ComputeParams p = lightParams();
  p.calculateEquipotentials = false;
  const FieldResult r = computeAll(field, p);
  int endedAtNull = 0;
  for (const auto& line : r.electric) {
    const Vec2 last = line.points.back();
    if (last.Length() >= 1.0) continue;  // escaped to infinity (total charge +19)
    CHECK(field.E(last).Length() < 0.05);
    CHECK(last.X > 0.3);
    CHECK_FALSE(field.HitCharge(last, p));
    ++endedAtNull;
  }
  CHECK(endedAtNull >= 1);
}

TEST_CASE("radius: collections and sessions round-trip the radius; absent means 0") {
  efapp::UserConfig c{"Balls", {{1, -2, 0}, {-1, 2, 0}}};
  c.radius = 1.25;
  const efapp::ParsedConfigs configs = efapp::parseConfigs(efapp::serializeConfigs({c, efapp::UserConfig{"Points", {{1, 0, 0}}}}));
  REQUIRE(configs.value.size() == 2);
  CHECK(configs.value[0].radius == 1.25);
  CHECK(configs.value[1].radius == 0);
  CHECK(efapp::serializeConfigs({configs.value[1]}).find("radius") == std::string::npos);  // point charges write nothing new
  // out of range or malformed: the charges stay, the radius falls back to 0
  const efapp::ParsedConfigs bad = efapp::parseConfigs("[configuration \"X\"]\nradius = -1\ncharge = 1, 0, 0\n[configuration \"Y\"]\nradius = abc\ncharge = 1, 0, 0\n");
  REQUIRE(bad.value.size() == 2);
  CHECK(bad.value[0].radius == 0);
  CHECK(bad.value[1].radius == 0);

  efapp::SavedTab t;
  t.title = "Balls 1";
  t.charges = {{1, -2, 0}};
  t.radius = 0.5;
  const efapp::ParsedSession s = efapp::parseSession(efapp::serializeSession({t}));
  REQUIRE(s.value.size() == 1);
  CHECK(s.value[0].radius == 0.5);
}
