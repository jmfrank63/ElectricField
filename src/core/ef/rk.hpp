#pragma once
// Port of RungeKutta.h / RungeKutta.inl (tableaus copied verbatim; algorithm unchanged).
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

#include "ef/math.hpp"
#include "ef/vec2.hpp"

namespace ef {
namespace rk {

inline double magnitude(double v) { return std::abs(v); }
template <typename T>
double magnitude(const Vector2D<T>& v) { return v.Length(); }

// Stages==1 (Euler) has no coefficient rows; keep array sizes >= 1.
template <unsigned Stages>
struct Tableau {
  double weights[Stages];
  double nodes[Stages];
  double coeff[(Stages > 1 ? Stages - 1 : 1)][(Stages > 1 ? Stages - 1 : 1)];  // row i = stage i+1
};

// Stage state lives in the member K, so use one solver instance per thread and never share it (Phase 2 threading rule).
template <typename T, unsigned Stages>
class RungeKutta {
 protected:
  Tableau<Stages> m_tab;
  std::array<T, Stages> K{};

 public:
  explicit RungeKutta(const Tableau<Stages>& tab) : m_tab(tab) {}

  // Fixed step (this is the whole method for the 6 non-adaptive ones).
  template <typename Func>
  T SolveStepFixed(Func&& f, const T& curVal, double t, double h) {
    T thesum(0.0);
    for (unsigned stage = 0; stage < Stages; ++stage) {
      T accum(0.0);
      for (unsigned j = 0; j < stage; ++j) accum += m_tab.coeff[stage - 1][j] * K[j];
      K[stage] = f(t + m_tab.nodes[stage] * h, curVal + h * accum);
      thesum += m_tab.weights[stage] * K[stage];
    }
    return curVal + h * thesum;
  }

  // Same signature as the adaptive one so callers can be generic.
  template <typename Func>
  T SolveStep(Func&& f, const T& curVal, double t, double& h, double& /*next_h*/, double /*tolerance*/,
              double /*max_step*/ = DBL_MAX, double /*min_step*/ = DBL_MIN) {
    return SolveStepFixed(f, curVal, t, h);
  }

  bool IsAdaptive() const { return false; }
};

template <typename T, unsigned Stages, unsigned Order>
class AdaptiveRungeKutta : public RungeKutta<T, Stages> {
  using Base = RungeKutta<T, Stages>;

 protected:
  std::array<double, Stages> m_low;

 public:
  AdaptiveRungeKutta(const Tableau<Stages>& tab, const std::array<double, Stages>& lowWeights) : Base(tab), m_low(lowWeights) {}

  // Test hook: both embedded solutions of one step (no step control).
  template <typename Func>
  void SolveStepPair(Func&& f, const T& curVal, double t, double h, T& high, T& low) {
    T sumHigh(0.0), sumLow(0.0);
    for (unsigned stage = 0; stage < Stages; ++stage) {
      T accum(0.0);
      for (unsigned j = 0; j < stage; ++j) accum += this->m_tab.coeff[stage - 1][j] * this->K[j];
      this->K[stage] = f(t + h * this->m_tab.nodes[stage], curVal + h * accum);
      sumHigh += this->m_tab.weights[stage] * this->K[stage];
      sumLow += m_low[stage] * this->K[stage];
    }
    high = curVal + h * sumHigh;
    low = curVal + h * sumLow;
  }

