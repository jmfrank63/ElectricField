#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ef/params.hpp"
#include "ef/vec2.hpp"
#include "ef/vec3.hpp"

namespace ef {

inline constexpr double kMinChargeSizePx = 2.0;

struct Charge {
  int charge = 1;
  Vec2 position;

  // v2.6 phase 3: identity + per-charge draw state. Trailing members with defaults keep every existing
  // aggregate init compiling (id 0 = unassigned, name "" = derived display "Charge N", sizePx 0 =
  // inherit the Options charge size). Draw-only: the field/potential math never reads them.
  std::uint64_t id = 0;
  std::string name{};
  float sizePx = 0.f;
  // Third coordinate. 0 for every preset and legacy file: such a system lies in the drawing plane and
  // keeps the original 2D pipeline bit for bit (see ChargeSystem::is3D).
  double z = 0;

  Vec3 position3() const { return Vec3(position.X, position.Y, z); }
  // `radius` > 0 makes the charge a uniformly charged ball of that radius (world units): exact Coulomb
  // outside, E = q r / R^3 and V = q (3R^2 - r^2) / (2R^3) inside, finite everywhere. radius 0 is the
  // point charge of the original (Charge.h:31/37), the limit of the same rules for R -> 0.
  double Potential(const Vec2& pos, double radius = 0) const {
    const double r = (pos - position).Length();
    if (r < radius) return charge * (3. * radius * radius - r * r) / (2. * radius * radius * radius);
    return charge / r;
  }
  Vec2 E(const Vec2& pos, double radius = 0) const {
    const Vec2 toPos = pos - position;
    const double len2 = toPos * toPos;
    if (len2 < radius * radius) return toPos * (charge / (radius * radius * radius));
    Vec2 result = toPos / (len2 * std::sqrt(len2));
    result *= charge;
    return result;
  }
  bool Hit(const Vec2& pos, double hitRadius) const { return (position - pos).Length() < hitRadius; }

  // The same rules at a point in space.
  double Potential(const Vec3& pos, double radius = 0) const {
    const double r = (pos - position3()).Length();
    if (r < radius) return charge * (3. * radius * radius - r * r) / (2. * radius * radius * radius);
    return charge / r;
  }
  Vec3 E(const Vec3& pos, double radius = 0) const {
    const Vec3 toPos = pos - position3();
    const double len2 = toPos * toPos;
    if (len2 < radius * radius) return toPos * (charge / (radius * radius * radius));
    Vec3 result = toPos / (len2 * std::sqrt(len2));
    result *= charge;
    return result;
  }
  bool Hit(const Vec3& pos, double hitRadius) const { return (position3() - pos).Length() < hitRadius; }
};

// Largest ball radius accepted from files, scripts and the MCP server (world units).
inline constexpr double kMaxChargeRadius = 1000.;

class ChargeSystem {
 public:
  std::vector<Charge> charges;
  // Ball radius of every charge in world units; 0 = point charges (presets, legacy files, parity).
  double radius = 0;
  // The view onto the system (orthographic, turned about the origin). Not physics.
  Camera camera;

  // True when the system needs the 3D pipeline: a charge off the drawing plane or a turned camera.
  // Otherwise everything lies in the plane the camera looks at and the original 2D pipeline applies.
  bool is3D() const {
    if (!camera.isDefault()) return true;
    for (const auto& c : charges)
      if (c.z != 0) return true;
    return false;
  }

  double Potential(const Vec3& pos) const {
    double P = 0;
    for (const auto& c : charges) P += c.Potential(pos, radius);
    return P;
  }
  Vec3 E(const Vec3& pos) const {
    Vec3 result;
    for (const auto& c : charges) result += c.E(pos, radius);
    return result;
  }
  Vec3 ENormalized(const Vec3& pos) const { return E(pos).Normalize(); }
  bool AtBallNull(const Vec3& pos, double tolerance) const {
    if (radius <= 0) return false;
    const double r3 = radius * radius * radius;
    double weakest = 0;
    bool inside = false;
    for (const auto& c : charges) {
      const Vec3 d = pos - c.position3();
      if (d * d >= radius * radius) continue;
      const double gradient = std::abs(static_cast<double>(c.charge)) / r3;
      if (!inside || gradient < weakest) weakest = gradient;
      inside = true;
    }
    return inside && E(pos).Length() < weakest * tolerance;
  }
  bool HitCharge(const Vec3& pos, const ComputeParams& p) const {
    for (const auto& c : charges) {
      const double dotPx = c.sizePx > 0.f ? static_cast<double>(c.sizePx) : p.chargeRadiusPx;
      if (c.Hit(pos, p.hitRadiusFor(dotPx))) return true;
    }
    return false;
  }

  double Potential(const Vec2& pos) const {
    double P = 0;
    for (const auto& c : charges) P += c.Potential(pos, radius);
    return P;
  }
  Vec2 E(const Vec2& pos) const {
    Vec2 result;
    for (const auto& c : charges) result += c.E(pos, radius);
    return result;
  }
  // A field line flowing into a ball does not have to reach the centre: inside a ball the field has a
  // null (where the ball's own field cancels the rest), and the line converges on it. True when `pos` lies
  // inside a ball within `tolerance` of such a null, judged by the ball's own field gradient |q|/R^3.
  bool AtBallNull(const Vec2& pos, double tolerance) const {
    if (radius <= 0) return false;
    const double r3 = radius * radius * radius;
    double weakest = 0;  // the smallest own-field gradient among the balls containing pos
    bool inside = false;
    for (const auto& c : charges) {
      if ((pos - c.position) * (pos - c.position) >= radius * radius) continue;
      const double gradient = std::abs(static_cast<double>(c.charge)) / r3;
      if (!inside || gradient < weakest) weakest = gradient;
      inside = true;
    }
    return inside && E(pos).Length() < weakest * tolerance;
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
