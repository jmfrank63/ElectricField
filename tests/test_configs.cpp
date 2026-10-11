// v2 Phase 1 (CFG-01..03): the user configuration file model - ini round-trip, skip rules, dedupe and the
// Save As decision core. No SDL/ImGui.
#include "app/configs.hpp"

#include <filesystem>

#include "doctest/doctest.h"

namespace {

efapp::UserConfig sample() {
  return {"My Dipole", {{1, -2.0, 0.0}, {-1, 2.5, 0.125}}};
}

}  // namespace

TEST_CASE("serialize -> parse round-trips names and charges losslessly (CFG-01)") {
  const std::vector<efapp::UserConfig> configs = {
      sample(),
      {"Tilted", {{2, -1.5, -1.5}, {-1, 1.5, -1.5}, {-3, 0.0000001, -12345.75}}},
      {"Single", {{-5, 0.0, 0.0}}},
  };
  const efapp::ParsedConfigs r = efapp::parseConfigs(efapp::serializeConfigs(configs));
  CHECK(r.skippedSections == 0);
  CHECK(r.skippedCharges == 0);
  CHECK(r.duplicateNames == 0);
  REQUIRE(r.value.size() == 3);
  CHECK(r.value[0].name == "My Dipole");
  REQUIRE(r.value[0].charges.size() == 2);
  CHECK(r.value[0].charges[0].charge == 1);
  CHECK(r.value[0].charges[0].x == doctest::Approx(-2.0));
  CHECK(r.value[0].charges[1].charge == -1);
  CHECK(r.value[0].charges[1].y == doctest::Approx(0.125));
  REQUIRE(r.value[1].charges.size() == 3);
  CHECK(r.value[1].charges[2].charge == -3);
  CHECK(r.value[1].charges[2].x == doctest::Approx(0.0000001));
  CHECK(r.value[1].charges[2].y == doctest::Approx(-12345.75));
  CHECK(r.value[2].name == "Single");
  REQUIRE(r.value[2].charges.size() == 1);
  CHECK(r.value[2].charges[0].charge == -5);
}

TEST_CASE("comments, blank lines, foreign sections and unknown lines are tolerated") {
  const std::string text = "# Electric Field user configurations\n"
                           "\n"
                           "[some other section]\n"
                           "key = value\n"
                           "\n"
                           "[configuration \"Real\"]\n"
                           "# a comment inside\n"
                           "charge = 2, -1, -2\n";
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  CHECK(r.skippedSections == 0);
  CHECK(r.skippedCharges == 0);
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].name == "Real");
  REQUIRE(r.value[0].charges.size() == 1);
  CHECK(r.value[0].charges[0].charge == 2);
}

TEST_CASE("invalid content is skipped and counted, never fatal (CFG-01, D-02)") {
  efapp::ParsedConfigs r = efapp::parseConfigs("[configuration \"No charges\"]\n");
  CHECK(r.value.empty());
  CHECK(r.skippedSections == 1);

  r = efapp::parseConfigs("[configuration ]\ncharge = 1, 0, 0\n");
  CHECK(r.value.empty());
  CHECK(r.skippedCharges == 1);  // charges before any valid section are not counted as section content

  r = efapp::parseConfigs("[configuration \"Bad lines\"]\n"
                          "charge = 1, 0\n"            // arity
                          "charge = x, 0, 0\n"         // value not an integer
                          "charge = 1, nan, 0\n"       // non-finite coordinate
                          "charge = 1, inf, 0\n"       // non-finite coordinate
                          "charge = 1, 0, 0\n"         // the one valid line keeps the section alive
                          "charge nonsense\n");        // no '=' -> skipped charge line
  REQUIRE(r.value.size() == 1);
  CHECK(r.skippedCharges == 5);
  REQUIRE(r.value[0].charges.size() == 1);

  r = efapp::parseConfigs("charge = 1, 0, 0\n");  // outside any section
  CHECK(r.value.empty());
  CHECK(r.skippedCharges == 1);

  r = efapp::parseConfigs("[configuration \"A\"]\ncharge = 1, 0, 0\n[configuration \"A\"]\ncharge = 2, 1, 1\n");
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].name == "A");
  CHECK(r.value[0].charges[0].charge == 1);  // first occurrence wins
  CHECK(r.duplicateNames == 1);
}

TEST_CASE("missing and unreadable files yield an empty list, never an error (CFG-01)") {
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-configs" / "does-not-exist.ini";
  CHECK(efapp::loadConfigs(tmp).empty());
}

TEST_CASE("file round-trip through a real path (CFG-01)") {
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-configs";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "configurations.ini";
  const std::vector<efapp::UserConfig> configs = {sample(), {"Second", {{-1, 3.25, 4.5}}}};

  std::string err;
  REQUIRE(efapp::saveConfigs(file, configs, err));
  CHECK(err.empty());
  const std::vector<efapp::UserConfig> loaded = efapp::loadConfigs(file);
  REQUIRE(loaded.size() == 2);
  CHECK(loaded[0].name == "My Dipole");
  REQUIRE(loaded[0].charges.size() == 2);
  CHECK(loaded[1].name == "Second");
  CHECK(loaded[1].charges[0].x == doctest::Approx(3.25));

  std::error_code ec;
  std::filesystem::remove(file, ec);
  std::filesystem::remove(tmp, ec);
}

TEST_CASE("upsertUserConfig: trim, reserved names, in-place overwrite, append (CFG-03, D-04)") {
  std::vector<efapp::UserConfig> configs = {{"My Dipole", {{1, 0, 0}}}, {"Other", {{-1, 1, 1}}}};
  const std::vector<std::string> reserved = {"Dipole", "Two Equal Charges", "Quadrupole"};

  CHECK(efapp::upsertUserConfig(configs, {"   ", {}}, reserved) == efapp::UpsertResult::EMPTY_NAME);
  CHECK(efapp::upsertUserConfig(configs, {"dipole", {{9, 9, 9}}}, reserved) == efapp::UpsertResult::RESERVED_NAME);
  CHECK(efapp::upsertUserConfig(configs, {"two equal charges", {{9, 9, 9}}}, reserved) == efapp::UpsertResult::RESERVED_NAME);

  // Existing name (case-insensitive): overwritten in place, position and new spelling kept.
  CHECK(efapp::upsertUserConfig(configs, {"MY DIPOLE", {{2, 1, 2}}}, reserved) == efapp::UpsertResult::OK);
  REQUIRE(configs.size() == 2);
  CHECK(configs[0].name == "MY DIPOLE");
  CHECK(configs[0].charges.size() == 1);
  CHECK(configs[0].charges[0].charge == 2);
  CHECK(configs[1].name == "Other");  // position preserved

  // New name: appended.
  CHECK(efapp::upsertUserConfig(configs, {"Third", {{3, 3, 3}}}, reserved) == efapp::UpsertResult::OK);
  REQUIRE(configs.size() == 3);
  CHECK(configs[2].name == "Third");
}
