#include "mcp/tools.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include "app/options.hpp"  // the GUI's Options ranges: the same limits apply here
#include "ef/csv.hpp"
#include "ef/forces.hpp"
#include "ef/method.hpp"
#include "ef/pdf.hpp"
#include "ef/presets.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"

namespace efmcp {

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxCharges = 64;
constexpr int kMaxChargeValue = 1000;
constexpr double kMaxCoordinate = 1000.;
constexpr double kMinSeparation = 1e-9;  // coincident charges make the field singular everywhere between them
constexpr long long kMaxSeedLines = 10000;
constexpr std::size_t kMaxEvaluationPoints = 10000;
constexpr std::size_t kMaxReturnedPoints = 1000000;
constexpr std::size_t kMaxInlineBytes = 8u * 1024u * 1024u;
constexpr long long kDefaultTimeoutMs = 60000;

// Actionable message for the model; becomes an isError result.
struct ToolError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void checkKeys(const json& object, std::initializer_list<const char*> allowed, const std::string& where) {
  for (const auto& item : object.items()) {
    const bool known = std::any_of(allowed.begin(), allowed.end(), [&](const char* key) { return item.key() == key; });
    if (!known) throw ToolError("unknown field '" + item.key() + "' in " + where);
  }
}

double getNumber(const json& object, const char* key, double lo, double hi, double fallback, bool required = false) {
  const auto it = object.find(key);
  if (it == object.end()) {
    if (required) throw ToolError(std::string("missing required field '") + key + "'");
    return fallback;
  }
  if (!it->is_number()) throw ToolError(std::string("'") + key + "' must be a number");
  const double value = it->get<double>();
  if (!std::isfinite(value) || value < lo || value > hi)
    throw ToolError(std::string("'") + key + "' must be between " + json(lo).dump() + " and " + json(hi).dump());
  return value;
}

long long getInt(const json& object, const char* key, long long lo, long long hi, long long fallback, bool required = false) {
  const auto it = object.find(key);
  if (it == object.end()) {
    if (required) throw ToolError(std::string("missing required field '") + key + "'");
    return fallback;
  }
  const std::string range = " between " + std::to_string(lo) + " and " + std::to_string(hi);
  const auto bad = [&]() { return ToolError(std::string("'") + key + "' must be an integer" + range); };
  long long value = 0;
  if (it->is_number_unsigned()) {
    const auto raw = it->get<std::uint64_t>();
    if (raw > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) throw bad();
    value = static_cast<long long>(raw);
  } else if (it->is_number_integer()) {
    value = it->get<long long>();
  } else if (it->is_number_float()) {  // JSON Schema's "integer" includes 16.0
    const double raw = it->get<double>();
    if (!(raw >= static_cast<double>(lo) && raw <= static_cast<double>(hi)) || raw != std::floor(raw)) throw bad();
    value = static_cast<long long>(raw);
  } else {
    throw bad();
  }
  if (value < lo || value > hi) throw bad();
  return value;
}

bool getBool(const json& object, const char* key, bool fallback) {
  const auto it = object.find(key);
  if (it == object.end()) return fallback;
  if (!it->is_boolean()) throw ToolError(std::string("'") + key + "' must be true or false");
  return it->get<bool>();
}

std::string getString(const json& object, const char* key, const std::string& fallback) {
  const auto it = object.find(key);
  if (it == object.end()) return fallback;
  if (!it->is_string()) throw ToolError(std::string("'") + key + "' must be a string");
  return it->get<std::string>();
}

std::string presetSlugs() {
  std::string out;
  for (const auto& p : ef::presets()) {
    if (!out.empty()) out += ", ";
    out += p.slug;
  }
  return out;
}

ef::Charge parseCharge(const json& item, std::size_t index) {
  const std::string where = "charges[" + std::to_string(index) + "]";
  if (!item.is_object()) throw ToolError(where + " must be an object");
  checkKeys(item, {"charge", "x", "y", "z", "size_px"}, where);
  ef::Charge c;
  c.charge = static_cast<int>(getInt(item, "charge", -kMaxChargeValue, kMaxChargeValue, 0, true));
  if (c.charge == 0) throw ToolError(where + ": 'charge' must not be 0");
  c.position.X = getNumber(item, "x", -kMaxCoordinate, kMaxCoordinate, 0, true);
  c.position.Y = getNumber(item, "y", -kMaxCoordinate, kMaxCoordinate, 0, true);
  c.z = getNumber(item, "z", -kMaxCoordinate, kMaxCoordinate, 0);
  const double size = getNumber(item, "size_px", 0, efapp::kChargeSizeMax, 0);
  if (size != 0 && size < ef::kMinChargeSizePx)
    throw ToolError(where + ": 'size_px' must be 0 (default) or between " + json(ef::kMinChargeSizePx).dump() + " and " + std::to_string(efapp::kChargeSizeMax));
  c.sizePx = static_cast<float>(size);
  return c;
}

// Exactly one of `preset` / `charges`.
ef::ChargeSystem parseField(const json& args) {
  const bool hasPreset = args.contains("preset"), hasCharges = args.contains("charges");
  if (hasPreset == hasCharges) throw ToolError("provide exactly one of 'preset' or 'charges'");
  const double radius = getNumber(args, "charge_radius", 0, ef::kMaxChargeRadius, 0);
  if (hasPreset) {
    const std::string slug = getString(args, "preset", "");
    for (const auto& p : ef::presets())
      if (p.slug == slug) {
        ef::ChargeSystem field = ef::makeSystem(p);
        field.radius = radius;
        return field;
      }
    throw ToolError("unknown preset '" + slug + "'; available: " + presetSlugs());
  }
  const json& list = args.at("charges");
  if (!list.is_array() || list.empty() || list.size() > kMaxCharges)
    throw ToolError("'charges' must be an array of 1 to " + std::to_string(kMaxCharges) + " charges");
  ef::ChargeSystem field;
  field.radius = radius;
  field.charges.reserve(list.size());
  for (std::size_t i = 0; i < list.size(); ++i) {
    ef::Charge c = parseCharge(list[i], i);
    for (std::size_t j = 0; j < field.charges.size(); ++j)
      if (radius <= 0 && (field.charges[j].position3() - c.position3()).Length() < kMinSeparation)
        throw ToolError("charges[" + std::to_string(j) + "] and charges[" + std::to_string(i) + "] are at the same position");
    field.charges.push_back(std::move(c));
  }
  return field;
}

struct Computation {
  ef::ChargeSystem field;
  ef::ComputeParams params;
  long long timeoutMs = kDefaultTimeoutMs;
};

Computation parseComputation(const json& args) {
  Computation c;
  c.field = parseField(args);
  c.field.camera.yaw = getNumber(args, "view_yaw", -360, 360, 0) * ef::kPi / 180.;
  c.field.camera.pitch = getNumber(args, "view_pitch", -90, 90, 0) * ef::kPi / 180.;
  const std::string method = getString(args, "method", std::string(ef::info(c.params.method).name));
  const auto m = ef::methodFromName(method);
  if (!m) throw ToolError("unknown method '" + method + "'; call list_methods for the valid names");
  c.params.method = *m;
  c.params.linesPerUnitCharge = static_cast<unsigned>(getInt(args, "lines_per_unit_charge", efapp::kLinesMin, efapp::kLinesMax, c.params.linesPerUnitCharge));
  c.params.calculateEquipotentials = getBool(args, "equipotentials", c.params.calculateEquipotentials);
  c.params.potentialInterval = getNumber(args, "potential_interval", efapp::kIntervalMin, efapp::kIntervalMax, c.params.potentialInterval);
  c.params.numThreads = static_cast<unsigned>(getInt(args, "threads", efapp::kThreadsMin, efapp::kThreadsMax, c.params.numThreads));
  c.timeoutMs = getInt(args, "timeout_ms", 100, 600000, kDefaultTimeoutMs);

  long long seedLines = 0;
  for (const auto& charge : c.field.charges) seedLines += std::abs(static_cast<long long>(charge.charge)) * c.params.linesPerUnitCharge;
  if (seedLines > kMaxSeedLines)
    throw ToolError("this request needs " + std::to_string(seedLines) + " field lines (limit " + std::to_string(kMaxSeedLines) + "); reduce the charges or 'lines_per_unit_charge'");
  return c;
}

struct Computed {
  ef::FieldResult result;
  long long elapsedMs = 0;
};

Computed compute(const Computation& c, const std::atomic<bool>* cancel) {
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  const auto deadline = start + std::chrono::milliseconds(c.timeoutMs);
  ef::ComputeSession session(c.field, c.params);
  ef::SessionStatus status;
  for (;;) {
    ef::PollBatch discarded;  // only the finalized result is reported
    status = session.poll(discarded);
    if (status.done) break;
    if (cancel && cancel->load()) throw ToolError("computation cancelled");  // ~ComputeSession cancels and joins, bounded
    if (Clock::now() >= deadline) throw ToolError("computation exceeded timeout_ms=" + std::to_string(c.timeoutMs) + "; raise it or simplify the request");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (status.failed) throw ToolError("computation failed: " + status.error);
  Computed out;
  if (!session.takeResult(out.result)) throw ToolError("computation ended without a result");
  out.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
  return out;
}

// Evenly thins to at most `limit` points, always keeping both endpoints.
json pointsJson(const std::vector<ef::Vec2>& points, std::size_t limit) {
  json out = json::array();
  const std::size_t n = points.size();
  const std::size_t count = std::min(n, limit);
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t index = count > 1 ? i * (n - 1) / (count - 1) : 0;
    out.push_back(json::array({points[index].X, points[index].Y}));
  }
  return out;
}

const char* methodName(const Computation& c) { return ef::info(c.params.method).name.data(); }

json computationSummary(const Computation& c, const Computed& done) {
  return json::object({{"charges", c.field.charges.size()},
                       {"dimensions", c.field.is3D() ? 3 : 2},
                       {"method", methodName(c)},
                       {"field_lines", done.result.electric.size()},
                       {"equipotentials", done.result.potential.size()},
                       {"elapsed_ms", done.elapsedMs}});
}

json okResult(const json& data, json extraContent = json::array()) {
  json content = json::array();
  content.push_back(json::object({{"type", "text"}, {"text", data.dump(-1, ' ', false, json::error_handler_t::replace)}}));
  for (auto& block : extraContent) content.push_back(std::move(block));
  return json::object({{"content", std::move(content)}, {"structuredContent", data}, {"isError", false}});
}

json errorResult(const std::string& message) {
  json content = json::array();
  content.push_back(json::object({{"type", "text"}, {"text", message}}));
  return json::object({{"content", std::move(content)}, {"isError", true}});
}

json listPresets(const json& args) {
  checkKeys(args, {}, "arguments");
  json presets = json::array();
  for (const auto& p : ef::presets()) {
    json charges = json::array();
    for (int i = 0; i < p.count; ++i) {
      const auto& c = p.charges[static_cast<std::size_t>(i)];
      charges.push_back(json::object({{"charge", c.charge}, {"x", c.x}, {"y", c.y}}));
    }
    presets.push_back(json::object({{"slug", std::string(p.slug)}, {"name", std::string(p.displayName)}, {"charges", std::move(charges)}}));
  }
  return okResult(json::object({{"presets", std::move(presets)}}));
}

json listMethods(const json& args) {
  checkKeys(args, {}, "arguments");
  json methods = json::array();
  for (const auto& m : ef::kMethods)
    methods.push_back(json::object({{"name", std::string(m.name)}, {"display_name", std::string(m.display)}, {"adaptive", m.adaptive}, {"order", m.order}}));
  return okResult(json::object({{"default", std::string(ef::info(ef::ComputeParams{}.method).name)}, {"methods", std::move(methods)}}));
}

json evaluateField(const json& args) {
  checkKeys(args, {"preset", "charges", "charge_radius", "points"}, "arguments");
  const ef::ChargeSystem field = parseField(args);
  const auto it = args.find("points");
  if (it == args.end()) throw ToolError("missing required field 'points'");
  if (!it->is_array() || it->empty() || it->size() > kMaxEvaluationPoints)
    throw ToolError("'points' must be an array of 1 to " + std::to_string(kMaxEvaluationPoints) + " points");
  const bool spatial = field.is3D();
  json values = json::array();
  for (std::size_t i = 0; i < it->size(); ++i) {
    const json& point = (*it)[i];
    const std::string where = "points[" + std::to_string(i) + "]";
    if (!point.is_object()) throw ToolError(where + " must be an object");
    checkKeys(point, {"x", "y", "z"}, where);
    const ef::Vec3 pos{getNumber(point, "x", -1e9, 1e9, 0, true), getNumber(point, "y", -1e9, 1e9, 0, true), getNumber(point, "z", -1e9, 1e9, 0)};
    // A point of the drawing plane of a planar system keeps the 2D answer (no z, no ez).
    const bool space = spatial || point.contains("z");
    const ef::Vec3 e = field.E(pos);
    const double potential = field.Potential(pos);
    const double magnitude = space ? e.Length() : std::hypot(e.X, e.Y);
    const bool singular = !std::isfinite(potential) || !std::isfinite(magnitude);
    json value = json::object({{"x", pos.X}, {"y", pos.Y}, {"singular", singular}});
    if (space) value["z"] = pos.Z;
    value["ex"] = singular ? json(nullptr) : json(e.X);
    value["ey"] = singular ? json(nullptr) : json(e.Y);
    if (space) value["ez"] = singular ? json(nullptr) : json(e.Z);
    value["magnitude"] = singular ? json(nullptr) : json(magnitude);
    value["potential"] = singular ? json(nullptr) : json(potential);
    values.push_back(std::move(value));
  }
  return okResult(json::object({{"values", std::move(values)}}));
}

json computeForces(const json& args) {
  checkKeys(args, {"preset", "charges", "charge_radius"}, "arguments");
  const ef::ChargeSystem field = parseField(args);
  const std::vector<ef::Vec3> f = ef::forces(field);
  const std::vector<int> body = field.bodies();
  json list = json::array();
  for (std::size_t i = 0; i < f.size(); ++i) {
    const ef::Charge& c = field.charges[i];
    list.push_back(json::object({{"charge", c.charge}, {"x", c.position.X}, {"y", c.position.Y}, {"z", c.z},
                                 {"fx", f[i].X}, {"fy", f[i].Y}, {"fz", f[i].Z}, {"magnitude", f[i].Length()}, {"body", body[i]}}));
  }
  return okResult(json::object({{"forces", std::move(list)}, {"potential_energy", ef::potentialEnergy(field)}}));
}

json computeFieldLines(const json& args, const std::atomic<bool>* cancel) {
  checkKeys(args, {"preset", "charges", "charge_radius", "method", "lines_per_unit_charge", "equipotentials", "potential_interval", "threads", "timeout_ms", "view_yaw", "view_pitch", "include_lines", "max_points_per_line"}, "arguments");
  const Computation c = parseComputation(args);
  const bool includeLines = getBool(args, "include_lines", false);
  const auto limit = static_cast<std::size_t>(getInt(args, "max_points_per_line", 2, 5000, 200));

  const Computed done = compute(c, cancel);
  json data = computationSummary(c, done);
  if (!includeLines) return okResult(data);

  std::size_t returned = 0;
  const auto add = [&](const std::vector<ef::Vec2>& points) {
    const std::vector<ef::Vec2> curve = ef::onCurvePoints(points);
    returned += std::min(curve.size(), limit);
    if (returned > kMaxReturnedPoints)
      throw ToolError("the geometry exceeds " + std::to_string(kMaxReturnedPoints) + " points; lower 'max_points_per_line' or the line density");
    return pointsJson(curve, limit);
  };
  json electric = json::array();
  for (const auto& line : done.result.electric) electric.push_back(json::object({{"points", add(line.points)}}));
  json potential = json::array();
  for (const auto& line : done.result.potential) {
    const bool closed = line.points.size() > 1 && line.points.front() == line.points.back();
    potential.push_back(json::object({{"potential", line.potential}, {"closed", closed}, {"points", add(line.points)}}));
  }
  data["lines"] = json::object({{"electric", std::move(electric)}, {"equipotential", std::move(potential)}});
  return okResult(data);
}

std::string base64(const std::string& bytes) {
  static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const auto at = [](std::uint32_t index) { return kAlphabet[index & 63u]; };
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  std::size_t i = 0;
  const auto byte = [&](std::size_t k) { return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[k])); };
  for (; i + 2 < bytes.size(); i += 3) {
    const std::uint32_t v = (byte(i) << 16) | (byte(i + 1) << 8) | byte(i + 2);
    out += at(v >> 18);
    out += at(v >> 12);
    out += at(v >> 6);
    out += at(v);
  }
  if (i + 1 == bytes.size()) {
    const std::uint32_t v = byte(i) << 16;
    out += at(v >> 18);
    out += at(v >> 12);
    out += "==";
  } else if (i + 2 == bytes.size()) {
    const std::uint32_t v = (byte(i) << 16) | (byte(i + 1) << 8);
    out += at(v >> 18);
    out += at(v >> 12);
    out += at(v >> 6);
    out += '=';
  }
  return out;
}

