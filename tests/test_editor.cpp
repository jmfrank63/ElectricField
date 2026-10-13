// v2 Phase 2 (EDIT-01..05, CALC-01..02): the editor core - hit testing, guarded mutations with realtime
// scene sync, stale marking, and explicit recalculation. No SDL/ImGui.
#include "app/editor.hpp"
#include "app/ui_logic.hpp"

#include <chrono>
#include <thread>

#include "app/configs.hpp"  // WR-03: the save->load identity pin round-trips through serialize/parse

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
  // v2.6 phase 3: (pixelsPerUnit, defaultRadiusPx) logical pairs (CR-01: the hit convention is the
  // phase-2 logical space, so bounds are k-independent). 25 px at 100 ppu + 0.05 slack = the old
  // fixed 0.30 bound; 45 px at 100 ppu = the old 0.50 probe.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{-1.9, 0.1}, 100.0, 25.0) == 0);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{2.2, 0.0}, 100.0, 25.0) == 1);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{0.0, 0.0}, 100.0, 25.0) == -1);  // midway: nothing near
  // nearest wins when two are in range
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{1.8, 0.0}, 100.0, 45.0) == 1);
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

TEST_CASE("flipSign and the view controls (v2.2 EDIT-09, ZOOM/PAN)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  t.selected = 0;
  CHECK(t.field.charges[0].charge == 1);
  CHECK(efapp::editor::flipSign(t));
  CHECK(t.field.charges[0].charge == -1);
  CHECK(t.scene.charges[0].charge == -1);  // scene follows
  CHECK(efapp::editor::flipSign(t));
  CHECK(t.field.charges[0].charge == 1);
  t.selected = -1;
  CHECK_FALSE(efapp::editor::flipSign(t));

  // Zoom keeps the anchor world point at its screen position (centre = anchor + (centre - anchor) / factor).
  efapp::Tab& v = dipoleTab(f);
  CHECK(v.centre.X == doctest::Approx(0.0));
  CHECK(efapp::editor::zoomAt(v, 2.0, ef::Vec2{0, 0}));
  CHECK(v.zoom == doctest::Approx(2.0));
  CHECK(v.centre.X == doctest::Approx(0.0));  // anchor == centre: centre unmoved
  CHECK(efapp::editor::zoomAt(v, 2.0, ef::Vec2{-2.0, 0.0}));
  CHECK(v.zoom == doctest::Approx(4.0));
  CHECK(v.centre.X == doctest::Approx(-1.0));  // 0 + (0 - (-2)) / 2
  CHECK_FALSE(efapp::editor::zoomAt(v, 0.0, ef::Vec2{0, 0}));
  CHECK(efapp::editor::zoomAt(v, 1.0 / 4.0, ef::Vec2{0, 0}));
  CHECK(v.zoom == doctest::Approx(1.0));

  // Pan moves the centre opposite to the screen delta; reset restores the default view.
  CHECK(efapp::editor::resetView(v));
  CHECK(efapp::editor::pan(v, ef::Vec2{50, -25}, 100.0));
  CHECK(v.centre.X == doctest::Approx(-0.5));
  CHECK(v.centre.Y == doctest::Approx(-0.25));
  CHECK(efapp::editor::resetView(v));
  CHECK(v.zoom == doctest::Approx(1.0));
  CHECK(v.centre.X == doctest::Approx(0.0));

  // View changes never mark the tab stale or dirty (they do not touch the computation).
  CHECK_FALSE(v.stale);
  CHECK_FALSE(v.dirty);
}

