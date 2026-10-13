// v2 Phase 1 (CFG-01..03): the user configuration file model - ini round-trip, skip rules, dedupe and the
// Save As decision core. No SDL/ImGui.
#include "app/configs.hpp"
#include "app/ini_text.hpp"  // IN-03: utf8Clamp

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

// v2.6 phase 3: the extended 6-field charge line `charge = value, x, y, sizePx, id, name`. Names are
// comma-free (',' stripped on write), stored as raw UTF-8 bytes, and the derived "Charge N" display
// name is never persisted - the model's empty name writes a trailing empty field.
TEST_CASE("extended 6-field charge lines round-trip size, id and name losslessly (v2.6 phase 3)") {
  const std::vector<efapp::UserConfig> configs = {
      {"Identity",
       {{1, -2.0, 0.0, 40.0f, 1, "My Charge"},
        {-1, 2.5, 0.125, 0.0f, 2, "Größerer Test"},
        {2, 0.0, 0.0, 2.0f, 3, "Has, Comma"}}},
  };
  const std::string text = efapp::serializeConfigs(configs);
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  CHECK(r.skippedSections == 0);
  CHECK(r.skippedCharges == 0);
  CHECK(r.duplicateNames == 0);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 3);
  CHECK(r.value[0].charges[0].sizePx == doctest::Approx(40.0));
  CHECK(r.value[0].charges[0].id == 1);
  CHECK(r.value[0].charges[0].name == "My Charge");
  CHECK(r.value[0].charges[1].sizePx == 0.0f);  // 0 = inherit the Options default, survives as 0
  CHECK(r.value[0].charges[1].id == 2);
  CHECK(r.value[0].charges[1].name == "Größerer Test");  // non-ASCII passes through byte-for-byte
  CHECK(r.value[0].charges[2].id == 3);
  CHECK(r.value[0].charges[2].name == "Has Comma");  // the model's ',' came back stripped

  // serialize -> parse -> serialize is stable (the second write is byte-identical to the first).
  CHECK(efapp::serializeConfigs(r.value) == text);
}

TEST_CASE("an empty name writes a trailing empty field and parses back empty (v2.6 phase 3)") {
  const std::vector<efapp::UserConfig> configs = {{"E", {{1, 0, 0, 0.0f, 1, ""}}}};
  const std::string text = efapp::serializeConfigs(configs);
  CHECK(text.find("charge = 1, 0, 0, 0, 1, \n") != std::string::npos);
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 1);
  CHECK(r.value[0].charges[0].id == 1);
  CHECK(r.value[0].charges[0].name.empty());
  CHECK(r.value[0].charges[0].sizePx == 0.0f);
}

TEST_CASE("legacy 3-field lines parse with defaults; 4/5-field lines are skipped and counted (v2.6 phase 3)") {
  const std::string text = "[configuration \"Mixed\"]\n"
                           "charge = 1, 0, 0\n"                  // legacy: no size, no identity
                           "charge = 2, 1, 1, 25\n"              // 4 fields: skipped (all-or-nothing after y)
                           "charge = 2, 1, 1, 25, 7\n"           // 5 fields: skipped
                           "charge = -1, 2, 2, 10, 4, Named\n";  // extended
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  CHECK(r.skippedCharges == 2);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 2);
  CHECK(r.value[0].charges[0].sizePx == 0.0f);
  CHECK(r.value[0].charges[0].id == 0);
  CHECK(r.value[0].charges[0].name.empty());
  CHECK(r.value[0].charges[1].sizePx == doctest::Approx(10.0));
  CHECK(r.value[0].charges[1].id == 4);
  CHECK(r.value[0].charges[1].name == "Named");
}

// WR-01: the extended size field obeys the mutator contract - 0 (inherit) or 2..120 px; anything
// else is a malformed line, skipped and counted, exactly like the mutators' validation.
TEST_CASE("out-of-range sizePx values are skipped and counted, never degraded into the model (WR-01)") {
  const std::string text = "[configuration \"Sizes\"]\n"
                           "charge = 1, 0, 0, 0, 1, Zero\n"        // 0 = inherit: legal
                           "charge = 1, 1, 0, 2, 2, Floor\n"       // the 2 px floor: legal
                           "charge = 1, 2, 0, 120, 3, Ceiling\n"   // the 120 px ceiling: legal
                           "charge = 1, 3, 0, -5, 4, Negative\n"   // negative: malformed
                           "charge = 1, 4, 0, 1, 5, Below\n"       // 0 < x < 2: malformed
                           "charge = 1, 5, 0, 1e30, 6, Huge\n"     // screen-filling dot: malformed
                           "charge = 1, 6, 0, 121, 7, Over\n";     // above the ceiling: malformed
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  CHECK(r.skippedCharges == 4);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 3);
  CHECK(r.value[0].charges[0].sizePx == 0.0f);
  CHECK(r.value[0].charges[1].sizePx == doctest::Approx(2.0));
  CHECK(r.value[0].charges[2].sizePx == doctest::Approx(120.0));
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

// WR-02: the id bound is the shared ef::kMaxChargeId ceiling the assignment path saturates at, so
// every id the writer can emit parses back - the save->load round-trip is closed.
TEST_CASE("the charge id bound matches the assignment ceiling (WR-02)") {
  const std::string text = "[configuration \"Ids\"]\n"
                           "charge = 1, 0, 0, 0, 1000000000, Top\n"    // == ef::kMaxChargeId: parses
                           "charge = 1, 1, 0, 0, 1000000001, Over\n";  // above the ceiling: skipped
  const efapp::ParsedConfigs r = efapp::parseConfigs(text);
  CHECK(r.skippedCharges == 1);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 1);
  CHECK(r.value[0].charges[0].id == ef::kMaxChargeId);
}

