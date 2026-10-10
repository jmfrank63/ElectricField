#include <chrono>
#include <cstdio>
#include <thread>
#include "app/workspace.hpp"
#include "ef/presets.hpp"
int main() {
  for (std::size_t i = 0; i < ef::kPresetCount; ++i) {
    efapp::Workspace ws{ef::ComputeParams{}};
    const unsigned s = ws.open(i);
    while (ws.find(s)->state == efapp::TabState::Computing) { ws.pollAll(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    const auto* t = ws.find(s);
    std::printf("%-38s received=%zu expected=%d scene.electric=%zu scene.potential=%zu\n", t->title.c_str(), t->linesReceived, t->expectedLines, t->scene.electric.size(), t->scene.potential.size());
  }
}