// v2.6 phase 2 (WR-02): zoom-in is capped by the adapted-calculation limit derived from the APPLIED
// dot: at the ceiling the floored seed circle (0.01 world) exactly fills the drawn dot
// (zoomCeiling = dotRadiusPx / kMinSeedWorld / ppuBase). The h25 canvas-min-side rule is long superseded.
TEST_CASE("zoom ceiling: the applied dot covers the floored seed circle (v2.6 phase 2 WR-02)") {
  // zoomCeiling = dotRadiusPx / kMinSeedWorld / ppuBase, clamped to [kZoomMin, kZoomMax].
  CHECK(efapp::editor::zoomCeiling(100.0, 25.0) == doctest::Approx(25.0));   // default dot at the logical base 100
  CHECK(efapp::editor::zoomCeiling(100.0, 2.0) == doctest::Approx(2.0));     // "from a dot": the annulus stays covered
  CHECK(efapp::editor::zoomCeiling(200.0, 25.0) == doctest::Approx(12.5));   // the formula is unit-agnostic: a doubled base halves it
  CHECK(efapp::editor::zoomCeiling(100.0, 120.0) == doctest::Approx(efapp::editor::kZoomMax));  // a huge dot: the static cap binds
  CHECK(efapp::editor::zoomCeiling(100000.0, 25.0) == doctest::Approx(efapp::editor::kZoomMin));  // extreme base: floor wins
  CHECK(efapp::editor::zoomCeiling(0.0, 25.0) == doctest::Approx(efapp::editor::kZoomMax));       // degenerate: static cap
  CHECK(efapp::editor::zoomCeiling(-5.0, 25.0) == doctest::Approx(efapp::editor::kZoomMax));
  CHECK(efapp::editor::zoomCeiling(100.0, 0.0) == doctest::Approx(efapp::editor::kZoomMax));
  CHECK(efapp::editor::zoomCeiling(100.0, -1.0) == doctest::Approx(efapp::editor::kZoomMax));

  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  // The ceiling caps zoom-in; the anchor rule still applies at the clamped value (centre unmoved when
  // the anchor is the centre, as the panel buttons do).
  CHECK(efapp::editor::zoomAt(t, 100.0, ef::Vec2{0, 0}, 25.0));
  CHECK(t.zoom == doctest::Approx(25.0));
  CHECK(t.centre.X == doctest::Approx(0.0));
  // A caller-supplied ceiling above the static cap is itself capped...
  CHECK(efapp::editor::zoomAt(t, 10.0, ef::Vec2{0, 0}, 500.0));
  CHECK(t.zoom == doctest::Approx(efapp::editor::kZoomMax));
  // ...and zooming out is never blocked by the ceiling.
  efapp::editor::resetView(t);
  CHECK(efapp::editor::zoomAt(t, 0.01, ef::Vec2{0, 0}, 25.0));
  CHECK(t.zoom == doctest::Approx(efapp::editor::kZoomMin));
  // The ceiling never marks the tab stale or dirty (view-only).
  CHECK_FALSE(t.stale);
  CHECK_FALSE(t.dirty);
}

// v2.6 phase 2: zoom-settle recalculation. The GUI arming lives in drawCanvas, so the doctest drives the
// WORKSPACE contract: pendingViewRecalcAt -2 -> deadline -> calculate with params.viewScale set, and no
// stale/dirty/undo anywhere on that path (a view change is not an edit).
TEST_CASE("viewRecalcTick fires the zoom-settle recalculation with the adapted view scale (v2.6 phase 2)") {
  std::vector<ef::ComputeParams> received;
  efapp::Workspace ws{ef::ComputeParams{}, [&](const ef::ChargeSystem&, const ef::ComputeParams& p) -> std::unique_ptr<efapp::ISession> {
    received.push_back(p);
    throw std::runtime_error("no compute");  // instant Failed tab: session null, recalc paths reachable
  }};
  const unsigned s = ws.openEmpty();
  efapp::Tab& t = *ws.find(s);
  REQUIRE(received.size() == 1);
  CHECK(received[0].viewScale == doctest::Approx(0.0));  // a fresh tab computes legacy (ef-cli parity)

  // The drawCanvas arming, simulated (the GUI side): a zoom to 800 ppu arms the marker.
  t.armedViewPpu = 800.0;
  t.viewPpu = 800.0;
  t.pendingViewRecalcAt = -2;
  ws.viewRecalcTick(100.0);  // -2 becomes the deadline, nothing fires yet
  CHECK(t.pendingViewRecalcAt == doctest::Approx(100.0 + efapp::kAutoRecalcDelay));
  CHECK(received.size() == 1);
  ws.viewRecalcTick(100.0 + efapp::kAutoRecalcDelay - 0.01);  // before the deadline: nothing
  CHECK(received.size() == 1);
  ws.viewRecalcTick(100.0 + efapp::kAutoRecalcDelay);  // due: recalculation with the stamped view scale
  REQUIRE(received.size() == 2);
  CHECK(received[1].viewScale == doctest::Approx(800.0));  // the factory received the adapted params
  CHECK(t.params.viewScale == doctest::Approx(800.0));
  CHECK_FALSE(t.stale);
  CHECK_FALSE(t.dirty);
  CHECK(t.undo.empty());
  CHECK(t.pendingViewRecalcAt < 0);  // disarmed by the fire

  // An explicit Calculate honours the armed view too and disarms the pending settle recalculation.
  t.viewPpu = 900.0;
  t.pendingViewRecalcAt = -2;
  CHECK(ws.calculate(s));
  REQUIRE(received.size() == 3);
  CHECK(received[2].viewScale == doctest::Approx(900.0));
  CHECK(t.pendingViewRecalcAt < 0);

  // Never-armed tab (armedViewPpu 0): calculate keeps the legacy params untouched.
  efapp::Tab& fresh = *ws.find(ws.openEmpty());
  CHECK(fresh.armedViewPpu == 0.0);
  CHECK(ws.calculate(fresh.serial));
  CHECK(fresh.params.viewScale == doctest::Approx(0.0));
}

