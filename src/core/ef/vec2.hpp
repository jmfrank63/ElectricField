#pragma once
#include <cmath>

namespace ef {

template <typename T>
struct Vector2D {
  T X{};
  T Y{};

  constexpr Vector2D() = default;
  explicit constexpr Vector2D(const T& v) : X(v), Y(v) {}
  constexpr Vector2D(const T& x, const T& y) : X(x), Y(y) {}

  constexpr Vector2D operator-() const { return Vector2D(-X, -Y); }
  constexpr Vector2D operator+(const Vector2D& o) const { return Vector2D(X + o.X, Y + o.Y); }
  constexpr Vector2D operator-(const Vector2D& o) const { return Vector2D(X - o.X, Y - o.Y); }
  constexpr T operator*(const Vector2D& o) const { return X * o.X + Y * o.Y; }  // dot product
  constexpr Vector2D operator*(T s) const { return Vector2D(X * s, Y * s); }
  constexpr Vector2D operator/(T s) const { return Vector2D(X / s, Y / s); }
  Vector2D& operator+=(const Vector2D& o) { return *this = *this + o; }
  Vector2D& operator-=(const Vector2D& o) { return *this = *this - o; }
  Vector2D& operator*=(T s) { return *this = *this * s; }
  Vector2D& operator/=(T s) { return *this = *this / s; }

  double Length() const {
    const double w = *this * *this;
    return std::sqrt(w);
  }
  Vector2D Normalize() const {
    const T len = static_cast<T>(Length());
    if (len != 0) return *this / len;  // zero vector stays zero
    return *this;
  }
};

template <typename T>
constexpr Vector2D<T> operator*(T s, const Vector2D<T>& v) { return v * s; }
template <typename T>
constexpr bool operator==(const Vector2D<T>& a, const Vector2D<T>& b) { return a.X == b.X && a.Y == b.Y; }

using Vec2 = Vector2D<double>;

}  // namespace ef
