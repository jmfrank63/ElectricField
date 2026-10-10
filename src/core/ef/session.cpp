#include "ef/session.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "ef/method.hpp"
#include "ef/tracer.hpp"

namespace ef {

// Hooks handed to the tracers: cooperative cancel flag + equipotential job posting (replaces the queue push + thread
// restart in PostCalculateEquipotential).
struct ComputeSession::JobHooks {
  ComputeSession* s;
  const Job* job;
  std::size_t nextChild = 1;

  bool cancelled() const noexcept { return s->stop_.load(std::memory_order_relaxed); }

  void postEquipotential(const Vec2& startPoint, double label) {
    Job child;
    child.seed = job->seed;
    child.isEquipotential = true;
    child.potentialLabel = label;
    child.start = startPoint;
    child.key = JobKey{job->key.parent, nextChild++};
    {
      std::lock_guard<std::mutex> lk(s->m_);
      s->queue_.push_back(std::move(child));
      ++s->jobsKnown_;
    }
    s->cv_.notify_one();
  }
};

ComputeSession::ComputeSession(ChargeSystem field, ComputeParams params) : field_(std::move(field)), params_(params) {
  const std::vector<Seed> seeds = planSeeds(field_, params_);
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    Job j;
    j.seed = seeds[i];
    j.start = seeds[i].point;
    j.key = JobKey{i, 0};
    queue_.push_back(std::move(j));
  }
  jobsKnown_ = seeds.size();

  const unsigned n = std::max(1u, params_.numThreads);
  activeWorkers_.store(n);
  workers_.reserve(n);
  try {
    for (unsigned i = 0; i < n; ++i) workers_.emplace_back([this] { workerMain(); });
  } catch (...) {  // thread creation failed: stop and join what already runs, then rethrow
    stop_.store(true);
    { std::lock_guard<std::mutex> lk(m_); }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
    throw;
  }
}

ComputeSession::~ComputeSession() {
  cancel();
  for (auto& t : workers_)
    if (t.joinable()) t.join();
}

void ComputeSession::cancel() {
  stop_.store(true);
  { std::lock_guard<std::mutex> lk(m_); }  // pairs with the predicate check under the lock: no lost wake-up
  cv_.notify_all();
  doneCv_.notify_all();
}

void ComputeSession::fail(const char* what) {
  {
    std::lock_guard<std::mutex> lk(m_);
    if (!failed_) {
      failed_ = true;
      try {
        error_ = what;
      } catch (...) {  // out of memory while recording the message: the failed flag alone is still reported
      }
    }
  }
  cancel();
}

void ComputeSession::workerMain() {
  try {
    withSolver(params_.method, [this](auto& solver) { workerLoop(solver); });  // one solver per thread: K[] is per-instance state
  } catch (const std::exception& e) {
    fail(e.what());
  } catch (...) {
    fail("unknown exception in compute worker");
  }
  if (activeWorkers_.fetch_sub(1, std::memory_order_acq_rel) == 1) finish();
}

template <typename Solver>
void ComputeSession::workerLoop(Solver& solver) {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lk(m_);
      cv_.wait(lk, [this] { return stop_.load(std::memory_order_relaxed) || !queue_.empty() || inFlight_ == 0; });
      if (stop_.load(std::memory_order_relaxed) || queue_.empty()) return;  // queue empty && nothing in flight: no job can appear any more
      job = std::move(queue_.front());
      queue_.pop_front();
      ++inFlight_;
    }

    JobHooks hooks{this, &job};
    TraceStatus st;
    std::shared_ptr<const KeyedElectricLine> e;
    std::shared_ptr<const KeyedPotentialLine> p;
    if (job.isEquipotential) {
      auto line = traceEquipotential(solver, field_, params_, job.start, job.potentialLabel, hooks, &st);
      if (!st.cancelled) p = std::make_shared<const KeyedPotentialLine>(KeyedPotentialLine{job.key, std::move(line)});
    } else {
      auto line = traceElectricLine(solver, field_, params_, job.seed, hooks, &st);
      if (!st.cancelled) e = std::make_shared<const KeyedElectricLine>(KeyedElectricLine{job.key, std::move(line)});
    }

    {
      std::lock_guard<std::mutex> lk(m_);
      --inFlight_;
      if (e) {
        allE_.push_back(e);
        pendingE_.push_back(std::move(e));
      }
      if (p) {
        allP_.push_back(p);
        pendingP_.push_back(std::move(p));
      }
      if (!st.cancelled) ++jobsCompleted_;
      if (queue_.empty() && inFlight_ == 0) cv_.notify_all();  // wake idle workers so they can exit
    }
  }
}

