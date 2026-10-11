// efapp: tab model over real ComputeSessions + the pure UI decisions fixed by 03-UI-SPEC.md. No SDL, no ImGui.
#include "doctest/doctest.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

// What one scripted session answers, shared between the fake (owned by a Tab) and the test (which may change it later).
struct Script {
  ef::SessionStatus status;      // returned by every poll()
  std::size_t electricPerPoll = 0;  // empty PollBatch::electric entries appended per poll (Workspace only counts them)
  bool takeResultOk = true;      // takeResult() return value
  int polls = 0;                 // number of poll() calls seen
};

enum class FactoryMode { Normal, ThrowStd, ThrowOther, ReturnNull };

// Test-owned state of the fake factory. Declare it BEFORE the Workspace: the Workspace destructor destroys the fakes, which
// write "destroy:<id>" into the log.
struct Harness {
  Script proto;                                // copied into each new session's script
  FactoryMode mode = FactoryMode::Normal;
  std::vector<std::shared_ptr<Script>> scripts;  // one per created session, in creation order (= the session id)
  std::vector<std::string> log;                // "cancel:<id>" / "destroy:<id>" in call order
};

struct ScriptedSession final : ISession {
  ScriptedSession(Harness& h, std::size_t i, std::shared_ptr<Script> s) : h_(h), id_(i), script_(std::move(s)) {}
  ~ScriptedSession() override { h_.log.push_back("destroy:" + std::to_string(id_)); }
  void cancel() override { h_.log.push_back("cancel:" + std::to_string(id_)); }
  ef::SessionStatus poll(ef::PollBatch& out) override {
    ++script_->polls;
    for (std::size_t i = 0; i < script_->electricPerPoll; ++i) out.electric.emplace_back();
    return script_->status;
  }
  bool takeResult(ef::FieldResult&) override { return script_->takeResultOk; }

 private:
  Harness& h_;
  std::size_t id_;
  std::shared_ptr<Script> script_;
};

