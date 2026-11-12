#pragma once
// Numeric export as CSV (comma separated, one header row, '.' decimal point, no quoting needed except
// for charge names). Two tables:
//
//  chargesCsv: one row per charge
//    index, name, charge, x, y, z, radius, body, potential, ex, ey, ez, fx, fy, fz, force,
//    screen_x, screen_y, view_yaw_deg, view_pitch_deg, system_energy
//    potential and e* are the potential and field of all OTHER charges at the charge's centre; f* is the
//    force on it (ef/forces.hpp); body is the melted body it belongs to; screen_* is where the camera
//    draws it; the view angles and the system's interaction energy repeat on every row.
//
//  bodiesCsv: one row per body (a group of balls that touch or overlap; a point charge is its own body)
//    body, balls, charge, x, y, z, px, py, pz, dipole, flux_out, flux_in, fx, fy, fz, force, model,
//    potential, surface_error
//    charge is the net charge; x, y, z the mean of the ball centres; p* the dipole moment about that
//    point and dipole its length; flux_out and flux_in the field leaving and entering the body's outer
//    surface in charge units, so flux_out - flux_in is the net charge and the number of field lines
//    leaving the body is about flux_out times the lines per unit charge. f* is the force on the body.
//    model is "uniform" or "conductor"; with the conductor model, potential is the body's constant
//    surface potential and surface_error how far the solved surface is from an equipotential (the
//    largest deviation relative to the largest body potential).
//    In the charge table, with the conductor model, potential is the body's and the force is the ball's
//    share of the force on its body.
//
//  linesCsv: one row per point of every drawn line
//    kind, line, point, x, y, z, screen_x, screen_y, potential, ex, ey, ez, magnitude, azimuth_deg,
//    elevation_deg, label
//    kind is "field" or "equipotential"; line and point count from 0; potential, the field vector, its
//    magnitude and its direction (azimuth = angle of (ex, ey) from the x axis, elevation = angle above
//    the xy plane) are evaluated at the point; label is the nominal level of an equipotential (a multiple of the potential
//    interval; the potential column holds the exact value along the line).
#include <string>

#include "ef/charge_system.hpp"
#include "ef/scene.hpp"

namespace ef {

std::string chargesCsv(const ChargeSystem& field);
std::string bodiesCsv(const ChargeSystem& field);
// `scene` must have been built from `field` (buildScene).
std::string linesCsv(const ChargeSystem& field, const FieldScene& scene);

// The points of a stored path that lie ON the curve: the layout is p0,[c1,c2,p3],... plus up to two
// trailing line points, so the control points in between are skipped.
std::vector<Vec2> onCurvePoints(const std::vector<Vec2>& points);

}  // namespace ef
