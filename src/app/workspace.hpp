#pragma once
// GUI-free tab model (Phase 3). Owns one compute session per open tab and the cached FieldScene.
// Std-only (same strict flags as the core): unit-testable with doctest, no SDL / ImGui. All calls on the UI thread.
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ef/params.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"

namespace efapp {

// Tab::preset value for tabs that do not come from the built-in registry (user configurations, the empty
// configuration). Everything downstream keys off the title, so the value is inert.
inline constexpr std::size_t kNoPreset = std::numeric_limits<std::size_t>::max();

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
  std::size_t preset = 0;    // index into ef::presets(); kNoPreset for user configurations
  unsigned number = 0;       // per-preset (or per-name) counter, starts at 1, never reused after a close
  std::string title;         // "<Display Name> <N>"
  ef::ChargeSystem field;
  ef::ComputeParams params;
  TabState state = TabState::Computing;
  std::size_t linesReceived = 0;  // cumulative field lines seen through poll()
  int expectedLines = 0;          // ef::expectedLineCount
  std::string error;              // Failed only
  ef::FieldScene scene;           // charges from the start; lines after Done (world units: never rebuilt on resize / scale change)
  std::unique_ptr<ISession> session;  // null once Done/Failed
  int selected = -1;              // editor (v2): index into field.charges, -1 = nothing selected
  bool stale = false;             // editor (v2): charges edited after the drawn lines were computed
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
  // Opens a tab from arbitrary charges (a user configuration, v2 Phase 1). The name is trimmed; the title is
  // "<name> <N>" with a per-name counter that is never reused after a close.
  unsigned openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges);
  // The "Empty Configuration" entry: a single +1 charge at the origin, immediately computable — the Phase 2
  // editor's starting point.
  unsigned openEmpty();
  // Charge editor (v2 CALC-01): restarts a Done/Failed tab from its CURRENT charges and params (the same
  // factory and Failed-tab paths as open()). False for unknown serials and tabs that are still computing.
  bool calculate(unsigned serial);
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
  // Shared tail of open/openUser/openEmpty: builds the tab, starts the session (Failed tab on error), pushes it.
  unsigned spawn(std::size_t presetIndex, unsigned number, const std::string& title, ef::ChargeSystem&& field);

  ef::ComputeParams defaults_;
  SessionFactory factory_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  std::array<unsigned, ef::kPresetCount> counters_{};
  std::map<std::string, unsigned> userCounters_;  // per user-configuration name (exact spelling)
  unsigned nextSerial_ = 1;
  unsigned opened_ = 0, closed_ = 0;
};

}  // namespace efapp
