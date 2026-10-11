#include "app/workspace.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <utility>

#include "app/ui_logic.hpp"
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
  return spawn(presetIndex, number, std::string(p.displayName) + " " + std::to_string(number), ef::makeSystem(p), std::string(p.displayName));
}

unsigned Workspace::openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges) {
  return openUser(name, charges, std::string());
}

unsigned Workspace::openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges, const std::string& source) {
  const std::string trimmed = trim(name);
  ef::ChargeSystem field;
  field.charges.reserve(charges.size());
  for (const ef::PresetCharge& c : charges) field.charges.push_back(ef::Charge{c.charge, ef::Vec2{c.x, c.y}});
  const unsigned number = ++userCounters_[trimmed];
  return spawn(kNoPreset, number, trimmed + " " + std::to_string(number), std::move(field), source.empty() ? trimmed : source);
}

unsigned Workspace::openEmpty() {
  // One +1 charge at the origin: computable from the first frame, and the Phase 2 editor's starting point.
  // source "" (v2.5 CFG-04): a fresh empty tab is not a saved configuration until Save Configuration As.
  return openUser("Empty Configuration", {ef::PresetCharge{1, 0.0, 0.0}}, "");
}

unsigned Workspace::spawn(const std::size_t presetIndex, const unsigned number, const std::string& title, ef::ChargeSystem&& field, const std::string& sourceConfig) {
  auto t = std::make_unique<Tab>();
  t->serial = nextSerial_++;
  t->preset = presetIndex;
  t->number = number;
  t->title = title;
  t->sourceConfig = sourceConfig;
  t->field = std::move(field);
  t->params = defaults_;
  t->computed = t->field;  // v2.3 EDIT-11: the state the first computation runs with
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
  t->computed = t->field;  // v2.3 EDIT-11: the charges this computation runs with (Esc reverts here)
  t->linesReceived = 0;
  t->state = TabState::Computing;
  t->stale = false;
  t->pendingRecalcAt = -1.0;  // a manual/automatic recalculation disarms the auto deadline
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

void Workspace::autoRecalcTick(const double nowSeconds) {
  for (auto& tp : tabs_) {
    Tab& t = *tp;
    if (t.autoRecalc && t.stale && t.pendingRecalcAt >= 0 && nowSeconds >= t.pendingRecalcAt) calculate(t.serial);
  }
}

bool Workspace::anyDirty() const {
  for (const auto& t : tabs_)
    if (t->dirty) return true;
  return false;
}

bool Workspace::rename(const unsigned serial, const std::string& name) {
  Tab* t = find(serial);
  const std::string trimmed = trim(name);
  if (!t || trimmed.empty()) return false;  // TAB-01: an empty name is refused
  t->title = trimmed;
  return true;
}

unsigned Workspace::openRestored(const std::string& title, const std::size_t presetIndex, const ef::ComputeParams& params,
                                 const std::vector<ef::PresetCharge>& charges, const bool autoRecalc, const GridSettings& grid) {
  ef::ChargeSystem field;
  field.charges.reserve(charges.size());
  for (const ef::PresetCharge& c : charges) field.charges.push_back(ef::Charge{c.charge, ef::Vec2{c.x, c.y}});
  const unsigned serial = spawn(presetIndex, 0, trim(title), std::move(field));
  Tab* t = find(serial);
  t->params = params;
  t->selected = -1;
  t->autoRecalc = autoRecalc;
  t->grid = grid;
  // v2.5 CFG-04: a restored tab keeps its configuration identity (a built-in preset's name, or a user
  // configuration's the title's base name - the same convention mergeDirtyTabsInto established).
  t->sourceConfig = presetIndex != kNoPreset ? std::string(ef::presets()[presetIndex].displayName) : baseTitle(t->title);
  t->computed = t->field;  // v2.3 EDIT-11
  // Bump the matching counter past the restored number so a later open never duplicates this title (SES-01).
  const std::size_t sp = t->title.rfind(' ');
  if (sp != std::string::npos && sp + 1 < t->title.size()) {
    bool digits = true;
    for (std::size_t i = sp + 1; i < t->title.size(); ++i)
      digits = std::isdigit(static_cast<unsigned char>(t->title[i])) != 0 && digits;
    if (digits) {
      const int n = std::atoi(t->title.c_str() + sp + 1);
      if (n > 0) {
        if (presetIndex != kNoPreset && presetIndex < ef::kPresetCount)
          counters_[presetIndex] = std::max(counters_[presetIndex], static_cast<unsigned>(n));
        else
          userCounters_[baseTitle(t->title)] = std::max(userCounters_[baseTitle(t->title)], static_cast<unsigned>(n));
      }
    }
  }
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
