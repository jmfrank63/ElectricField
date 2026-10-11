// Phase 4 (OPT-01..OPT-04): the user options model - validation, ini round-trip, per-key fallback and
// the dialog edit state. No file system beyond a temp directory; no SDL/ImGui.
#include "app/options.hpp"

#include <filesystem>
#include <fstream>

#include "doctest/doctest.h"

namespace {

efapp::Options defaults() { return efapp::Options{}; }

void writeFile(const std::filesystem::path& file, const std::string& text) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  REQUIRE(out.good());
  out << text;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
  for (const std::string& x : v)
    if (x == s) return true;
  return false;
}

}  // namespace

TEST_CASE("defaults validate clean and carry the MFC values") {
  const efapp::Options o = defaults();
  CHECK(efapp::validate(o).empty());
  CHECK(o.compute.linesPerUnitCharge == 16);
  CHECK(o.compute.calculateEquipotentials);
  CHECK(o.compute.potentialInterval == doctest::Approx(0.10));
  CHECK(o.compute.numThreads == 4);
  CHECK(o.compute.method == ef::Method::Euler);
  CHECK(o.draw.positiveCharge.r == 0);
  CHECK(o.draw.positiveCharge.b == 255);
  CHECK(o.draw.negativeCharge.r == 255);
  CHECK(o.draw.electricLine.r == 0);
  CHECK(o.draw.electricLine.g == 0);
  CHECK(o.draw.potentialLine.r == 192);
  CHECK(o.draw.potentialLine.g == 192);
  CHECK(o.draw.potentialLine.b == 192);
  CHECK(o.draw.electricThickness == 1.f);
  CHECK(o.draw.potentialThickness == 1.f);
}

TEST_CASE("validate accepts the range edges and rejects beyond them (OPT-01, OPT-02)") {
  efapp::Options o = defaults();
  o.compute.linesPerUnitCharge = 4;
  CHECK(efapp::validate(o).empty());
  o.compute.linesPerUnitCharge = 128;
  CHECK(efapp::validate(o).empty());
  o.compute.linesPerUnitCharge = 3;
  CHECK_FALSE(efapp::validate(o).empty());
  o.compute.linesPerUnitCharge = 129;
  CHECK_FALSE(efapp::validate(o).empty());

  o = defaults();
  o.compute.numThreads = 1;
  CHECK(efapp::validate(o).empty());
  o.compute.numThreads = 128;
  CHECK(efapp::validate(o).empty());
  o.compute.numThreads = 129;
  CHECK_FALSE(efapp::validate(o).empty());

  o = defaults();
  o.compute.potentialInterval = 0.01;
  CHECK(efapp::validate(o).empty());
  o.compute.potentialInterval = 0.50;
  CHECK(efapp::validate(o).empty());
  o.compute.potentialInterval = 0.009;
  CHECK_FALSE(efapp::validate(o).empty());
  o.compute.potentialInterval = 0.51;
  CHECK_FALSE(efapp::validate(o).empty());

  o = defaults();
  o.draw.electricThickness = 1.f;
  CHECK(efapp::validate(o).empty());
  o.draw.electricThickness = 64.f;
  CHECK(efapp::validate(o).empty());
  o.draw.electricThickness = 0.f;
  CHECK_FALSE(efapp::validate(o).empty());
  o.draw.electricThickness = 65.f;
  CHECK_FALSE(efapp::validate(o).empty());

  // The MFC dialog validated the electric thickness twice and never the equipotential one; both are checked here.
  o = defaults();
  o.draw.potentialThickness = 1.f;
  CHECK(efapp::validate(o).empty());
  o.draw.potentialThickness = 64.f;
  CHECK(efapp::validate(o).empty());
  o.draw.potentialThickness = 0.f;
  CHECK_FALSE(efapp::validate(o).empty());
  o.draw.potentialThickness = 65.f;
  CHECK_FALSE(efapp::validate(o).empty());
}

