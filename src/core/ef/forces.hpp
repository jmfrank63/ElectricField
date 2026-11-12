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
//
// Conductor model (ChargeModel::Conductor): the force acts on a body as a whole. It is the field of all
// OTHER bodies acting on the body's solved sources, which is exact for the solved field. forces() reports
// each ball's share (the part acting on its own sources); only the sum over a body is meaningful.
// potentialEnergy() is then the total electrostatic energy, (1/2) sum over bodies of charge * potential,
// which includes each body's own energy.
#include <cstddef>
#include <vector>

#include "ef/charge_system.hpp"
#include "ef/conductor.hpp"

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
  if (field.conductor()) {
    const std::shared_ptr<const SurfaceSolution> s = solveSurface(field);
    for (const SurfaceSolution::Ball& ball : s->balls)
      for (std::size_t k = ball.begin; k < ball.end; ++k) {
        Vec3 e;
        for (const SurfaceSolution::Ball& other : s->balls) {
          if (other.body == ball.body) continue;
          for (std::size_t j = other.begin; j < other.end; ++j) {
            const Vec3 d = s->sources[k].at - s->sources[j].at;
            const double d2 = d * d;
            e += d * (s->sources[j].q / (d2 * std::sqrt(d2)));
          }
        }
        out[ball.index] += e * s->sources[k].q;
      }
    return out;
  }
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
  if (field.conductor()) {
    const std::shared_ptr<const SurfaceSolution> s = solveSurface(field);
    for (const SurfaceSolution::Ball& ball : s->balls)
      u += 0.5 * field.charges[ball.index].charge * s->bodyPotential[static_cast<std::size_t>(ball.body)];
    return u;
  }
  const std::size_t n = field.charges.size();
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i + 1; j < n; ++j)
      u += field.charges[i].charge * field.charges[j].charge *
           pairEnergy((field.charges[i].position3() - field.charges[j].position3()).Length(), field.radius);
  return u;
}

}  // namespace ef