// Checked BEFORE the computation, so a bad path fails fast. Writing is opt-in and never replaces a file by accident.
fs::path validateOutputPath(const std::string& text, const std::string& format, bool overwrite) {
  if (text.find('\0') != std::string::npos) throw ToolError("'output_path' contains a NUL character");
  const fs::path path = fs::u8path(text);
  if (!path.is_absolute()) throw ToolError("'output_path' must be an absolute path");
  if (path.extension().u8string() != "." + format) throw ToolError("'output_path' must end in ." + format + " for format '" + format + "'");
  std::error_code ec;
  if (!fs::is_directory(path.parent_path(), ec)) throw ToolError("the directory of 'output_path' does not exist");
  if (fs::is_directory(path, ec)) throw ToolError("'output_path' is a directory");
  if (fs::exists(path, ec) && !overwrite) throw ToolError("'output_path' already exists; pass \"overwrite\": true to replace it");
  return path;
}

void writeFile(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) throw ToolError("cannot open 'output_path' for writing");
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  file.close();  // disk-full / I/O errors only surface here
  if (!file) throw ToolError("cannot write 'output_path'");
}

json renderField(const json& args, const PngEncoder& encodePng, const std::atomic<bool>* cancel) {
  checkKeys(args, {"preset", "charges", "charge_radius", "method", "lines_per_unit_charge", "equipotentials", "potential_interval", "threads", "timeout_ms", "view_yaw", "view_pitch", "format", "width", "height", "pixels_per_unit", "center_x", "center_y", "output_path", "overwrite"}, "arguments");
  const Computation c = parseComputation(args);
  const std::string format = getString(args, "format", encodePng ? "png" : "svg");
  if (format != "png" && format != "svg" && format != "pdf") throw ToolError("'format' must be png, svg or pdf");
  if (format == "png" && !encodePng) throw ToolError("this ef-mcp was built without PNG support (-Dgui=false); use svg or pdf");

  ef::View view;
  view.widthPx = static_cast<int>(getInt(args, "width", 16, 8192, view.widthPx));
  view.heightPx = static_cast<int>(getInt(args, "height", 16, 8192, view.heightPx));
  view.pixelsPerUnit = getNumber(args, "pixels_per_unit", 1, 10000, view.pixelsPerUnit);
  view.centre.X = getNumber(args, "center_x", -kMaxCoordinate, kMaxCoordinate, 0);
  view.centre.Y = getNumber(args, "center_y", -kMaxCoordinate, kMaxCoordinate, 0);

  const std::string outputText = getString(args, "output_path", "");
  const bool overwrite = getBool(args, "overwrite", false);
  if (args.contains("overwrite") && outputText.empty()) throw ToolError("'overwrite' needs 'output_path'");
  if (format == "pdf" && outputText.empty()) throw ToolError("format 'pdf' needs 'output_path' (PDF cannot be returned inline)");
  fs::path outputPath;
  if (!outputText.empty()) outputPath = validateOutputPath(outputText, format, overwrite);

  const Computed done = compute(c, cancel);
  const ef::FieldScene scene = ef::buildScene(c.field, done.result);
  const ef::DrawStyle style;
  std::string bytes;
  if (format == "svg") {
    bytes = ef::renderSvg(scene, style, view);
  } else if (format == "pdf") {
    bytes = ef::renderPdf(scene, style, view);
  } else {
    const std::string error = encodePng(scene, style, view, bytes);
    if (!error.empty()) throw ToolError("PNG rendering failed: " + error);
  }

  json data = computationSummary(c, done);
  data["format"] = format;
  data["width"] = view.widthPx;
  data["height"] = view.heightPx;
  data["bytes"] = bytes.size();
  if (!outputText.empty()) {
    writeFile(outputPath, bytes);
    data["output_path"] = outputText;
    return okResult(data);
  }
  if (bytes.size() > kMaxInlineBytes) throw ToolError("the rendered " + format + " is too large to return inline; pass 'output_path'");
  json extra = json::array();
  if (format == "svg")
    extra.push_back(json::object({{"type", "text"}, {"text", bytes}}));
  else
    extra.push_back(json::object({{"type", "image"}, {"data", base64(bytes)}, {"mimeType", "image/png"}}));
  return okResult(data, std::move(extra));
}