TEST_CASE("serialize -> parse round-trips every field (OPT-04)") {
  efapp::Options o = defaults();
  o.compute.linesPerUnitCharge = 42;
  o.compute.calculateEquipotentials = false;
  o.compute.potentialInterval = 0.025;
  o.compute.numThreads = 7;
  o.compute.method = ef::Method::AdaptiveDormandPrince;
  o.draw.positiveCharge = ef::Color{1, 2, 3, 255};
  o.draw.negativeCharge = ef::Color{4, 5, 6, 255};
  o.draw.electricLine = ef::Color{7, 8, 9, 255};
  o.draw.potentialLine = ef::Color{10, 11, 12, 255};
  o.draw.electricThickness = 13.f;
  o.draw.potentialThickness = 14.f;

  const efapp::ParsedOptions r = efapp::parse(efapp::serialize(o));
  CHECK(r.rejectedKeys.empty());
  CHECK(r.value.compute.linesPerUnitCharge == 42);
  CHECK_FALSE(r.value.compute.calculateEquipotentials);
  CHECK(r.value.compute.potentialInterval == doctest::Approx(0.025));
  CHECK(r.value.compute.numThreads == 7);
  CHECK(r.value.compute.method == ef::Method::AdaptiveDormandPrince);
  CHECK(r.value.draw.positiveCharge.r == 1);
  CHECK(r.value.draw.positiveCharge.g == 2);
  CHECK(r.value.draw.positiveCharge.b == 3);
  CHECK(r.value.draw.negativeCharge.r == 4);
  CHECK(r.value.draw.negativeCharge.g == 5);
  CHECK(r.value.draw.negativeCharge.b == 6);
  CHECK(r.value.draw.electricLine.r == 7);
  CHECK(r.value.draw.electricLine.g == 8);
  CHECK(r.value.draw.electricLine.b == 9);
  CHECK(r.value.draw.potentialLine.r == 10);
  CHECK(r.value.draw.potentialLine.g == 11);
  CHECK(r.value.draw.potentialLine.b == 12);
  CHECK(r.value.draw.electricThickness == 13.f);
  CHECK(r.value.draw.potentialThickness == 14.f);
}

TEST_CASE("comments, blank lines and unknown keys are tolerated") {
  const std::string text = "# Electric Field options\n"
                           "\n"
                           "linesPerUnitCharge = 8\n"
                           "someFutureKey = whatever\n"
                           "# potentialInterval = 0.99 (commented out)\n";
  const efapp::ParsedOptions r = efapp::parse(text);
  CHECK(r.rejectedKeys.empty());
  CHECK(r.value.compute.linesPerUnitCharge == 8);
  CHECK(r.value.compute.potentialInterval == doctest::Approx(0.10));  // default, the commented line does not count
}

TEST_CASE("a rejected key falls back to its default while the rest still applies (OPT-04, D-03)") {
  efapp::ParsedOptions r = efapp::parse("linesPerUnitCharge = 999\nnumThreads = 2\n");
  CHECK(contains(r.rejectedKeys, "linesPerUnitCharge"));
  CHECK_FALSE(contains(r.rejectedKeys, "numThreads"));
  CHECK(r.value.compute.linesPerUnitCharge == 16);  // default, never clamped to 128
  CHECK(r.value.compute.numThreads == 2);

  r = efapp::parse("potentialInterval = 0.9\n");
  CHECK(contains(r.rejectedKeys, "potentialInterval"));
  CHECK(r.value.compute.potentialInterval == doctest::Approx(0.10));

  r = efapp::parse("potentialInterval = yesterday\n");
  CHECK(contains(r.rejectedKeys, "potentialInterval"));
  CHECK(r.value.compute.potentialInterval == doctest::Approx(0.10));

  r = efapp::parse("method = nope\n");
  CHECK(contains(r.rejectedKeys, "method"));
  CHECK(r.value.compute.method == ef::Method::Euler);

  r = efapp::parse("electricLineThickness = 0\npotentialLineThickness = 65\n");
  CHECK(contains(r.rejectedKeys, "electricLineThickness"));
  CHECK(contains(r.rejectedKeys, "potentialLineThickness"));
  CHECK(r.value.draw.electricThickness == 1.f);
  CHECK(r.value.draw.potentialThickness == 1.f);

  r = efapp::parse("positiveChargeColor = #00ff7\nnegativeChargeColor = ff0000\n");
  CHECK(contains(r.rejectedKeys, "positiveChargeColor"));
  CHECK(contains(r.rejectedKeys, "negativeChargeColor"));
  CHECK(r.value.draw.positiveCharge.r == 0);
  CHECK(r.value.draw.positiveCharge.b == 255);
  CHECK(r.value.draw.negativeCharge.r == 255);

  r = efapp::parse("calculateEquipotentials = yes\n");
  CHECK(contains(r.rejectedKeys, "calculateEquipotentials"));
  CHECK(r.value.compute.calculateEquipotentials);
}

