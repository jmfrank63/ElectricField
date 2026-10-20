#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ef/params.hpp"
#include "ef/vec2.hpp"

namespace ef {

struct Charge {
  int charge = 1;
  Vec2 position;

  // v2.6 phase 3: identity + per-charge draw state. Trailing members with defaults keep every existing
  // aggregate init compiling (id 0 = unassigned, name "" = derived display "Charge N", sizePx 0 =
  // inherit the Options charge size). Draw-only: the field/potential math never reads them.
  std::uint64_t id = 0;
  std::string name{};
  float sizePx = 0.f;

  double Potential(const Vec2& pos) const { return charge / (pos - position).Length(); }  // Charge.h:31
  Vec2 E(const Vec2& pos) const {                                                          // Charge.h:37
    const Vec2 toPos = pos - position;
    const double len2 = toPos * toPos;
    Vec2 result = toPos / (len2 * std::sqrt(len2));
    result *= charge;
    return result;
  }
  bool Hit(const Vec2& pos, double hitRadius) const { return (position - pos).Length() < hitRadius; }
};

class ChargeSystem {
 public:
  std::vector<Charge> charges;

  double Potential(const Vec2& pos) const {
    double P = 0;
    for (const auto& c : charges) P += c.Potential(pos);
    return P;
  }
  Vec2 E(const Vec2& pos) const {
    Vec2 result;
    for (const auto& c : charges) result += c.E(pos);
    return result;
  }
  Vec2 ENormalized(const Vec2& pos) const { return E(pos).Normalize(); }
  bool HitCharge(const Vec2& pos, const ComputeParams& p) const {
    for (const auto& c : charges) {
      // 261020-ef1: the hit target is the charge's OWN effective drawn dot (its sizePx, else the
      // Options default) - a field line ends on the dot the user drew, at every zoom.
      const double dotPx = c.sizePx > 0.f ? static_cast<double>(c.sizePx) : p.chargeRadiusPx;
      if (c.Hit(pos, p.hitRadiusFor(dotPx))) return true;
    }
    return false;
  }

  // TheElectricField::GetTotalCharge (TheElectricField.cpp:65-80). Guarded for empty systems.
  int totalCharge(bool& hasDifferentSigns) const {
    hasDifferentSigns = false;
    if (charges.empty()) return 0;
    int s = charges.front().charge < 0 ? -1 : 1;
    int total = 0;
    for (const auto& c : charges) {
      total += c.charge;
      if (s != (c.charge > 0 ? 1 : -1)) hasDifferentSigns = true;
    }
    return total;
  }
};

}  // namespace ef
