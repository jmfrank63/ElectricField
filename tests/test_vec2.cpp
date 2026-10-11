#include "doctest/doctest.h"

#include "ef/math.hpp"
#include "ef/vec2.hpp"

using ef::Vec2;

TEST_CASE("Vec2 arithmetic") {
  const Vec2 a(3, 4), b(1, -2);
  CHECK((a + b) == Vec2(4, 2));
  CHECK((a - b) == Vec2(2, 6));
  CHECK((a * b) == doctest::Approx(-5.0));   // dot product
  CHECK((a * 2.0) == Vec2(6, 8));
  CHECK((2.0 * a) == Vec2(6, 8));
  CHECK((a / 2.0) == Vec2(1.5, 2));
  CHECK((-a) == Vec2(-3, -4));
  CHECK(a.Length() == doctest::Approx(5.0));
  Vec2 c = a;
  c += b;
  c -= b;
  c *= 2.0;
  c /= 2.0;
  CHECK(c == a);
}

TEST_CASE("Vec2 Normalize keeps the zero vector (ENormalized relies on it)") {
  CHECK(Vec2(0, 0).Normalize() == Vec2(0, 0));
  const Vec2 n = Vec2(3, 4).Normalize();
  CHECK(n.X == doctest::Approx(0.6));
  CHECK(n.Y == doctest::Approx(0.8));
  CHECK(n.Length() == doctest::Approx(1.0));
}

TEST_CASE("sign(0) == +1 (load-bearing for net-zero configurations)") {
  CHECK(ef::sign(0) == 1);
  CHECK(ef::sign(0.0) == 1);
  CHECK(ef::sign(-0.0) == 1);  // -0.0 >= 0 is true
  CHECK(ef::sign(2) == 1);
  CHECK(ef::sign(-1e-300) == -1);
  CHECK(ef::sign(-3) == -1);
}

TEST_CASE("kPi") { CHECK(ef::kPi == doctest::Approx(3.14159265358979323846)); }
