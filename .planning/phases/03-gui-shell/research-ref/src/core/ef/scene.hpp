#pragma once
// Toolkit-free scene + painter layer (D-11). Replaces FieldLine::Draw / Charge::Draw / TheElectricField::Draw
// (Direct2D + GDI). Geometry decisions (draw order, y-up/centred transform, 100 px/unit, culling, closed figures)
// live here ONCE; backends (SVG, ImGui ImDrawList, later anything else) only implement Painter.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ef/charge_system.hpp"
#include "ef/finalizer.hpp"

namespace ef {

struct Color {
  std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

// MFC defaults: Options.cpp:8-17 (positive RGB(0,0,255), negative RGB(255,0,0), E lines RGB(0,0,0), potential lines
// RGB(192,192,192), thickness 1, chargeRadius 25 px); background = ElectricFieldView.cpp:129 Clear(White).
struct DrawStyle {
  Color positiveCharge{0, 0, 255};
  Color negativeCharge{255, 0, 0};
  Color electricLine{0, 0, 0};
  Color potentialLine{192, 192, 192};
  Color background{255, 255, 255};
  float electricThickness = 1.f;
  float potentialThickness = 1.f;
  float chargeRadiusPx = 25.f;
};

// Output surface: origin at the centre, y up, `pixelsPerUnit` device pixels per world unit (view scale only, never physics).
struct View {
  int widthPx = 1024;
  int heightPx = 768;
  double pixelsPerUnit = 100.;
};

struct ScenePath {
  std::vector<Vec2> points;  // world units; layout p0,[c1,c2,p3],[c4,c5,p6],... (+ up to 2 trailing line points)
  bool closed = false;       // equipotential whose first and last point coincide (MFC: isPotential && front==back)
};

struct SceneCharge {
  Vec2 position;
  int charge = 0;
};

struct FieldScene {
  std::vector<ScenePath> electric;
  std::vector<ScenePath> potential;
  std::vector<SceneCharge> charges;
};

FieldScene buildScene(const ChargeSystem& field, const FieldResult& result);

struct PointF {
  float x = 0, y = 0;
};

// ~4 calls. Coordinates are DEVICE pixels (origin top-left, y down); a Painter never sees world units or options.
class Painter {
 public:
  virtual ~Painter() = default;
  virtual void begin(int widthPx, int heightPx, Color background) = 0;
  // pts[0..3*bezierSegments] is a chain of cubic segments (p0, then c1,c2,p3 per segment); the remaining points are
  // straight line-to points. bezierSegments == 0 => polyline. closed => connect the last point back to the first.
  virtual void strokePath(const PointF* pts, std::size_t n, std::size_t bezierSegments, bool closed, Color color, float width) = 0;
  virtual void fillCircle(PointF center, float radius, Color color) = 0;
  virtual void end() = 0;
};

// Draw order as in TheElectricField::Draw: electric lines, potential lines, charges.
void renderScene(Painter& painter, const FieldScene& scene, const DrawStyle& style, const View& view);

}  // namespace ef
