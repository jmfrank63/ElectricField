#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "ef/method.hpp"

using namespace ef;

namespace {

// A: rotation y' = (-y2, y1), y(0) = (1,0)  -> (cos t, sin t).  Linear + autonomous: clean asymptotic orders.
struct Rotation {
  Vec2 operator()(double, const Vec2& y) const { return Vec2(-y.Y, y.X); }
  static Vec2 exact(double t) { return Vec2(std::cos(t), std::sin(t)); }
  static Vec2 y0() { return Vec2(1, 0); }
};
// B: u' = u(1-u), v' = -2 t v, y(0) = (1/2, 1) -> (1/(1+e^-t), e^(-t^2)).  Non-linear AND time dependent:
// exercises the nonlinear order conditions and the node (c_i) coefficients that a linear problem cannot see.
struct NonLinear {
  Vec2 operator()(double t, const Vec2& y) const { return Vec2(y.X * (1. - y.X), -2. * t * y.Y); }
  static Vec2 exact(double t) { return Vec2(1. / (1. + std::exp(-t)), std::exp(-t * t)); }
  static Vec2 y0() { return Vec2(0.5, 1.0); }
};

template <class Problem, class Solver>
double globalError(Solver& s, int n) {
  Vec2 y = Problem::y0();
  const double h = 1.0 / n;
  for (int i = 0; i < n; ++i) y = s.SolveStepFixed(Problem{}, y, i * h, h);
  return (y - Problem::exact(1.0)).Length();
}

// observed order from the last two refinements of a sequence n, 2n, 4n
template <class Problem, class Solver>
double observedOrder(Solver& s, int n0) {
  const double e1 = globalError<Problem>(s, n0);
  const double e2 = globalError<Problem>(s, 2 * n0);
  return std::log2(e1 / e2);
}

template <class Solver>
void checkOrder(int nominal, int n0Linear, int n0NonLinear) {
  Solver s;
  const double pLin = observedOrder<Rotation>(s, n0Linear);
  CHECK(std::abs(pLin - nominal) <= 0.3);
  const double pNon = observedOrder<NonLinear>(s, n0NonLinear);
  CHECK(pNon >= nominal - 0.5);  // one-sided: pre-asymptotic super-convergence on the non-linear problem is fine
}

}  // namespace

TEST_CASE("method table: 11 methods in the original order with stable names") {
  static_assert(kMethodCount == 11, "original app has 11 integrators");
  const std::vector<std::string_view> names = {"euler", "midpoint", "ralston", "heun", "rk4", "rk3per8",
                                               "adaptive-heun-euler", "adaptive-bogacki-shampine",
                                               "adaptive-cash-karp", "adaptive-fehlberg", "adaptive-dormand-prince"};
  for (std::size_t i = 0; i < kMethodCount; ++i) {
    CHECK(static_cast<std::size_t>(kMethods[i].method) == i);  // enum value == original Options.h order
    CHECK(kMethods[i].name == names[i]);
  }
}

TEST_CASE("withSolver dispatches every method and reports adaptivity consistently") {
  for (const auto& m : kMethods) {
    const bool adaptive = withSolver(m.method, [](auto& solver) { return solver.IsAdaptive(); });
    CHECK(adaptive == m.adaptive);
  }
}

TEST_CASE("convergence order of the 6 fixed-step methods") {
  SUBCASE("Euler") { checkOrder<rk::Euler<Vec2>>(1, 64, 64); }
  SUBCASE("Midpoint") { checkOrder<rk::Midpoint<Vec2>>(2, 32, 64); }
  SUBCASE("Ralston") { checkOrder<rk::Ralston<Vec2>>(2, 32, 64); }
  SUBCASE("Heun") { checkOrder<rk::Heun<Vec2>>(2, 32, 64); }
  SUBCASE("RK4") { checkOrder<rk::RK4<Vec2>>(4, 16, 16); }
  SUBCASE("RK3/8") { checkOrder<rk::RK3per8<Vec2>>(4, 16, 16); }
}

TEST_CASE("convergence order of the stepping (high-order) solution of the 5 adaptive pairs") {
  SUBCASE("Heun-Euler 2(1)") { checkOrder<rk::AdaptiveHeunEuler<Vec2>>(2, 32, 64); }
  SUBCASE("Bogacki-Shampine 3(2)") { checkOrder<rk::AdaptiveBogackiShampine<Vec2>>(3, 32, 64); }
  SUBCASE("Cash-Karp 5(4)") { checkOrder<rk::AdaptiveCashKarp<Vec2>>(5, 4, 4); }
  SUBCASE("Fehlberg 5(4)") { checkOrder<rk::AdaptiveFehlberg<Vec2>>(5, 4, 16); }
  SUBCASE("Dormand-Prince 5(4)") { checkOrder<rk::AdaptiveDormandPrince<Vec2>>(5, 4, 16); }
}

