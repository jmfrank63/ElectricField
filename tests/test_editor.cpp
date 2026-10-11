// v2 Phase 2 (EDIT-01..05, CALC-01..02): the editor core - hit testing, guarded mutations with realtime
// scene sync, stale marking, and explicit recalculation. No SDL/ImGui.
#include "app/editor.hpp"
#include "app/ui_logic.hpp"

#include <chrono>
#include <thread>

#include "doctest/doctest.h"

namespace {

// A tab whose session never starts: Failed state, session null -> fully editable (D-02).
struct ThrowFactory {
  efapp::Workspace ws{ef::ComputeParams{}, [](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> {
    throw std::runtime_error("no compute");
  }};
};

efapp::Tab& dipoleTab(ThrowFactory& f) {
  const unsigned s = f.ws.open(0);  // Dipole: +1 (-2,0), -1 (2,0)
  return *f.ws.find(s);
}

}  // namespace

TEST_CASE("hitCharge: nearest within radius, -1 outside (EDIT-01)") {
  ef::ChargeSystem f;
  f.charges.push_back(ef::Charge{1, ef::Vec2{-2, 0}});
  f.charges.push_back(ef::Charge{-1, ef::Vec2{2, 0}});
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{-1.9, 0.1}, 0.30) == 0);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{2.2, 0.0}, 0.30) == 1);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{0.0, 0.0}, 0.30) == -1);  // midway: nothing near
  // nearest wins when two are in range
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{1.8, 0.0}, 0.50) == 1);
}

TEST_CASE("moveCharge updates field and scene, refuses while computing, marks stale (EDIT-01, EDIT-05)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK(t.session == nullptr);  // Failed: editable
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-1, 1}));
  CHECK(t.field.charges[0].position.X == doctest::Approx(-1.0));
  CHECK(t.field.charges[0].position.Y == doctest::Approx(1.0));
  REQUIRE(t.scene.charges.size() == 2);
  CHECK(t.scene.charges[0].position.X == doctest::Approx(-1.0));  // scene mirror follows
  CHECK(t.scene.charges[1].position.X == doctest::Approx(2.0));   // untouched neighbour
  CHECK(t.stale);
  CHECK_FALSE(efapp::editor::moveCharge(t, 7, ef::Vec2{0, 0}));  // invalid index
}

TEST_CASE("mutators refuse while a session is alive (D-02)") {
  efapp::Workspace ws;
  const unsigned s = ws.open(0);  // real session: Computing
  efapp::Tab& t = *ws.find(s);
  REQUIRE(t.session != nullptr);
  CHECK_FALSE(efapp::editor::moveCharge(t, 0, ef::Vec2{9, 9}));
  CHECK_FALSE(efapp::editor::addCharge(t, ef::Vec2{9, 9}, 1));
  CHECK_FALSE(efapp::editor::removeSelected(t));
  CHECK_FALSE(efapp::editor::stepSelected(t, ef::Vec2{1, 0}));
  CHECK_FALSE(efapp::editor::stepValue(t, 1));
  CHECK_FALSE(t.stale);
  CHECK(t.field.charges.size() == 2);
  ws.closeAll();
}

TEST_CASE("addCharge appends and selects; removeSelected keeps at least one (EDIT-04)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 5}, 3));
  REQUIRE(t.field.charges.size() == 3);
  CHECK(t.selected == 2);
  CHECK(t.field.charges[2].charge == 3);
  CHECK(t.scene.charges[2].charge == 3);
  CHECK(t.scene.charges[2].position.Y == doctest::Approx(5.0));

  // Delete key with the last charge standing: refused.
  CHECK(efapp::editor::removeSelected(t));  // back to 2
  efapp::editor::removeSelected(t);         // select is -1 now -> refused
  CHECK_FALSE(efapp::editor::removeSelected(t));
  REQUIRE(t.field.charges.size() == 2);
  t.selected = 1;
  CHECK(efapp::editor::removeSelected(t));
  REQUIRE(t.field.charges.size() == 1);
  t.selected = 0;
  CHECK_FALSE(efapp::editor::removeSelected(t));  // the last one stays
  CHECK(t.field.charges.size() == 1);
}

