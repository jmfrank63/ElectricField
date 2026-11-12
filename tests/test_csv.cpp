// Numeric CSV export: the table of the charges and the points of the lines.
#include "doctest/doctest.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "ef/csv.hpp"
#include "ef/presets.hpp"
#include "ef/session.hpp"
#include "test_params.hpp"

using namespace ef;

namespace {
std::vector<std::vector<std::string>> rows(const std::string& csv) {
  std::vector<std::vector<std::string>> out;
  std::istringstream is(csv);
  std::string line;
  while (std::getline(is, line)) {
    std::vector<std::string> cells;
    std::string cell;
    std::istringstream ls(line);
    while (std::getline(ls, cell, ',')) cells.push_back(cell);
    if (!line.empty() && line.back() == ',') cells.emplace_back();
    out.push_back(cells);
  }
  return out;
}

FieldScene compute(const ChargeSystem& field, const ComputeParams& p) {
  ComputeSession session(field, p);
  REQUIRE_FALSE(session.wait().failed);
  FieldResult r;
  REQUIRE(session.takeResult(r));
  return buildScene(field, r);
}
}  // namespace

TEST_CASE("csv: the charge table carries position, the field of the others, the force and the energy") {
  const ChargeSystem field = makeSystem(presets()[0]);  // +1 at (-2, 0), -1 at (2, 0)
  const auto t = rows(chargesCsv(field));
  REQUIRE(t.size() == 3);
  REQUIRE(t[0].size() == 21);
  CHECK(t[0][0] == "index");
  CHECK(t[0][20] == "system_energy");
  REQUIRE(t[1].size() == 21);
  CHECK(t[1][1] == "\"Charge 1\"");
  CHECK(t[1][2] == "1");
  CHECK(std::stod(t[1][3]) == -2.0);
  CHECK(std::stod(t[1][8]) == doctest::Approx(-0.25));      // potential of the -1 at distance 4
  CHECK(std::stod(t[1][9]) == doctest::Approx(1.0 / 16));   // its field points at it
  CHECK(std::stod(t[1][12]) == doctest::Approx(1.0 / 16));  // and so does the force on the +1
  CHECK(std::stod(t[2][12]) == doctest::Approx(-1.0 / 16));
  CHECK(std::stod(t[1][20]) == doctest::Approx(-0.25));
  CHECK(t[1][7] == "0");
  CHECK(t[2][7] == "1");  // point charges: each its own body
}

TEST_CASE("csv: planar lines list their on-curve points with potential, field and direction") {
  const ChargeSystem field = makeSystem(presets()[0]);
  const FieldScene scene = compute(field, lightParams());
  const auto t = rows(linesCsv(field, scene));
  REQUIRE(t.size() > 10);
  REQUIRE(t[0].size() == 16);
  std::size_t fieldRows = 0, equiRows = 0;
  for (std::size_t i = 1; i < t.size(); ++i) {
    REQUIRE(t[i].size() == 16);
    const Vec2 p(std::stod(t[i][3]), std::stod(t[i][4]));
    CHECK(std::stod(t[i][5]) == 0.0);
    CHECK(std::stod(t[i][8]) == doctest::Approx(field.Potential(p)));
    CHECK(std::stod(t[i][12]) == doctest::Approx(field.E(p).Length()));
    CHECK(std::stod(t[i][14]) == 0.0);  // no elevation in the plane
    if (t[i][0] == "field") {
      ++fieldRows;
      CHECK(t[i][15].empty());
    } else {
      CHECK(t[i][0] == "equipotential");
      ++equiRows;
      // the label is the nominal level (a multiple of the interval); the line runs within one interval of it
      CHECK(std::abs(std::stod(t[i][8]) - std::stod(t[i][15])) <= 0.5);
    }
  }
  CHECK(fieldRows > 0);
  CHECK(equiRows > 0);
  std::size_t expected = 0;
  for (const auto& l : scene.electric) expected += onCurvePoints(l.points).size();
  CHECK(fieldRows == expected);
}

TEST_CASE("csv: lines traced in space list their 3D points") {
  ChargeSystem field;
  Charge a{1, Vec2(0, 0)}, b{-1, Vec2(0, 0)};
  a.z = -2;
  b.z = 2;
  field.charges = {a, b};
  field.camera.yaw = 0.5;
  const FieldScene scene = compute(field, lightParams());
  REQUIRE(scene.electric.size() == 4);
  for (const auto& l : scene.electric) CHECK(l.space.size() >= 2);
  const auto t = rows(linesCsv(field, scene));
  bool offPlane = false;
  for (std::size_t i = 1; i < t.size(); ++i) {
    REQUIRE(t[i].size() == 16);
    const Vec3 p(std::stod(t[i][3]), std::stod(t[i][4]), std::stod(t[i][5]));
    if (p.Z != 0) offPlane = true;
    const Vec2 screen = field.camera.project(p);
    CHECK(std::stod(t[i][6]) == doctest::Approx(screen.X).epsilon(1e-8));
    CHECK(std::stod(t[i][8]) == doctest::Approx(field.Potential(p)).epsilon(1e-8));
  }
  CHECK(offPlane);
  const auto c = rows(chargesCsv(field));
  CHECK(std::stod(c[1][18]) == doctest::Approx(0.5 * 180 / kPi));  // the view angle rides along
}
