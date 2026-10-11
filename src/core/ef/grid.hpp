#pragma once
// The snap grid of a tab (v2.3 GRID, moved into the core in v2.4 so renderScene can draw it).
// active = snapping on; visible = drawn; Cartesian (spacing) or polar (radial spacing + angular step),
// both around a definable origin. v2.5: lines are light green, and an intersections-only mode draws
// just the lattice points in a distinctly different colour.
#include "ef/vec2.hpp"

namespace ef {

struct GridSettings {
  bool active = false;
  bool visible = false;
  bool polar = false;
  bool intersectionsOnly = false;  // v2.5 GRID-06: draw only lattice points, no lines
  Vec2 origin;
  double spacing = 0.5;
  double angleStepDeg = 15.0;
};

}  // namespace ef