void ComputeSession::finish() {
  // Runs on the last worker thread, outside workerMain's try/catch: nothing may escape (std::terminate), and done_ must
  // ALWAYS be published or wait() blocks forever. Every throwing step (copies, sort, reserve, finalize) is therefore guarded.
  FieldResult r;
  bool haveR = false;
  try {
    std::vector<std::shared_ptr<const KeyedElectricLine>> e;
    std::vector<std::shared_ptr<const KeyedPotentialLine>> p;
    bool proceed = false;
    {
      std::lock_guard<std::mutex> lk(m_);
      if (!stop_.load() && !failed_) {
        e = std::move(allE_);  // all workers are done: allE_/allP_ have no further users, so hand them over without copying
        p = std::move(allP_);
        proceed = true;
      }
    }
    // WR-02: cancel() is honoured between every finalization stage and per line inside finalize(), so the destructor's
    // join is not held up for the whole finalize time. A stop at any point leaves haveR false -> published as cancelled.
    if (proceed) {
      // D-04: deterministic order regardless of which worker finished first
      std::sort(e.begin(), e.end(), [](const auto& a, const auto& b) { return a->key < b->key; });
      std::sort(p.begin(), p.end(), [](const auto& a, const auto& b) { return a->key < b->key; });
      std::vector<FieldLine> rawE;
      std::vector<PotentialLine> rawP;
      rawE.reserve(e.size());
      rawP.reserve(p.size());
      for (const auto& x : e) {
        if (stop_.load(std::memory_order_relaxed)) break;
        rawE.push_back(x->line);  // lines are shared immutable objects (UI may still hold them): copy
      }
      for (const auto& x : p) {
        if (stop_.load(std::memory_order_relaxed)) break;
        rawP.push_back(x->line);
      }
      e.clear();
      p.clear();

      if (!stop_.load()) {
        r = ef::finalize(std::move(rawE), std::move(rawP), field_, params_, &stop_);
        haveR = !stop_.load();  // a stop during finalize() leaves a partial result: discard it
      }
    }
  } catch (const std::exception& ex) {
    fail(ex.what());
  } catch (...) {
    fail("unknown exception while finalizing the result");
  }

  {
    std::lock_guard<std::mutex> lk(m_);
    if (failed_) {
      cancelled_ = false;
    } else if (haveR && !stop_.load()) {  // WR-11: final stop check in the publication block; a cancel() that has returned wins
      result_ = std::move(r);
      haveResult_ = true;
    } else {
      cancelled_ = true;  // stop was requested before or during finalization: no result
    }
    done_ = true;
  }
  doneCv_.notify_all();
}

SessionStatus ComputeSession::statusLocked() const {
  SessionStatus s;
  s.done = done_;
  s.cancelled = cancelled_;
  s.failed = failed_;
  s.error = error_;
  s.jobsCompleted = jobsCompleted_;
  s.jobsKnown = jobsKnown_;
  return s;
}

SessionStatus ComputeSession::poll(PollBatch& out) {
  std::lock_guard<std::mutex> lk(m_);
  // drain-and-swap: O(1) under the lock when the caller's vectors are empty
  if (out.electric.empty()) out.electric.swap(pendingE_);
  else { out.electric.insert(out.electric.end(), pendingE_.begin(), pendingE_.end()); pendingE_.clear(); }
  if (out.potential.empty()) out.potential.swap(pendingP_);
  else { out.potential.insert(out.potential.end(), pendingP_.begin(), pendingP_.end()); pendingP_.clear(); }
  return statusLocked();
}

SessionStatus ComputeSession::wait() {
  std::unique_lock<std::mutex> lk(m_);
  doneCv_.wait(lk, [this] { return done_; });
  return statusLocked();
}

bool ComputeSession::takeResult(FieldResult& out) {
  std::lock_guard<std::mutex> lk(m_);
  if (!done_ || !haveResult_) return false;
  out = std::move(result_);
  haveResult_ = false;
  return true;
}

}  // namespace ef
