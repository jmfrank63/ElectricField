#pragma once
// Shapes: named rigid groups of balls for the equipotential (Conductor) charge model. A shape is the
// relative arrangement of ball centres in units of the ball radius, so it scales with the system's
// charge radius. Placed in a system, its balls share a group number (Charge::group) and are moved and
// turned as one unit; the body carries one total charge, kept on its first ball.
#include <cmath>
#include <string>
#include <vector>

#include "ef/charge_system.hpp"

namespace ef {

struct Shape {
  std::string name;
  std::vector<Vec3> balls;  // centres relative to the shape's centre, in units of the ball radius
};

// Largest number of balls in one shape, and the largest offset (in radii) accepted from files.
inline constexpr std::size_t kMaxShapeBalls = 32;
inline constexpr double kMaxShapeOffset = 1000.;

// A shape is usable when it has 1..kMaxShapeBalls balls with finite offsets and all of them hang
// together (every ball within two radii of the rest), so it forms one body.
inline bool validShape(const Shape& shape) {
  const std::size_t n = shape.balls.size();
  if (n == 0 || n > kMaxShapeBalls) return false;
  for (const Vec3& b : shape.balls)
    if (!std::isfinite(b.X) || !std::isfinite(b.Y) || !std::isfinite(b.Z) || b.Length() > kMaxShapeOffset) return false;
  std::vector<bool> reached(n, false);
  std::vector<std::size_t> open{0};
  reached[0] = true;
  while (!open.empty()) {
    const std::size_t i = open.back();
    open.pop_back();
    for (std::size_t j = 0; j < n; ++j)
      if (!reached[j] && (shape.balls[i] - shape.balls[j]).Length() <= 2. + 1e-9) {
        reached[j] = true;
        open.push_back(j);
      }
  }
  for (const bool r : reached)
    if (!r) return false;
  return true;
}

// The shapes every installation has.
inline const std::vector<Shape>& builtinShapes() {
  static const std::vector<Shape> shapes = [] {
    const double t = 0.9;                  // triangle: distance of each ball from the centre
    const double h = std::sqrt(3.) / 2.;
    const double e = 1.5 / std::sqrt(8.);  // tetrahedron of edge 1.5: corners at (+-e, +-e, +-e)
    return std::vector<Shape>{
        {"Ball", {Vec3(0, 0, 0)}},
        {"Eight", {Vec3(0, 0.75, 0), Vec3(0, -0.75, 0)}},
        {"Chain of three", {Vec3(-1.5, 0, 0), Vec3(0, 0, 0), Vec3(1.5, 0, 0)}},
        {"Triangle", {Vec3(0, t, 0), Vec3(-t * h, -t / 2., 0), Vec3(t * h, -t / 2., 0)}},
        {"Tetrahedron", {Vec3(e, e, e), Vec3(e, -e, -e), Vec3(-e, e, -e), Vec3(-e, -e, e)}},
    };
  }();
  return shapes;
}

// The smallest unused group number of a system (groups count from 1; 0 = not part of a shape).
inline unsigned nextGroup(const ChargeSystem& field) {
  unsigned highest = 0;
  for (const Charge& c : field.charges) highest = c.group > highest ? c.group : highest;
  return highest + 1;
}

// The balls of `shape` placed around `centre` for a system of ball radius `radius`, as one new group
// carrying `totalCharge` (on the first ball; the others hold 0, which is allowed inside a group).
inline std::vector<Charge> placeShape(const Shape& shape, const Vec3& centre, double radius, int totalCharge, unsigned group) {
  std::vector<Charge> out;
  out.reserve(shape.balls.size());
  for (std::size_t i = 0; i < shape.balls.size(); ++i) {
    const Vec3 at = centre + shape.balls[i] * radius;
    Charge c;
    c.charge = i == 0 ? totalCharge : 0;
    c.position = Vec2(at.X, at.Y);
    c.z = at.Z;
    c.group = group;
    out.push_back(c);
  }
  return out;
}

// The shape formed by the given balls of a system (their arrangement about their mean centre).
inline Shape shapeOf(const ChargeSystem& field, const std::vector<std::size_t>& members, const std::string& name) {
  Shape shape;
  shape.name = name;
  if (members.empty() || field.radius <= 0) return shape;
  Vec3 centre;
  for (const std::size_t i : members) centre += field.charges[i].position3();
  centre /= static_cast<double>(members.size());
  for (const std::size_t i : members) shape.balls.push_back((field.charges[i].position3() - centre) / field.radius);
  return shape;
}

}  // namespace ef
