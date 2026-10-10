// Research measurement harness (not part of the repo). Counts, timing, step statistics.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "ef/method.hpp"
#include "ef/presets.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"

using namespace ef;
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }


struct TimingHooks {
  std::vector<std::pair<Vec2, double>> children;
  Clock::time_point last{};
  bool started = false;
  double maxBlockMs = 0;
  std::size_t calls = 0;
  bool cancelled() {
    const auto now = Clock::now();
    if (started) maxBlockMs = std::max(maxBlockMs, ms(last, now));
    last = now;
    started = true;
    ++calls;
    return false;
  }
  void postEquipotential(const Vec2& pt, double label) { children.emplace_back(pt, label); }
};

template <typename Solver>
void lineMode(const Preset& pr, Method m, Solver& solver) {
  ComputeParams p;
  p.method = m;
  const ChargeSystem f = makeSystem(pr);
  const auto seeds = planSeeds(f, p);
  double maxE = 0, maxEq = 0, maxBlock = 0, totalE = 0, totalEq = 0;
  std::size_t maxCallsE = 0, maxCallsEq = 0, nEq = 0;
  for (const auto& s : seeds) {
    TimingHooks h;
    const auto t0 = Clock::now();
    traceElectricLine(solver, f, p, s, h);
    const double dt = ms(t0, Clock::now());
    totalE += dt;
    maxE = std::max(maxE, dt);
    maxCallsE = std::max(maxCallsE, h.calls);
    maxBlock = std::max(maxBlock, h.maxBlockMs);
    for (auto& c : h.children) {
      TimingHooks h2;
      const auto t1 = Clock::now();
      traceEquipotential(solver, f, p, c.first, c.second, h2);
      const double d2 = ms(t1, Clock::now());
      totalEq += d2;
      maxEq = std::max(maxEq, d2);
      maxCallsEq = std::max(maxCallsEq, h2.calls);
      maxBlock = std::max(maxBlock, h2.maxBlockMs);
      ++nEq;
    }
  }
  std::printf("%s,%s,%zu,%zu,%.1f,%.1f,%zu,%zu,%.1f,%.1f,%.3f\n", std::string(pr.slug).c_str(), std::string(info(m).name).c_str(), seeds.size(), nEq,
              totalE, totalEq, maxCallsE * 4096, maxCallsEq * 4096, maxE, maxEq, maxBlock);
}

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "counts";
  unsigned threads = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : 4;
  int methodIdx = argc > 3 ? std::atoi(argv[3]) : 0;

  if (!std::strcmp(mode, "counts")) {
    std::printf("slug,method,threads,seeds,expectedE,rawEq,final_E,final_Eq,ptsE,ptsEq,ms\n");
    for (const auto& pr : presets()) {
      ComputeParams p;
      p.numThreads = threads;
      p.method = kMethods[static_cast<std::size_t>(methodIdx)].method;
      const ChargeSystem f = makeSystem(pr);
      const auto t0 = Clock::now();
      ComputeSession s(f, p);
      const SessionStatus st = s.wait();
      FieldResult r;
      s.takeResult(r);
      const auto t1 = Clock::now();
      std::size_t pe = 0, pp = 0;
      for (auto& l : r.electric) pe += l.points.size();
      for (auto& l : r.potential) pp += l.points.size();
      std::printf("%s,%s,%u,%zu,%d,%zu,%zu,%zu,%zu,%zu,%.0f\n", std::string(pr.slug).c_str(), std::string(kMethods[static_cast<std::size_t>(methodIdx)].name).c_str(), threads,
                  planSeeds(f, p).size(), expectedLineCount(f, p), st.jobsKnown - planSeeds(f, p).size(), r.electric.size(), r.potential.size(), pe, pp, ms(t0, t1));
    }
    return 0;
  }

  if (!std::strcmp(mode, "finalize")) {
    std::printf("slug,rawPts,finalizeMs,svgMs,svgBytes,sceneMs\n");
    for (const auto& pr : presets()) {
      ComputeParams p;
      const ChargeSystem f = makeSystem(pr);
      ComputeSession s(f, p);
      PollBatch b;
      s.wait();
      s.poll(b);
      std::vector<FieldLine> e; std::vector<PotentialLine> q;
      std::size_t pts = 0;
      for (auto& x : b.electric) { e.push_back(x->line); pts += x->line.points.size(); }
      for (auto& x : b.potential) { q.push_back(x->line); pts += x->line.points.size(); }
      const auto t0 = Clock::now();
      FieldResult r = finalize(std::move(e), std::move(q), f, p);
      const auto t1 = Clock::now();
      const FieldScene sc = buildScene(f, r);
      const auto t2 = Clock::now();
      const std::string svg = renderSvg(sc, DrawStyle{}, View{});
      const auto t3 = Clock::now();
      std::printf("%s,%zu,%.2f,%.2f,%zu,%.2f\n", std::string(pr.slug).c_str(), pts, ms(t0, t1), ms(t2, t3), svg.size(), ms(t1, t2));
    }
    return 0;
  }

  if (!std::strcmp(mode, "bezier")) {
    std::printf("slug,method,groups,fallbacks,minAbsDivi\n");
    for (const auto& pr : presets()) {
      ComputeParams p;
      p.method = kMethods[static_cast<std::size_t>(methodIdx)].method;
      const ChargeSystem f = makeSystem(pr);
      ComputeSession s(f, p);
      s.wait();
      PollBatch b;
      s.poll(b);
      std::size_t groups = 0, fb = 0;
      double minDivi = 1e300;
      auto scan = [&](std::vector<Vec2> pts) {
        for (std::size_t i = 0; i + 3 < pts.size(); i += 3) {
          ++groups;
          double t1 = (pts[i + 1] - pts[i]).Length();
          double t2 = t1 + (pts[i + 2] - pts[i + 1]).Length();
          const double t3 = t1 + t2 + (pts[i + 3] - pts[i + 2]).Length();
          t1 /= t3; t2 /= t3;
          const double divi = std::abs(3. * t1 * t2 * (1. - t1) * (1. - t2) * (t2 - t1));
          if (!(divi > 1e-9)) ++fb;
          if (divi < 1e-3) { Vec2 o1, o2; adjustForBezier(pts[i], pts[i + 1], pts[i + 2], pts[i + 3], o1, o2); std::printf("  small divi=%.3g t1=%.3g t2=%.3g |o1-p0|=%.3g |o2-p3|=%.3g chord=%.3g\n", divi, t1, t2, (o1 - pts[i]).Length(), (o2 - pts[i + 3]).Length(), (pts[i + 3] - pts[i]).Length()); }
          if (divi == divi) minDivi = std::min(minDivi, divi);
        }
      };
      for (auto& x : b.electric) scan(x->line.points);
      for (auto& x : b.potential) scan(x->line.points);
      std::printf("%s,%s,%zu,%zu,%.4g\n", std::string(pr.slug).c_str(), std::string(info(p.method).name).c_str(), groups, fb, minDivi);
    }
    return 0;
  }
  if (!std::strcmp(mode, "lines")) {
    std::printf("slug,method,seeds,eqjobs,totalE_ms,totalEq_ms,maxStepsE(<=),maxStepsEq(<=),maxE_ms,maxEq_ms,maxBlock_ms(4096 steps)\n");
    for (const auto& pr : presets())
      for (const auto& mi : kMethods) {
        if (methodIdx >= 0 && static_cast<int>(mi.method) != methodIdx) continue;
        withSolver(mi.method, [&](auto& solver) { lineMode(pr, mi.method, solver); });
      }
    return 0;
  }
  return 0;
}
