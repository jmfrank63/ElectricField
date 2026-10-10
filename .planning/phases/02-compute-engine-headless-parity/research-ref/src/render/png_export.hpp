#pragma once
// Headless PNG export: ImGui ImDrawList rendered by SDL's software renderer into an offscreen SDL_Surface, saved with
// SDL_SavePNG. Needs NO window, NO display and NO GPU (no SDL_Init required for SDL_CreateSoftwareRenderer).
#include <string>

#include "ef/scene.hpp"

namespace efrender {

// Returns an empty string on success, otherwise an error message. Not thread-safe (uses the global ImGui context).
std::string renderPng(const ef::FieldScene& scene, const ef::DrawStyle& style, const ef::View& view, const char* path);

}  // namespace efrender
