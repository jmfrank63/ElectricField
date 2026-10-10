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
#include "test_params.hpp"

using namespace ef;
using Clock = std::chrono::steady_clock;

namespace {
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

double msSince(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }
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

TEST_CASE("field-line and equipotential invariants per preset (QA-02)") {
  for (const auto& pr : presets()) {
    CAPTURE(pr.slug);
    ComputeParams p = lightParams();
    const ChargeSystem field = makeSystem(pr);
    SessionStatus st;
    const FieldResult r = compute(pr, p, &st);
    CHECK_FALSE(st.cancelled);
    CHECK_FALSE(st.failed);
    // exactly the source seeds record a line (sinks only start equipotentials): expectedLineCount minus one per sink
    bool diff = false;
    const int total = field.totalCharge(diff);
    int sources = 0;
    for (const auto& c : field.charges)
      if (c.charge != 0 && sign(total) == sign(c.charge)) sources += std::abs(c.charge) * static_cast<int>(p.linesPerUnitCharge);
    CHECK(static_cast<int>(r.electric.size()) == sources);
    for (const auto& l : r.electric) {
      CHECK(l.points.size() >= 2);
      for (const auto& pt : l.points) CHECK(std::isfinite(pt.X + pt.Y));  // terminated and finite (no NaN from the Bezier fit)
    }
    // equipotential labels are multiples of the interval (floor(V/i)*i, then +- i)
    REQUIRE(!r.potential.empty());
    for (const auto& l : r.potential) {
      const double k = l.potential / p.potentialInterval;
      CHECK(std::abs(k - std::round(k)) < 1e-6);
      CHECK(l.points.size() >= 2);
      for (const auto& pt : l.points) CHECK(std::isfinite(pt.X + pt.Y));
    }
  }
}

TEST_CASE("de-duplication on Quadrupole (nothing to remove) and Parallel dipoles (duplicates removed)") {
  ComputeParams p = lightParams();
  for (const char* slug : {"quadrupole", "parallel-dipoles"}) {
    CAPTURE(slug);
    ComputeSession s(makeSystem(presetBySlug(slug)), p);
    const SessionStatus st = s.wait();
    FieldResult r;
    REQUIRE(s.takeResult(r));
    const std::size_t raw = st.jobsKnown - planSeeds(makeSystem(presetBySlug(slug)), p).size();
    if (std::string_view(slug) == "quadrupole")
      CHECK(r.potential.size() == raw);  // the positive-potential regions are disconnected: no duplicates exist
    else
      CHECK(r.potential.size() < raw);   // the two + charges share merged contours
    // postcondition of the de-dup: no two kept lines of (nearly) the same potential have (nearly) the same weight centre
    // (the LAST potential group is exempt: MFC appends it unfiltered)
    const double interval = p.potentialInterval;
    for (std::size_t i = 0; i + 1 < r.potential.size(); ++i) {
      const auto& a = r.potential[i];
      for (std::size_t j = i + 1; j < r.potential.size() && j < i + 3; ++j) {
        const auto& b = r.potential[j];
        const bool dup = std::abs(a.potential - b.potential) < interval / 3. && (a.weightCenter - b.weightCenter).Length() * 100. < 2. * 25.;
        if (dup) CHECK(j + 1 >= r.potential.size() - 2);  // allowed only inside the unfiltered tail group
      }
    }
  }
}

TEST_CASE("poll() drains each completed line exactly once and does not touch worker containers (D-03)") {
  ComputeParams p = lightParams();
  p.numThreads = 3;
  ComputeSession s(makeSystem(presetBySlug("quadrupole")), p);
  PollBatch batch;
  std::set<std::pair<std::size_t, std::size_t>> seen;
  SessionStatus st;
  for (;;) {
    PollBatch fresh;
    st = s.poll(fresh);
    for (const auto& e : fresh.electric) CHECK(seen.insert({e->key.parent, e->key.child}).second);
    for (const auto& q : fresh.potential) CHECK(seen.insert({q->key.parent, q->key.child + 1000000}).second);
    if (st.done) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  PollBatch rest;  // lines completed between the last poll and done
  st = s.poll(rest);
  for (const auto& e : rest.electric) CHECK(seen.insert({e->key.parent, e->key.child}).second);
  for (const auto& q : rest.potential) CHECK(seen.insert({q->key.parent, q->key.child + 1000000}).second);
  CHECK(seen.size() == st.jobsKnown);  // every job known was delivered once
  CHECK(st.jobsCompleted == st.jobsKnown);
}

TEST_CASE("cancel(): returns promptly mid-computation without a result, even for the heaviest single jobs (D-02, CORE-06)") {
  // two-unequal-charges-different-signs + Dormand-Prince has the longest single field line (1M-step budget, ~0.4 s in release)
  ComputeParams p;
  p.method = Method::AdaptiveDormandPrince;
  p.numThreads = 4;
  double worst = 0;
  for (int delayMs : {0, 5, 30}) {
    ComputeSession s(makeSystem(presetBySlug("two-unequal-charges-different-signs")), p);
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    const auto t0 = Clock::now();
    s.cancel();
    const SessionStatus st = s.wait();
    const double dt = msSince(t0);
    worst = std::max(worst, dt);
    CHECK(st.done);
    CHECK(st.cancelled);
    FieldResult r;
    CHECK_FALSE(s.takeResult(r));
  }
  MESSAGE("worst cancel latency ms: " << worst);
  CHECK(worst < 200.);
}

TEST_CASE("destroying a running session joins its workers within ~200 ms (tab close / app quit), all methods") {
  for (const auto& m : kMethods) {
    CAPTURE(m.name);
    ComputeParams p;
    p.method = m.method;
    p.numThreads = 4;
    const auto t0 = Clock::now();
    {
      ComputeSession s(makeSystem(presetBySlug("two-unequal-charges-different-signs")), p);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }  // ~ComputeSession = cancel + join
    const double dt = msSince(t0);
    CHECK(dt < 10. + 200.);
  }
}

TEST_CASE("stress: many short-lived sessions with random cancel points (TSan target, D-20)") {
  ComputeParams p = lightParams();
  unsigned seed = 12345;
  for (int i = 0; i < 60; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const auto& pr = presets()[(seed >> 8) % presets().size()];
    p.method = kMethods[(seed >> 16) % kMethods.size()].method;
    ComputeSession s(makeSystem(pr), p);
    PollBatch b;
    const unsigned delay = (seed >> 4) % 6;
    for (unsigned k = 0; k < delay; ++k) {
      s.poll(b);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (i % 4 != 3) s.cancel();  // 3 of 4: cancel mid-run; the 4th is destroyed while (probably) still running
    if (i % 2 == 0) s.wait();    // wait() after cancel() must return, also when the session is already done
  }
}

TEST_CASE("an empty charge system finishes immediately with an empty result; an invalid Method fails cleanly (D-15)") {
  {
    ComputeSession s(ChargeSystem{}, ComputeParams{});
    const SessionStatus st = s.wait();
    FieldResult r;
    CHECK(st.done);
    CHECK(s.takeResult(r));
    CHECK(r.electric.empty());
    CHECK(r.potential.empty());
  }
  {
    ComputeParams p;
    p.method = static_cast<Method>(99);
    ComputeSession s(makeSystem(presets()[0]), p);
    const SessionStatus st = s.wait();
    CHECK(st.failed);
    CHECK_FALSE(st.error.empty());
    FieldResult r;
    CHECK_FALSE(s.takeResult(r));
  }
}