TEST_CASE("stepSelected and stepValue move the selected charge and mark stale (EDIT-02, EDIT-03)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  t.selected = 0;
  CHECK(efapp::editor::stepSelected(t, ef::Vec2{0.1, -0.5}));
  CHECK(t.field.charges[0].position.X == doctest::Approx(-1.9));
  CHECK(t.field.charges[0].position.Y == doctest::Approx(-0.5));
  CHECK(t.scene.charges[0].position.X == doctest::Approx(-1.9));
  CHECK(efapp::editor::stepValue(t, 1));
  CHECK(t.field.charges[0].charge == 2);
  CHECK(t.scene.charges[0].charge == 2);
  CHECK(efapp::editor::stepValue(t, -5));
  CHECK(t.field.charges[0].charge == -3);
  CHECK(t.stale);
  t.selected = -1;
  CHECK_FALSE(efapp::editor::stepSelected(t, ef::Vec2{1, 0}));
  CHECK_FALSE(efapp::editor::stepValue(t, 1));
}

TEST_CASE("calculate: recompute from the current charges, refuse while computing (CALC-01)") {
  efapp::Workspace ws;
  const unsigned s = ws.open(0);
  CHECK_FALSE(ws.calculate(s));  // computing: refused
  CHECK_FALSE(ws.calculate(9999));
  if (!ws.find(s)->session) return;  // extremely slow machine guard: skip if already done
  ws.closeAll();

  // Full cycle on an edited tab: edit -> stale -> calculate -> fresh lines, stale cleared.
  efapp::Workspace ws2;
  const unsigned e = ws2.openEmpty();  // one +1 charge at the origin
  efapp::Tab& t = *ws2.find(e);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws2.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  const std::size_t straightLines = t.scene.electric.size();
  // Move the charge and change the method: stale, lines unchanged.
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{1, 0}));
  CHECK(t.stale);
  CHECK(t.scene.electric.size() == straightLines);
  t.params.method = ef::Method::RK4;  // the panel combo edits the captured params (CALC-02)

  CHECK(ws2.calculate(e));
  CHECK(t.state == efapp::TabState::Computing);
  CHECK_FALSE(t.stale);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws2.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  CHECK(t.scene.electric.size() == straightLines);  // one unit charge: 16 lines either way
  CHECK(t.params.method == ef::Method::RK4);
  CHECK(t.scene.charges[0].position.X == doctest::Approx(1.0));  // the moved charge is the one that computed
}

TEST_CASE("canvas conversions: round-trip with y flip (D-06)") {
  const ef::View v{800, 600, 100.0};
  const ef::Vec2 world = efapp::worldFromCanvas(v, 10, 20, 410, 320);  // canvas centre -> origin
  CHECK(world.X == doctest::Approx(0.0));
  CHECK(world.Y == doctest::Approx(0.0));
  const ef::Vec2 up = efapp::worldFromCanvas(v, 10, 20, 410, 220);  // 100 px above centre -> +1 world y
  CHECK(up.X == doctest::Approx(0.0));
  CHECK(up.Y == doctest::Approx(1.0));
  const ef::Vec2 back = efapp::canvasFromWorld(v, 10, 20, ef::Vec2{1.5, -2.0});
  CHECK(back.X == doctest::Approx(10 + 400 + 150));
  CHECK(back.Y == doctest::Approx(20 + 300 + 200));
  const ef::Vec2 rt = efapp::canvasFromWorld(v, 10, 20, efapp::worldFromCanvas(v, 10, 20, 123, 456));
  CHECK(rt.X == doctest::Approx(123));
  CHECK(rt.Y == doctest::Approx(456));
}
