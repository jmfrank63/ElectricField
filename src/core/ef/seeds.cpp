#include "ef/seeds.hpp"

#include <cmath>
#include <cstdlib>

#include "ef/math.hpp"

namespace ef {

std::vector<Seed> planSeeds(const ChargeSystem& field, const ComputeParams& p) {
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);
  const double r = p.seedRadius();

  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    const Charge& charge = field.charges[ci];
    if (charge.charge == 0) continue;

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

int expectedLineCount(const ChargeSystem& field, const ComputeParams& p) {
  bool diff = false;
  const int total = field.totalCharge(diff);
  int result = 0;
  for (const auto& c : field.charges) {
    if (c.charge == 0) continue;
    if (sign(total) == sign(c.charge))
      result += std::abs(c.charge) * static_cast<int>(p.linesPerUnitCharge);
    else
      ++result;
  }
  return result;
}

}  // namespace ef
