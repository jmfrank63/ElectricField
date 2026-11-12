#pragma once
// The Conductor charge model (ChargeModel::Conductor): solves for the field in which the outer surface
// of every body is an equipotential, so field lines meet it at a right angle.
#include "ef/charge_system.hpp"

namespace ef {

// Solves the surface problem of a Conductor system, or returns null for any other system (Uniform balls,
// point charges). Method of fundamental solutions: point sources on a sphere of 0.6 R inside every ball,
// fitted by least squares so that the potential is constant over each body's outer surface and the
// sources of a body sum to its charge.
std::shared_ptr<const SurfaceSolution> solveSurface(const ChargeSystem& field);

// Makes `field` ready to be evaluated: solves and attaches the surface solution when the model needs
// one, and removes a stale one otherwise. Every computation calls this on its own copy.
void prepare(ChargeSystem& field);
inline ChargeSystem prepared(ChargeSystem field) {
  prepare(field);
  return field;
}

}  // namespace ef
