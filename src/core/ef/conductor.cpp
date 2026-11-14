#include "ef/conductor.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace ef {

namespace {

constexpr double kSourceDepth = 0.6;  // sources sit on a sphere of this fraction of R
constexpr std::size_t kSourceBudget = 400;  // total sources: keeps the solve interactive
constexpr std::size_t kMinSourcesPerBall = 24, kMaxSourcesPerBall = 160;
constexpr double kCloseGap = 0.5;            // neighbours of another body within this many R get image sources
constexpr std::size_t kImageSources = 24;    // at most this many per ball and neighbour

Vec3 fibonacci(std::size_t i, std::size_t n) {
  const double goldenAngle = kPi * (3. - std::sqrt(5.));
  const double h = 1. - (2. * static_cast<double>(i) + 1.) / static_cast<double>(n);
  const double ring = std::sqrt(1. - h * h);
  const double angle = goldenAngle * static_cast<double>(i);
  return Vec3(ring * std::cos(angle), ring * std::sin(angle), h);
}

// Solves the symmetric positive definite system A x = b in place (Cholesky); false if A is not SPD.
bool choleskySolve(std::vector<double>& a, std::vector<double>& b, std::size_t n) {
  for (std::size_t j = 0; j < n; ++j) {
    double d = a[j * n + j];
    for (std::size_t k = 0; k < j; ++k) d -= a[j * n + k] * a[j * n + k];
    if (!(d > 0)) return false;
    d = std::sqrt(d);
    a[j * n + j] = d;
    for (std::size_t i = j + 1; i < n; ++i) {
      double v = a[i * n + j];
      for (std::size_t k = 0; k < j; ++k) v -= a[i * n + k] * a[j * n + k];
      a[i * n + j] = v / d;
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    double v = b[i];
    for (std::size_t k = 0; k < i; ++k) v -= a[i * n + k] * b[k];
    b[i] = v / a[i * n + i];
  }
  for (std::size_t i = n; i-- > 0;) {
    double v = b[i];
    for (std::size_t k = i + 1; k < n; ++k) v -= a[k * n + i] * b[k];
    b[i] = v / a[i * n + i];
  }
  return true;
}

// The surface charges depend only on the charges, their positions and the radius, so a solution is
// computed once and reused: turning the view, changing the line density or the method, exporting and the
// force table all find it here. The few most recent systems are kept.
struct Solved {
  std::vector<double> key;
  std::shared_ptr<const SurfaceSolution> solution;
};
std::mutex gCacheMutex;
std::vector<Solved> gCache;  // most recent first
constexpr std::size_t kCacheSize = 16;

std::vector<double> keyOf(const ChargeSystem& field) {
  std::vector<double> key{field.radius};
  key.reserve(1 + 4 * field.charges.size());
  for (const Charge& c : field.charges) {
    key.push_back(c.charge);
    key.push_back(c.position.X);
    key.push_back(c.position.Y);
    key.push_back(c.z);
  }
  return key;
}

std::shared_ptr<const SurfaceSolution> solveUncached(const ChargeSystem& field);

}  // namespace

std::shared_ptr<const SurfaceSolution> solveSurface(const ChargeSystem& field) {
  if (!field.conductor() || field.charges.empty()) return nullptr;
  std::vector<double> key = keyOf(field);
  {
    std::lock_guard<std::mutex> lock(gCacheMutex);
    for (std::size_t i = 0; i < gCache.size(); ++i)
      if (gCache[i].key == key) {
        std::rotate(gCache.begin(), gCache.begin() + static_cast<std::ptrdiff_t>(i), gCache.begin() + static_cast<std::ptrdiff_t>(i) + 1);
        return gCache.front().solution;
      }
  }
  std::shared_ptr<const SurfaceSolution> solution = solveUncached(field);  // outside the lock: other systems may solve meanwhile
  std::lock_guard<std::mutex> lock(gCacheMutex);
  gCache.insert(gCache.begin(), Solved{std::move(key), solution});
  if (gCache.size() > kCacheSize) gCache.pop_back();
  return solution;
}

namespace {

std::shared_ptr<const SurfaceSolution> solveUncached(const ChargeSystem& field) {
  const double R = field.radius;
  const std::size_t n = field.charges.size();
  const std::vector<int> body = field.bodies();
  std::size_t bodies = 0;
  for (const int b : body) bodies = std::max(bodies, static_cast<std::size_t>(b) + 1);

  auto solution = std::make_shared<SurfaceSolution>();
  SurfaceSolution& s = *solution;
  s.radius = R;
  s.balls.resize(n);
  s.bodyPotential.assign(bodies, 0.);

  // A ball on the same centre as an earlier one adds its charge to the body but no surface of its own.
  std::vector<bool> duplicate(n, false);
  std::size_t unique = 0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < i; ++j)
      if (field.charges[j].position3() == field.charges[i].position3()) duplicate[i] = true;
    if (!duplicate[i]) ++unique;
  }
  const std::size_t perBall = std::clamp(kSourceBudget / std::max<std::size_t>(unique, 1), kMinSourcesPerBall, kMaxSourcesPerBall);