// v2.6 phase 2: the settle recalculation runs a REAL adapted computation and lands back in Done with
// clean flags - the same full cycle the CALC-03 test drives for the edit debounce.
TEST_CASE("viewRecalcTick with a real session completes the adapted computation (v2.6 phase 2)") {
  ef::ComputeParams light;  // light defaults: the adapted sweep stays cheap in Debug builds
  light.linesPerUnitCharge = 4;
  light.potentialInterval = 0.5;
  efapp::Workspace ws{light};
  const unsigned s = ws.openEmpty();
  efapp::Tab& t = *ws.find(s);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  REQUIRE(t.scene.electric.size() > 0);

  // Zoom to 800 ppu (the drawCanvas arming, simulated) and let the tick fire.
  t.armedViewPpu = 800.0;
  t.viewPpu = 800.0;
  t.pendingViewRecalcAt = -2;
  ws.viewRecalcTick(100.0);
  ws.viewRecalcTick(100.0 + efapp::kAutoRecalcDelay + 0.01);
  CHECK(t.state == efapp::TabState::Computing);
  CHECK_FALSE(t.stale);
  CHECK_FALSE(t.dirty);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  CHECK(t.scene.electric.size() > 0);
  CHECK_FALSE(t.stale);
  CHECK_FALSE(t.dirty);
  CHECK(t.undo.empty());
  CHECK(t.params.viewScale == doctest::Approx(800.0));
  ws.closeAll();
}

// v2.6 phase 2 (WR-05): the drawCanvas arming decision as a pure helper. Previously deleting the arming
// block in the GUI passed every suite (the verifier's behavior_unverified item) - these pins close that
// gap: Done-only, first-frame-safe, inert re-frames, arm on a regime change, disarm on a legacy settle,
// and the WR-01 epsilon band.
TEST_CASE("armViewRecalc: the zoom-settle arming decision (v2.6 phase 2 WR-05)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  REQUIRE(t.state == efapp::TabState::Failed);
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 800.0));  // not Done: never arms, never stamps
  CHECK(t.armedViewPpu == 0.0);
  CHECK(t.pendingViewRecalcAt == -1.0);

  t.state = efapp::TabState::Done;  // the arming unit runs on a finished tab; the compute itself is the next test
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 100.0));  // first frame (armedViewPpu 0): never arms
  CHECK(t.armedViewPpu == doctest::Approx(100.0));
  CHECK(t.viewPpu == doctest::Approx(100.0));
  CHECK(t.pendingViewRecalcAt == -1.0);

  CHECK_FALSE(efapp::editor::armViewRecalc(t, 100.0));  // identical re-frame (same zoom, any DPI): inert
  CHECK(t.pendingViewRecalcAt == -1.0);

  CHECK(efapp::editor::armViewRecalc(t, 800.0));  // zoom into the adapted regime: arms
  CHECK(t.pendingViewRecalcAt == -2);
  CHECK(t.viewPpu == doctest::Approx(800.0));
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 800.0));  // settled (redraws at the same zoom): no re-arm
  CHECK(t.pendingViewRecalcAt == -2);

  // Zoom back into the legacy regime: the settle would reproduce the legacy calculation - disarms.
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 100.0));
  CHECK(t.pendingViewRecalcAt == -1.0);

  // WR-01: a ppu a hair above the reference (fp round-trip drift of a wheel factor) is legacy - the
  // stamp snaps down to kRefPpu EXACTLY, so no armed compute can land in the band.
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 100.0 * (1.0 + 5e-10)));
  CHECK(t.viewPpu == 100.0);  // exact equality: snapped, not approximately 100
  CHECK(t.pendingViewRecalcAt == -1.0);
  CHECK(efapp::editor::armViewRecalc(t, 100.001));  // genuinely above the band: the adapted regime arms
  CHECK(t.pendingViewRecalcAt == -2);
}

