#pragma once
// The snap grid of a tab (v2.3 GRID, moved into the core in v2.4 so renderScene can draw it).
// active = snapping on; visible = drawn; Cartesian (spacing) or polar (radial spacing + angular step),
// both around a definable origin.
#include "ef/vec2.hpp"

namespace ef {

struct GridSettings {
  bool active = false;
  bool visible = false;
  bool polar = false;
  Vec2 origin;
  double spacing = 0.5;
  double angleStepDeg = 15.0;
};

}  // namespace ef