  std::vector<int> sourceBody;
  struct Gap {
    Vec3 pole, axis;  // the point of the ball nearest its neighbour, and the direction to it
    double width;
    int body;
  };
  std::vector<Gap> gaps;
  for (std::size_t i = 0; i < n; ++i) {
    SurfaceSolution::Ball& ball = s.balls[i];
    ball.centre = field.charges[i].position3();
    ball.body = body[i];
    ball.index = i;
    ball.begin = s.sources.size();
    if (!duplicate[i])
      for (std::size_t k = 0; k < perBall; ++k) {
        s.sources.push_back({ball.centre + fibonacci(k, perBall) * (kSourceDepth * R), 0.});
        sourceBody.push_back(body[i]);
      }
    // Close neighbours of another body: the charge crowds into the gap in a peak far narrower than the
    // sphere of sources can form. The exact field of two spheres is a series of image charges on the line
    // of centres, x' = R^2 / (d - x), so sources are put on those points.
    if (!duplicate[i])
      for (std::size_t j = 0; j < n; ++j) {
        if (j == i || duplicate[j] || body[j] == body[i]) continue;
        const Vec3 axis = field.charges[j].position3() - ball.centre;
        const double d = axis.Length();
        if (d > (2. + kCloseGap) * R) continue;
        const Vec3 unit = axis / d;
        double x = 0;
        for (std::size_t k = 0; k < kImageSources; ++k) {
          const double next = R * R / (d - x);
          if (next - x < 1e-4 * R) break;
          x = next;
          s.sources.push_back({ball.centre + unit * x, 0.});
          sourceBody.push_back(body[i]);
        }
        gaps.push_back({ball.centre + unit * R, unit, d - 2. * R, body[i]});
      }
    ball.end = s.sources.size();
  }
  const std::size_t K = s.sources.size();

  // Collocation points: about twice as many as sources, on the outer surface of the bodies.
  struct Point {
    Vec3 at;
    int body;
  };
  std::vector<Point> points;
  const std::size_t perBallPoints = 2 * perBall + 1;
  for (std::size_t i = 0; i < n; ++i) {
    if (duplicate[i]) continue;
    for (std::size_t k = 0; k < perBallPoints; ++k) {
      const Vec3 at = s.balls[i].centre + fibonacci(k, perBallPoints) * R;
      bool hidden = false;
      for (std::size_t j = 0; j < n && !hidden; ++j) {
        if (j == i || duplicate[j]) continue;
        const Vec3 d = at - s.balls[j].centre;
        hidden = d * d < R * R * (1. - 1e-9);
      }
      if (!hidden) points.push_back({at, body[i]});
    }
  }