// v2.6 phase 2 (WR-05): the full GUI-side composition the verifier flagged as untested - arming through
// the helper, the tick converting the marker, one fire with the stamped scale.
TEST_CASE("armViewRecalc -> viewRecalcTick -> calculate: the zoom settle fires once with the stamped scale (v2.6 phase 2 WR-05)") {
  std::vector<ef::ComputeParams> received;
  efapp::Workspace ws{ef::ComputeParams{}, [&](const ef::ChargeSystem&, const ef::ComputeParams& p) -> std::unique_ptr<efapp::ISession> {
    received.push_back(p);
    throw std::runtime_error("no compute");  // instant Failed tab: session null, recalc paths reachable
  }};
  const unsigned s = ws.openEmpty();
  efapp::Tab& t = *ws.find(s);
  REQUIRE(received.size() == 1);
  CHECK(received[0].viewScale == doctest::Approx(0.0));  // the first computation is legacy
  t.state = efapp::TabState::Done;                       // it finished (simulated - the factory above throws)

  CHECK_FALSE(efapp::editor::armViewRecalc(t, 100.0));  // first frame
  CHECK(efapp::editor::armViewRecalc(t, 800.0));        // the user zooms and the wheel stops: armed
  CHECK(t.pendingViewRecalcAt == -2);
  ws.viewRecalcTick(100.0);                             // -2 becomes the deadline
  CHECK_FALSE(efapp::editor::armViewRecalc(t, 800.0));  // redraws while pending are inert...
  CHECK(t.pendingViewRecalcAt == doctest::Approx(100.0 + efapp::kAutoRecalcDelay));  // ...deadline intact
  CHECK(received.size() == 1);
  ws.viewRecalcTick(100.0 + efapp::kAutoRecalcDelay);   // due: fires exactly once
  REQUIRE(received.size() == 2);
  CHECK(received[1].viewScale == doctest::Approx(800.0));
  CHECK(t.params.viewScale == doctest::Approx(800.0));
  CHECK(t.pendingViewRecalcAt < 0);                     // disarmed by the fire
  CHECK_FALSE(t.stale);
  CHECK_FALSE(t.dirty);
  CHECK(t.undo.empty());
}

TEST_CASE("dirty tracking and anyDirty (v2.2 leave prompt)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK_FALSE(f.ws.anyDirty());
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-1, 0}));
  CHECK(t.dirty);
  CHECK(f.ws.anyDirty());
  t.dirty = false;  // Save As marks the tab clean (App does exactly this)
  CHECK_FALSE(f.ws.anyDirty());
}

TEST_CASE("autoRecalcTick recomputes stale autoRecalc tabs after the deadline (v2.2 CALC-03)") {
  efapp::Workspace ws;
  const unsigned s = ws.openEmpty();
  efapp::Tab& t = *ws.find(s);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  const std::size_t lines = t.scene.electric.size();
  REQUIRE(lines > 0);

  // Continuous calculation on, one edit: stale, and the tick only fires once the deadline passes.
  t.autoRecalc = true;
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{1, 0}));
  CHECK(t.stale);
  const double now = 100.0;
  t.pendingRecalcAt = now + efapp::kAutoRecalcDelay;
  ws.autoRecalcTick(now);  // before the deadline: nothing happens
  CHECK(t.state == efapp::TabState::Done);
  CHECK(t.stale);
  ws.autoRecalcTick(now + efapp::kAutoRecalcDelay + 0.01);  // deadline passed: recalculation starts
  CHECK(t.state == efapp::TabState::Computing);
  CHECK_FALSE(t.stale);
  CHECK(t.pendingRecalcAt < 0);  // disarmed by calculate

  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  CHECK(t.scene.electric.size() == lines);  // one +1 charge: same count, drawn at the new position

  // Continuous calculation off: edits stay stale, no tick effect.
  t.autoRecalc = false;
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{2, 0}));
  ws.autoRecalcTick(1000.0);
  CHECK(t.state == efapp::TabState::Done);
  CHECK(t.stale);
}

// v2.5 (261012-h24): a mouse drag coalesces into ONE undo entry — the pre-drag state — through the
// editor begin/commit drag transaction. Plain moves outside a transaction keep one entry per call.
TEST_CASE("a mouse drag coalesces into one undo entry (v2.5)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  REQUIRE(t.field.charges[0].position.X == doctest::Approx(-2.0));  // dipole: +1 at (-2,0)
  CHECK_FALSE(efapp::editor::dragActive(t));

  efapp::editor::beginDrag(t, 0);
  CHECK(efapp::editor::dragActive(t));
  // Moves while the transaction is open stay silent for history, but keep every realtime-draw semantic:
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-1, 0}));
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{1, 0}));
  CHECK(t.field.charges[0].position.X == doctest::Approx(1.0));
  CHECK(t.scene.charges[0].position.X == doctest::Approx(1.0));  // scene mirror still synced
  CHECK(t.stale);
  CHECK(t.dirty);
  CHECK(t.pendingRecalcAt == -2);
  CHECK(t.undo.empty());

  efapp::editor::beginDrag(t, 0);  // a second begin mid-drag: no-op, never overwrites the snapshot
  efapp::editor::commitDrag(t);
  CHECK_FALSE(efapp::editor::dragActive(t));
  REQUIRE(t.undo.size() == 1);  // the whole drag is exactly one entry

  CHECK(efapp::editor::undo(t));  // one undo restores the pre-drag position
  CHECK(t.field.charges[0].position.X == doctest::Approx(-2.0));
  CHECK(t.field.charges[0].position.Y == doctest::Approx(0.0));
  CHECK(t.scene.charges[0].position.X == doctest::Approx(-2.0));  // re-synced
  REQUIRE(t.redo.size() == 1);
  CHECK(efapp::editor::redo(t));  // redo returns to the dragged position
  CHECK(t.field.charges[0].position.X == doctest::Approx(1.0));
}

