#pragma once
#include <cmath>
#include <vector>

#include "ef/params.hpp"
#include "ef/vec2.hpp"

namespace ef {

struct Charge {
  int charge = 1;
  Vec2 position;

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
    for (const auto& c : charges)
      if (c.Hit(pos, p.hitRadius())) return true;
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