SessionFactory scriptedFactory(Harness& h) {
  return [&h](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<ISession> {
    switch (h.mode) {
      case FactoryMode::ThrowStd: throw std::runtime_error("no threads");
      case FactoryMode::ThrowOther: throw 42;
      case FactoryMode::ReturnNull: return nullptr;
      case FactoryMode::Normal: break;
    }
    auto script = std::make_shared<Script>(h.proto);
    h.scripts.push_back(script);
    return std::make_unique<ScriptedSession>(h, h.scripts.size() - 1, script);
  };
}

ef::SessionStatus doneStatus() {
  ef::SessionStatus s;
  s.done = true;
  return s;
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

// ---- Wave 0 gaps (03-RESEARCH): FramePacer decisions and the scripted-session Workspace state machine ----

TEST_CASE("frame pacer: blocks only when idle with no settle frames left") {
  FramePacer p;
  CHECK(p.pendingFrames() == 0);
  CHECK(p.shouldBlock(false));
  CHECK_FALSE(p.shouldBlock(true));  // a computing session keeps the loop running
  p.notifyActivity();
  CHECK(p.pendingFrames() == FramePacer::kSettleFrames);
  CHECK(p.pendingFrames() == 3);
  for (int i = 0; i < 3; ++i) {
    CHECK_FALSE(p.shouldBlock(false));  // frames 1..3 are still owed
    p.frameRendered();
  }
  CHECK(p.shouldBlock(false));
  CHECK_FALSE(p.shouldBlock(true));
  p.frameRendered();  // extra frames never go below 0
  p.frameRendered();
  CHECK(p.pendingFrames() == 0);
  CHECK(p.shouldBlock(false));
}

TEST_CASE("frame pacer: a modal arms 12 frames and a smaller request never lowers a pending larger one") {
  FramePacer p;
  p.notifyActivity(FramePacer::kModalFadeFrames);
  CHECK(p.pendingFrames() == 12);
  p.notifyActivity();  // 3 < 12
  CHECK(p.pendingFrames() == 12);
  p.frameRendered();
  CHECK(p.pendingFrames() == 11);
  p.notifyActivity();  // still 3 < 11
  CHECK(p.pendingFrames() == 11);
  CHECK(FramePacer::kModalFadeFrames == 12);
  CHECK(FramePacer::kIdleWaitMs == 250);
}

TEST_CASE("workspace: a failed session keeps its error, an empty one reads Unknown error") {
  Harness h;
  h.proto.status = doneStatus();
  h.proto.status.failed = true;
  h.proto.status.error = "boom";
  Workspace ws(lightParams(), scriptedFactory(h));
  const unsigned a = ws.open(0);
  CHECK(ws.anyComputing());
  ws.pollAll();
  Tab* t = ws.find(a);
  REQUIRE(t != nullptr);
  CHECK(t->state == TabState::Failed);
  CHECK(t->error == "boom");
  CHECK(t->session == nullptr);
  CHECK_FALSE(ws.anyComputing());
  CHECK(tabLabel(*t, 0.0).rfind("! ", 0) == 0);

  h.proto.status.error.clear();
  const unsigned b = ws.open(0);
  ws.pollAll();
  t = ws.find(b);
  REQUIRE(t != nullptr);
  CHECK(t->state == TabState::Failed);
  CHECK(t->error.empty());
  CHECK(failureDetail(t->error) == "Unknown error");
}

TEST_CASE("workspace: cancelled, no result and done end in the states the UI-SPEC fixes") {
  Harness h;
  Workspace ws(lightParams(), scriptedFactory(h));

  h.proto.status = doneStatus();
  h.proto.status.cancelled = true;
  const unsigned c = ws.open(0);

  h.proto.status = doneStatus();
  h.proto.takeResultOk = false;
  const unsigned n = ws.open(1);

  h.proto.takeResultOk = true;
  const unsigned d = ws.open(2);

  ws.pollAll();
  CHECK(ws.find(c)->state == TabState::Failed);
  CHECK(ws.find(c)->error == "The computation was cancelled.");
  CHECK(ws.find(c)->session == nullptr);
  CHECK(ws.find(n)->state == TabState::Failed);
  CHECK(ws.find(n)->error == "The computation produced no result.");
  CHECK(ws.find(n)->session == nullptr);
  CHECK(ws.find(d)->state == TabState::Done);
  CHECK(ws.find(d)->error.empty());
  CHECK(ws.find(d)->session == nullptr);
  CHECK(ws.find(d)->scene.charges.size() == 2);  // the charges survive the scene rebuild
  CHECK_FALSE(ws.anyComputing());
}

TEST_CASE("workspace: a factory that throws or returns null gives a Failed tab, never an exception") {
  Harness h;
  Workspace ws(lightParams(), scriptedFactory(h));

  h.mode = FactoryMode::ThrowStd;
  unsigned s = 0;
  REQUIRE_NOTHROW(s = ws.open(0));
  CHECK(ws.find(s)->state == TabState::Failed);
  CHECK(ws.find(s)->error == "no threads");
  CHECK(ws.find(s)->session == nullptr);

  h.mode = FactoryMode::ReturnNull;
  REQUIRE_NOTHROW(s = ws.open(0));
  CHECK(ws.find(s)->state == TabState::Failed);
  CHECK(ws.find(s)->error == "no session");

  h.mode = FactoryMode::ThrowOther;
  REQUIRE_NOTHROW(s = ws.open(0));
  CHECK(ws.find(s)->state == TabState::Failed);
  CHECK(ws.find(s)->error == "unknown error while starting the computation");

  CHECK(ws.tabs().size() == 3);  // the failed tabs stay, with their charges drawn
  CHECK(ws.find(s)->scene.charges.size() == 2);
  CHECK_FALSE(ws.anyComputing());
  CHECK(ws.find(s)->title == "Dipole 3");
}

TEST_CASE("workspace: poll accumulates the line count and finished tabs are not polled again") {
  Harness h;
  h.proto.electricPerPoll = 2;
  Workspace ws(lightParams(), scriptedFactory(h));
  const unsigned s = ws.open(0);
  ws.pollAll();
  CHECK(ws.find(s)->linesReceived == 2);
  ws.pollAll();
  CHECK(ws.find(s)->linesReceived == 4);  // cumulative
  CHECK(ws.find(s)->state == TabState::Computing);
  CHECK(h.scripts[0]->polls == 2);

  h.scripts[0]->status = doneStatus();  // the session now reports done: one more poll, then it is finished
  ws.pollAll();
  CHECK(ws.find(s)->state == TabState::Done);
  CHECK(ws.find(s)->linesReceived == 6);
  CHECK(h.scripts[0]->polls == 3);
  ws.pollAll();
  ws.pollAll();
  CHECK(h.scripts[0]->polls == 3);  // Done is never polled again
  CHECK(ws.find(s)->linesReceived == 6);

  Harness hf;
  hf.proto.status = doneStatus();
  hf.proto.status.failed = true;
  Workspace wf(lightParams(), scriptedFactory(hf));
  const unsigned f = wf.open(0);
  wf.pollAll();
  wf.pollAll();
  CHECK(wf.find(f)->state == TabState::Failed);
  CHECK(hf.scripts[0]->polls == 1);  // nor is Failed
}

TEST_CASE("workspace: close cancels before destroying and closeAll cancels every session first") {
  Harness h;  // outlives the workspace: the destructors below log into it
  Workspace ws(lightParams(), scriptedFactory(h));
  const unsigned a = ws.open(0);
  const unsigned b = ws.open(1);
  const unsigned c = ws.open(2);
  const unsigned d = ws.open(3);
  CHECK(a < b);
  CHECK(b < c);
  CHECK(c < d);

  REQUIRE(ws.close(b));
  REQUIRE(h.log.size() == 2);
  CHECK(h.log[0] == "cancel:1");
  CHECK(h.log[1] == "destroy:1");
  CHECK(ws.closed() == 1);
  CHECK(ws.find(b) == nullptr);
  REQUIRE(ws.tabs().size() == 3);
  CHECK(ws.tabs()[0]->serial == a);  // open order is kept
  CHECK(ws.tabs()[1]->serial == c);
  CHECK(ws.tabs()[2]->serial == d);

  h.log.clear();
  ws.closeAll();
  REQUIRE(h.log.size() == 6);
  for (std::size_t i = 0; i < 3; ++i) CHECK(h.log[i].rfind("cancel:", 0) == 0);   // all cancels ...
  for (std::size_t i = 3; i < 6; ++i) CHECK(h.log[i].rfind("destroy:", 0) == 0);  // ... before the first destroy
  CHECK(ws.tabs().empty());
  CHECK(ws.closed() == 4);
  CHECK(ws.opened() == 4);
  CHECK_FALSE(ws.anyComputing());
}

TEST_CASE("workspace: an empty workspace is inert") {
  Harness h;
  Workspace ws(lightParams(), scriptedFactory(h));
  CHECK(ws.tabs().empty());
  CHECK_FALSE(ws.anyComputing());
  ws.pollAll();
  ws.cancelAll();
  ws.closeAll();
  CHECK(ws.opened() == 0);
  CHECK(ws.closed() == 0);
  CHECK(ws.find(42) == nullptr);
  CHECK_FALSE(ws.close(42));
  CHECK(ws.closed() == 0);
  CHECK(h.log.empty());
}

TEST_CASE("workspace: two tabs of one preset stay independent and numbers keep rising after closes") {
  Harness h;
  Workspace ws(lightParams(), scriptedFactory(h));
  const unsigned a = ws.open(0);
  const unsigned b = ws.open(0);
  CHECK(a != b);
  CHECK(ws.find(a)->title == "Dipole 1");
  CHECK(ws.find(b)->title == "Dipole 2");
  CHECK(ws.find(a)->session.get() != ws.find(b)->session.get());
  CHECK(ws.close(b));
  const unsigned c = ws.open(0);
  CHECK(c > b);  // serials are never reused either
  CHECK(ws.find(c)->title == "Dipole 3");
  CHECK(ws.find(c)->number > ws.find(a)->number);
}

TEST_CASE("openUser: trimmed title, per-name counter never reused, charges and kNoPreset (v2 CFG-02)") {
  efapp::Workspace ws(ef::ComputeParams{}, [](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> {
    throw std::runtime_error("no compute in this structural test");
  });
  const unsigned s1 = ws.openUser("  Mine  ", {{1, -2, 0}, {-1, 2, 0}});
  const efapp::Tab* t = ws.find(s1);
  REQUIRE(t != nullptr);
  CHECK(t->title == "Mine 1");
  CHECK(t->preset == efapp::kNoPreset);
  CHECK(t->number == 1);
  CHECK(t->state == efapp::TabState::Failed);  // the throwing factory marks it Failed; the structure is intact
  REQUIRE(t->field.charges.size() == 2);
  CHECK(t->field.charges[0].charge == 1);
  CHECK(t->field.charges[0].position.X == doctest::Approx(-2.0));
  CHECK(t->field.charges[1].charge == -1);
  CHECK(t->field.charges[1].position.X == doctest::Approx(2.0));

  // The per-name counter is never reused after a close; a different name counts from 1.
  CHECK(ws.close(s1));
  const unsigned s2 = ws.openUser("Mine", {});
  CHECK(ws.find(s2)->title == "Mine 2");
  const unsigned s3 = ws.openUser("Other", {});
  CHECK(ws.find(s3)->title == "Other 1");
  CHECK(ws.find(s3)->number == 1);
}

TEST_CASE("openEmpty: one +1 charge at the origin, own counter, presets unaffected (v2 CFG-02)") {
  efapp::Workspace ws(ef::ComputeParams{}, [](const ef::ChargeSystem&, const ef::ComputeParams&) -> std::unique_ptr<efapp::ISession> {
    throw std::runtime_error("no compute in this structural test");
  });
  const unsigned s1 = ws.openEmpty();
  const efapp::Tab* t = ws.find(s1);
  REQUIRE(t != nullptr);
  CHECK(t->title == "Empty Configuration 1");
  REQUIRE(t->field.charges.size() == 1);
  CHECK(t->field.charges[0].charge == 1);
  CHECK(t->field.charges[0].position.X == doctest::Approx(0.0));
  CHECK(t->field.charges[0].position.Y == doctest::Approx(0.0));

  CHECK(ws.close(s1));
  CHECK(ws.openEmpty() == s1 + 1);  // next serial; the counter would now read "Empty Configuration 2"
  CHECK(ws.find(ws.open(0))->title == "Dipole 1");  // preset counters untouched by user tabs
}

TEST_CASE("openUser computes through the real session path like a preset tab (v2 CFG-02)") {
  efapp::Workspace ws;
  const unsigned s = ws.openUser("Mine", {{1, -2, 0}, {-1, 2, 0}});
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (ws.find(s) != nullptr && ws.find(s)->state == efapp::TabState::Computing && std::chrono::steady_clock::now() < deadline) {
    ws.pollAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  REQUIRE(ws.find(s) != nullptr);
  REQUIRE(ws.find(s)->state == efapp::TabState::Done);
  CHECK(ws.find(s)->scene.electric.size() == 16);  // dipole charges, default options: the parity count
}
