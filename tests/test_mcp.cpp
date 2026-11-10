// ef-mcp protocol layer and tools, driven through Server::handleLine (no process, no SDL).
#include "doctest/doctest.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "ef/presets.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"
#include "mcp/server.hpp"

using nlohmann::json;
using efmcp::Server;

namespace {

const char* const kVersion = "io.modelcontextprotocol/protocolVersion";
const char* const kCapabilities = "io.modelcontextprotocol/clientCapabilities";

json modernMeta(const char* version = "2026-07-28") { return json::object({{kVersion, version}, {kCapabilities, json::object()}}); }

json request(const std::string& method, json params = json::object(), int id = 1) {
  return json::object({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}});
}

json send(Server& server, const json& message, const std::atomic<bool>* cancel = nullptr) {
  const auto response = server.handleLine(message.dump(), cancel);
  REQUIRE(response.has_value());
  CHECK(response->find('\n') == std::string::npos);
  return json::parse(*response);
}

json callModern(Server& server, const std::string& tool, json arguments, const std::atomic<bool>* cancel = nullptr) {
  json params = json::object({{"name", tool}, {"arguments", std::move(arguments)}, {"_meta", modernMeta()}});
  const json response = send(server, request("tools/call", std::move(params)), cancel);
  REQUIRE(response.contains("result"));
  return response["result"];
}

std::string errorText(const json& result) {
  REQUIRE(result["isError"] == true);
  return result["content"][0]["text"].get<std::string>();
}

efmcp::PngEncoder fakePng() {
  return [](const ef::FieldScene&, const ef::DrawStyle&, const ef::View&, std::string& png) {
    png = "abcd";
    return std::string();
  };
}

const json kDipole = json::array({json::object({{"charge", 1}, {"x", -2}, {"y", 0}}), json::object({{"charge", -1}, {"x", 2}, {"y", 0}})});