json exportCsv(const json& args, const std::atomic<bool>* cancel) {
  checkKeys(args, {"preset", "charges", "charge_radius", "method", "lines_per_unit_charge", "equipotentials", "potential_interval", "threads", "timeout_ms", "view_yaw", "view_pitch", "table", "output_path", "overwrite"}, "arguments");
  const Computation c = parseComputation(args);
  const std::string table = getString(args, "table", "lines");
  if (table != "lines" && table != "charges" && table != "bodies") throw ToolError("'table' must be lines, charges or bodies");
  const std::string outputText = getString(args, "output_path", "");
  const bool overwrite = getBool(args, "overwrite", false);
  if (args.contains("overwrite") && outputText.empty()) throw ToolError("'overwrite' needs 'output_path'");
  fs::path outputPath;
  if (!outputText.empty()) outputPath = validateOutputPath(outputText, "csv", overwrite);

  json data = json::object({{"table", table}, {"charges", c.field.charges.size()}, {"dimensions", c.field.is3D() ? 3 : 2}});
  std::string csv;
  if (table == "charges") {
    csv = ef::chargesCsv(c.field);
  } else if (table == "bodies") {
    csv = ef::bodiesCsv(c.field);
  } else {
    const Computed done = compute(c, cancel);
    csv = ef::linesCsv(c.field, ef::buildScene(c.field, done.result));
    data = computationSummary(c, done);
    data["table"] = table;
  }
  data["rows"] = static_cast<std::size_t>(std::count(csv.begin(), csv.end(), '\n')) - 1;
  data["bytes"] = csv.size();
  if (!outputText.empty()) {
    writeFile(outputPath, csv);
    data["output_path"] = outputText;
    return okResult(data);
  }
  if (csv.size() > kMaxInlineBytes) throw ToolError("the CSV is too large to return inline; pass 'output_path'");
  json extra = json::array();
  extra.push_back(json::object({{"type", "text"}, {"text", csv}}));
  return okResult(data, std::move(extra));
}

