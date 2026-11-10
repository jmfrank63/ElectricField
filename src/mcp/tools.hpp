#pragma once
// The MCP tools of ef-mcp: thin, strictly validated wrappers over the physics core. No SDL/ImGui here (PNG encoding
// is injected), so everything is unit-testable under the same strict flags as the core.
#include <atomic>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

#include "ef/scene.hpp"

namespace efmcp {

// Encodes a scene as PNG into `png`; returns "" on success, otherwise an error message. Empty function = a build
// without PNG support (-Dgui=false). Always called on one thread at a time (the export uses the global ImGui context).
using PngEncoder = std::function<std::string(const ef::FieldScene&, const ef::DrawStyle&, const ef::View&, std::string& png)>;

// The `tools` array of a tools/list result, in a fixed order.
nlohmann::json toolDefinitions(bool pngAvailable);

bool isKnownTool(const std::string& name);

// Runs a known tool and returns a CallToolResult (content / structuredContent / isError). Bad arguments and failed or
// cancelled computations are reported IN the result (isError), so that a model can correct itself. `cancel`, when it
// becomes true, stops a running computation promptly.
nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments, const PngEncoder& encodePng, const std::atomic<bool>* cancel);

}  // namespace efmcp
