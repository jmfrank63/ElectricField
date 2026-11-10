#pragma once
// MCP protocol layer of ef-mcp, independent of the tools' physics and of real stdio (streams are parameters).
//
// Dual-era per the 2026-07-28 specification: a request carrying the per-request `_meta` is served statelessly under
// that revision, and an `initialize` request opens a legacy session (2025-11-25 and earlier) for the process.
#include <atomic>
#include <iosfwd>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "mcp/tools.hpp"

namespace efmcp {

class Server {
 public:
  Server(std::string version, PngEncoder encodePng);

  // One newline-free JSON-RPC message in, the serialized response (without newline) out; nullopt when none is due.
  // `cancel` is forwarded to a running tool.
  std::optional<std::string> handleLine(const std::string& line, const std::atomic<bool>* cancel = nullptr);

 private:
  std::optional<nlohmann::json> handleMessage(const nlohmann::json& message, const std::atomic<bool>* cancel);
  std::optional<nlohmann::json> handleBatch(const nlohmann::json& batch, const std::atomic<bool>* cancel);
  nlohmann::json initializeResult(const std::string& requestedVersion);
  nlohmann::json modernResult(nlohmann::json result, bool cacheable) const;

  std::string version_;
  PngEncoder encodePng_;
  std::optional<std::string> legacyVersion_;  // set once a client has initialized
};

// Serves newline-delimited messages until `in` ends. Stdin keeps being read while a tool runs, so that
// notifications/cancelled takes effect; a cancelled request gets no response. After end of input, work still in
// progress gets a short grace period (so `echo ... | ef-mcp` answers) and is then cancelled. Returns the exit code.
int serve(std::istream& in, std::ostream& out, Server& server);

}  // namespace efmcp
