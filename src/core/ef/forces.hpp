#pragma once
// Forces and electrostatic energy between the charges of a system, in space. Units as everywhere in the
// core: charges in elementary units, Coulomb constant 1, so F = q1 q2 / d^2 and U = q1 q2 / d.
//
// Rules:
//  - Point charges (radius 0) and balls that do not overlap (d >= 2R): exact Coulomb between the centres
//    (a uniformly charged ball acts on the outside, and is acted on, like a point charge).
//  - Overlapping balls (d < 2R): the balls are rigid and pass through each other, each keeping its uniform
//    charge. The force is the field of one ball integrated over the charge of the other, in closed form
//    with s = d / R:
//        U(d) = q1 q2 / R   * (6/5 - s^2/2 + 3 s^3/16 - s^5/160)
//        F(d) = q1 q2 / R^2 * (s - 9 s^2/16 + s^4/32)
//    Both join Coulomb smoothly at d = 2R; at d = 0 the force is 0 and U = (6/5) q1 q2 / R.
#include <cstddef>
#include <vector>

#include "ef/charge_system.hpp"

namespace ef {

// Magnitude factor of the pair force for unit charges at centre distance d (positive = repulsive for
// like charges). 0 for coincident point charges, where the direction is undefined.
inline double pairForce(double d, double radius) {
  if (d <= 0) return 0;
  if (radius > 0 && d < 2. * radius) {
    const double s = d / radius;
    return (s - 9. * s * s / 16. + s * s * s * s / 32.) / (radius * radius);
  }
  return 1. / (d * d);
}

// Interaction energy of the pair for unit charges at centre distance d. Infinite for coincident points.
inline double pairEnergy(double d, double radius) {
  if (radius > 0 && d < 2. * radius) {
    const double s = d / radius;
    return (6. / 5. - s * s / 2. + 3. * s * s * s / 16. - s * s * s * s * s / 160.) / radius;
  }
  return 1. / d;
}

// The force on every charge from all the others (same order as field.charges).
inline std::vector<Vec3> forces(const ChargeSystem& field) {
  const std::size_t n = field.charges.size();
  std::vector<Vec3> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      const Vec3 d = field.charges[i].position3() - field.charges[j].position3();
      const double len = d.Length();
      if (len <= 0) continue;
      const Vec3 f = d * (field.charges[i].charge * field.charges[j].charge * pairForce(len, field.radius) / len);
      out[i] += f;
      out[j] -= f;
    }
  }
  return out;
}

// Interaction energy of the system: the sum over all pairs. The self energy of the balls is not included
// (it is the same constant for every arrangement).
inline double potentialEnergy(const ChargeSystem& field) {
  double u = 0;
  const std::size_t n = field.charges.size();
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i + 1; j < n; ++j)
      u += field.charges[i].charge * field.charges[j].charge *
           pairEnergy((field.charges[i].position3() - field.charges[j].position3()).Length(), field.radius);
  return u;
}

}  // namespace ef
