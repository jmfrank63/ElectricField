#include "mcp/server.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <istream>
#include <mutex>
#include <ostream>
#include <thread>
#include <utility>

namespace efmcp {

using nlohmann::json;

namespace {

constexpr const char* kModernVersion = "2026-07-28";
constexpr const char* kLatestLegacyVersion = "2025-11-25";
constexpr const char* kBatchingVersion = "2025-03-26";  // the only revision with JSON-RPC batches
constexpr std::array<const char*, 4> kLegacyVersions{kLatestLegacyVersion, "2025-06-18", kBatchingVersion, "2024-11-05"};
constexpr const char* kVersionKey = "io.modelcontextprotocol/protocolVersion";
constexpr const char* kCapabilitiesKey = "io.modelcontextprotocol/clientCapabilities";
constexpr const char* kServerInfoKey = "io.modelcontextprotocol/serverInfo";
constexpr const char* kServerName = "electric-field";
constexpr long long kCacheTtlMs = 3600000;  // the tool set is fixed for the lifetime of a binary

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kInternalError = -32603;
constexpr int kUnsupportedProtocolVersion = -32022;

constexpr const char* kInstructions =
    "Simulates electric field lines and equipotentials of 2D point-charge configurations. Use list_presets and "
    "list_methods to see the options, evaluate_field for values at points, compute_field_lines for line counts or "
    "geometry, and render_field for an image.";

constexpr std::chrono::seconds kShutdownGrace{5};

std::string serialize(const json& value) { return value.dump(-1, ' ', false, json::error_handler_t::replace); }

// No id member when the id could not be read (MCP schema: JSONRPCErrorResponse.id is optional).
json errorResponse(const json* id, int code, const std::string& message, json data = nullptr) {
  json error = json::object({{"code", code}, {"message", message}});
  if (!data.is_null()) error["data"] = std::move(data);
  json response = json::object({{"jsonrpc", "2.0"}});
  if (id) response["id"] = *id;
  response["error"] = std::move(error);
  return response;
}

json resultResponse(const json& id, json result) { return json::object({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}); }

bool isValidRequestId(const json& id) { return id.is_string() || id.is_number_integer(); }

json serverCapabilities() { return json::object({{"tools", json::object()}}); }

}  // namespace

Server::Server(std::string version, PngEncoder encodePng) : version_(std::move(version)), encodePng_(std::move(encodePng)) {}

json Server::initializeResult(const std::string& requestedVersion) {
  const bool known = std::any_of(kLegacyVersions.begin(), kLegacyVersions.end(), [&](const char* v) { return requestedVersion == v; });
  legacyVersion_ = known ? requestedVersion : std::string(kLatestLegacyVersion);
  return json::object({{"protocolVersion", *legacyVersion_},
                       {"capabilities", serverCapabilities()},
                       {"serverInfo", json::object({{"name", kServerName}, {"version", version_}})},
                       {"instructions", kInstructions}});
}

json Server::modernResult(json result, bool cacheable) const {
  result["resultType"] = "complete";
  result["_meta"] = json::object({{kServerInfoKey, json::object({{"name", kServerName}, {"version", version_}})}});
  if (cacheable) {
    result["ttlMs"] = kCacheTtlMs;
    result["cacheScope"] = "public";
  }
  return result;
}

std::optional<json> Server::handleBatch(const json& batch, const std::atomic<bool>* cancel) {
  if (batch.empty() || legacyVersion_ != std::optional<std::string>(kBatchingVersion)) return errorResponse(nullptr, kInvalidRequest, "JSON-RPC batches are not supported by this protocol version");
  json responses = json::array();
  for (const json& message : batch)
    if (auto response = handleMessage(message, cancel)) responses.push_back(std::move(*response));
  if (responses.empty()) return std::nullopt;
  return responses;
}

std::optional<json> Server::handleMessage(const json& message, const std::atomic<bool>* cancel) {
  if (!message.is_object()) return errorResponse(nullptr, kInvalidRequest, "a message must be a JSON object");
  const auto idIt = message.find("id");
  const auto methodIt = message.find("method");
  if (methodIt == message.end() && idIt != message.end() && (message.contains("result") || message.contains("error"))) return std::nullopt;  // a response: this server never sends requests

  const bool validId = idIt != message.end() && isValidRequestId(*idIt);
  const json* const replyId = validId ? &*idIt : nullptr;
  const auto version = message.find("jsonrpc");
  if (version == message.end() || *version != "2.0" || methodIt == message.end() || !methodIt->is_string()) return errorResponse(replyId, kInvalidRequest, "not a JSON-RPC 2.0 message");
  if (idIt == message.end()) return std::nullopt;  // notifications (initialized, cancelled, ...) need no action here
  if (!validId) return errorResponse(nullptr, kInvalidRequest, "a request id must be a string or an integer");

  const json& id = *idIt;
  const std::string method = methodIt->get<std::string>();
  static const json kNoParams = json::object();
  const auto paramsIt = message.find("params");
  if (paramsIt != message.end() && !paramsIt->is_object()) return errorResponse(&id, kInvalidParams, "params must be an object");
  const json& params = paramsIt != message.end() ? *paramsIt : kNoParams;

  if (method == "initialize") {
    const auto requested = params.find("protocolVersion");
    if (requested == params.end() || !requested->is_string()) return errorResponse(&id, kInvalidParams, "initialize needs a protocolVersion string");
    return resultResponse(id, initializeResult(requested->get<std::string>()));
  }

  const auto metaIt = params.find("_meta");
  const bool hasMeta = metaIt != params.end() && metaIt->is_object() && metaIt->contains(kVersionKey);
  if (hasMeta) {
    const json& requested = metaIt->at(kVersionKey);
    if (!requested.is_string()) return errorResponse(&id, kInvalidParams, std::string(kVersionKey) + " must be a string");
    if (requested != kModernVersion)
      return errorResponse(&id, kUnsupportedProtocolVersion, "Unsupported protocol version", json::object({{"supported", json::array({kModernVersion})}, {"requested", requested}}));
    const auto capabilities = metaIt->find(kCapabilitiesKey);
    if (capabilities == metaIt->end() || !capabilities->is_object()) return errorResponse(&id, kInvalidParams, std::string("_meta needs the object ") + kCapabilitiesKey);
  } else if (!legacyVersion_) {
    if (method == "ping") return resultResponse(id, json::object());
    return errorResponse(&id, kInvalidParams, std::string("the request needs _meta with ") + kVersionKey + " and " + kCapabilitiesKey + ", or send initialize first");
  }

  if (method == "server/discover" && hasMeta) {
    json result = json::object({{"supportedVersions", json::array({kModernVersion})}, {"capabilities", serverCapabilities()}, {"instructions", kInstructions}});
    return resultResponse(id, modernResult(std::move(result), true));
  }
  if (method == "ping" && !hasMeta) return resultResponse(id, json::object());
  if (method == "tools/list") {
    json result = json::object({{"tools", toolDefinitions(static_cast<bool>(encodePng_))}});
    return resultResponse(id, hasMeta ? modernResult(std::move(result), true) : std::move(result));
  }
  if (method == "tools/call") {
    const auto name = params.find("name");
    if (name == params.end() || !name->is_string()) return errorResponse(&id, kInvalidParams, "tools/call needs a tool name");
    if (!isKnownTool(name->get<std::string>())) return errorResponse(&id, kInvalidParams, "Unknown tool: " + name->get<std::string>());
    const auto arguments = params.find("arguments");
    if (arguments != params.end() && !arguments->is_object()) return errorResponse(&id, kInvalidParams, "arguments must be an object");
    json result = callTool(name->get<std::string>(), arguments != params.end() ? *arguments : kNoParams, encodePng_, cancel);
    return resultResponse(id, hasMeta ? modernResult(std::move(result), false) : std::move(result));
  }
  return errorResponse(&id, kMethodNotFound, "Method not found: " + method);
}

std::optional<std::string> Server::handleLine(const std::string& line, const std::atomic<bool>* cancel) {
  const json message = json::parse(line, nullptr, false);
  if (message.is_discarded()) return serialize(errorResponse(nullptr, kParseError, "Parse error"));
  try {
    auto response = message.is_array() ? handleBatch(message, cancel) : handleMessage(message, cancel);
    if (!response) return std::nullopt;
    return serialize(*response);
  } catch (const std::exception& e) {
    const json* id = nullptr;
    if (message.is_object()) {
      const auto it = message.find("id");
      if (it != message.end() && isValidRequestId(*it)) id = &*it;
    }
    return serialize(errorResponse(id, kInternalError, std::string("Internal error: ") + e.what()));
  }
}

namespace {

struct Pending {
  std::string line;
  std::string key;  // serialized request id; "" when the message cannot be cancelled
};

// Classifies a line without acting on it: either a cancellation (its target in `cancelTarget`) or work.
Pending inspect(std::string line, std::string& cancelTarget) {
  Pending pending;
  const json message = json::parse(line, nullptr, false);
  if (message.is_object()) {
    const auto method = message.find("method");
    const auto id = message.find("id");
    if (method != message.end() && *method == "notifications/cancelled" && id == message.end()) {
      const auto params = message.find("params");
      if (params != message.end() && params->is_object()) {
        const auto target = params->find("requestId");
        if (target != params->end() && isValidRequestId(*target)) cancelTarget = target->dump();
      }
    } else if (method != message.end() && id != message.end() && isValidRequestId(*id)) {
      pending.key = id->dump();
    }
  }
  pending.line = std::move(line);
  return pending;
}

}  // namespace

int serve(std::istream& in, std::ostream& out, Server& server) {
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Pending> queue;
  std::string activeKey;
  bool busy = false, finished = false;
  std::atomic<bool> cancel{false};

  std::thread worker([&] {
    for (;;) {
      Pending item;
      {
        std::unique_lock<std::mutex> lock(mutex);
        busy = false;
        changed.notify_all();
        changed.wait(lock, [&] { return !queue.empty() || finished; });
        if (queue.empty()) return;
        item = std::move(queue.front());
        queue.pop_front();
        activeKey = item.key;
        cancel = false;
        busy = true;
      }
      const auto response = server.handleLine(item.line, &cancel);
      std::lock_guard<std::mutex> lock(mutex);
      activeKey.clear();
      if (response && !cancel.load()) {
        out << *response << '\n';
        out.flush();
      }
    }
  });

  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.find_first_not_of(" \t") == std::string::npos) continue;
    std::string cancelTarget;
    Pending pending = inspect(std::move(line), cancelTarget);
    std::lock_guard<std::mutex> lock(mutex);
    if (!cancelTarget.empty()) {
      if (cancelTarget == activeKey) cancel = true;
      queue.erase(std::remove_if(queue.begin(), queue.end(), [&](const Pending& p) { return p.key == cancelTarget; }), queue.end());
    } else {
      queue.push_back(std::move(pending));
      changed.notify_all();
    }
  }

  {
    std::unique_lock<std::mutex> lock(mutex);
    changed.wait_for(lock, kShutdownGrace, [&] { return queue.empty() && !busy; });
    queue.clear();
    cancel = true;
    finished = true;
    changed.notify_all();
  }
  worker.join();
  return 0;
}

}  // namespace efmcp