const char* const kFieldProperties = R"json({
  "preset": {"type": "string", "description": "Slug of a built-in configuration (see list_presets). Provide exactly one of preset or charges."},
  "charges": {
    "type": "array", "minItems": 1, "maxItems": 64,
    "description": "Charges at distinct positions (balls, with charge_radius above 0, may coincide). Provide exactly one of preset or charges.",
    "items": {
      "type": "object", "additionalProperties": false, "required": ["charge", "x", "y"],
      "properties": {
        "z": {"type": "number", "minimum": -1000, "maximum": 1000, "default": 0, "description": "World units, towards the viewer. Any charge with z other than 0 makes the system 3D."},
        "charge": {"type": "integer", "minimum": -1000, "maximum": 1000, "description": "Non-zero integer charge."},
        "x": {"type": "number", "minimum": -1000, "maximum": 1000, "description": "World units, x to the right."},
        "y": {"type": "number", "minimum": -1000, "maximum": 1000, "description": "World units, y up."},
        "size_px": {"type": "number", "minimum": 0, "maximum": 120, "default": 0, "description": "Drawn radius in px: 0 (the 25 px default) or 2 to 120. Field lines start and end on this circle."}
      }
    }
  },
  "charge_radius": {"type": "number", "minimum": 0, "maximum": 1000, "default": 0, "description": "Physical radius of every charge in world units. 0 = point charges. Above 0 each charge is a uniformly charged ball: Coulomb outside. Each ball keeps its uniform charge and overlapping charges add. Balls whose centres are at most 2R apart form one body (they may even share a centre); field lines are placed on a body's outer surface in proportion to the field flowing through it and end where they meet a surface; the inside is not traced."}
})json";

