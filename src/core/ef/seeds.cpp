#include "ef/seeds.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "ef/math.hpp"

namespace ef {

namespace {
// Seeds of a ball sit this factor outside its radius, so the start point is not "inside" by rounding.
constexpr double kSurfaceSeed = 1. + 1e-9;
// Surface samples per line of an isolated ball. In space the samples follow a Fibonacci spiral and the
// seeds are every 433rd of them on an isolated ball: 433 golden angles are 0.39 of a turn, close to the
// golden angle itself, so the seeds are again evenly spread.
constexpr unsigned kSamplesPerLine2 = 64, kSamplesPerLine3 = 433;
constexpr unsigned kMinSamples = 720, kMaxSamples = 400000;

unsigned sampleCount(const Charge& c, const ComputeParams& p, bool space) {
  const double lines = std::max(1., std::abs(static_cast<double>(c.charge))) * p.linesPerUnitCharge;
  return static_cast<unsigned>(std::clamp(lines * (space ? kSamplesPerLine3 : kSamplesPerLine2), static_cast<double>(kMinSamples),
                                          static_cast<double>(kMaxSamples)));
}

// Direction of surface sample i of M: around the circle in the drawing plane, or over the sphere.
Vec3 sampleDirection(unsigned i, unsigned M, bool space) {
  if (!space) {
    const double angle = -kPi + 2. * kPi * (i + 0.5) / M;
    return Vec3(std::cos(angle), std::sin(angle), 0);
  }
  const double goldenAngle = kPi * (3. - std::sqrt(5.));
  const double h = 1. - (2. * i + 1.) / M;
  const double ring = std::sqrt(1. - h * h);
  return Vec3(ring * std::cos(goldenAngle * i), ring * std::sin(goldenAngle * i), h);
}

// A point of ball `ball`'s surface is not part of the body's outer surface when it lies inside another
// ball, or when an earlier ball has the same centre (their surfaces coincide: count it once).
bool covered(const ChargeSystem& field, std::size_t ball, const Vec3& at) {
  const Vec3 own = field.charges[ball].position3();
  for (std::size_t j = 0; j < field.charges.size(); ++j) {
    if (j == ball) continue;
    const Vec3 c = field.charges[j].position3();
    if ((at - c) * (at - c) < field.radius * field.radius) return true;
    if (j < ball && c == own) return true;
  }
  return false;
}

// Balls (radius > 0): the lines of a body are placed on its outer surface in proportion to the field
// flowing through it. Each sample of a ball's surface carries the weight
//     w = linesPerUnitCharge * R^2 * (E . n) / M
// so an isolated ball of charge q has total weight |q| * linesPerUnitCharge, the same count as a point
// charge, and a seed is placed every time the running sum over a body passes the next whole line.
// With a total charge >= 0 every line begins where the field leaves a surface: those places are seeded
// and traced along the field. With a negative total every line ends where the field enters a surface:
// those are seeded and traced against the field. Either way each line is drawn once.
std::vector<Seed> planFluxSeeds(const ChargeSystem& field, const ComputeParams& p, bool space) {
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);
  const int mode = sign(total);  // +1: seed outflow, trace forward; -1: seed inflow, trace backward
  const std::vector<int> body = field.bodies();
  int bodyCount = 0;
  for (const int b : body) bodyCount = std::max(bodyCount, b + 1);
  const double R = field.radius;

  const auto emit = [&](std::size_t ball, const Vec3& at, int charge, bool first) {
    Seed s;
    s.chargeIndex = ball;
    s.charge = charge;
    s.totalCharge = total;
    s.hasDifferentSigns = hasDifferentSigns;
    s.angle = std::atan2(at.Y - field.charges[ball].position.Y, at.X - field.charges[ball].position.X);
    s.first = first;
    s.isSource = sign(total) == sign(charge);
    s.point3 = at;
    s.point = space ? field.camera.project(at) : Vec2(at.X, at.Y);
    out.push_back(s);
  };

  for (int b = 0; b < bodyCount; ++b) {
    double sum = 0, next = 0.5;  // the running weight over the whole body, and the next line's mark
    for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
      const Charge& charge = field.charges[ci];
      if (body[ci] != b || charge.charge == 0) continue;
      const unsigned M = sampleCount(charge, p, space);
      const bool sourceBall = sign(charge.charge) == mode;
      bool firstGiven = false;
      double strongestOpposite = 0;
      Vec3 oppositeAt;
      for (unsigned i = 0; i < M; ++i) {
        const Vec3 n = sampleDirection(i, M, space);
        const Vec3 at = charge.position3() + n * (R * kSurfaceSeed);
        if (covered(field, ci, at)) continue;
        const double w = static_cast<double>(mode) * static_cast<double>(p.linesPerUnitCharge) * R * R * (field.E(at) * n) / M;
        if (w > 0) {
          sum += w;
          if (sum >= next) {
            emit(ci, at, mode, sourceBall && !firstGiven);
            firstGiven = true;
            next = std::floor(sum - 0.5) + 1.5;
          }
        } else if (-w > strongestOpposite) {
          strongestOpposite = -w;
          oppositeAt = at;
        }
      }
      // A ball of the other sign records no line of its own, but in the plane its equipotentials are
      // posted from one unrecorded line that leaves it where the field crosses its surface most strongly.
      if (!space && !sourceBall && p.calculateEquipotentials && strongestOpposite > 0) emit(ci, oppositeAt, -mode, true);
    }
  }
  return out;
}
}  // namespace