TEST_CASE("a drag with no net movement records nothing (v2.5)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);

  efapp::editor::beginDrag(t, 0);
  efapp::editor::commitDrag(t);  // never moved
  CHECK(t.undo.empty());
  CHECK_FALSE(efapp::editor::dragActive(t));

  efapp::editor::beginDrag(t, 0);
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{3, 0}));
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-2, 0}));  // back to the exact start (exact X/Y equality)
  efapp::editor::commitDrag(t);
  CHECK(t.undo.empty());
  CHECK_FALSE(efapp::editor::dragActive(t));
}

TEST_CASE("discrete moves keep one entry per call and never open a transaction (v2.5)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK_FALSE(efapp::editor::dragActive(t));
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-1, 0}));
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-0.5, 0}));
  REQUIRE(t.undo.size() == 2);  // one entry per moveCharge call
  CHECK_FALSE(efapp::editor::dragActive(t));  // plain moves never open a transaction
  t.selected = 0;
  CHECK(efapp::editor::stepSelected(t, ef::Vec2{0.5, 0}));
  REQUIRE(t.undo.size() == 3);  // the arrow-key rule: one entry per press
}

TEST_CASE("a drag transaction spans a computation and commits once (v2.5)") {
  efapp::Workspace ws;
  const unsigned s = ws.openEmpty();
  efapp::Tab& t = *ws.find(s);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);

  efapp::editor::beginDrag(t, 0);
  CHECK(efapp::editor::dragActive(t));
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{1, 0}));  // silent while the transaction is open
  CHECK(t.undo.empty());

  CHECK(ws.calculate(s));  // a mid-drag computation does not close the transaction
  CHECK(t.state == efapp::TabState::Computing);
  CHECK_FALSE(efapp::editor::moveCharge(t, 0, ef::Vec2{2, 0}));  // refused while computing (D-02)
  CHECK(efapp::editor::dragActive(t));  // the transaction survives the computation
  while (t.state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(t.state == efapp::TabState::Done);
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{2, 0}));  // silent again
  efapp::editor::commitDrag(t);
  REQUIRE(t.undo.size() == 1);  // the whole physical drag is one entry, not one per computation boundary
  CHECK(efapp::editor::undo(t));
  CHECK(t.field.charges[0].position.X == doctest::Approx(0.0));  // the pre-drag origin
  CHECK(t.field.charges[0].position.Y == doctest::Approx(0.0));
  ws.closeAll();
}

TEST_CASE("commitDrag after the dragged charge was removed mid-drag records nothing (v2.5)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  t.selected = 1;
  efapp::editor::beginDrag(t, 1);
  CHECK(efapp::editor::dragActive(t));
  CHECK(efapp::editor::moveCharge(t, 1, ef::Vec2{4, 1}));  // silent
  CHECK(t.undo.empty());
  // Delete mid-drag: removals record no history entry in this codebase (as today — add/remove are not
  // undoable), so the drag transaction is the only thing the commit below could contribute to.
  CHECK(efapp::editor::removeSelected(t));
  REQUIRE(t.field.charges.size() == 1);
  efapp::editor::commitDrag(t);  // the dragged index is out of range: discard, no crash
  CHECK_FALSE(efapp::editor::dragActive(t));
  CHECK(t.undo.empty());
}

// ---------------------------------------------------------------------------
// v2.6 phase 3: charge identity (id / name / per-charge size).
// ---------------------------------------------------------------------------

TEST_CASE("ids: presets get 1..N on open, addCharge assigns max+1, removed ids are never reused") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  REQUIRE(t.field.charges.size() == 2);
  CHECK(t.field.charges[0].id == 1);  // presets: assignChargeIds on open
  CHECK(t.field.charges[1].id == 2);
  CHECK(efapp::editor::nextChargeId(t.field) == 3);

  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 5}, 3));
  CHECK(t.field.charges[2].id == 3);  // max over the system + 1
  CHECK(t.scene.charges[2].sizePx == 0.0f);

  // Removing the max-id charge and adding again must NOT hand out id 3 again.
  t.selected = 2;
  CHECK(efapp::editor::removeSelected(t));
  REQUIRE(t.field.charges.size() == 2);
  CHECK(efapp::editor::nextChargeId(t.field) == 3);  // the pure helper only sees the live system...
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 6}, 1));
  CHECK(t.field.charges[2].id == 4);  // ...but the tab's high-water mark keeps 3 retired
}

