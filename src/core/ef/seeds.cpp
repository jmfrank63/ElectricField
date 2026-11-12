#include "ef/seeds.hpp"

#include <cmath>
#include <cstdlib>

#include "ef/math.hpp"

namespace ef {

std::vector<Seed> planSeeds(const ChargeSystem& field, const ComputeParams& p) {
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);

  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    const Charge& charge = field.charges[ci];
    if (charge.charge == 0) continue;

    // 261020-ef1: the seed circle is the charge's OWN effective drawn dot (its sizePx, else the
    // Options default) - the lines are born on the dot the user drew, at every zoom. A charge
    // without its own size reproduces the former p.seedRadius() value exactly (parity).
    const double dotPx = charge.sizePx > 0.f ? static_cast<double>(charge.sizePx) : p.chargeRadiusPx;
    const double r = p.seedRadiusFor(dotPx);

    const double angle_step = 2. * kPi / (std::abs(static_cast<double>(charge.charge)) * p.linesPerUnitCharge);

    double angle_start = -angle_step / 2. - kPi;
    const bool source = sign(total) == sign(charge.charge);
    if (!source) angle_start += kPi + angle_step;

    for (double angle = angle_start; angle < 2. * kPi + angle_start - angle_step / 4.; angle += angle_step) {
      const bool first = (angle == angle_start);
      if ((!first || !p.calculateEquipotentials) && !source) break;

      Seed s;
      s.chargeIndex = ci;
      s.charge = charge.charge;
      s.totalCharge = total;
      s.hasDifferentSigns = hasDifferentSigns;
      s.angle = angle;
      s.first = first;
      s.isSource = source;
      s.point.X = charge.position.X + r * std::cos(angle);
      s.point.Y = charge.position.Y + r * std::sin(angle);
      out.push_back(s);
    }
  }
  return out;
}

std::vector<Seed> planSeeds3(const ChargeSystem& field, const ComputeParams& p) {
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);
  const double goldenAngle = kPi * (3. - std::sqrt(5.));

  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    const Charge& charge = field.charges[ci];
    if (charge.charge == 0 || sign(total) != sign(charge.charge)) continue;

    const double dotPx = charge.sizePx > 0.f ? static_cast<double>(charge.sizePx) : p.chargeRadiusPx;
    const double r = p.seedRadiusFor(dotPx);
    const unsigned n = static_cast<unsigned>(std::abs(charge.charge)) * p.linesPerUnitCharge;

    for (unsigned i = 0; i < n; ++i) {
      const double h = 1. - (2. * i + 1.) / n;  // height on the unit sphere, evenly spaced
      const double ring = std::sqrt(1. - h * h);
      const double angle = goldenAngle * i;
      Seed s;
      s.chargeIndex = ci;
      s.charge = charge.charge;
      s.totalCharge = total;
      s.hasDifferentSigns = hasDifferentSigns;
      s.angle = angle;
      s.first = (i == 0);
      s.isSource = true;
      s.point3 = charge.position3() + Vec3(ring * std::cos(angle), ring * std::sin(angle), h) * r;
      s.point = field.camera.project(s.point3);
      out.push_back(s);
    }
  }
  return out;
}

int expectedLineCount(const ChargeSystem& field, const ComputeParams& p) {
  bool diff = false;
  const int total = field.totalCharge(diff);
  int result = 0;
  const bool space = field.is3D();
  for (const auto& c : field.charges) {
    if (c.charge == 0) continue;
    if (sign(total) == sign(c.charge))
      result += std::abs(c.charge) * static_cast<int>(p.linesPerUnitCharge);
    else if (!space)
      ++result;
  }
  return result;
}

}  // namespace ef
