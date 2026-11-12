#pragma once
// 3D vector and the orthographic camera that maps 3D space onto the 2D drawing plane.
#include <cmath>

#include "ef/math.hpp"
#include "ef/vec2.hpp"

namespace ef {

struct Vec3 {
  double X = 0;
  double Y = 0;
  double Z = 0;

  constexpr Vec3() = default;
  explicit constexpr Vec3(double v) : X(v), Y(v), Z(v) {}
  constexpr Vec3(double x, double y, double z) : X(x), Y(y), Z(z) {}

  constexpr Vec3 operator-() const { return Vec3(-X, -Y, -Z); }
  constexpr Vec3 operator+(const Vec3& o) const { return Vec3(X + o.X, Y + o.Y, Z + o.Z); }
  constexpr Vec3 operator-(const Vec3& o) const { return Vec3(X - o.X, Y - o.Y, Z - o.Z); }
  constexpr double operator*(const Vec3& o) const { return X * o.X + Y * o.Y + Z * o.Z; }  // dot product
  constexpr Vec3 operator*(double s) const { return Vec3(X * s, Y * s, Z * s); }
  constexpr Vec3 operator/(double s) const { return Vec3(X / s, Y / s, Z / s); }
  Vec3& operator+=(const Vec3& o) { return *this = *this + o; }
  Vec3& operator-=(const Vec3& o) { return *this = *this - o; }
  Vec3& operator*=(double s) { return *this = *this * s; }
  Vec3& operator/=(double s) { return *this = *this / s; }

  double Length() const { return std::sqrt(*this * *this); }
  Vec3 Normalize() const {
    const double len = Length();
    if (len != 0) return *this / len;  // zero vector stays zero
    return *this;
  }
};

constexpr Vec3 operator*(double s, const Vec3& v) { return v * s; }
constexpr bool operator==(const Vec3& a, const Vec3& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z; }
constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
  return Vec3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
}

// Orthographic camera turned about the world origin: yaw about the world y axis, then pitch about the
// screen's horizontal axis (radians). The default (0, 0) looks down the z axis, so a point (x, y, z)
// lands on (x, y): the plane the 2D app has always drawn. View only, never physics.
struct Camera {
  double yaw = 0;
  double pitch = 0;

  bool isDefault() const { return yaw == 0 && pitch == 0; }
  Vec3 right() const { return Vec3(std::cos(yaw), 0, std::sin(yaw)); }
  Vec3 up() const { return Vec3(std::sin(yaw) * std::sin(pitch), std::cos(pitch), -std::cos(yaw) * std::sin(pitch)); }
  Vec3 towardsViewer() const { return Vec3(-std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)); }
  Vec2 project(const Vec3& p) const {
    if (isDefault()) return Vec2(p.X, p.Y);  // exact, no rounding through the trigonometry
    return Vec2(p * right(), p * up());
  }
  double depth(const Vec3& p) const { return isDefault() ? p.Z : p * towardsViewer(); }  // larger = nearer
};

}  // namespace ef
