#pragma once
// Pure, std-only UI decisions (Phase 3 research prototype): strings, counters and metrics that the UI-SPEC fixes.
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
// "Electric Field" or "Electric Field \xE2\x80\x94 <title>" (UTF-8 em dash, OS-rendered title).
std::string windowTitle(const Tab* active);
// "Version <semver> (<hash>)", "(unknown)" when the hash is empty.
std::string versionLine(std::string_view semver, std::string_view hash);
// "Ctrl+N" / "Cmd+N" for the menu shortcut column (ImGui::GetKeyChordName always prints "Ctrl+" even on macOS).
std::string shortcutLabel(const char* keyName, bool mac);
// session error text or "Unknown error"
std::string failureDetail(const std::string& error);

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