namespace {
template <class Solver>
double lowSolutionOrder() {
  Solver s;
  auto err = [&](int n) {
    Vec2 y = Rotation::y0();
    const double h = 1.0 / n;
    for (int i = 0; i < n; ++i) {
      Vec2 hi, lo;
      s.SolveStepPair(Rotation{}, y, i * h, h, hi, lo);
      y = lo;
    }
    return (y - Rotation::exact(1.0)).Length();
  };
  return std::log2(err(32) / err(64));
}
}  // namespace

TEST_CASE("embedded lower-order solution is one order lower (error estimate is meaningful)") {
  CHECK(std::abs(lowSolutionOrder<rk::AdaptiveHeunEuler<Vec2>>() - 1) <= 0.3);
  CHECK(std::abs(lowSolutionOrder<rk::AdaptiveBogackiShampine<Vec2>>() - 2) <= 0.3);
  CHECK(std::abs(lowSolutionOrder<rk::AdaptiveCashKarp<Vec2>>() - 4) <= 0.3);
  CHECK(std::abs(lowSolutionOrder<rk::AdaptiveFehlberg<Vec2>>() - 4) <= 0.3);
}

TEST_CASE("KNOWN DEFECT (kept for parity): Dormand-Prince 'low' weights are the error coefficients b-b*, not b*") {
  // MFC RungeKutta.inl:191 stores DPweightsLow = b - b*.  The 'difference' used as error estimate is therefore
  // b*.K*h ~ h*|f|, NOT the local error.  Characterisation test: change only together with a re-baseline.
  rk::AdaptiveDormandPrince<Vec2> s;
  Vec2 hi, lo;
  s.SolveStepPair(Rotation{}, Rotation::y0(), 0.0, 0.1, hi, lo);
  CHECK((hi - lo).Length() == doctest::Approx(0.1).epsilon(0.01));      // ~ h*|f| (|f| = 1)
  CHECK((hi - Rotation::exact(0.1)).Length() < 1e-9);                  // while the true local error is ~1e-10
}

namespace {
template <class Solver>
int stepsToIntegrate(double tol) {
  Solver s;
  Vec2 y = Rotation::y0();
  double t = 0, h = 0.1, next = h;
  int steps = 0;
  while (t < 2.0 && steps < 200000) {
    h = std::min(h, 2.0 - t);
    y = s.SolveStep(Rotation{}, y, t, h, next, tol, 1.0);
    t += h;
    h = next;
    ++steps;
  }
  CHECK(t == doctest::Approx(2.0));
  CHECK((y - Rotation::exact(2.0)).Length() < 20 * tol);  // global error stays within a small multiple of tol
  return steps;
}
}  // namespace

TEST_CASE("adaptive step control: tighter tolerance => more (and smaller) steps, error stays near tolerance") {
  CHECK(stepsToIntegrate<rk::AdaptiveHeunEuler<Vec2>>(1e-6) > 5 * stepsToIntegrate<rk::AdaptiveHeunEuler<Vec2>>(1e-3));
  CHECK(stepsToIntegrate<rk::AdaptiveBogackiShampine<Vec2>>(1e-6) > 3 * stepsToIntegrate<rk::AdaptiveBogackiShampine<Vec2>>(1e-3));
  CHECK(stepsToIntegrate<rk::AdaptiveCashKarp<Vec2>>(1e-9) > stepsToIntegrate<rk::AdaptiveCashKarp<Vec2>>(1e-3));
  CHECK(stepsToIntegrate<rk::AdaptiveFehlberg<Vec2>>(1e-9) > stepsToIntegrate<rk::AdaptiveFehlberg<Vec2>>(1e-3));
}

TEST_CASE("adaptive step control: shrinks an oversized step, grows an undersized one, honours max_step") {
  rk::AdaptiveCashKarp<Vec2> s;
  double h = 1.5, next = h;  // far too large for tol 1e-6
  const Vec2 y = s.SolveStep(Rotation{}, Rotation::y0(), 0.0, h, next, 1e-6, 10.0);
  CHECK(h < 1.5);                                          // h is reduced in place (by reference)
  CHECK((y - Rotation::exact(h)).Length() <= 1e-6);        // accepted step meets the tolerance
  double h2 = 1e-4, next2 = h2;
  s.SolveStep(Rotation{}, Rotation::y0(), 0.0, h2, next2, 1e-3, 10.0);
  CHECK(next2 > h2);                                       // error << tol => step grows
  double h3 = 1e-4, next3 = h3;
  s.SolveStep(Rotation{}, Rotation::y0(), 0.0, h3, next3, 1e-3, 2e-4);
  CHECK(next3 == doctest::Approx(2e-4));                   // clamped to max_step
}