  template <typename Func>
  T SolveStep(Func&& f, const T& curVal, double t, double& h, double& next_h, double tolerance,
              double max_step = DBL_MAX, double min_step = DBL_MIN) {
    for (unsigned loop = 0;; ++loop) {
      T sumHigh(0.0), sumLow(0.0);
      for (unsigned stage = 0; stage < Stages; ++stage) {
        T accum(0.0);
        for (unsigned j = 0; j < stage; ++j) accum += this->m_tab.coeff[stage - 1][j] * this->K[j];
        this->K[stage] = f(t + h * this->m_tab.nodes[stage], curVal + h * accum);
        sumHigh += this->m_tab.weights[stage] * this->K[stage];
        sumLow += m_low[stage] * this->K[stage];
      }

      const double error = magnitude((sumHigh - sumLow) * h);

      if (error <= tolerance || loop > 1000 || std::abs(h) <= min_step) {
        T result = curVal + h * sumHigh;  // the HIGH-order solution is the one stepped with

        if (error <= DBL_MIN)
          next_h = h * 2.;
        else if (error < tolerance)
          next_h = h * 0.9 * std::pow(tolerance / error, 1. / (Order + 1));
        else
          next_h = h;

        if (std::abs(next_h) < min_step)
          next_h = min_step * sign(next_h);
        else if (std::abs(next_h) > max_step)
          next_h = max_step * sign(next_h);

        return result;
      }

      // decrease the step size
      h *= 0.9 * std::pow(tolerance / error, 1. / Order);
    }
  }

