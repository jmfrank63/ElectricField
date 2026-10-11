#include "app/workspace.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <utility>

#include "ef/seeds.hpp"

namespace efapp {

namespace {
struct RealSession final : ISession {
  RealSession(const ef::ChargeSystem& f, const ef::ComputeParams& p) : s(f, p) {}
  void cancel() override { s.cancel(); }
  ef::SessionStatus poll(ef::PollBatch& out) override { return s.poll(out); }
  bool takeResult(ef::FieldResult& out) override { return s.takeResult(out); }
  ef::ComputeSession s;
};

std::string trim(const std::string& s) {
  const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  std::size_t b = 0, e = s.size();
  while (b < e && isSpace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}
}  // namespace

SessionFactory defaultSessionFactory() {
  return [](const ef::ChargeSystem& f, const ef::ComputeParams& p) -> std::unique_ptr<ISession> { return std::make_unique<RealSession>(f, p); };
}

unsigned Workspace::open(std::size_t presetIndex) {
  const ef::Preset& p = ef::presets().at(presetIndex);
  const unsigned number = ++counters_[presetIndex];
  return spawn(presetIndex, number, std::string(p.displayName) + " " + std::to_string(number), ef::makeSystem(p));
}

unsigned Workspace::openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges) {
  const std::string trimmed = trim(name);
  ef::ChargeSystem field;
  field.charges.reserve(charges.size());
  for (const ef::PresetCharge& c : charges) field.charges.push_back(ef::Charge{c.charge, ef::Vec2{c.x, c.y}});
  const unsigned number = ++userCounters_[trimmed];
  return spawn(kNoPreset, number, trimmed + " " + std::to_string(number), std::move(field));
}

unsigned Workspace::openEmpty() {
  // One +1 charge at the origin: computable from the first frame, and the Phase 2 editor's starting point.
  return openUser("Empty Configuration", {ef::PresetCharge{1, 0.0, 0.0}});
}

unsigned Workspace::spawn(std::size_t presetIndex, const unsigned number, const std::string& title, ef::ChargeSystem&& field) {
  auto t = std::make_unique<Tab>();
  t->serial = nextSerial_++;
  t->preset = presetIndex;
  t->number = number;
  t->title = title;
  t->field = std::move(field);
  t->params = defaults_;
  t->expectedLines = ef::expectedLineCount(t->field, t->params);
  t->scene = ef::buildScene(t->field, ef::FieldResult{});  // charges only: drawn from the first frame
  try {
    t->session = factory_(t->field, t->params);
    if (!t->session) throw std::runtime_error("no session");
  } catch (const std::exception& e) {
    t->state = TabState::Failed;
    t->error = e.what();
  } catch (...) {
    t->state = TabState::Failed;
    t->error = "unknown error while starting the computation";
  }
  const unsigned serial = t->serial;
  tabs_.push_back(std::move(t));
  ++opened_;
  return serial;
}

bool Workspace::calculate(const unsigned serial) {
  Tab* t = find(serial);
  if (!t || t->session) return false;  // unknown serial or still computing (v2 D-03)
  t->expectedLines = ef::expectedLineCount(t->field, t->params);
  t->scene = ef::buildScene(t->field, ef::FieldResult{});  // parity: a fresh computation starts charges-only
  t->linesReceived = 0;
  t->state = TabState::Computing;
  t->stale = false;
  try {
    t->session = factory_(t->field, t->params);
    if (!t->session) throw std::runtime_error("no session");
  } catch (const std::exception& e) {
    t->state = TabState::Failed;
    t->error = e.what();
  } catch (...) {
    t->state = TabState::Failed;
    t->error = "unknown error while starting the computation";
  }
  return true;
}

bool Workspace::close(unsigned serial) {
  const auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto& t) { return t->serial == serial; });
  if (it == tabs_.end()) return false;
  if ((*it)->session) (*it)->session->cancel();
  tabs_.erase(it);  // ~Tab -> ~ComputeSession joins the workers (measured <= 5 ms, contract 200 ms)
  ++closed_;
  return true;
}

void Workspace::poll(Tab& t) {
  ef::PollBatch batch;
  const ef::SessionStatus st = t.session->poll(batch);
  t.linesReceived += batch.electric.size();  // only the count is needed; the shared lines are dropped here
  if (!st.done) return;
  if (st.failed) {
    t.state = TabState::Failed;
    t.error = st.error;
  } else if (st.cancelled) {
    t.state = TabState::Failed;
    t.error = "The computation was cancelled.";
  } else {
    ef::FieldResult r;
    if (t.session->takeResult(r)) {
      t.scene = ef::buildScene(t.field, r);
      t.state = TabState::Done;
    } else {
      t.state = TabState::Failed;
      t.error = "The computation produced no result.";
    }
  }
  t.session.reset();  // every worker has finished: the join is immediate
}

void Workspace::pollAll() {
  for (auto& t : tabs_)
    if (t->state == TabState::Computing && t->session) poll(*t);
}

void Workspace::cancelAll() noexcept {
  for (auto& t : tabs_)
    if (t->session) t->session->cancel();
}

void Workspace::closeAll() {
  cancelAll();
  closed_ += static_cast<unsigned>(tabs_.size());
  tabs_.clear();
}

bool Workspace::anyComputing() const {
  return std::any_of(tabs_.begin(), tabs_.end(), [](const auto& t) { return t->state == TabState::Computing; });
}

Tab* Workspace::find(unsigned serial) {
  for (auto& t : tabs_)
    if (t->serial == serial) return t.get();
  return nullptr;
}

}  // namespace efapp
