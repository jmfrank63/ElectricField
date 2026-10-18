#pragma once
// Pure, std-only UI decisions (Phase 3 GUI shell): strings, counters and metrics that the UI-SPEC fixes.
// Unit-tested with doctest; the ImGui layer only calls these.
#include <cstddef>
#include <string>
#include <string_view>

#include "app/workspace.hpp"
#include "ef/scene.hpp"

namespace efapp {

// UI-SPEC "Tab bar": the busy glyph cycles | / - \ at 8 steps per second.
char busyGlyph(double seconds);
// "<glyph> <title>###ef<serial>" while computing, "! <title>###ef<serial>" when failed, "<title>###ef<serial>" when done.
std::string tabLabel(const Tab& t, double seconds);
// "Computing... <n> / <m> lines"; n is clamped to m (n may exceed m: sink lines are counted by poll() but not by expectedLineCount).
std::string overlayText(std::size_t linesReceived, int expectedLines);
// "Electric Field - <collection>" or "Electric Field - <collection> - <tab>" (v2.3 TITLE-01; UTF-8 em dash,
// OS-rendered title).
std::string windowTitle(const Tab* active, const std::string& collection);
// "Version <semver> (<hash>)", "(unknown)" when the hash is empty.
std::string versionLine(std::string_view semver, std::string_view hash);
// "Ctrl+N" / "Cmd+N" for the menu shortcut column (ImGui::GetKeyChordName always prints "Ctrl+" even on macOS).
std::string shortcutLabel(const char* keyName, bool mac);
// session error text or "Unknown error"
std::string failureDetail(const std::string& error);
// The tab title without the trailing " <N>" counter ("Dipole 1" -> "Dipole"); prefills Save As (v2).
std::string baseTitle(const std::string& title);

// v2.6 (review): the charge editor's Size-field text parsed into the size to apply, clamped to
// [0, 120] (0 = inherit the default), or -1 when the text is transient or not a number ("-", "",
// "60x") - nothing applies then. The result is always acceptable to editor::setChargeSize except
// the (0, 2) open range it refuses (e.g. "1"), which stays "not applied" by the core's contract.
double chargeSizeFromText(const char* text);

// Display-scale k: clamp to [0.75, 4.0]; non-finite or <= 0 falls back to 1.0.
float clampScale(float k);

struct IntSize {
  int w = 0, h = 0;
};
// Default window: (1024 x 824) * k, clamped to 90% of the usable bounds (no lower bound here; the minimum is minimumWindowSize).
IntSize defaultWindowSize(float k, int usableW, int usableH);
IntSize minimumWindowSize(float k);

// Canvas view for the shared renderScene path: pixelsPerUnit = 100 * k, size = floor(avail), at least 1 x 1.
ef::View canvasView(float availW, float availH, float k);
// The draw style scaled by k (thickness and radius); colours unchanged.
ef::DrawStyle scaledStyle(float k);
// Scales the given (user, Phase 4) draw style by k; colours unchanged.
ef::DrawStyle scaledStyle(const ef::DrawStyle& base, float k);

// v2 editor: canvas (device px) <-> world conversions. The world origin sits at the canvas centre, y up;
// (originX, originY) is the canvas top-left corner in the same coordinate space as screenX/screenY.
ef::Vec2 worldFromCanvas(const ef::View& v, double originX, double originY, double screenX, double screenY);
ef::Vec2 canvasFromWorld(const ef::View& v, double originX, double originY, const ef::Vec2& world);

// UI-SPEC "Main loop (D-12)": idle => block in SDL_WaitEventTimeout(kIdleWaitMs); any session computing => poll + render every frame;
// at least kSettleFrames rendered frames after every input event or state change (ImGui needs them for hover/focus/popup
// settling); a modal opening or closing arms kModalFadeFrames (ImGui fades the dim layer in over ~0.2 s).
class FramePacer {
 public:
  static constexpr int kSettleFrames = 3;
  static constexpr int kModalFadeFrames = 12;
  static constexpr int kIdleWaitMs = 250;
  void notifyActivity(int frames = kSettleFrames) { if (frames > settle_) settle_ = frames; }
  void frameRendered() { if (settle_ > 0) --settle_; }
  bool shouldBlock(bool anyComputing) const { return !anyComputing && settle_ == 0; }
  int pendingFrames() const { return settle_; }

 private:
  int settle_ = 0;
};

}  // namespace efapp
