// ComputeSession: determinism (D-04), poll drain-and-swap (D-03), cancellation (D-02), stress for TSan (D-20).
#include "doctest/doctest.h"

#include <chrono>
#include <cmath>
#include <set>
#include <string_view>
#include <thread>

#include "ef/method.hpp"
#include "ef/presets.hpp"
#include "ef/session.hpp"

using namespace ef;

namespace {
ComputeParams lightParams() {
  ComputeParams p;
  p.linesPerUnitCharge = 4;
  p.potentialInterval = 0.5;
  p.numThreads = 4;
  return p;
}

const Preset& presetBySlug(std::string_view slug) {
  for (const auto& p : presets())
    if (p.slug == slug) return p;
  FAIL("unknown preset " << slug);
  return presets()[0];
}

FieldResult compute(const Preset& pr, ComputeParams p, SessionStatus* statusOut = nullptr) {
  ComputeSession s(makeSystem(pr), p);
  const SessionStatus st = s.wait();
  if (statusOut) *statusOut = st;
  FieldResult r;
  REQUIRE(s.takeResult(r));
  return r;
}

}  // namespace

TEST_CASE("1 thread and N threads give the same result (D-04: job-sequence order, tolerance 1e-12)") {
  for (const char* slug : {"dipole", "parallel-dipoles"}) {
    CAPTURE(slug);
    ComputeParams p = lightParams();
    p.numThreads = 1;
    const FieldResult a = compute(presetBySlug(slug), p);
    p.numThreads = 7;
    const FieldResult b = compute(presetBySlug(slug), p);
    REQUIRE(a.electric.size() == b.electric.size());
    REQUIRE(a.potential.size() == b.potential.size());
    for (std::size_t i = 0; i < a.electric.size(); ++i) {
      REQUIRE(a.electric[i].points.size() == b.electric[i].points.size());
      for (std::size_t k = 0; k < a.electric[i].points.size(); ++k) {
        CHECK(std::abs(a.electric[i].points[k].X - b.electric[i].points[k].X) < 1e-12);
        CHECK(std::abs(a.electric[i].points[k].Y - b.electric[i].points[k].Y) < 1e-12);
      }
    }
    for (std::size_t i = 0; i < a.potential.size(); ++i) {
      CHECK(a.potential[i].potential == doctest::Approx(b.potential[i].potential).epsilon(1e-12));
      CHECK(a.potential[i].points.size() == b.potential[i].points.size());
    }
  }
}
