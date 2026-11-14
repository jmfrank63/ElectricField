#pragma once
// Shared between the flat renderer (scene.cpp) and the 3D one (scene3d.cpp). Not part of the public API.
#include "ef/scene.hpp"

namespace ef {

// A neutral body (total charge 0) is drawn in the line colour.
inline Color chargeColor(const DrawStyle& style, int charge) {
  return charge > 0 ? style.positiveCharge : charge < 0 ? style.negativeCharge : style.electricLine;
}

void drawGrid(Painter& painter, const GridSettings& g, const View& view);

// renderScene for FieldScene::space.
void renderSceneSpace(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view, const GridSettings* grid);

}  // namespace ef
