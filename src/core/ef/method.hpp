#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "ef/rk.hpp"

namespace ef {

// Original order of Options::CalculationMethod (Options.h:6-18). Numeric values are NOT persisted: names are.
enum class Method {
  Euler,
  Midpoint,
  Ralston,
  Heun,
  RK4,
  RK3per8,
  AdaptiveHeunEuler,
  AdaptiveBogackiShampine,
  AdaptiveCashKarp,
  AdaptiveFehlberg,
  AdaptiveDormandPrince
};

inline constexpr std::size_t kMethodCount = 11;

struct MethodInfo {
  Method method;
  std::string_view name;      // stable persistence key
  std::string_view display;   // UI text = radio-button captions of IDD_COMPUTATIONPROPERTYPAGE (ElectricField.rc:740-750)
  bool adaptive;
  int order;                  // order of the solution that is stepped with
};

inline constexpr std::array<MethodInfo, kMethodCount> kMethods{{
    {Method::Euler, "euler", "Euler", false, 1},
    {Method::Midpoint, "midpoint", "Midpoint", false, 2},
    {Method::Ralston, "ralston", "Ralston", false, 2},
    {Method::Heun, "heun", "Heun", false, 2},
    {Method::RK4, "rk4", "RK4", false, 4},
    {Method::RK3per8, "rk3per8", "RK3/8", false, 4},
    {Method::AdaptiveHeunEuler, "adaptive-heun-euler", "Heun Euler (adaptive)", true, 2},
    {Method::AdaptiveBogackiShampine, "adaptive-bogacki-shampine", "Bogacki Shampine (adaptive)", true, 3},
    {Method::AdaptiveCashKarp, "adaptive-cash-karp", "Cash Karp (adaptive)", true, 5},
    {Method::AdaptiveFehlberg, "adaptive-fehlberg", "Fehlberg (adaptive)", true, 5},
    {Method::AdaptiveDormandPrince, "adaptive-dormand-prince", "Dormand Prince (adaptive)", true, 5},
}};

// D-15 (WR-03): bounds-checked; throws std::out_of_range for a Method value outside the 11 known ones.
inline constexpr const MethodInfo& info(Method m) { return kMethods.at(static_cast<std::size_t>(m)); }

// Persistence key -> Method (nullopt for unknown names; callers fall back to the default explicitly).
inline constexpr std::optional<Method> methodFromName(std::string_view name) {
  for (const auto& mi : kMethods)
    if (mi.name == name) return mi.method;
  return std::nullopt;
}

// Calls visitor(solver) with a freshly constructed solver of the right static type (one per thread).
// Mirrors FieldLinesCalculator::StartComputingThread's switch, but keeps the hot loop non-virtual.
template <typename V>
decltype(auto) withSolver(Method m, V&& visitor) {
  using S = Vec2;
  switch (m) {
    case Method::Euler: { rk::Euler<S> s; return visitor(s); }
    case Method::Midpoint: { rk::Midpoint<S> s; return visitor(s); }
    case Method::Ralston: { rk::Ralston<S> s; return visitor(s); }
    case Method::Heun: { rk::Heun<S> s; return visitor(s); }
    case Method::RK4: { rk::RK4<S> s; return visitor(s); }
    case Method::RK3per8: { rk::RK3per8<S> s; return visitor(s); }
    case Method::AdaptiveHeunEuler: { rk::AdaptiveHeunEuler<S> s; return visitor(s); }
    case Method::AdaptiveBogackiShampine: { rk::AdaptiveBogackiShampine<S> s; return visitor(s); }
    case Method::AdaptiveCashKarp: { rk::AdaptiveCashKarp<S> s; return visitor(s); }
    case Method::AdaptiveFehlberg: { rk::AdaptiveFehlberg<S> s; return visitor(s); }
    case Method::AdaptiveDormandPrince: { rk::AdaptiveDormandPrince<S> s; return visitor(s); }
  }
  throw std::invalid_argument("ef::withSolver: invalid Method value");  // D-15: no silent fallback to Euler
}

}  // namespace ef