const char* const kComputeProperties = R"json({
  "method": {"type": "string", "default": "euler", "description": "Integration method name (see list_methods)."},
  "lines_per_unit_charge": {"type": "integer", "minimum": 4, "maximum": 128, "default": 16},
  "equipotentials": {"type": "boolean", "default": true, "description": "Also compute equipotential lines."},
  "potential_interval": {"type": "number", "minimum": 0.01, "maximum": 0.5, "default": 0.1, "description": "Potential difference between equipotentials."},
  "threads": {"type": "integer", "minimum": 1, "maximum": 128, "default": 4},
  "timeout_ms": {"type": "integer", "minimum": 100, "maximum": 600000, "default": 60000},
  "view_yaw": {"type": "number", "minimum": -360, "maximum": 360, "default": 0, "description": "Camera turn about the vertical (y) axis in degrees. The view is orthographic and turns about the world origin. A turned camera, or a charge with z other than 0, selects the 3D pipeline: field lines start evenly spread over a sphere around each source charge, are traced in space and returned or drawn as their projection; equipotentials (surfaces in space) are not drawn."},
  "view_pitch": {"type": "number", "minimum": -90, "maximum": 90, "default": 0, "description": "Camera tilt about the screen's horizontal axis in degrees."}
})json";

json objectSchema(json properties, std::initializer_list<const char*> required = {}) {
  json schema = json::object({{"type", "object"}, {"additionalProperties", false}, {"properties", std::move(properties)}});
  if (required.size() != 0) schema["required"] = json(std::vector<std::string>(required.begin(), required.end()));
  return schema;
}