  // Extra points around each gap: rings whose spacing grows with the distance from the pole.
  for (const Gap& gap : gaps) {
    Vec3 u = cross(gap.axis, std::abs(gap.axis.X) < 0.9 ? Vec3(1, 0, 0) : Vec3(0, 1, 0));
    u = u / u.Length();
    const Vec3 v = cross(gap.axis, u);
    const Vec3 centre = gap.pole - gap.axis * R;
    const double first = 0.3 * std::sqrt(std::max(gap.width, 1e-3 * R) / R);
    std::size_t ring = 0;
    for (double theta = 0; theta < 1.2; theta = theta == 0 ? first : theta * 1.3, ++ring) {
      const std::size_t around = theta == 0 ? 1 : 8;
      for (std::size_t a = 0; a < around; ++a) {
        const double phi = 2. * kPi * (static_cast<double>(a) + 0.5 * static_cast<double>(ring % 2)) / static_cast<double>(around);
        const Vec3 at = centre + (gap.axis * std::cos(theta) + (u * std::cos(phi) + v * std::sin(phi)) * std::sin(theta)) * R;
        bool hidden = false;  // a third ball may cover part of the gap
        for (std::size_t j = 0; j < n && !hidden; ++j) {
          const Vec3 d = at - s.balls[j].centre;
          hidden = !duplicate[j] && d * d < R * R * (1. - 1e-6);
        }
        if (!hidden) points.push_back({at, gap.body});
      }
    }
  }

  std::vector<double> bodyCharge(bodies, 0.);
  for (std::size_t i = 0; i < n; ++i) bodyCharge[static_cast<std::size_t>(body[i])] += field.charges[i].charge;

  // Unknowns: the K source charges, then the potential of each body in units of 1/R (so all the
  // coefficients are of the size 1/R). Rows: one per collocation point,
  //     sum_k q_k / |x_i - y_k|  -  V_body(i)  =  0,
  // and one heavily weighted row per body fixing its total charge. Solved through the normal equations
  // with a small ridge, which the nearly dependent sources of this method need.
  const std::size_t N = K + bodies;
  std::vector<double> normal(N * N, 0.), rhs(N, 0.), row(N);
  const auto addRow = [&](double value) {
    for (std::size_t a = 0; a < N; ++a) {
      if (row[a] == 0) continue;
      rhs[a] += row[a] * value;
      for (std::size_t b = 0; b <= a; ++b) normal[a * N + b] += row[a] * row[b];
    }
  };
  for (const Point& p : points) {
    for (std::size_t k = 0; k < K; ++k) row[k] = 1. / (p.at - s.sources[k].at).Length();
    for (std::size_t b = 0; b < bodies; ++b) row[K + b] = static_cast<int>(b) == p.body ? -1. / R : 0.;
    addRow(0.);
  }
  const double weight = 100. * std::sqrt(static_cast<double>(std::max<std::size_t>(points.size(), 1))) / R;
  for (std::size_t b = 0; b < bodies; ++b) {
    for (std::size_t k = 0; k < K; ++k) row[k] = sourceBody[k] == static_cast<int>(b) ? weight : 0.;
    for (std::size_t c = 0; c < bodies; ++c) row[K + c] = 0.;
    addRow(weight * bodyCharge[b]);
  }
  double trace = 0;
  for (std::size_t a = 0; a < N; ++a) trace += normal[a * N + a];
  std::vector<double> x;
  for (double ridge = 1e-13; ridge < 1.; ridge *= 100.) {
    std::vector<double> a = normal;
    x = rhs;
    for (std::size_t d = 0; d < N; ++d) a[d * N + d] += ridge * trace / static_cast<double>(N);
    if (choleskySolve(a, x, N)) break;
    x.assign(N, 0.);
  }

  for (std::size_t k = 0; k < K; ++k) s.sources[k].q = x[k];
  for (std::size_t b = 0; b < bodies; ++b) s.bodyPotential[b] = x[K + b] / R;
  for (SurfaceSolution::Ball& ball : s.balls)
    for (std::size_t k = ball.begin; k < ball.end; ++k) {
      ball.charge += s.sources[k].q;
      ball.dipole += (s.sources[k].at - ball.centre) * s.sources[k].q;
    }

  // How well the surfaces are equipotentials.
  double worst = 0, scale = 0;
  for (const double v : s.bodyPotential) scale = std::max(scale, std::abs(v));
  for (const Point& p : points) {
    double v = 0;
    for (const SurfaceSolution::Source& q : s.sources) v += q.q / (p.at - q.at).Length();
    worst = std::max(worst, std::abs(v - s.bodyPotential[static_cast<std::size_t>(p.body)]));
  }
  s.residual = scale > 0 ? worst / scale : worst;
  return solution;
}

}  // namespace

void prepare(ChargeSystem& field) { field.surface = solveSurface(field); }

}  // namespace ef
