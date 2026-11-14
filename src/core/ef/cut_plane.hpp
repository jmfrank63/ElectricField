#pragma once
// Equipotentials of a 3D system. In space they are surfaces; what is drawn is their cut with one plane:
// the plane parallel to the screen through the centre of the charges. Inside that plane the potential is
// an ordinary function of two coordinates, so the 2D equipotential tracer follows its contours.
#include <algorithm>
#include <cmath>

#include "ef/charge_system.hpp"
#include "ef/params.hpp"
#include "ef/seeds.hpp"
#include "ef/tracer.hpp"

namespace ef {

// The field of a ChargeSystem restricted to the cut plane, in screen coordinates (world units). It has
// the members the 2D equipotential tracer uses of a ChargeSystem.
struct CutPlane {
  const ChargeSystem* field = nullptr;
  double depth = 0;  // of the plane, along the camera's line of sight

  explicit CutPlane(const ChargeSystem& f) : field(&f) {
    for (const Charge& c : f.charges) depth += f.camera.depth(c.position3());
    if (!f.charges.empty()) depth /= static_cast<double>(f.charges.size());
  }

  Vec3 lift(const Vec2& p) const { return field->camera.unproject(p, depth); }
  double Potential(const Vec2& p) const { return field->Potential(lift(p)); }
  // The part of the field that lies in the plane.
  Vec2 E(const Vec2& p) const {
    const Vec3 e = field->E(lift(p));
    return Vec2(e * field->camera.right(), e * field->camera.up());
  }
  int BallAt(const Vec3& flat) const { return field->BallAt(lift(Vec2(flat.X, flat.Y))); }
  // The circle a ball cuts out of the plane: its centre and radius (0 when the ball misses the plane).
  Vec2 centre(std::size_t index) const { return field->camera.project(field->charges[index].position3()); }
  double cutRadius(std::size_t index) const {
    const double h = field->camera.depth(field->charges[index].position3()) - depth;
    return field->radius > std::abs(h) ? std::sqrt(field->radius * field->radius - h * h) : 0.;
  }
  // The point of ball `index`'s circle on the ray from its centre through `flat`.
  Vec3 OnSurface(int index, const Vec3& flat) const {
    const Vec2 c = centre(static_cast<std::size_t>(index));
    const Vec2 d = Vec2(flat.X, flat.Y) - c;
    const double len = d.Length();
    if (len <= 0) return flat;
    const Vec2 at = c + d * (cutRadius(static_cast<std::size_t>(index)) / len);
    return Vec3(at.X, at.Y, 0);
  }
};

// Finds where the equipotentials of one charge cross the cut plane and posts one job per level, the way
// the first field line of a charge does in 2D: a walk inside the plane from the charge away along the
// in-plane field, posting whenever the potential has moved by one interval. Only levels of the charge's
// own sign are posted, so a level between two opposite charges is found once.
template <typename Hooks>
void probeCutPlane(const CutPlane& cut, const ComputeParams& p, const Seed& seed, Hooks& hooks, bool* cancelled = nullptr) {
  const ChargeSystem& field = *cut.field;
  const Charge& charge = field.charges[seed.chargeIndex];
  const int s = sign(charge.charge);
  if (s == 0) return;
  // away from the charge = downhill for a positive charge, uphill for a negative one
  const auto away = [&cut, s](const Vec2& at) {
    const Vec2 e = cut.E(at);
    const double len = e.Length();
    return len > 0 ? e * (static_cast<double>(s) / len) : Vec2(0, 0);
  };
  const auto blocked = [&cut, &field, &p](const Vec2& at) {
    if (cut.BallAt(Vec3(at.X, at.Y, 0)) >= 0) return true;
    return field.radius <= 0 && field.HitCharge(cut.lift(at), p);
  };

  // Start: next to the charge where it lies in the plane, else at the point of the plane nearest to it.
  const Vec2 c = cut.centre(seed.chargeIndex);
  const double h = std::abs(field.camera.depth(charge.position3()) - cut.depth);
  const double dotPx = charge.sizePx > 0.f ? static_cast<double>(charge.sizePx) : p.chargeRadiusPx;
  double reach = 0.01;
  if (field.radius > 0) {
    if (cut.cutRadius(seed.chargeIndex) > 0) reach = cut.cutRadius(seed.chargeIndex) * 1.001 + 1e-6;
  } else if (h < p.seedRadiusFor(dotPx)) {
    reach = p.seedRadiusFor(dotPx);
  }
  Vec2 point;
  bool found = false;
  for (int k = 0; k < 16 && !found; ++k) {  // another ball may cover part of the circle
    const double angle = 0.35 + 2. * kPi * k / 16.;
    point = c + Vec2(std::cos(angle), std::sin(angle)) * reach;
    found = !blocked(point);
  }
  if (!found) return;

  // Off the plane the peak of the in-plane potential need not lie under the charge: climb to it first.
  if (reach == 0.01) {
    for (int i = 0; i < 20000; ++i) {
      const Vec2 dir = away(point);
      const Vec2 next = point - dir * 0.002;
      if ((dir.X == 0 && dir.Y == 0) || blocked(next)) break;
      if (static_cast<double>(s) * (cut.Potential(next) - cut.Potential(point)) <= 0) break;
      point = next;
    }
  }

  const Vec2 origin = point;
  double level = 0;  // the last level posted (shouldPostEquipotential's oldPotential)
  for (std::size_t i = 0; i < 400000; ++i) {
    if ((i % 256) == 0 && hooks.cancelled()) {
      if (cancelled) *cancelled = true;
      return;
    }
    const Vec2 e = cut.E(point);
    const double strength = e.Length();
    if (strength <= 0) return;
    // a step moves the potential by at most a quarter of an interval
    const double step = std::clamp(0.25 * p.potentialInterval / strength, 1e-5, 0.05);
    const Vec2 mid = point + away(point) * (0.5 * step);
    point += away(mid) * step;
    if (blocked(point) || (point - origin).Length() > 60.) return;
    const double potential = cut.Potential(point);
    if (static_cast<double>(s) * potential <= 0) return;

    while (shouldPostEquipotential(cut, p, seed, point, level)) {
      // put the start exactly on the level (the walk only brackets it)
      Vec2 start = point;
      for (int n = 0; n < 3; ++n) {
        const Vec2 g = cut.E(start);
        const double g2 = g * g;
        if (g2 <= 0) break;
        Vec2 move = g * ((cut.Potential(start) - level) / g2);
        if (move.Length() > 0.05) move = move * (0.05 / move.Length());
        start += move;
      }
      hooks.postEquipotential(blocked(start) ? point : start, level);
    }
  }
}

}  // namespace ef