TEST_CASE("ids: no reuse across undo/redo - snapshots restore ids, the high-water mark survives them") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);  // ids 1, 2
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 5}, 3));  // id 3
  CHECK(efapp::editor::moveCharge(t, 0, ef::Vec2{-1, 0}));  // history entry (pre-state has ids 1,2,3)

  t.selected = 2;
  CHECK(efapp::editor::removeSelected(t));  // removals record no history
  CHECK(efapp::editor::addCharge(t, ef::Vec2{1, 1}, 1));
  CHECK(t.field.charges[2].id == 4);  // 3 stays retired

  CHECK(efapp::editor::undo(t));  // the snapshot brings ids 1,2,3 back exactly
  REQUIRE(t.field.charges.size() == 3);
  CHECK(t.field.charges[0].id == 1);
  CHECK(t.field.charges[1].id == 2);
  CHECK(t.field.charges[2].id == 3);

  CHECK(efapp::editor::addCharge(t, ef::Vec2{2, 2}, 1));
  CHECK(t.field.charges[3].id == 5);  // 4 was handed out above: still retired after the undo

  CHECK(efapp::editor::redo(t));  // redo restores the moved-forward state (ids 1,2 and the id-4 add)
  REQUIRE(t.field.charges.size() == 3);
  CHECK(t.field.charges[2].id == 4);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{2, 2}, 1));
  CHECK(t.field.charges[3].id == 6);  // 3, 4 and 5 are all retired: max over system (4) and water (5) + 1
}

// WR-02: the assignment path saturates at the parsers' shared ceiling - id 0 (unassigned, derived
// display, still addressable by index) instead of an id a save->load round-trip would drop.
TEST_CASE("ids: saturate at the parser ceiling instead of minting an unparseable id (WR-02)") {
  ThrowFactory f;
  const unsigned u = f.ws.openUser("Ceiling", {ef::PresetCharge{1, 0, 0, 0.f, ef::kMaxChargeId, "Top"}});
  efapp::Tab& t = *f.ws.find(u);
  REQUIRE(t.field.charges.size() == 1);
  CHECK(t.field.charges[0].id == ef::kMaxChargeId);
  CHECK(t.chargeIdWater == ef::kMaxChargeId);

  CHECK(efapp::editor::addCharge(t, ef::Vec2{1, 1}, 1));
  CHECK(t.field.charges[1].id == 0);                   // 0 = unassigned, never ceiling + 1
  CHECK(t.chargeIdWater == ef::kMaxChargeId + 1);      // the watermark advances: the range stays retired
  CHECK(efapp::editor::addCharge(t, ef::Vec2{2, 2}, 1));
  CHECK(t.field.charges[2].id == 0);                   // sticky: still saturated
  CHECK(efapp::editor::nextChargeId(t.field) == ef::kMaxChargeId + 1);  // the pure helper is unchanged

  // assignChargeIds leaves id-0 charges unassigned once the ceiling is reached.
  ef::ChargeSystem g;
  g.charges.push_back(ef::Charge{1, ef::Vec2{0, 0}});
  g.charges[0].id = ef::kMaxChargeId;
  g.charges.push_back(ef::Charge{-1, ef::Vec2{1, 0}});  // id 0
  efapp::editor::assignChargeIds(g);
  CHECK(g.charges[0].id == ef::kMaxChargeId);
  CHECK(g.charges[1].id == 0);
}

TEST_CASE("ids: openUser / openRestored preserve persisted ids and only fill id == 0") {
  ThrowFactory f;
  const unsigned u = f.ws.openUser("Kept", {ef::PresetCharge{1, 0, 0, 0.f, 5, "Five"}, ef::PresetCharge{-1, 1, 1}});
  efapp::Tab& ut = *f.ws.find(u);
  REQUIRE(ut.field.charges.size() == 2);
  CHECK(ut.field.charges[0].id == 5);  // the persisted id survived
  CHECK(ut.field.charges[1].id == 6);  // only id == 0 got assigned (monotonic max+1)
  CHECK(ut.field.charges[0].name == "Five");
  CHECK(efapp::editor::nextChargeId(ut.field) == 7);  // addCharge continues past the persisted ids
  CHECK(efapp::editor::addCharge(ut, ef::Vec2{2, 2}, 1));
  CHECK(ut.field.charges[2].id == 7);

  const unsigned r = f.ws.openRestored("Restored", efapp::kNoPreset, ef::ComputeParams{},
                                       {ef::PresetCharge{2, 0, 0, 25.f, 9, "Nine"}, ef::PresetCharge{-2, 1, 0}});
  efapp::Tab& rt = *f.ws.find(r);
  REQUIRE(rt.field.charges.size() == 2);
  CHECK(rt.field.charges[0].id == 9);
  CHECK(rt.field.charges[1].id == 10);
  CHECK(rt.field.charges[0].sizePx == 25.0f);
  CHECK(rt.chargeIdWater == 10);  // later adds never collide with a restored id
}

