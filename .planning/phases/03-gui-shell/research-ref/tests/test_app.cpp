// efapp: tab model over real ComputeSessions + the pure UI decisions fixed by 03-UI-SPEC.md. No SDL, no ImGui.
#include "doctest/doctest.h"

#include <chrono>
#include <string>
#include <thread>

#include "app/ui_logic.hpp"
#include "app/workspace.hpp"
#include "ef/presets.hpp"
#include "test_params.hpp"

using namespace efapp;

namespace {
bool waitUntil(Workspace& ws, unsigned serial, std::chrono::milliseconds limit) {
  const auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    ws.pollAll();
    const Tab* t = ws.find(serial);
    if (!t || t->state != TabState::Computing) return t && t->state == TabState::Done;
    if (std::chrono::steady_clock::now() - t0 > limit) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}
}  // namespace

TEST_CASE("tab titles use a per-preset counter that is never reused") {
  Workspace ws(lightParams());
  const unsigned a = ws.open(0);
  const unsigned b = ws.open(0);
  const unsigned c = ws.open(4);
  CHECK(ws.find(a)->title == "Dipole 1");
  CHECK(ws.find(b)->title == "Dipole 2");
  CHECK(ws.find(c)->title == "Quadrupole 1");
  CHECK(ws.close(a));
  const unsigned d = ws.open(0);
  CHECK(ws.find(d)->title == "Dipole 3");  // not "Dipole 1" again
  CHECK(ws.opened() == 4);
  CHECK(ws.closed() == 1);
  CHECK_FALSE(ws.close(999));
}

TEST_CASE("a new tab shows its charges at once and finishes with a cached scene") {
  Workspace ws(lightParams());
  const unsigned s = ws.open(0);
  Tab* t = ws.find(s);
  REQUIRE(t != nullptr);
  CHECK(t->scene.charges.size() == 2);  // drawn from the first frame
  CHECK(t->scene.electric.empty());
  CHECK(t->state == TabState::Computing);
  CHECK(t->expectedLines > 0);
  REQUIRE(waitUntil(ws, s, std::chrono::seconds(60)));
  t = ws.find(s);
  CHECK(t->state == TabState::Done);
  CHECK_FALSE(t->scene.electric.empty());
  CHECK(t->session == nullptr);
  CHECK_FALSE(ws.anyComputing());
}

TEST_CASE("closing a computing tab and closeAll() return promptly and leave no session") {
  Workspace ws(lightParams());
  ef::ComputeParams heavy;  // MFC defaults: the heaviest realistic session
  Workspace wsHeavy(heavy);
  for (std::size_t i = 0; i < ef::kPresetCount; ++i) wsHeavy.open(i);
  CHECK(wsHeavy.anyComputing());
  const auto t0 = std::chrono::steady_clock::now();
  wsHeavy.closeAll();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  CHECK(wsHeavy.tabs().empty());
  CHECK(wsHeavy.closed() == ef::kPresetCount);
  CHECK(ms < 1000);  // 9 sessions x 4 threads cancelled first, joined afterwards
}

TEST_CASE("busy glyph cycles | / - \\ at 8 steps per second") {
  CHECK(busyGlyph(0.0) == '|');
  CHECK(busyGlyph(0.125) == '/');
  CHECK(busyGlyph(0.25) == '-');
  CHECK(busyGlyph(0.375) == '\\');
  CHECK(busyGlyph(0.5) == '|');
}

TEST_CASE("tab label keeps a stable id while the marker changes") {
  Tab t;
  t.serial = 7;
  t.title = "Dipole 1";
  t.state = TabState::Computing;
  CHECK(tabLabel(t, 0.0) == "| Dipole 1###ef7");
  CHECK(tabLabel(t, 0.125) == "/ Dipole 1###ef7");
  t.state = TabState::Failed;
  CHECK(tabLabel(t, 0.0) == "! Dipole 1###ef7");
  t.state = TabState::Done;
  CHECK(tabLabel(t, 0.0) == "Dipole 1###ef7");
}

TEST_CASE("overlay text clamps n to m") {
  CHECK(overlayText(3, 17) == "Computing... 3 / 17 lines");
  CHECK(overlayText(20, 17) == "Computing... 17 / 17 lines");
  CHECK(overlayText(0, 0) == "Computing... 0 / 0 lines");
}

TEST_CASE("window title, version line, shortcut labels, failure text") {
  CHECK(windowTitle(nullptr) == "Electric Field");
  Tab t;
  t.title = "Dipole 1";
  CHECK(windowTitle(&t) == "Electric Field \xE2\x80\x94 Dipole 1");
  CHECK(versionLine("0.1.0", "3880738") == "Version 0.1.0 (3880738)");
  CHECK(versionLine("0.1.0", "") == "Version 0.1.0 (unknown)");
  CHECK(shortcutLabel("N", false) == "Ctrl+N");
  CHECK(shortcutLabel("W", true) == "Cmd+W");
  CHECK(failureDetail("") == "Unknown error");
  CHECK(failureDetail("boom") == "boom");
}

TEST_CASE("display scale and window metrics") {
  CHECK(clampScale(0.1f) == doctest::Approx(0.75f));
  CHECK(clampScale(9.f) == doctest::Approx(4.f));
  CHECK(clampScale(1.5f) == doctest::Approx(1.5f));
  CHECK(clampScale(0.f) == doctest::Approx(1.f));
  CHECK(clampScale(std::nanf("")) == doctest::Approx(1.f));
  const IntSize w1 = defaultWindowSize(1.f, 1920, 1080);
  CHECK(w1.w == 1024);
  CHECK(w1.h == 824);
  const IntSize w2 = defaultWindowSize(1.5f, 1920, 1080);  // 1536 x 1236 wanted, 90% of 1080 = 972
  CHECK(w2.w == 1536);
  CHECK(w2.h == 972);
  const IntSize m = minimumWindowSize(2.f);
  CHECK(m.w == 1280);
  CHECK(m.h == 960);
}

TEST_CASE("canvas view: 100 px per unit times k, floor of the available area, at least 1 px") {
  const ef::View v = canvasView(1023.7f, 768.2f, 1.5f);
  CHECK(v.widthPx == 1023);
  CHECK(v.heightPx == 768);
  CHECK(v.pixelsPerUnit == doctest::Approx(150.0));
  const ef::View tiny = canvasView(0.f, -3.f, 1.f);
  CHECK(tiny.widthPx == 1);
  CHECK(tiny.heightPx == 1);
  const ef::DrawStyle s = scaledStyle(2.f);
  CHECK(s.chargeRadiusPx == doctest::Approx(50.f));
  CHECK(s.electricThickness == doctest::Approx(2.f));
  CHECK(s.positiveCharge.b == 255);  // colours are not scaled
}
