#include "doctest/doctest.h"

#include <cmath>

#include "ef/charge_system.hpp"
#include "ef/presets.hpp"

using namespace ef;

TEST_CASE("point charge field and potential match the analytic values") {
  Charge q;
  q.charge = 3;
  q.position = Vec2(1, -1);
  const Vec2 p(4, 3);  // r = (3,4), |r| = 5
  CHECK(q.Potential(p) == doctest::Approx(3.0 / 5.0).epsilon(1e-12));
  const Vec2 e = q.E(p);  // q r / |r|^3
  CHECK(e.X == doctest::Approx(3.0 * 3.0 / 125.0).epsilon(1e-12));
  CHECK(e.Y == doctest::Approx(3.0 * 4.0 / 125.0).epsilon(1e-12));
  CHECK(e.Length() == doctest::Approx(3.0 / 25.0).epsilon(1e-12));  // Coulomb 1/r^2

  Charge n;
  n.charge = -2;
  CHECK(n.Potential(Vec2(0, 2)) == doctest::Approx(-1.0));
  CHECK(n.E(Vec2(0, 2)).Y == doctest::Approx(-0.5));  // pointing towards the negative charge
}

TEST_CASE("superposition: dipole on its axis and on the mid-plane") {
  ChargeSystem s = makeSystem(presets()[0]);  // +1 at (-2,0), -1 at (2,0)
  CHECK(s.Potential(Vec2(0, 0)) == doctest::Approx(0.0).epsilon(1e-15));
  CHECK(s.Potential(Vec2(0, 5)) == doctest::Approx(0.0).epsilon(1e-12));  // symmetric mid-plane is the V=0 line
  const Vec2 e0 = s.E(Vec2(0, 0));  // each charge contributes 1/4 along +x
  CHECK(e0.X == doctest::Approx(0.5).epsilon(1e-12));
  CHECK(std::abs(e0.Y) < 1e-15);
  CHECK(s.ENormalized(Vec2(0, 0)).X == doctest::Approx(1.0));
  // far field decays like a dipole (1/r^3) - field at (0,10) is tiny and still along +x
  CHECK(s.E(Vec2(0, 10)).Length() < 5e-3);
}

TEST_CASE("E at a charge position is not finite (callers must stop at HitCharge)") {
  Charge q;
  const Vec2 e = q.E(q.position);
  CHECK_FALSE(std::isfinite(e.X));
  CHECK_FALSE(std::isfinite(q.Potential(q.position)));
}

TEST_CASE("HitCharge radius = chargeRadius / 1.5 / pixelsPerUnit") {
  ComputeParams p;
  CHECK(p.seedRadius() == doctest::Approx(0.25));
  CHECK(p.hitRadius() == doctest::Approx(0.25 / 1.5));
  ChargeSystem s = makeSystem(presets()[0]);
  CHECK(s.HitCharge(Vec2(-2 + 0.16, 0), p));
  CHECK_FALSE(s.HitCharge(Vec2(-2 + 0.17, 0), p));
}

TEST_CASE("totalCharge / hasDifferentSigns") {
  bool diff = true;
  ChargeSystem s;
  CHECK(s.totalCharge(diff) == 0);  // empty is guarded (the MFC code dereferences begin())
  CHECK_FALSE(diff);
  s = makeSystem(presets()[0]);
  CHECK(s.totalCharge(diff) == 0);
  CHECK(diff);
  s = makeSystem(presets()[2]);
  CHECK(s.totalCharge(diff) == 3);
  CHECK_FALSE(diff);
  s = makeSystem(presets()[3]);
  CHECK(s.totalCharge(diff) == 1);
  CHECK(diff);
}
