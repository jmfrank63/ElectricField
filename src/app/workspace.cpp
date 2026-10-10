#include "app/workspace.hpp"

#include <algorithm>
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
}  // namespace

SessionFactory defaultSessionFactory() {
  return [](const ef::ChargeSystem& f, const ef::ComputeParams& p) -> std::unique_ptr<ISession> { return std::make_unique<RealSession>(f, p); };
}

unsigned Workspace::open(std::size_t presetIndex) {
  const ef::Preset& p = ef::presets().at(presetIndex);
  auto t = std::make_unique<Tab>();
  t->serial = nextSerial_++;
  t->preset = presetIndex;
  t->number = ++counters_[presetIndex];
  t->title = std::string(p.displayName) + " " + std::to_string(t->number);
  t->field = ef::makeSystem(p);
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
