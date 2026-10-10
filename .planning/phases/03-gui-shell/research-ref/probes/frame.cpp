// Probe: per-frame CPU cost of redrawing a finished scene every frame (no draw-list caching), 1024x768, k=1 and k=2.
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include "ef/presets.hpp"
#include "ef/session.hpp"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "render/imgui_painter.hpp"
using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
int main() {
  const std::size_t idx[] = {0, 4, 5, 8};
  for (int k : {1, 2}) {
    const int W = 1024 * k, H = 768 * k;
    SDL_Surface* surf = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(static_cast<float>(W), static_cast<float>(H));
    ImGui::GetStyle().CurveTessellationTol = 0.3f;
    ImGui::GetStyle().AntiAliasedLinesUseTex = false;
    ImGui_ImplSDLRenderer3_Init(r);
    for (std::size_t i : idx) {
      const auto& p = ef::presets()[i];
      const ef::ChargeSystem f = ef::makeSystem(p);
      ef::ComputeSession s(f, ef::ComputeParams{});
      s.wait();
      ef::FieldResult res;
      s.takeResult(res);
      const ef::FieldScene scene = ef::buildScene(f, res);
      ef::View v;
      v.widthPx = W; v.heightPx = H; v.pixelsPerUnit = 100.0 * k;
      ef::DrawStyle st;
      double tTess = 0, tRaster = 0; int verts = 0, idxs = 0;
      const int N = 10;
      for (int n = 0; n < N + 2; ++n) {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();
        const auto t0 = clk::now();
        { efrender::ImGuiPainter painter(ImGui::GetBackgroundDrawList(), ImVec2(0, 0)); ef::renderScene(painter, scene, st, v); }
        ImGui::Render();
        const auto t1 = clk::now();
        SDL_RenderClear(r);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), r);
        const auto t2 = clk::now();
        if (n >= 2) { tTess += ms(t0, t1); tRaster += ms(t1, t2); }
        verts = ImGui::GetDrawData()->TotalVtxCount; idxs = ImGui::GetDrawData()->TotalIdxCount;
      }
      std::printf("k=%d %-28s paths=%zu+%zu  tessellate+Render: %6.2f ms   software raster: %6.1f ms   vtx=%d idx=%d\n", k, std::string(p.displayName).c_str(),
                  scene.electric.size(), scene.potential.size(), tTess / N, tRaster / N, verts, idxs);
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(surf);
  }
  return 0;
}
