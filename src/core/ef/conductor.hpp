#pragma once
// The Conductor charge model (ChargeModel::Conductor): solves for the field in which the outer surface
// of every body is an equipotential, so field lines meet it at a right angle.
#include "ef/charge_system.hpp"

namespace ef {

// Solves the surface problem of a Conductor system, or returns null for any other system (Uniform balls,
// point charges). Method of fundamental solutions: point sources on a sphere of 0.6 R inside every ball,
// fitted by least squares so that the potential is constant over each body's outer surface and the
// sources of a body sum to its charge.
// The result depends only on the charges, their positions and the radius; it is computed once and the
// most recent solutions are kept, so recalculating the lines, exporting or asking for the forces of the
// same system reuses it. Safe to call from any thread.
std::shared_ptr<const SurfaceSolution> solveSurface(const ChargeSystem& field);

// Makes `field` ready to be evaluated: solves and attaches the surface solution when the model needs
// one, and removes a stale one otherwise. Every computation calls this on its own copy.
void prepare(ChargeSystem& field);
inline ChargeSystem prepared(ChargeSystem field) {
  prepare(field);
  return field;
}

}  // namespace ef
