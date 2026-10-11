#pragma once
// GUI-free tab model (Phase 3). Owns one compute session per open tab and the cached FieldScene.
// Std-only (same strict flags as the core): unit-testable with doctest, no SDL / ImGui. All calls on the UI thread.
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ef/params.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"

namespace efapp {

// The slice of ef::ComputeSession the UI needs. A seam so the state machine (failed / cancelled / no-result paths, which a
// real session only reaches under allocation failure) is testable with scripted fakes and so smoke can show the failed tab.
struct ISession {
  virtual ~ISession() = default;
  virtual void cancel() = 0;
  virtual ef::SessionStatus poll(ef::PollBatch& out) = 0;
  virtual bool takeResult(ef::FieldResult& out) = 0;
};
using SessionFactory = std::function<std::unique_ptr<ISession>(const ef::ChargeSystem&, const ef::ComputeParams&)>;
SessionFactory defaultSessionFactory();  // wraps ef::ComputeSession

enum class TabState { Computing, Done, Failed };

struct Tab {
  unsigned serial = 0;       // stable ImGui id seed ("###ef<serial>"), never reused
  std::size_t preset = 0;    // index into ef::presets()
  unsigned number = 0;       // per-preset counter, starts at 1, never reused after a close
  std::string title;         // "<Display Name> <N>"
  ef::ChargeSystem field;
  ef::ComputeParams params;
  TabState state = TabState::Computing;
  std::size_t linesReceived = 0;  // cumulative field lines seen through poll()
  int expectedLines = 0;          // ef::expectedLineCount
  std::string error;              // Failed only
  ef::FieldScene scene;           // charges from the start; lines after Done (world units: never rebuilt on resize / scale change)
  std::unique_ptr<ISession> session;  // null once Done/Failed
};

class Workspace {
 public:
  explicit Workspace(ef::ComputeParams defaults = ef::ComputeParams{}, SessionFactory factory = defaultSessionFactory())
      : defaults_(defaults), factory_(std::move(factory)) {}
  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;
  ~Workspace() { closeAll(); }

  // Opens a tab and starts its session. Never throws for a session that cannot start: that yields a Failed tab.
  unsigned open(std::size_t presetIndex);
  // Options dialog (Phase 4): new tabs open with these params; tabs already open keep the params they were opened with.
  void setDefaults(const ef::ComputeParams& p) { defaults_ = p; }
  // Cancels and destroys the session (RAII join) and removes the tab. False if the serial is unknown.
  bool close(unsigned serial);
  // One poll() per computing tab; Done -> takeResult + buildScene once; Failed -> error text.
  void pollAll();
  void cancelAll() noexcept;  // quit path: request cancel on every session first so the joins overlap
  void closeAll();            // cancelAll() then destroy
  bool anyComputing() const;
  void setFactory(SessionFactory f) { factory_ = std::move(f); }  // smoke / tests only

  Tab* find(unsigned serial);
  const std::vector<std::unique_ptr<Tab>>& tabs() const { return tabs_; }
  unsigned opened() const { return opened_; }
  unsigned closed() const { return closed_; }

 private:
  void poll(Tab& t);

  ef::ComputeParams defaults_;
  SessionFactory factory_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  std::array<unsigned, ef::kPresetCount> counters_{};
  unsigned nextSerial_ = 1;
  unsigned opened_ = 0, closed_ = 0;
};

}  // namespace efapp