TEST_CASE("addCharge stores the validated default size on the charge and its scene mirror (v2.6 phase 3)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 1}, 2, 40.f));
  CHECK(t.field.charges[2].sizePx == 40.0f);
  CHECK(t.scene.charges[2].sizePx == 40.0f);

  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 2}, 2, 999.f));  // out of range degrades to 0 = inherit
  CHECK(t.field.charges[3].sizePx == 0.0f);
  CHECK(t.scene.charges[3].sizePx == 0.0f);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 3}, 2, 1.f));  // below the 2 px floor degrades too
  CHECK(t.field.charges[4].sizePx == 0.0f);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 4}, 2, 120.f));  // the range edges are kept
  CHECK(t.field.charges[5].sizePx == 120.0f);
  CHECK(efapp::editor::addCharge(t, ef::Vec2{0, 5}, 2));  // the default argument stays 0
  CHECK(t.field.charges[6].sizePx == 0.0f);
}

TEST_CASE("setChargeName: dirty only (never stale), strips commas, identical no-op, history (v2.6 phase 3)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  REQUIRE_FALSE(t.stale);
  CHECK(t.pendingRecalcAt == -1.0);
  const std::size_t linesBefore = t.scene.electric.size();

  CHECK(efapp::editor::setChargeName(t, 0, "A,B"));
  CHECK(t.field.charges[0].name == "AB");  // ',' is the persistence separator: stripped
  CHECK(t.field.charges[0].id == 1);       // identity untouched
  CHECK_FALSE(t.stale);                    // name edits never invalidate the drawn lines
  CHECK(t.pendingRecalcAt == -1.0);        // and never arm the auto recalculation
  CHECK(t.dirty);
  CHECK(t.scene.electric.size() == linesBefore);
  CHECK(t.scene.charges.size() == 2);      // no scene write happens for a name

  CHECK(efapp::editor::setChargeName(t, 0, "AB"));  // identical name: no-op
  REQUIRE(t.undo.size() == 1);                      // ...and no history entry

  CHECK(efapp::editor::setChargeName(t, 1, "Other"));
  CHECK(efapp::editor::undo(t));
  CHECK(t.field.charges[1].name.empty());
  CHECK(efapp::editor::redo(t));
  CHECK(t.field.charges[1].name == "Other");

  CHECK_FALSE(efapp::editor::setChargeName(t, 7, "X"));  // bad index
  efapp::editor::setChargeName(t, 0, "");                 // empty is allowed: back to the derived name
  CHECK(t.field.charges[0].name.empty());
}

// WR-03: names are trimmed at the entry point so save->load is the identity (splitCsv trims every
// field on read); a whitespace-only name collapses to "" (the derived display).
TEST_CASE("setChargeName trims leading/trailing whitespace (WR-03)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK(efapp::editor::setChargeName(t, 0, "  Pad "));
  CHECK(t.field.charges[0].name == "Pad");  // the model can never hold trimmable-only differences
  CHECK(efapp::editor::setChargeName(t, 1, "\t\n"));
  CHECK(t.field.charges[1].name.empty());   // whitespace-only -> derived "Charge N"
  // The name survives the writer's emit + the reader's field trim byte-identically.
  std::vector<efapp::UserConfig> configs;
  configs.push_back({"R", {ef::toPresetCharge(t.field.charges[0])}});
  const efapp::ParsedConfigs r = efapp::parseConfigs(efapp::serializeConfigs(configs));
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 1);
  CHECK(r.value[0].charges[0].name == "Pad");
}

