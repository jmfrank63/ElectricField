// Shapes: rigid groups of balls for the equipotential charge model, the built-in library and custom ones.
#include "doctest/doctest.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include "app/configs.hpp"
#include "app/editor.hpp"
#include "app/session_file.hpp"
#include "app/shapes_file.hpp"
#include "app/workspace.hpp"
#include "ef/conductor.hpp"
#include "ef/seeds.hpp"
#include "ef/shapes.hpp"
#include "test_params.hpp"

namespace {
struct ThrowFactory {
  efapp::Workspace ws{ef::ComputeParams{}, [](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> {
    throw std::runtime_error("no compute");
  }};
};

// A dipole tab of balls with the equipotential model: +1 (-2,0), -1 (2,0), radius 1.
efapp::Tab& conductorTab(ThrowFactory& f) {
  efapp::Tab& t = *f.ws.find(f.ws.open(0));
  REQUIRE(efapp::editor::setChargeRadius(t, 1));
  REQUIRE(efapp::editor::setChargeModel(t, ef::ChargeModel::Conductor));
  return t;
}
}  // namespace

TEST_CASE("shapes: the built-in shapes are valid single bodies") {
  REQUIRE(ef::builtinShapes().size() == 5);
  for (const ef::Shape& s : ef::builtinShapes()) {
    CHECK_MESSAGE(ef::validShape(s), s.name);
    ef::ChargeSystem field;
    field.radius = 2;
    field.charges = ef::placeShape(s, ef::Vec3(1, 2, 3), 2, 5, 7);
    const std::vector<int> body = field.bodies();
    for (const int b : body) CHECK(b == 0);  // every ball in the one body
    int total = 0;
    ef::Vec3 centre;
    for (const ef::Charge& c : field.charges) {
      total += c.charge;
      centre += c.position3();
      CHECK(c.group == 7);
    }
    CHECK(total == 5);
    CHECK((centre / static_cast<double>(field.charges.size()) - ef::Vec3(1, 2, 3)).Length() < 1e-12);
  }
  CHECK_FALSE(ef::validShape(ef::Shape{"Empty", {}}));
  CHECK_FALSE(ef::validShape(ef::Shape{"Apart", {ef::Vec3(0, 0, 0), ef::Vec3(2.5, 0, 0)}}));  // not one body
  CHECK(ef::validShape(ef::Shape{"Touching", {ef::Vec3(0, 0, 0), ef::Vec3(2, 0, 0)}}));
}

TEST_CASE("shapes: inserting needs the equipotential model; the shape is one group with one total charge") {
  ThrowFactory f;
  efapp::Tab& plain = *f.ws.find(f.ws.open(0));
  CHECK_FALSE(efapp::editor::insertShape(plain, ef::builtinShapes()[1], ef::Vec3(0, 3, 0), 2));  // point charges
  REQUIRE(efapp::editor::setChargeRadius(plain, 1));
  CHECK_FALSE(efapp::editor::insertShape(plain, ef::builtinShapes()[1], ef::Vec3(0, 3, 0), 2));  // uniform balls

  efapp::Tab& t = conductorTab(f);
  const std::size_t entries = t.undo.size();
  REQUIRE(efapp::editor::insertShape(t, ef::builtinShapes()[1], ef::Vec3(0, 4, 0), 3));  // the Eight
  REQUIRE(t.field.charges.size() == 4);
  CHECK(t.undo.size() == entries + 1);
  CHECK(t.selected == 2);
  CHECK(t.field.charges[2].group == 1);
  CHECK(t.field.charges[3].group == 1);
  CHECK(t.field.charges[2].charge == 3);
  CHECK(t.field.charges[3].charge == 0);  // allowed inside a group
  CHECK(t.field.charges[2].id != 0);
  CHECK(t.field.charges[2].id != t.field.charges[3].id);
  CHECK(efapp::editor::chargeValue(t.field, 3) == 3);
  CHECK(efapp::editor::groupMembers(t.field, 3) == std::vector<std::size_t>{2, 3});
  CHECK(efapp::editor::groupMembers(t.field, 0) == std::vector<std::size_t>{0});
  CHECK(t.scene.charges[3].charge == 3);  // drawn with the sign of its body
  // a second shape gets its own group
  REQUIRE(efapp::editor::insertShape(t, ef::builtinShapes()[0], ef::Vec3(0, -4, 0), -1));
  CHECK(t.field.charges[4].group == 2);
  // the uncharged ball of the Eight still carries lines: its surface is part of the body
  ef::ComputeParams p = lightParams();
  p.calculateEquipotentials = false;
  bool onSecondBall = false;
  for (const ef::Seed& s : ef::planSeeds3(ef::prepared(t.field), p)) onSecondBall = onSecondBall || s.chargeIndex == 3;
  CHECK(onSecondBall);
}

TEST_CASE("shapes: a shape moves, turns, changes charge and is removed as one unit") {
  ThrowFactory f;
  efapp::Tab& t = conductorTab(f);
  REQUIRE(efapp::editor::insertShape(t, ef::builtinShapes()[1], ef::Vec3(0, 4, 0), 3));  // balls at (0, 4.75) and (0, 3.25)
  const double gap = (t.field.charges[2].position3() - t.field.charges[3].position3()).Length();
  CHECK(gap == doctest::Approx(1.5));

  REQUIRE(efapp::editor::moveCharge(t, 3, ef::Vec2{1, 0.25}));  // dragging the second ball
  CHECK(t.field.charges[3].position.X == 1.0);
  CHECK(t.field.charges[2].position.X == 1.0);  // the first followed
  CHECK(t.field.charges[2].position.Y == doctest::Approx(1.75));
  REQUIRE(efapp::editor::setChargeZ(t, 2, 2.0));
  CHECK(t.field.charges[3].z == 2.0);
  CHECK(t.field.charges[0].z == 0.0);  // other charges stay

  REQUIRE(efapp::editor::rotateGroup(t, 2, 2, 90));  // about z: the vertical 8 lies down
  CHECK((t.field.charges[2].position3() - t.field.charges[3].position3()).Length() == doctest::Approx(gap));
  CHECK(t.field.charges[2].position.Y == doctest::Approx(t.field.charges[3].position.Y));
  CHECK(std::abs(t.field.charges[2].position.X - t.field.charges[3].position.X) == doctest::Approx(1.5));
  CHECK(efapp::editor::undo(t));
  CHECK(t.field.charges[2].position.X == doctest::Approx(t.field.charges[3].position.X));
  CHECK_FALSE(efapp::editor::rotateGroup(t, 0, 2, 90));  // not part of a shape

  REQUIRE(efapp::editor::setValue(t, 3, 0));  // a neutral body is allowed
  CHECK(efapp::editor::chargeValue(t.field, 2) == 0);
  CHECK(t.field.charges[2].charge == 0);
  CHECK_FALSE(efapp::editor::setValue(t, 0, 0));  // a lone charge still cannot be 0
  REQUIRE(efapp::editor::setValue(t, 3, -4));
  CHECK(t.field.charges[2].charge == -4);  // kept on the first ball
  CHECK(t.field.charges[3].charge == 0);
  t.selected = 3;
  REQUIRE(efapp::editor::flipSign(t));
  CHECK(efapp::editor::chargeValue(t.field, 2) == 4);
  REQUIRE(efapp::editor::stepValue(t, -4));
  CHECK(efapp::editor::chargeValue(t.field, 2) == 0);  // steps through 0

  REQUIRE(efapp::editor::removeSelected(t));
  CHECK(t.field.charges.size() == 2);  // both balls gone
  for (const ef::Charge& c : t.field.charges) CHECK(c.group == 0);
}

TEST_CASE("shapes: a body is saved as a custom shape and the shapes file round-trips") {
  ThrowFactory f;
  efapp::Tab& t = conductorTab(f);
  // three free balls touching in an L: one body
  REQUIRE(efapp::editor::addCharge(t, ef::Vec2{-2, 1.5}, 1));
  REQUIRE(efapp::editor::addCharge(t, ef::Vec2{-0.5, 1.5}, 1));
  const ef::Shape l = efapp::editor::shapeFromBody(t, 0, "My L");
  REQUIRE(l.balls.size() == 3);
  CHECK(ef::validShape(l));
  ef::Vec3 sum;
  for (const ef::Vec3& b : l.balls) sum += b;
  CHECK(sum.Length() < 1e-12);  // about its own centre
  CHECK(efapp::editor::shapeFromBody(t, 1, "Lone").balls.size() == 1);

  std::vector<ef::Shape> custom;
  CHECK(efapp::upsertShape(custom, l) == efapp::SaveShapeResult::OK);
  CHECK(efapp::upsertShape(custom, ef::Shape{"  ", l.balls}) == efapp::SaveShapeResult::EMPTY_NAME);
  CHECK(efapp::upsertShape(custom, ef::Shape{"eight", l.balls}) == efapp::SaveShapeResult::RESERVED_NAME);
  CHECK(efapp::upsertShape(custom, ef::Shape{"Bad", {}}) == efapp::SaveShapeResult::INVALID_SHAPE);
  CHECK(efapp::upsertShape(custom, ef::Shape{"my l", {ef::Vec3(0, 0, 0)}}) == efapp::SaveShapeResult::OK);  // replaces
  REQUIRE(custom.size() == 1);
  CHECK(custom[0].balls.size() == 1);
  CHECK(efapp::upsertShape(custom, l) == efapp::SaveShapeResult::OK);

  CHECK(efapp::findShape(custom, "EIGHT") == 1);
  CHECK(efapp::findShape(custom, "my l") == 5);
  CHECK(efapp::findShape(custom, "nope") == -1);
  CHECK(efapp::shapeAt(custom, 5).name == "My L");

  const std::vector<ef::Shape> back = efapp::parseShapes(efapp::serializeShapes(custom));
  REQUIRE(back.size() == 1);
  CHECK(back[0].name == "My L");
  REQUIRE(back[0].balls.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) CHECK((back[0].balls[i] - l.balls[i]).Length() < 1e-9);
  // bad sections are skipped: a built-in name, balls that do not touch, a malformed line
  const std::vector<ef::Shape> bad = efapp::parseShapes(
      "[shape \"Eight\"]\nball = 0, 0, 0\n[shape \"Apart\"]\nball = 0, 0, 0\nball = 5, 0, 0\n[shape \"Broken\"]\nball = 0, x, 0\n[shape \"Fine\"]\nball = 0, 0, 0\n");
  REQUIRE(bad.size() == 1);
  CHECK(bad[0].name == "Fine");
}

TEST_CASE("shapes: groups and uncharged group balls survive collections and sessions") {
  efapp::UserConfig c{"With shape", {{2, -2, 0}, {3, 0, 4.75}, {0, 0, 3.25}}};
  c.radius = 1;
  c.model = ef::ChargeModel::Conductor;
  c.charges[1].group = 1;
  c.charges[2].group = 1;
  const std::string text = efapp::serializeConfigs({c, efapp::UserConfig{"Plain", {{1, 0, 0}}}});
  const efapp::ParsedConfigs configs = efapp::parseConfigs(text);
  REQUIRE(configs.value.size() == 2);
  REQUIRE(configs.value[0].charges.size() == 3);
  CHECK(configs.value[0].charges[0].group == 0);
  CHECK(configs.value[0].charges[1].group == 1);
  CHECK(configs.value[0].charges[1].charge == 3);
  CHECK(configs.value[0].charges[2].group == 1);
  CHECK(configs.value[0].charges[2].charge == 0);  // stays 0 inside its group
  CHECK(efapp::serializeConfigs({configs.value[1]}).find("group") == std::string::npos);
  // a 0 outside a group still loads as +1
  CHECK(efapp::parseConfigs("[configuration \"Z\"]\ncharge = 0, 1, 1\n").value[0].charges[0].charge == 1);

  efapp::SavedTab t;
  t.title = "With shape 1";
  t.charges = c.charges;
  const efapp::ParsedSession s = efapp::parseSession(efapp::serializeSession({t}));
  REQUIRE(s.value.size() == 1);
  CHECK(s.value[0].charges[2].group == 1);
  CHECK(s.value[0].charges[2].charge == 0);
  CHECK(s.value[0].charges[0].group == 0);
}