std::vector<BodyFlux> bodyFlux(const ChargeSystem& field) {
  const std::vector<int> body = field.bodies();
  int bodyCount = 0;
  for (const int b : body) bodyCount = std::max(bodyCount, b + 1);
  std::vector<BodyFlux> flux(static_cast<std::size_t>(bodyCount));
  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    BodyFlux& f = flux[static_cast<std::size_t>(body[ci])];
    const Charge& charge = field.charges[ci];
    if (field.radius <= 0) {  // a point charge: everything leaves or everything arrives
      (charge.charge > 0 ? f.out : f.in) += std::abs(static_cast<double>(charge.charge));
      continue;
    }
    const unsigned M = 20000;
    for (unsigned i = 0; i < M; ++i) {
      const Vec3 n = sampleDirection(i, M, true);
      const Vec3 at = charge.position3() + n * (field.radius * kSurfaceSeed);
      if (covered(field, ci, at)) continue;
      const double w = field.radius * field.radius * (field.E(at) * n) / M;
      (w > 0 ? f.out : f.in) += std::abs(w);
    }
  }
  return flux;
}

std::vector<Seed> planSeeds(const ChargeSystem& field, const ComputeParams& p) {
  if (field.radius > 0) return planFluxSeeds(field, p, false);
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);

  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    const Charge& charge = field.charges[ci];
    if (charge.charge == 0) continue;

    // 261020-ef1: the seed circle is the charge's OWN effective drawn dot (its sizePx, else the
    // Options default) - the lines are born on the dot the user drew, at every zoom. A charge
    // without its own size reproduces the former p.seedRadius() value exactly (parity).
    const double dotPx = charge.sizePx > 0.f ? static_cast<double>(charge.sizePx) : p.chargeRadiusPx;
    const double r = p.seedRadiusFor(dotPx);

    const double angle_step = 2. * kPi / (std::abs(static_cast<double>(charge.charge)) * p.linesPerUnitCharge);

    double angle_start = -angle_step / 2. - kPi;
    const bool source = sign(total) == sign(charge.charge);
    if (!source) angle_start += kPi + angle_step;

    bool emitted = false;
    for (double angle = angle_start; angle < 2. * kPi + angle_start - angle_step / 4.; angle += angle_step) {
      const Vec2 at(charge.position.X + r * std::cos(angle), charge.position.Y + r * std::sin(angle));
      const bool first = !emitted;  // the first seed of the charge (the one at angle_start for point charges)
      if ((!first || !p.calculateEquipotentials) && !source) break;
      emitted = true;

      Seed s;
      s.chargeIndex = ci;
      s.charge = charge.charge;
      s.totalCharge = total;
      s.hasDifferentSigns = hasDifferentSigns;
      s.angle = angle;
      s.first = first;
      s.isSource = source;
      s.point = at;
      out.push_back(s);
    }
  }
  return out;
}

std::vector<Seed> planSeeds3(const ChargeSystem& field, const ComputeParams& p) {
  if (field.radius > 0) return planFluxSeeds(field, p, true);
  std::vector<Seed> out;
  bool hasDifferentSigns = false;
  const int total = field.totalCharge(hasDifferentSigns);
  const double goldenAngle = kPi * (3. - std::sqrt(5.));

  for (std::size_t ci = 0; ci < field.charges.size(); ++ci) {
    const Charge& charge = field.charges[ci];
    if (charge.charge == 0 || sign(total) != sign(charge.charge)) continue;

    const double dotPx = charge.sizePx > 0.f ? static_cast<double>(charge.sizePx) : p.chargeRadiusPx;
    const double r = p.seedRadiusFor(dotPx);
    const unsigned n = static_cast<unsigned>(std::abs(charge.charge)) * p.linesPerUnitCharge;

    for (unsigned i = 0; i < n; ++i) {
      const double h = 1. - (2. * i + 1.) / n;  // height on the unit sphere, evenly spaced
      const double ring = std::sqrt(1. - h * h);
      const double angle = goldenAngle * i;
      const Vec3 at = charge.position3() + Vec3(ring * std::cos(angle), ring * std::sin(angle), h) * r;
      Seed s;
      s.chargeIndex = ci;
      s.charge = charge.charge;
      s.totalCharge = total;
      s.hasDifferentSigns = hasDifferentSigns;
      s.angle = angle;
      s.first = (i == 0);
      s.isSource = true;
      s.point3 = at;
      s.point = field.camera.project(s.point3);
      out.push_back(s);
    }
  }
  return out;
}

int expectedLineCount(const ChargeSystem& field, const ComputeParams& p) {
  // Balls: the count follows the flux through the bodies' surfaces, so it is the seed plan itself.
  if (field.radius > 0) return static_cast<int>((field.is3D() ? planSeeds3(field, p) : planSeeds(field, p)).size());
  bool diff = false;
  const int total = field.totalCharge(diff);
  int result = 0;
  const bool space = field.is3D();
  for (const auto& c : field.charges) {
    if (c.charge == 0) continue;
    if (sign(total) == sign(c.charge))
      result += std::abs(c.charge) * static_cast<int>(p.linesPerUnitCharge);
    else if (!space)
      ++result;
  }
  return result;
}

}  // namespace ef