TEST_CASE("setChargeSize: validation, scene sync, dirty only, history (v2.6 phase 3)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK_FALSE(t.stale);
  CHECK(t.pendingRecalcAt == -1.0);

  CHECK(efapp::editor::setChargeSize(t, 0, 40.f));
  CHECK(t.field.charges[0].sizePx == 40.0f);
  CHECK(t.scene.charges[0].sizePx == 40.0f);  // the scene mirror follows so the canvas redraws at once
  CHECK_FALSE(t.stale);
  CHECK(t.pendingRecalcAt == -1.0);
  CHECK(t.dirty);

  CHECK(efapp::editor::setChargeSize(t, 0, 40.f));  // identical size: no-op
  REQUIRE(t.undo.size() == 1);

  // 0 (inherit), 2 and 120 are accepted; 1, 121 and -5 are refused without touching anything.
  CHECK(efapp::editor::setChargeSize(t, 1, 0.f));
  CHECK(t.field.charges[1].sizePx == 0.0f);
  CHECK(efapp::editor::setChargeSize(t, 1, 2.f));
  CHECK(efapp::editor::setChargeSize(t, 1, 120.f));
  CHECK_FALSE(efapp::editor::setChargeSize(t, 1, 1.f));
  CHECK_FALSE(efapp::editor::setChargeSize(t, 1, 121.f));
  CHECK_FALSE(efapp::editor::setChargeSize(t, 1, -5.f));
  CHECK(t.field.charges[1].sizePx == 120.0f);
  CHECK_FALSE(efapp::editor::setChargeSize(t, 7, 40.f));  // bad index

  // The undo/redo snapshots carry size (and id) with the whole ChargeSystem.
  CHECK(efapp::editor::setChargeSize(t, 0, 3.f));
  CHECK(efapp::editor::undo(t));
  CHECK(t.field.charges[0].sizePx == 40.0f);
  CHECK(t.field.charges[0].id == 1);
  CHECK(t.scene.charges[0].sizePx == 40.0f);
  CHECK(efapp::editor::redo(t));
  CHECK(t.field.charges[0].sizePx == 3.0f);
}

TEST_CASE("undo/redo restore name+size+id together through the ChargeSystem snapshot (v2.6 phase 3)") {
  ThrowFactory f;
  efapp::Tab& t = dipoleTab(f);
  CHECK(efapp::editor::setChargeName(t, 0, "Alpha"));
  CHECK(efapp::editor::setChargeSize(t, 0, 60.f));
  CHECK(t.undo.size() == 2);
  CHECK(efapp::editor::undo(t));  // back to name=Alpha, size=0
  CHECK(t.field.charges[0].name == "Alpha");
  CHECK(t.field.charges[0].sizePx == 0.0f);
  CHECK(t.field.charges[0].id == 1);
  CHECK(efapp::editor::undo(t));  // back to the untouched dipole
  CHECK(t.field.charges[0].name.empty());
  CHECK(t.field.charges[0].id == 1);
  CHECK(t.field.charges[1].id == 2);
  CHECK(efapp::editor::redo(t));
  CHECK(t.field.charges[0].name == "Alpha");
  CHECK(efapp::editor::redo(t));
  CHECK(t.field.charges[0].sizePx == 60.0f);
}

TEST_CASE("chargeDisplayName derives \"Charge N\" only when the name is empty (v2.6 phase 3)") {
  ef::Charge unnamed;
  CHECK(efapp::editor::chargeDisplayName(unnamed, 0) == "Charge 1");
  CHECK(efapp::editor::chargeDisplayName(unnamed, 4) == "Charge 5");  // 1-based index
  ef::Charge named;
  named.name = "My Charge";
  CHECK(efapp::editor::chargeDisplayName(named, 4) == "My Charge");
}

TEST_CASE("hitCharge: per-charge bounds, nearest wins, default-radius parity (v2.6 phase 3)") {
  ef::ChargeSystem f;
  ef::Charge big{1, ef::Vec2{0, 0}};
  big.sizePx = 60.f;  // bound 0.60 + 0.05 world at ppu 100
  ef::Charge small{-1, ef::Vec2{0.15, 0}};
  small.sizePx = 10.f;  // bound 0.10 + 0.05
  f.charges.push_back(big);
  f.charges.push_back(small);
  f.charges.push_back(ef::Charge{2, ef::Vec2{2, 0}});  // default radius (25 px -> 0.30)

  // The small charge sits inside the big one's bound: a point near it selects the SMALL one.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{0.14, 0}, 100.0, 25.0) == 1);
  // A point near the big charge's rim (far from the small one) selects the big one.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{-0.60, 0}, 100.0, 25.0) == 0);
  // Between the two but inside both bounds: nearest wins.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{0.10, 0}, 100.0, 25.0) == 1);
  // The default-radius charge behaves exactly like the old fixed 0.30 bound.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{2.25, 0}, 100.0, 25.0) == 2);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{2.31, 0}, 100.0, 25.0) == -1);  // 0.25 + 0.05 is the edge
  // Outside every bound.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{0, 0.80}, 100.0, 25.0) == -1);
  // Degenerate ppu falls back to 1.0: the default radius is then 25 world units.
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{10, 0}, 0.0, 25.0) == 2);
  CHECK(efapp::editor::hitCharge(f, ef::Vec2{10, 0}, -5.0, 25.0) == 2);
}
