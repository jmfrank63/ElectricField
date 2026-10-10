// Probe: Windows DPI awareness after SDL_Init(SDL_INIT_VIDEO) with the real video driver. Creates NO window.
#include <SDL3/SDL.h>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
int main() {
  if (!SDL_Init(SDL_INIT_VIDEO)) { std::printf("SDL_Init failed: %s\n", SDL_GetError()); return 1; }
  std::printf("video driver: %s\n", SDL_GetCurrentVideoDriver());
#ifdef _WIN32
  using GetCtx = DPI_AWARENESS_CONTEXT(WINAPI*)();
  using GetAw = DPI_AWARENESS(WINAPI*)(DPI_AWARENESS_CONTEXT);
  using AreEq = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
  HMODULE u = GetModuleHandleA("user32.dll");
  auto getCtx = reinterpret_cast<GetCtx>(reinterpret_cast<void*>(GetProcAddress(u, "GetThreadDpiAwarenessContext")));
  auto getAw = reinterpret_cast<GetAw>(reinterpret_cast<void*>(GetProcAddress(u, "GetAwarenessFromDpiAwarenessContext")));
  auto areEq = reinterpret_cast<AreEq>(reinterpret_cast<void*>(GetProcAddress(u, "AreDpiAwarenessContextsEqual")));
  if (getCtx && getAw && areEq) {
    DPI_AWARENESS_CONTEXT c = getCtx();
    std::printf("awareness=%d (0 unaware, 1 system, 2 per-monitor) perMonitorV2=%d\n", static_cast<int>(getAw(c)),
                areEq(c, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ? 1 : 0);
  }
#endif
  int n = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&n);
  for (int i = 0; i < n; ++i) {
    SDL_Rect b{}, u2{};
    SDL_GetDisplayBounds(ids[i], &b);
    SDL_GetDisplayUsableBounds(ids[i], &u2);
    std::printf("display %d id=%u name=%s content_scale=%.3f bounds=%dx%d+%d+%d usable=%dx%d+%d+%d\n", i, ids[i], SDL_GetDisplayName(ids[i]),
                static_cast<double>(SDL_GetDisplayContentScale(ids[i])), b.w, b.h, b.x, b.y, u2.w, u2.h, u2.x, u2.y);
  }
  std::printf("primary=%u\n", SDL_GetPrimaryDisplay());
  SDL_free(ids);
  SDL_Quit();
  return 0;
}
