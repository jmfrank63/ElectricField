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
#include "ef/grid.hpp"

namespace efapp {

using GridSettings = ef::GridSettings;  // v2.4: the type moved into the core (renderScene draws it)

// Continuous calculation (v2.2 CALC-03): after an edit, recompute once this many seconds have passed
// without a further edit (a drag arms the deadline on every frame, so one recompute fires when it settles).
inline constexpr double kAutoRecalcDelay = 0.35;

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
  std::string sourceConfig;  // the configuration this tab came from ("" = unsaved) - Save Configuration writes to the tab's CURRENT name and uses this field as the provenance to rename that entry in place (see the configs.hpp saveIntoUserConfig contract)
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
  bool dirty = false;             // v2.2: charges edited since the tab was opened / last saved to a collection
  // View state (v2.2 zoom/pan): device px = centre-anchored transform, pixelsPerUnit *= zoom. View changes
  // are never stale edits (they do not touch the computation).
  double zoom = 1.0;              // 1 = 100 px per world unit (at k = 1); clamped to [0.05, 40], and zoom-in is
                                  // further capped by the canvas's zoomCeiling (v2.6: the adapted-calculation limit)
  ef::Vec2 centre;                // world point shown at the canvas centre
  // Continuous calculation (v2.2 CALC-03): when autoRecalc is on and the tab is stale, the GUI calls
  // autoRecalcTick(now) every frame and the tab recomputes once `pendingRecalcAt` (now + delay) passes.
  bool autoRecalc = false;
  double pendingRecalcAt = -1.0;  // absolute deadline in seconds; -1 = not armed
  // v2.6 phase 2: zoom-settle recalculation. drawCanvas arms pendingViewRecalcAt = -2 when a Done tab's
  // view scale differs from the armed one (armedViewPpu starts 0: the first frame never arms);
  // Workspace::viewRecalcTick converts the marker into a deadline (the same protocol as pendingRecalcAt)
  // and recalculates with params.viewScale = viewPpu. A view change is not an edit: no stale/dirty/undo.
  double viewPpu = 0;                 // the ppu the pending view recalculation will run with
  double armedViewPpu = 0;            // the ppu seen on the previous Done frame
  double pendingViewRecalcAt = -1.0;  // absolute deadline; -1 = not armed, -2 = arm at the next tick
  // v2.3 EDIT-11/12: the charges of the last (re)computation (Esc reverts here) and the per-tab history.
  ef::ChargeSystem computed;
  struct HistoryEntry {
    ef::ChargeSystem field;
    ef::Method method;
  };
  std::vector<HistoryEntry> undo, redo;
  // v2.5 (261012-h24): an open mouse-drag transaction. dragIndex = the dragged charge while the canvas
  // drag runs, -1 = none (same sentinel style as selected / pendingRecalcAt). dragPre is the pre-drag
  // snapshot beginDrag captured — valid only while dragIndex >= 0 — and becomes ONE undo entry at
  // commitDrag, so a whole physical drag coalesces into a single history entry.
  int dragIndex = -1;
  HistoryEntry dragPre;
  // v2.3 GRID: snap grid settings (independently active and visible; Cartesian or polar; own origin).
  GridSettings grid;
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
  // "<name> <N>" with a per-name counter that is never reused after a close. The 3-arg form stores the
  // configuration the tab came from (v2.5 CFG-04); openEmpty passes "" (an unsaved tab has no source).
  unsigned openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges);
  unsigned openUser(const std::string& name, const std::vector<ef::PresetCharge>& charges, const std::string& source);
  // The "Empty Configuration" entry: a single +1 charge at the origin, immediately computable — the Phase 2
  // editor's starting point.
  unsigned openEmpty();
  // Charge editor (v2 CALC-01): restarts a Done/Failed tab from its CURRENT charges and params (the same
  // factory and Failed-tab paths as open()). False for unknown serials and tabs that are still computing.
  bool calculate(unsigned serial);
  // Renames a tab to the exact trimmed name (v2.1 TAB-01). False for unknown serials and empty names.
  bool rename(unsigned serial, const std::string& name);
  // Restores a saved session tab (v2.1 SES-01) with its EXACT title and params. Bumps the matching counter
  // so later opens never duplicate a restored title. A non-empty `source` (h22) overrides the title-derived
  // configuration identity so Save Configuration still renames the tab's entry in place after a restart;
  // empty keeps the legacy derivation.
  unsigned openRestored(const std::string& title, std::size_t presetIndex, const ef::ComputeParams& params,
                        const std::vector<ef::PresetCharge>& charges, bool autoRecalc = false, const GridSettings& grid = GridSettings{},
                        const std::string& source = std::string());
  // Continuous calculation (v2.2 CALC-03): recalculates every tab that has autoRecalc on, is stale, and
  // whose pending deadline has passed. Called every frame by the GUI with the current time in seconds.
  void autoRecalcTick(double nowSeconds);
  // v2.6 phase 2: zoom-settle recalculation. Converts every armed pendingViewRecalcAt = -2 marker into a
  // deadline (now + kAutoRecalcDelay) and fires calculate() with params.viewScale = viewPpu once due.
  // Never sets stale/dirty and records no undo: a view change is not an edit. Called every frame by the GUI.
  void viewRecalcTick(double nowSeconds);
  // v2.2: true when at least one open tab has charge edits that are not stored in any collection file.
  bool anyDirty() const;
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
  // The common body of open/openUser/openEmpty: builds the tab, starts the session (Failed tab on error), pushes it.
  unsigned spawn(std::size_t presetIndex, unsigned number, const std::string& title, ef::ChargeSystem&& field,
                 const std::string& sourceConfig = std::string());

  ef::ComputeParams defaults_;
  SessionFactory factory_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  std::array<unsigned, ef::kPresetCount> counters_{};
  std::map<std::string, unsigned> userCounters_;  // per user-configuration name (exact spelling)
  unsigned nextSerial_ = 1;
  unsigned opened_ = 0, closed_ = 0;
};

}  // namespace efapp