json toolEntry(const char* name, const char* title, const char* description, json inputSchema, bool readOnly) {
  json annotations = json::object({{"title", title}, {"readOnlyHint", readOnly}, {"openWorldHint", false}});
  if (readOnly) annotations["idempotentHint"] = true;
  return json::object({{"name", name}, {"title", title}, {"description", description}, {"inputSchema", std::move(inputSchema)}, {"annotations", std::move(annotations)}});
}

}  // namespace

json toolDefinitions(bool pngAvailable) {
  const json fieldProperties = json::parse(kFieldProperties);
  const json computeProperties = json::parse(kComputeProperties);

  json tools = json::array();
  tools.push_back(toolEntry("list_presets", "List presets", "The built-in charge configurations: slug, display name and charges.", objectSchema(json::object()), true));
  tools.push_back(toolEntry("list_methods", "List integration methods", "The Runge-Kutta integration methods: name, display name, adaptive flag and order.", objectSchema(json::object()), true));

  json evaluate = fieldProperties;
  evaluate["points"] = json::parse(R"json({
    "type": "array", "minItems": 1, "maxItems": 10000,
    "items": {"type": "object", "additionalProperties": false, "required": ["x", "y"], "properties": {"x": {"type": "number"}, "y": {"type": "number"}, "z": {"type": "number", "default": 0}}}
  })json");
  tools.push_back(toolEntry("evaluate_field", "Evaluate field at points", "Electric field vector, magnitude and potential at world points, with E = sum q*r/|r|^3 and V = sum q/|r| (unit Coulomb constant). A point on a charge is reported with singular=true and null values. When a charge or the point has a z coordinate the answer also carries z and ez.", objectSchema(std::move(evaluate), {"points"}), true));

  tools.push_back(toolEntry("compute_forces", "Compute forces between charges", "The force on every charge from all the others, in space, and the interaction energy of the system (unit Coulomb constant: F = q1 q2 / d^2, U = q1 q2 / d). With charge_radius above 0, overlapping balls (d < 2R) are rigid and interpenetrate: F = q1 q2 / R^2 * (s - 9 s^2/16 + s^4/32) and U = q1 q2 / R * (6/5 - s^2/2 + 3 s^3/16 - s^5/160) with s = d / R.", objectSchema(fieldProperties), true));

  json computeLines = fieldProperties;
  computeLines.update(computeProperties);
  computeLines["include_lines"] = json::parse(R"json({"type": "boolean", "default": false, "description": "Also return the line geometry as [x, y] world points on each curve (in 3D: the projection through the camera)."})json");
  computeLines["max_points_per_line"] = json::parse(R"json({"type": "integer", "minimum": 2, "maximum": 5000, "default": 200, "description": "Thin each returned line to at most this many points."})json");
  tools.push_back(toolEntry("compute_field_lines", "Compute field lines", "Traces the electric field lines and equipotentials of a configuration and returns the line counts, optionally with the geometry.", objectSchema(std::move(computeLines)), true));

  json render = fieldProperties;
  render.update(computeProperties);
  json formats = json::array();
  if (pngAvailable) formats.push_back("png");
  formats.push_back("svg");
  formats.push_back("pdf");
  render["format"] = json::object({{"type", "string"}, {"enum", std::move(formats)}, {"default", pngAvailable ? "png" : "svg"}, {"description", "png and svg are returned inline unless output_path is set; pdf needs output_path."}});
  render.update(json::parse(R"json({
    "width": {"type": "integer", "minimum": 16, "maximum": 8192, "default": 1024},
    "height": {"type": "integer", "minimum": 16, "maximum": 8192, "default": 768},
    "pixels_per_unit": {"type": "number", "minimum": 1, "maximum": 10000, "default": 100},
    "center_x": {"type": "number", "minimum": -1000, "maximum": 1000, "default": 0, "description": "World point shown at the image centre."},
    "center_y": {"type": "number", "minimum": -1000, "maximum": 1000, "default": 0},
    "output_path": {"type": "string", "description": "Absolute path of a file to write, in an existing directory, with the extension of the format. Omit to get the result inline."},
    "overwrite": {"type": "boolean", "default": false, "description": "Allow output_path to replace an existing file."}
  })json"));
  tools.push_back(toolEntry("render_field", "Render field image", "Computes a configuration and renders field lines, equipotentials and charges as PNG, SVG or PDF, inline or to a file.", objectSchema(std::move(render)), false));
  json csv = fieldProperties;
  csv.update(computeProperties);
  csv.update(json::parse(R"json({
    "table": {"type": "string", "enum": ["lines", "charges", "bodies"], "default": "lines", "description": "lines: one row per point of every field line and equipotential with x, y, z, screen position, potential, field vector, magnitude and direction angles. charges: one row per charge with position, body, potential and field of the other charges at its centre, the force on it and the system energy. bodies: one row per group of touching balls with net charge, dipole moment and the field flux leaving and entering its surface."},
    "output_path": {"type": "string", "description": "Absolute path of a .csv file to write, in an existing directory. Omit to get the CSV inline."},
    "overwrite": {"type": "boolean", "default": false, "description": "Allow output_path to replace an existing file."}
  })json"));
  tools.push_back(toolEntry("export_csv", "Export numbers as CSV", "Computes a configuration and returns its numbers as a CSV table: the points of the lines, or the charges with forces.", objectSchema(std::move(csv)), false));
  return tools;
}

bool isKnownTool(const std::string& name) {
  return name == "list_presets" || name == "list_methods" || name == "evaluate_field" || name == "compute_forces" || name == "compute_field_lines" || name == "export_csv" || name == "render_field";
}

json callTool(const std::string& name, const json& arguments, const PngEncoder& encodePng, const std::atomic<bool>* cancel) {
  try {
    if (name == "list_presets") return listPresets(arguments);
    if (name == "list_methods") return listMethods(arguments);
    if (name == "evaluate_field") return evaluateField(arguments);
    if (name == "compute_forces") return computeForces(arguments);
    if (name == "compute_field_lines") return computeFieldLines(arguments, cancel);
    if (name == "export_csv") return exportCsv(arguments, cancel);
    if (name == "render_field") return renderField(arguments, encodePng, cancel);
    return errorResult("unknown tool '" + name + "'");
  } catch (const std::exception& e) {
    return errorResult(e.what());
  }
}

}  // namespace efmcp