// A fresh, empty directory removed at scope end.
struct TempDir {
  std::filesystem::path path;
  TempDir() {
    path = std::filesystem::temp_directory_path() / ("ef-mcp-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

}  // namespace

TEST_CASE("mcp: server/discover answers modern clients") {
  Server server("9.9.9", nullptr);
  const json r = send(server, request("server/discover", json::object({{"_meta", modernMeta()}})));
  const json& result = r["result"];
  CHECK(r["id"] == 1);
  CHECK(result["resultType"] == "complete");
  CHECK(result["supportedVersions"] == json::array({"2026-07-28"}));
  CHECK(result["capabilities"].contains("tools"));
  CHECK(result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"] == "9.9.9");
  CHECK(result["ttlMs"].get<long long>() > 0);
  CHECK(result["cacheScope"] == "public");
}

TEST_CASE("mcp: per-request metadata is enforced") {
  Server server("1", nullptr);
  SUBCASE("unsupported version names the supported ones") {
    const json r = send(server, request("tools/list", json::object({{"_meta", modernMeta("1900-01-01")}})));
    CHECK(r["error"]["code"] == -32022);
    CHECK(r["error"]["data"]["requested"] == "1900-01-01");
    CHECK(r["error"]["data"]["supported"] == json::array({"2026-07-28"}));
  }
  SUBCASE("missing _meta without a legacy session is invalid params") {
    CHECK(send(server, request("tools/list"))["error"]["code"] == -32602);
  }
  SUBCASE("missing capabilities is invalid params") {
    json meta = json::object({{kVersion, "2026-07-28"}});
    CHECK(send(server, request("tools/list", json::object({{"_meta", meta}})))["error"]["code"] == -32602);
  }
}

TEST_CASE("mcp: initialize opens a legacy session") {
  Server server("1", nullptr);
  SUBCASE("a known version is echoed and tools/list works without _meta") {
    const json init = send(server, request("initialize", json::object({{"protocolVersion", "2025-06-18"}, {"capabilities", json::object()}})));
    CHECK(init["result"]["protocolVersion"] == "2025-06-18");
    CHECK(init["result"]["serverInfo"]["name"] == "electric-field");
    const json list = send(server, request("tools/list"));
    CHECK(list["result"]["tools"].size() == 5);
    CHECK_FALSE(list["result"].contains("resultType"));
  }
  SUBCASE("an unknown version falls back to the latest legacy one") {
    const json init = send(server, request("initialize", json::object({{"protocolVersion", "2099-01-01"}})));
    CHECK(init["result"]["protocolVersion"] == "2025-11-25");
  }
  SUBCASE("ping") {
    send(server, request("initialize", json::object({{"protocolVersion", "2025-11-25"}})));
    CHECK(send(server, request("ping"))["result"].empty());
  }
  SUBCASE("batches only under 2025-03-26") {
    const json batch = json::array({request("ping", json::object(), 7)});
    CHECK(send(server, batch)["error"]["code"] == -32600);
    send(server, request("initialize", json::object({{"protocolVersion", "2025-03-26"}})));
    CHECK(send(server, batch)[0]["id"] == 7);
  }
}

TEST_CASE("mcp: JSON-RPC edge cases") {
  Server server("1", nullptr);
  CHECK(json::parse(*server.handleLine("{nope"))["error"]["code"] == -32700);
  CHECK_FALSE(server.handleLine(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").has_value());
  CHECK(send(server, json::object({{"jsonrpc", "1.0"}, {"id", 1}, {"method", "x"}}))["error"]["code"] == -32600);
  CHECK(send(server, json::object({{"jsonrpc", "2.0"}, {"id", nullptr}, {"method", "x"}}))["error"]["code"] == -32600);
  CHECK(send(server, request("nope", json::object({{"_meta", modernMeta()}})))["error"]["code"] == -32601);
  json unknownTool = json::object({{"name", "nope"}, {"_meta", modernMeta()}});
  CHECK(send(server, request("tools/call", unknownTool))["error"]["code"] == -32602);
}

TEST_CASE("mcp: tools/list reflects PNG availability") {
  const auto formats = [](Server& server) {
    const json r = send(server, request("tools/list", json::object({{"_meta", modernMeta()}})));
    for (const json& tool : r["result"]["tools"])
      if (tool["name"] == "render_field") return tool["inputSchema"]["properties"]["format"]["enum"];
    return json();
  };
  Server without("1", nullptr), with("1", fakePng());
  CHECK(formats(without) == json::array({"svg", "pdf"}));
  CHECK(formats(with) == json::array({"png", "svg", "pdf"}));
}

TEST_CASE("mcp: list_presets and list_methods") {
  Server server("1", nullptr);
  const json presets = callModern(server, "list_presets", json::object());
  REQUIRE(presets["structuredContent"]["presets"].size() == ef::kPresetCount);
  CHECK(presets["structuredContent"]["presets"][0]["slug"] == std::string(ef::presets()[0].slug));
  CHECK(json::parse(presets["content"][0]["text"].get<std::string>()) == presets["structuredContent"]);
  const json methods = callModern(server, "list_methods", json::object());
  CHECK(methods["structuredContent"]["methods"].size() == ef::kMethodCount);
  CHECK(methods["structuredContent"]["default"] == "euler");
}

TEST_CASE("mcp: evaluate_field") {
  Server server("1", nullptr);
  json points = json::array({json::object({{"x", 0}, {"y", 0}}), json::object({{"x", -2}, {"y", 0}})});
  const json result = callModern(server, "evaluate_field", json::object({{"charges", kDipole}, {"points", points}}));
  REQUIRE(result["isError"] == false);
  const json& origin = result["structuredContent"]["values"][0];
  CHECK(origin["singular"] == false);
  CHECK(origin["ex"].get<double>() == doctest::Approx(0.5));  // 2 * q / d^2 with q = 1, d = 2
  CHECK(origin["ey"].get<double>() == doctest::Approx(0.0));
  CHECK(origin["potential"].get<double>() == doctest::Approx(0.0));
  const json& onCharge = result["structuredContent"]["values"][1];
  CHECK(onCharge["singular"] == true);
  CHECK(onCharge["potential"].is_null());
}

TEST_CASE("mcp: invalid arguments come back as tool errors") {
  Server server("1", nullptr);
  const auto fails = [&](json arguments, const char* expected) {
    const std::string text = errorText(callModern(server, "compute_field_lines", std::move(arguments)));
    CHECK_MESSAGE(text.find(expected) != std::string::npos, text);
  };
  fails(json::object(), "exactly one of");
  fails(json::object({{"preset", "dipole"}, {"charges", kDipole}}), "exactly one of");
  fails(json::object({{"preset", "nope"}}), "unknown preset");
  fails(json::object({{"preset", "dipole"}, {"method", "nope"}}), "unknown method");
  fails(json::object({{"preset", "dipole"}, {"typo", 1}}), "unknown field 'typo'");
  fails(json::object({{"preset", "dipole"}, {"threads", 0}}), "'threads'");
  fails(json::object({{"preset", "dipole"}, {"threads", 1.5}}), "'threads'");
  fails(json::object({{"preset", "dipole"}, {"potential_interval", 1}}), "'potential_interval'");
  fails(json::object({{"charges", json::array({json::object({{"charge", 0}, {"x", 0}, {"y", 0}})})}}), "must not be 0");
  fails(json::object({{"charges", json::array({json::object({{"charge", 1}, {"x", 0}})})}}), "'y'");
  fails(json::object({{"charges", json::array({json::object({{"charge", 1}, {"x", 0}, {"y", 0}, {"size_px", 1}})})}}), "'size_px'");
  fails(json::object({{"charges", json::array({json::object({{"charge", 1}, {"x", 1}, {"y", 1}}), json::object({{"charge", 2}, {"x", 1}, {"y", 1}})})}}), "same position");
  fails(json::object({{"charges", json::array({json::object({{"charge", 1000}, {"x", 0}, {"y", 0}})})}, {"lines_per_unit_charge", 128}}), "field lines");
}

TEST_CASE("mcp: compute_field_lines agrees with a direct session") {
  ef::ComputeSession session(ef::makeSystem(ef::presets()[0]), ef::ComputeParams{});
  session.wait();
  ef::FieldResult direct;
  REQUIRE(session.takeResult(direct));

  Server server("1", nullptr);
  const json summary = callModern(server, "compute_field_lines", json::object({{"preset", std::string(ef::presets()[0].slug)}}));
  REQUIRE(summary["isError"] == false);
  CHECK(summary["structuredContent"]["field_lines"] == direct.electric.size());
  CHECK(summary["structuredContent"]["equipotentials"] == direct.potential.size());
  CHECK_FALSE(summary["structuredContent"].contains("lines"));

  const json detailed = callModern(server, "compute_field_lines", json::object({{"preset", std::string(ef::presets()[0].slug)}, {"include_lines", true}, {"max_points_per_line", 5}}));
  const json& lines = detailed["structuredContent"]["lines"];
  REQUIRE(lines["electric"].size() == direct.electric.size());
  for (std::size_t i = 0; i < direct.electric.size(); ++i) {
    const json& points = lines["electric"][i]["points"];
    REQUIRE(points.size() >= 2);
    CHECK(points.size() <= 5);
    CHECK(points[0][0].get<double>() == doctest::Approx(direct.electric[i].points.front().X));  // endpoints are preserved
  }
}

TEST_CASE("mcp: a raised cancel flag stops the computation") {
  Server server("1", nullptr);
  const std::atomic<bool> cancel{true};
  const std::string text = errorText(callModern(server, "compute_field_lines", json::object({{"preset", "parallel-dipoles"}, {"lines_per_unit_charge", 128}, {"method", "rk4"}}), &cancel));
  CHECK(text.find("cancelled") != std::string::npos);
}

TEST_CASE("mcp: render_field") {
  Server server("1", fakePng());
  const std::string slug(ef::presets()[0].slug);

  SUBCASE("svg inline equals the core renderer's output") {
    const json result = callModern(server, "render_field", json::object({{"preset", slug}, {"format", "svg"}}));
    REQUIRE(result["isError"] == false);
    ef::ComputeSession session(ef::makeSystem(ef::presets()[0]), ef::ComputeParams{});
    session.wait();
    ef::FieldResult direct;
    REQUIRE(session.takeResult(direct));
    const std::string expected = ef::renderSvg(ef::buildScene(ef::makeSystem(ef::presets()[0]), direct), ef::DrawStyle{}, ef::View{});
    CHECK(result["content"][1]["text"].get<std::string>() == expected);
  }
  SUBCASE("png inline is base64 image content, the default format") {
    const json result = callModern(server, "render_field", json::object({{"preset", slug}}));
    REQUIRE(result["isError"] == false);
    CHECK(result["content"][1]["type"] == "image");
    CHECK(result["content"][1]["mimeType"] == "image/png");
    CHECK(result["content"][1]["data"] == "YWJjZA==");
  }
  SUBCASE("file output never replaces a file unless asked") {
    TempDir dir;
    const std::string path = (dir.path / "out.svg").u8string();
    json args = json::object({{"preset", slug}, {"format", "svg"}, {"output_path", path}});
    const json first = callModern(server, "render_field", args);
    REQUIRE(first["isError"] == false);
    CHECK(first["content"].size() == 1);
    CHECK(std::filesystem::file_size(dir.path / "out.svg") == first["structuredContent"]["bytes"].get<std::uintmax_t>());
    CHECK(errorText(callModern(server, "render_field", args)).find("already exists") != std::string::npos);
    args["overwrite"] = true;
    CHECK(callModern(server, "render_field", args)["isError"] == false);
  }
  SUBCASE("output path rules") {
    TempDir dir;
    const auto text = [&](const std::string& path, const char* format) {
      return errorText(callModern(server, "render_field", json::object({{"preset", slug}, {"format", format}, {"output_path", path}})));
    };
    CHECK(text("relative.svg", "svg").find("absolute") != std::string::npos);
    CHECK(text((dir.path / "out.png").u8string(), "svg").find("must end in .svg") != std::string::npos);
    CHECK(text((dir.path / "missing" / "out.svg").u8string(), "svg").find("does not exist") != std::string::npos);
  }
  SUBCASE("pdf needs a path and writes a PDF") {
    CHECK(errorText(callModern(server, "render_field", json::object({{"preset", slug}, {"format", "pdf"}}))).find("output_path") != std::string::npos);
    TempDir dir;
    const auto file = dir.path / "out.pdf";
    REQUIRE(callModern(server, "render_field", json::object({{"preset", slug}, {"format", "pdf"}, {"output_path", file.u8string()}}))["isError"] == false);
    std::ifstream in(file, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(bytes.rfind("%PDF-", 0) == 0);
  }
}

TEST_CASE("mcp: a build without PNG refuses png and defaults to svg") {
  Server server("1", nullptr);
  const std::string slug(ef::presets()[0].slug);
  CHECK(errorText(callModern(server, "render_field", json::object({{"preset", slug}, {"format", "png"}}))).find("without PNG") != std::string::npos);
  CHECK(callModern(server, "render_field", json::object({{"preset", slug}}))["structuredContent"]["format"] == "svg");
}

TEST_CASE("mcp: serve answers line by line until input ends") {
  Server server("1", nullptr);
  std::istringstream in(request("server/discover", json::object({{"_meta", modernMeta()}}), 11).dump() + "\n\n" +
                        R"({"jsonrpc":"2.0","method":"notifications/initialized"})" + "\n" +
                        request("tools/list", json::object({{"_meta", modernMeta()}}), 12).dump() + "\r\n");
  std::ostringstream out;
  CHECK(efmcp::serve(in, out, server) == 0);
  std::istringstream lines(out.str());
  std::string first, second, third;
  REQUIRE(std::getline(lines, first));
  REQUIRE(std::getline(lines, second));
  CHECK_FALSE(std::getline(lines, third));
  CHECK(json::parse(first)["id"] == 11);
  CHECK(json::parse(second)["id"] == 12);
}
