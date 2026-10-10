#pragma once
// Portable numerics helpers for the physics core (std-only).
#include <cmath>

namespace ef {

// Never use M_PI (macro on glibc/macOS) and never redefine it.
inline constexpr double kPi = 3.1415926535897932384626433832795028841971693993751;

// Original macro: #define sign(x) ((x) >= 0 ? 1 : -1)  -- sign(0) == +1 is LOAD-BEARING
// (net-zero configurations choose their source charges with it).
inline constexpr int sign(double x) noexcept { return x >= 0 ? 1 : -1; }

}  // namespace ef
