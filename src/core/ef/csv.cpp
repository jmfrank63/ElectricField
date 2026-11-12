#include "ef/csv.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ef/conductor.hpp"
#include "ef/forces.hpp"
#include "ef/seeds.hpp"

namespace ef {

namespace {

void num(std::string& out, double v) {
  if (!std::isfinite(v)) return;  // an undefined value (on a point charge) is an empty cell
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%.10g", v);
  out += buf;
}

void cell(std::string& out, double v) {
  out += ',';
  num(out, v);
}

std::string quoted(const std::string& text) {
  std::string out = "\"";
  for (const char ch : text) {
    if (ch == '"') out += '"';
    if (ch != '\n' && ch != '\r') out += ch;
  }
  return out + '"';
}

constexpr double kDeg = 180. / kPi;

void pointRow(std::string& out, const ChargeSystem& field, const char* kind, std::size_t line, std::size_t index, const Vec3& p,
              const double* label) {
  const Vec2 screen = field.camera.project(p);
  const Vec3 e = field.E(p);
  const double mag = e.Length();
  out += kind;
  out += ',' + std::to_string(line) + ',' + std::to_string(index);
  cell(out, p.X);
  cell(out, p.Y);
  cell(out, p.Z);
  cell(out, screen.X);
  cell(out, screen.Y);
  cell(out, field.Potential(p));
  cell(out, e.X);
  cell(out, e.Y);
  cell(out, e.Z);
  cell(out, mag);
  const bool direction = std::isfinite(mag) && mag > 0;
  cell(out, direction ? std::atan2(e.Y, e.X) * kDeg : std::nan(""));
  cell(out, direction ? std::asin(std::clamp(e.Z / mag, -1., 1.)) * kDeg : std::nan(""));
  out += ',';
  if (label) num(out, *label);
  out += '\n';
}

}  // namespace

std::vector<Vec2> onCurvePoints(const std::vector<Vec2>& points) {
  std::vector<Vec2> out;
  if (points.empty()) return out;
  const std::size_t segments = (points.size() - 1) / 3;
  out.reserve(segments + 3);
  for (std::size_t k = 0; k <= segments; ++k) out.push_back(points[3 * k]);
  for (std::size_t i = 3 * segments + 1; i < points.size(); ++i) out.push_back(points[i]);
  return out;
}

std::string chargesCsv(const ChargeSystem& raw) {
  const ChargeSystem field = prepared(raw);
  std::string out =
      "index,name,charge,x,y,z,radius,body,potential,ex,ey,ez,fx,fy,fz,force,screen_x,screen_y,view_yaw_deg,view_pitch_deg,system_energy\n";
  const std::vector<Vec3> f = forces(field);
  const std::vector<int> body = field.bodies();
  const double energy = potentialEnergy(field);
  for (std::size_t i = 0; i < field.charges.size(); ++i) {
    const Charge& c = field.charges[i];
    const Vec3 at = c.position3();
    double potential = 0;
    Vec3 e;
    if (field.surface) {  // Conductor: the ball is at its body's potential and holds no field
      potential = field.surface->bodyPotential[static_cast<std::size_t>(body[i])];
    } else {
      for (std::size_t j = 0; j < field.charges.size(); ++j) {
        if (j == i) continue;
        potential += field.charges[j].Potential(at, field.radius);
        e += field.charges[j].E(at, field.radius);
      }
    }
    const Vec2 screen = field.camera.project(at);
    out += std::to_string(i) + ',' + quoted(c.name.empty() ? "Charge " + std::to_string(i + 1) : c.name) + ',' + std::to_string(c.charge);
    cell(out, at.X);
    cell(out, at.Y);
    cell(out, at.Z);
    cell(out, field.radius);
    out += ',' + std::to_string(body[i]);
    cell(out, potential);
    cell(out, e.X);
    cell(out, e.Y);
    cell(out, e.Z);
    cell(out, f[i].X);
    cell(out, f[i].Y);
    cell(out, f[i].Z);
    cell(out, f[i].Length());
    cell(out, screen.X);
    cell(out, screen.Y);
    cell(out, field.camera.yaw * kDeg);
    cell(out, field.camera.pitch * kDeg);
    cell(out, energy);
    out += '\n';
  }
  return out;
}

std::string bodiesCsv(const ChargeSystem& raw) {
  const ChargeSystem field = prepared(raw);
  std::string out = "body,balls,charge,x,y,z,px,py,pz,dipole,flux_out,flux_in,fx,fy,fz,force,model,potential,surface_error\n";
  const std::vector<Vec3> f = forces(field);
  const std::vector<int> body = field.bodies();
  const std::vector<BodyFlux> flux = bodyFlux(field);
  for (std::size_t b = 0; b < flux.size(); ++b) {
    int balls = 0, charge = 0;
    Vec3 centre;
    for (std::size_t i = 0; i < body.size(); ++i) {
      if (body[i] != static_cast<int>(b)) continue;
      ++balls;
      charge += field.charges[i].charge;
      centre += field.charges[i].position3();
    }
    centre /= static_cast<double>(std::max(balls, 1));
    Vec3 dipole;
    for (std::size_t i = 0; i < body.size(); ++i)
      if (body[i] == static_cast<int>(b)) dipole += (field.charges[i].position3() - centre) * static_cast<double>(field.charges[i].charge);
    out += std::to_string(b) + ',' + std::to_string(balls) + ',' + std::to_string(charge);
    cell(out, centre.X);
    cell(out, centre.Y);
    cell(out, centre.Z);
    cell(out, dipole.X);
    cell(out, dipole.Y);
    cell(out, dipole.Z);
    cell(out, dipole.Length());
    cell(out, flux[b].out);
    cell(out, flux[b].in);
    Vec3 force;
    for (std::size_t i = 0; i < body.size(); ++i)
      if (body[i] == static_cast<int>(b)) force += f[i];
    cell(out, force.X);
    cell(out, force.Y);
    cell(out, force.Z);
    cell(out, force.Length());
    out += field.surface ? ",conductor" : ",uniform";
    cell(out, field.surface ? field.surface->bodyPotential[b] : std::nan(""));
    cell(out, field.surface ? field.surface->residual : std::nan(""));
    out += '\n';
  }
  return out;
}

std::string linesCsv(const ChargeSystem& raw, const FieldScene& scene) {
  const ChargeSystem field = prepared(raw);
  std::string out = "kind,line,point,x,y,z,screen_x,screen_y,potential,ex,ey,ez,magnitude,azimuth_deg,elevation_deg,label\n";
  const auto rows = [&](const std::vector<ScenePath>& paths, const char* kind, bool labelled) {
    for (std::size_t l = 0; l < paths.size(); ++l) {
      const ScenePath& path = paths[l];
      const double* label = labelled ? &path.label : nullptr;
      if (!path.space.empty()) {  // a line traced in space: its recorded 3D points
        for (std::size_t i = 0; i < path.space.size(); ++i) pointRow(out, field, kind, l, i, path.space[i], label);
      } else {  // a planar line: the points on the fitted curve
        const std::vector<Vec2> curve = onCurvePoints(path.points);
        for (std::size_t i = 0; i < curve.size(); ++i) pointRow(out, field, kind, l, i, Vec3(curve[i].X, curve[i].Y, 0), label);
      }
    }
  };
  rows(scene.electric, "field", false);
  rows(scene.potential, "equipotential", true);
  return out;
}

}  // namespace ef
