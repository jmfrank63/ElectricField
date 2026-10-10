#pragma once
// RAII compute session (D-01..D-04): owns a condition-variable job queue and numThreads workers, joins them in the
// destructor. Replaces FieldLinesCalculator / CalcThread / ComputationThread (detached threads, delete this,
// finishedThreads restart hack, theApp.calculators hand-off).
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ef/finalizer.hpp"

namespace ef {

// Deterministic identity of a job (D-04): the field line of seed i has key {i, 0}; the equipotential jobs spawned while
// tracing that (single-threaded, sequential) line get {i, 1}, {i, 2}, ... in spawn order.
struct JobKey {
  std::size_t parent = 0;
  std::size_t child = 0;
  friend bool operator<(const JobKey& a, const JobKey& b) { return a.parent != b.parent ? a.parent < b.parent : a.child < b.child; }
};

struct KeyedElectricLine {
  JobKey key;
  FieldLine line;
};
struct KeyedPotentialLine {
  JobKey key;
  PotentialLine line;
};

struct SessionStatus {
  bool done = false;  // finished (result available), cancelled or failed
  bool cancelled = false;
  bool failed = false;
  std::string error;
  std::size_t jobsCompleted = 0;
  std::size_t jobsKnown = 0;  // seeds + equipotential jobs posted so far (grows while running)
};

// Raw lines completed since the previous poll(). Immutable shared objects: the UI thread never touches anything a worker can write.
struct PollBatch {
  std::vector<std::shared_ptr<const KeyedElectricLine>> electric;
  std::vector<std::shared_ptr<const KeyedPotentialLine>> potential;
};

class ComputeSession {
 public:
  ComputeSession(ChargeSystem field, ComputeParams params);  // plans seeds, starts max(1, numThreads) workers, returns at once
  ~ComputeSession();                                         // cancel() + join(); bounded by the cancel stride, also while finalizing
  ComputeSession(const ComputeSession&) = delete;
  ComputeSession& operator=(const ComputeSession&) = delete;

  // Non-blocking, idempotent. Honoured by the tracers (cancel stride) AND between/inside the finalization stages. The stop
  // flag is re-checked in the publication block (under the session lock), so a cancel() that has returned before the result
  // is published wins: the session ends cancelled with no result. cancel() does not take that lock to set the flag, so a
  // cancel() racing the publication itself may land on either side; status and takeResult() always agree. A cancel() after
  // done is a no-op (the result stays available).
  void cancel();

  // Drain-and-swap (D-03): appends lines completed since the last call to `out`, returns the current status. Any thread.
  SessionStatus poll(PollBatch& out);

  // Blocks until done / cancelled / failed (CLI, tests).
  SessionStatus wait();

  // After a successful wait()/poll(): moves the finalized result out (true exactly once).
  bool takeResult(FieldResult& out);

 private:
  struct Job {
    Seed seed;
    bool isEquipotential = false;
    double potentialLabel = 0;  // job.old_potential
    Vec2 start;                 // equipotential jobs: the point of the parent line at posting time
    JobKey key;
  };
  struct JobHooks;
  friend struct JobHooks;

  void workerMain();
  template <typename Solver>
  void workerLoop(Solver& solver);
  void fail(const char* what);
  void finish();  // last worker: sort by job key, finalize, publish
  SessionStatus statusLocked() const;

  const ChargeSystem field_;
  const ComputeParams params_;

  mutable std::mutex m_;
  std::condition_variable cv_;      // workers: job available / stop / all done
  std::condition_variable doneCv_;  // wait()
  std::deque<Job> queue_;
  std::size_t inFlight_ = 0;
  std::size_t jobsKnown_ = 0, jobsCompleted_ = 0;
  std::vector<std::shared_ptr<const KeyedElectricLine>> allE_, pendingE_;
  std::vector<std::shared_ptr<const KeyedPotentialLine>> allP_, pendingP_;
  bool done_ = false, cancelled_ = false, failed_ = false, haveResult_ = false;
  std::string error_;
  FieldResult result_;

  std::atomic<bool> stop_{false};
  std::atomic<unsigned> activeWorkers_{0};
  std::vector<std::thread> workers_;
};

}  // namespace ef