  bool IsAdaptive() const { return true; }
};

// ---- tableaus (verbatim from RungeKutta.inl) ----
namespace tab {
inline constexpr Tableau<1> euler{{1.}, {0.}, {{0.}}};
inline constexpr Tableau<2> midpoint{{0., 1.}, {0., 1. / 2.}, {{1. / 2.}}};
inline constexpr Tableau<2> ralston{{1. / 4., 3. / 4.}, {0., 2. / 3.}, {{2. / 3.}}};
inline constexpr Tableau<2> heun{{1. / 2., 1. / 2.}, {0., 1.}, {{1.}}};
inline constexpr Tableau<4> rk4{{1. / 6., 1. / 3., 1. / 3., 1. / 6.},
                                {0., 1. / 2., 1. / 2., 1.},
                                {{1. / 2., 0., 0.}, {0., 1. / 2., 0.}, {0., 0., 1.}}};
inline constexpr Tableau<4> rk3per8{{1. / 8., 3. / 8., 3. / 8., 1. / 8.},
                                    {0., 1. / 3., 2. / 3., 1.},
                                    {{1. / 3., 0., 0.}, {-1. / 3., 1., 0.}, {1., -1., 1.}}};
inline constexpr std::array<double, 2> eulerExt{1., 0.};
inline constexpr Tableau<4> bs{{2. / 9., 1. / 3., 4. / 9., 0.},
                               {0., 1. / 2., 3. / 4., 1.},
                               {{1. / 2., 0., 0.}, {0., 3. / 4., 0.}, {2. / 9., 1. / 3., 4. / 9.}}};
inline constexpr std::array<double, 4> bsLow{7. / 24., 1. / 4., 1. / 3., 1. / 8.};
inline constexpr Tableau<6> ck{{37. / 378., 0., 250. / 621., 125. / 594., 0., 512. / 1771.},
                               {0., 1. / 5., 3. / 10., 3. / 5., 1., 7. / 8.},
                               {{1. / 5., 0., 0., 0., 0.},
                                  {3. / 40, 9. / 40., 0., 0., 0.},
                                  {3. / 10., -9. / 10., 6. / 5., 0., 0.},
                                  {-11. / 54., 5. / 2., -70. / 27., 35. / 27., 0.},
                                  {1631. / 55296., 175. / 512., 575. / 13824., 44275. / 110592., 253. / 4096.}}};
inline constexpr std::array<double, 6> ckLow{2825. / 27648., 0., 18575. / 48384., 13525. / 55296., 277. / 14336., 1. / 4.};
inline constexpr Tableau<6> rkf{{16. / 135., 0., 6656. / 12825., 28561. / 56430., -9. / 50., 2. / 55.},
                                {0., 1. / 4., 3. / 8., 12. / 13., 1., 1. / 2.},
                                {{1. / 4., 0., 0., 0., 0.},
                                   {3. / 32., 9. / 32., 0., 0., 0.},
                                   {1932. / 2197., -7200. / 2197., 7296. / 2197., 0., 0.},
                                   {439. / 216., -8., 3680. / 513., -845. / 4104., 0.},
                                   {-8. / 27., 2., -3544. / 2565., 1859. / 4104., -11. / 40.}}};
inline constexpr std::array<double, 6> rkfLow{25. / 216., 0., 1408. / 2565., 2197. / 4104., -1. / 5., 0.};
inline constexpr Tableau<7> dp{{35. / 384., 0., 500. / 1113., 125. / 192., -2187. / 6784., 11. / 84., 0.},
                               {0., 1. / 5., 3. / 10., 4. / 5., 8. / 9., 1., 1.},
                               {{1. / 5., 0., 0., 0., 0., 0.},
                                  {3. / 40., 9. / 40., 0., 0., 0., 0.},
                                  {44. / 45., -56. / 15., 32. / 9., 0., 0., 0.},
                                  {19372. / 6561., -25360. / 2187., 64448. / 6561., -212. / 729., 0., 0.},
                                  {9017. / 3168., -355. / 33., 46732. / 5247., 49. / 176., -5103. / 18656., 0.},
                                  {35. / 384., 0., 500. / 1113., 125. / 192., -2187. / 6784., 11. / 84.}}};
// NOTE: in the MFC source this array holds the ERROR coefficients (b - b*), not b*. Kept on purpose for MFC parity
// (RungeKutta.inl:191), pinned by the characterisation test in tests/test_rk_order.cpp; whether to fix it is a Phase 2
// decision (01-RESEARCH.md Open Questions (RESOLVED) item 1).
inline constexpr std::array<double, 7> dpLow{71. / 57600., 0., -71. / 16695., 71. / 1920., -17253. / 339200., 22. / 525., -1. / 40.};
}  // namespace tab

template <typename T> struct Euler : RungeKutta<T, 1> { Euler() : RungeKutta<T, 1>(tab::euler) {} };
template <typename T> struct Midpoint : RungeKutta<T, 2> { Midpoint() : RungeKutta<T, 2>(tab::midpoint) {} };
template <typename T> struct Ralston : RungeKutta<T, 2> { Ralston() : RungeKutta<T, 2>(tab::ralston) {} };
template <typename T> struct Heun : RungeKutta<T, 2> { Heun() : RungeKutta<T, 2>(tab::heun) {} };
template <typename T> struct RK4 : RungeKutta<T, 4> { RK4() : RungeKutta<T, 4>(tab::rk4) {} };
template <typename T> struct RK3per8 : RungeKutta<T, 4> { RK3per8() : RungeKutta<T, 4>(tab::rk3per8) {} };
template <typename T> struct AdaptiveHeunEuler : AdaptiveRungeKutta<T, 2, 2> { AdaptiveHeunEuler() : AdaptiveRungeKutta<T, 2, 2>(tab::heun, tab::eulerExt) {} };
template <typename T> struct AdaptiveBogackiShampine : AdaptiveRungeKutta<T, 4, 3> { AdaptiveBogackiShampine() : AdaptiveRungeKutta<T, 4, 3>(tab::bs, tab::bsLow) {} };
template <typename T> struct AdaptiveCashKarp : AdaptiveRungeKutta<T, 6, 5> { AdaptiveCashKarp() : AdaptiveRungeKutta<T, 6, 5>(tab::ck, tab::ckLow) {} };
template <typename T> struct AdaptiveFehlberg : AdaptiveRungeKutta<T, 6, 5> { AdaptiveFehlberg() : AdaptiveRungeKutta<T, 6, 5>(tab::rkf, tab::rkfLow) {} };
template <typename T> struct AdaptiveDormandPrince : AdaptiveRungeKutta<T, 7, 5> { AdaptiveDormandPrince() : AdaptiveRungeKutta<T, 7, 5>(tab::dp, tab::dpLow) {} };

}  // namespace rk
}  // namespace ef
