// Hello-window: SDL3 + Dear ImGui (sdlrenderer3). `--smoke` = headless one-frame start-up check for CI.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // Windows GUI subsystem: must be in the TU that defines main

#include <cstdio>
#include <cstring>

#include "ef/presets.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

int main(int argc, char** argv) {
  bool smoke = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--smoke") == 0) smoke = true;

  if (smoke) {  // do not depend on environment variables for the headless path
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("Electric Field", 960, 640, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }
  std::printf("video=%s renderer=%s presets=%zu\n", SDL_GetCurrentVideoDriver(), SDL_GetRendererName(renderer), ef::presets().size());

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  if (smoke) ImGui::GetIO().IniFilename = nullptr;  // IN-03: the headless check must not write imgui.ini into the cwd
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  bool running = true;
  int frames = 0;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL3_ProcessEvent(&e);
      if (e.type == SDL_EVENT_QUIT) running = false;
    }
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Electric Field");
    ImGui::Text("Hello from SDL3 + Dear ImGui");
    ImGui::End();
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    ++frames;
    if (smoke && frames >= 2) running = false;  // 2nd frame: first frame builds the font atlas
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::printf("smoke ok frames=%d\n", frames);
  return 0;
}