TEST_CASE("parse accepts the documented formats: #RRGGBB colours, method names, true/false") {
  const efapp::ParsedOptions r = efapp::parse("positiveChargeColor = #00FF7f\n"
                                              "method = adaptive-dormand-prince\n"
                                              "calculateEquipotentials = false\n");
  CHECK(r.rejectedKeys.empty());
  CHECK(r.value.draw.positiveCharge.r == 0x00);
  CHECK(r.value.draw.positiveCharge.g == 0xFF);
  CHECK(r.value.draw.positiveCharge.b == 0x7f);
  CHECK(r.value.compute.method == ef::Method::AdaptiveDormandPrince);
  CHECK_FALSE(r.value.compute.calculateEquipotentials);
}

TEST_CASE("file round-trip and missing-file defaults (OPT-04, D-03)") {
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-options";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "options.ini";

  CHECK(efapp::loadOptions(file) == defaults());  // missing file -> defaults

  efapp::Options o = defaults();
  o.compute.linesPerUnitCharge = 64;
  o.compute.method = ef::Method::RK4;
  o.draw.electricLine = ef::Color{16, 32, 48, 255};
  o.draw.potentialThickness = 3.f;
  std::string err;
  REQUIRE(efapp::saveOptions(file, o, err));
  CHECK(err.empty());
  CHECK(efapp::loadOptions(file) == o);

  writeFile(file, "this is not an ini file at all\n");
  CHECK(efapp::loadOptions(file) == defaults());  // nothing parseable -> all defaults

  std::error_code ec;
  std::filesystem::remove(file, ec);
  std::filesystem::remove(tmp, ec);
}

TEST_CASE("OptionsEdit: reset copies, validate rejects without clamping, apply converts (D-04)") {
  efapp::OptionsEdit e;
  e.reset(defaults());
  CHECK(e.validate());
  CHECK(e.error.empty());
  CHECK(e.apply() == defaults());

  e.reset(defaults());
  e.lines = 3;
  CHECK_FALSE(e.validate());
  CHECK_FALSE(e.error.empty());
  e.lines = 128;
  e.threads = 0;  // int fields can go negative in the dialog; still rejected
  CHECK_FALSE(e.validate());
  e.threads = 1;
  e.electricThickness = 65;
  CHECK_FALSE(e.validate());
  e.electricThickness = 1;
  e.potentialThickness = -1;
  CHECK_FALSE(e.validate());
  e.potentialThickness = 1;
  e.interval = 0.0;
  CHECK_FALSE(e.validate());
  e.interval = 0.35;
  CHECK(e.validate());
  CHECK(e.error.empty());

  const efapp::Options applied = e.apply();
  CHECK(applied.compute.linesPerUnitCharge == 128);
  CHECK(applied.compute.numThreads == 1);
  CHECK(applied.compute.potentialInterval == doctest::Approx(0.35));
  CHECK(applied.draw.electricThickness == 1.f);
  CHECK(applied.draw.potentialThickness == 1.f);

  // Colours round-trip through the 0..1 floats.
  efapp::Options custom = defaults();
  custom.draw.potentialLine = ef::Color{51, 102, 153, 255};
  e.reset(custom);
  CHECK(e.validate());
  const efapp::Options back = e.apply();
  CHECK(back.draw.potentialLine.r == 51);
  CHECK(back.draw.potentialLine.g == 102);
  CHECK(back.draw.potentialLine.b == 153);
  CHECK(back == custom);

  e.reset(custom);
  e.methodIndex = 10;  // last of the 11 methods
  CHECK(e.validate());
  CHECK(e.apply().compute.method == ef::Method::AdaptiveDormandPrince);
}