// IN-03: the panel copy helper truncates on a UTF-8 code point boundary, never mid-sequence.
TEST_CASE("utf8Clamp cuts on code point boundaries only (IN-03)") {
  using efapp::ini::utf8Clamp;
  CHECK(utf8Clamp("", 8).empty());
  CHECK(utf8Clamp("Pad", 8) == "Pad");          // fits: untouched
  CHECK(utf8Clamp("Pad", 3) == "Pad");          // exact fit: untouched
  CHECK(utf8Clamp("Padding", 4) == "Padd");     // ASCII: hard cut
  // 'Größer' = G r ö(2 bytes) ß(2 bytes) e r = 8 bytes; a 3-byte cut would split the ö.
  CHECK(utf8Clamp("Größer", 3) == "Gr");        // back over the continuation byte AND the lead byte
  CHECK(utf8Clamp("Größer", 4) == "Grö");       // the cut lands between ö and ß: keep the ö
  CHECK(utf8Clamp("Größer", 6) == "Größ");      // ß complete at byte 6
  CHECK(utf8Clamp("Größer", 8) == "Größer");    // full fit
  // A 4-byte emoji (U+1F600 = F0 9F 98 80) never survives as a fragment.
  CHECK(utf8Clamp("a\xF0\x9F\x98\x80", 2) == "a");
  CHECK(utf8Clamp("a\xF0\x9F\x98\x80", 5) == "a\xF0\x9F\x98\x80");
  // The panel use: a 300-byte name clamps into a 255-byte buffer on a boundary.
  const std::string longName(300, 'x');
  CHECK(utf8Clamp(longName, 255).size() == 255);
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

// h22: the Save Configuration decision core - the tab's CURRENT base title is the configuration's
// identity, and the save reconciles the tab's source entry with it.
TEST_CASE("saveIntoUserConfig: rename in place, foreign refuses, vanished appends (h22, CFG-04)") {
  const std::vector<std::string> reserved = {"Dipole", "Two Equal Charges", "Quadrupole"};

  // a. Rename in place preserves the position and every other entry.
  std::vector<efapp::UserConfig> configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "A", {"Renamed A", {{9, 9, 9}}}, reserved) == efapp::SaveIntoResult::OK);
  REQUIRE(configs.size() == 3);
  CHECK(configs[0].name == "Renamed A");
  REQUIRE(configs[0].charges.size() == 1);
  CHECK(configs[0].charges[0].charge == 9);
  CHECK(configs[1].name == "B");
  CHECK(configs[1].charges[0].charge == 2);
  CHECK(configs[2].name == "C");
  CHECK(configs[2].charges[0].charge == 3);

  // b. Case-only respell of the tab's own entry: position kept, the candidate's spelling adopted.
  configs = {{"a", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "a", {"A", {{4, 4, 4}}}, reserved) == efapp::SaveIntoResult::OK);
  REQUIRE(configs.size() == 3);
  CHECK(configs[0].name == "A");
  CHECK(configs[0].charges[0].charge == 4);
  CHECK(configs[1].name == "B");
  CHECK(configs[2].name == "C");

  // c. Unrenamed save (target == at): charges replaced in place.
  configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "B", {"B", {{5, 5, 5}}}, reserved) == efapp::SaveIntoResult::OK);
  REQUIRE(configs.size() == 3);
  CHECK(configs[1].name == "B");
  CHECK(configs[1].charges[0].charge == 5);
  CHECK(configs[0].charges[0].charge == 1);

  // d. A foreign collision refuses and mutates nothing.
  configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "A", {"C", {{9, 9, 9}}}, reserved) == efapp::SaveIntoResult::NAME_TAKEN);
  REQUIRE(configs.size() == 3);
  CHECK(configs[0].name == "A");
  CHECK(configs[0].charges[0].charge == 1);
  CHECK(configs[2].name == "C");
  CHECK(configs[2].charges[0].charge == 3);

  // e. A vanished source appends under the fresh name (last position).
  configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "Ghost", {"New", {{7, 7, 7}}}, reserved) == efapp::SaveIntoResult::OK);
  REQUIRE(configs.size() == 4);
  CHECK(configs[3].name == "New");
  CHECK(configs[3].charges[0].charge == 7);

  // f. A vanished source never earns the right to clobber a foreign entry.
  configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "Ghost", {"C", {{9, 9, 9}}}, reserved) == efapp::SaveIntoResult::NAME_TAKEN);
  REQUIRE(configs.size() == 3);
  CHECK(configs[2].name == "C");
  CHECK(configs[2].charges[0].charge == 3);

  // g. Reserved names are refused: a renamed tab's new name, and the unrenamed-preset shape (the
  //    source lookup misses because presets are never user configs in the list).
  configs = {{"A", {{1, 0, 0}}}, {"B", {{2, 1, 1}}}, {"C", {{3, 2, 2}}}};
  CHECK(efapp::saveIntoUserConfig(configs, "A", {"dipole", {{9, 9, 9}}}, reserved) == efapp::SaveIntoResult::RESERVED_NAME);
  CHECK(efapp::saveIntoUserConfig(configs, "dipole", {"dipole", {{9, 9, 9}}}, reserved) == efapp::SaveIntoResult::RESERVED_NAME);
  REQUIRE(configs.size() == 3);
  CHECK(configs[0].name == "A");

  // h. Empty candidate name.
  CHECK(efapp::saveIntoUserConfig(configs, "A", {"   ", {}}, reserved) == efapp::SaveIntoResult::EMPTY_NAME);
}
