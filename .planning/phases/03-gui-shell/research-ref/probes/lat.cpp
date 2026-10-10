// Probe: UI-thread cost of closing a computing tab = cancel() + ~ComputeSession (joins), for every preset at the MFC defaults
// (4 threads, 16 lines/unit, Euler, equipotentials 0.10), at several ages of the session, plus the "finalizing" window.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
#include "ef/presets.hpp"
#include "ef/session.hpp"
using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
int main() {
  const int ages[] = {0, 10, 50, 150, 400, 1000};
  double worst = 0;
  for (const auto& p : ef::presets()) {
    std::printf("%-38s", std::string(p.displayName).c_str());
    for (int age : ages) {
      auto s = std::make_unique<ef::ComputeSession>(ef::makeSystem(p), ef::ComputeParams{});
      std::this_thread::sleep_for(std::chrono::milliseconds(age));
      ef::PollBatch b;
      const auto st = s->poll(b);
      if (st.done) { std::printf(" %6s", "done"); continue; }
      const auto t0 = clk::now();
      s->cancel();
      s.reset();
      const double d = ms(t0, clk::now());
      worst = std::max(worst, d);
      std::printf(" %6.1f", d);
    }
    // finalizing window: cancel as soon as every posted job has completed but the session is not done yet
    double fin = -1;
    for (int attempt = 0; attempt < 5 && fin < 0; ++attempt) {
      auto s = std::make_unique<ef::ComputeSession>(ef::makeSystem(p), ef::ComputeParams{});
      for (;;) {
        ef::PollBatch b;
        const auto st = s->poll(b);
        if (st.done) break;
        if (st.jobsKnown > 0 && st.jobsCompleted == st.jobsKnown) {
          const auto t0 = clk::now();
          s->cancel();
          s.reset();
          fin = ms(t0, clk::now());
          break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    }
    if (fin >= 0) worst = std::max(worst, fin);
    std::printf("  finalizing:%s%.1f\n", fin < 0 ? " n/a " : " ", fin);
  }
  for (int n : {9, 27}) {  // quit with N busy sessions: cancel all first (joins overlap), then destroy
    std::vector<std::unique_ptr<ef::ComputeSession>> v;
    for (int i = 0; i < n; ++i) v.push_back(std::make_unique<ef::ComputeSession>(ef::makeSystem(ef::presets()[static_cast<std::size_t>(i) % ef::kPresetCount]), ef::ComputeParams{}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto t0 = clk::now();
    for (auto& x : v) x->cancel();
    v.clear();
    std::printf("quit with %d busy sessions (4 threads each): %.1f ms\n", n, ms(t0, clk::now()));
  }
  std::printf("worst cancel+destroy = %.1f ms\n", worst);
  return 0;
}
