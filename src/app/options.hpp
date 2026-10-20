#pragma once
// User options (Phase 4): the Computation and Draw settings of the MFC View > Options property sheet
// (ElectricField/Options.h, ComputationPropertyPage / DrawPropertyPage), persisted in a portable ini
// file instead of the registry (D-01..D-03). Std-only, same strict flags as the core: unit-testable
// without SDL or ImGui; the GUI resolves the file location (SDL_GetPrefPath) and hands over a path.
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ef/method.hpp"
#include "ef/params.hpp"
#include "ef/scene.hpp"

namespace efapp {

// The user-visible options: a ComputeParams slice (the internal constants pixelsPerUnit / chargeRadiusPx
// stay at their defaults and are never persisted) plus a DrawStyle (charge radius is internal too).
struct Options {
  ef::ComputeParams compute{};
  ef::DrawStyle draw{};
  // 261020-ef2: when set, applying a charge's own drawn size also sets its VALUE to that size
  // (sign preserved; the coupled pair starts at value 1 = size 1). The checkbox lives in the charge
  // editor panel under the Size row; persisted here so it survives restarts.
  bool coupleValueSize = false;
};

// Field-wise equality. Exact on purpose: every persisted format is lossless for the dialog's values
// (integers, #RRGGBB bytes, method names, the interval at three decimals).
inline bool operator==(const Options& a, const Options& b) {
  const ef::ComputeParams& x = a.compute;
  const ef::ComputeParams& y = b.compute;
  const ef::DrawStyle& s = a.draw;
  const ef::DrawStyle& t = b.draw;
  return x.linesPerUnitCharge == y.linesPerUnitCharge && x.calculateEquipotentials == y.calculateEquipotentials &&
         x.potentialInterval == y.potentialInterval && x.numThreads == y.numThreads && x.method == y.method &&
         s.positiveCharge.r == t.positiveCharge.r && s.positiveCharge.g == t.positiveCharge.g && s.positiveCharge.b == t.positiveCharge.b &&
         s.negativeCharge.r == t.negativeCharge.r && s.negativeCharge.g == t.negativeCharge.g && s.negativeCharge.b == t.negativeCharge.b &&
         s.electricLine.r == t.electricLine.r && s.electricLine.g == t.electricLine.g && s.electricLine.b == t.electricLine.b &&
         s.potentialLine.r == t.potentialLine.r && s.potentialLine.g == t.potentialLine.g && s.potentialLine.b == t.potentialLine.b &&
         s.electricThickness == t.electricThickness && s.potentialThickness == t.potentialThickness &&
         s.chargeRadiusPx == t.chargeRadiusPx &&  // v2.5 SIZE-01
         a.coupleValueSize == b.coupleValueSize;  // 261020-ef2
}

// MFC ranges (DDV_MinMaxUInt / spin ranges): lines 4-128, threads 1-128, interval 0.01-0.50, thicknesses
// 1-64. Unlike MFC - whose DrawPropertyPage validated the electric thickness twice and never the
// equipotential one - BOTH thicknesses are validated (roadmap OPT-02).
constexpr unsigned kLinesMin = 4, kLinesMax = 128;
constexpr unsigned kThreadsMin = 1, kThreadsMax = 128;
constexpr double kIntervalMin = 0.01, kIntervalMax = 0.50;
constexpr unsigned kThicknessMin = 1, kThicknessMax = 64;
// v2.5 SIZE-01: the DRAWN charge size, from a dot to very large. Since the per-charge seed/hit
// radii (261020-ef1) the size also drives where a charge's field lines start and end; a resize
// therefore marks the tab for recalculation. 261020-ef2: the minimum is 1 px (the coupled
// value-and-size mapping starts at value 1 = size 1).
constexpr unsigned kChargeSizeMin = 1, kChargeSizeMax = 120;

// "" when every field is in range, otherwise the first error message (shown in the dialog, D-04).
std::string validate(const Options& o);

// Ini text, one `key = value` per line, '#' comments (D-02): colours as #RRGGBB, the method by its
// stable kMethods name, the equipotential switch as true/false, the interval with three decimals
// (MFC persisted 1000 * interval).
std::string serialize(const Options& o);

struct ParsedOptions {
  Options value;
  std::vector<std::string> rejectedKeys;  // fell back to their default (bad text or out of range)
};

// Parses serialized() output. Missing file semantics live in loadOptions; here: unknown keys are
// ignored, a key that fails to parse or is out of range keeps its DEFAULT (never clamped, D-03)
// while the rest of the file still applies.
ParsedOptions parse(const std::string& text);

// File I/O. The path is std::filesystem::path end to end, so non-ASCII directory names survive on
// Windows (the GUI converts SDL's UTF-8 with std::filesystem::u8path, D-07). loadOptions returns the
// defaults when the file is missing or unreadable. saveOptions returns false with a message on failure.
bool saveOptions(const std::filesystem::path& file, const Options& o, std::string& error);
Options loadOptions(const std::filesystem::path& file);

// Working copy behind the Options dialog (D-04): numeric fields stay text-like (ints) until the dialog
// is accepted, colours are ImGui's 0..1 float triples. Pure value type - the ImGui layer copies the
// widget values in and reads validation / the applied result out, so the rules are unit-testable.
struct OptionsEdit {
  int lines = 16, threads = 4, electricThickness = 1, potentialThickness = 1;
  double chargeSize = 25.0;  // v2.5 SIZE-01: the drawn charge radius, 1-120 px
  double interval = 0.10;
  int methodIndex = 0;  // index into ef::kMethods
  bool coupleValueSize = false;  // 261020-ef2: round-tripped so a dialog OK never resets the panel's flag
  float positive[3] = {0.f, 0.f, 1.f};
  float negative[3] = {1.f, 0.f, 0.f};
  float electric[3] = {0.f, 0.f, 0.f};
  float potential[3] = {192.f / 255.f, 192.f / 255.f, 192.f / 255.f};
  bool equipotentials = true;
  std::string error;  // last validate() message; "" while valid

  void reset(const Options& current);
  bool validate();        // false -> error is set (out-of-range input is rejected, never clamped)
  Options apply() const;  // only after validate() == true
};

}  // namespace efapp
