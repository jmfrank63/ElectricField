// v2.1 (SES-01/02): the open-tab session file - round-trip of titles, preset sources, methods and charges,
// plus skip rules. No SDL/ImGui.
#include "app/session_file.hpp"

#include <filesystem>

#include "doctest/doctest.h"

namespace {

efapp::SavedTab sample() {
  return {"Dipole 1", 0, "euler", false, {}, {{1, -2.0, 0.0}, {-1, 2.0, 0.0}}};
}

}  // namespace

TEST_CASE("serialize -> parse round-trips titles, sources, methods and charges (SES-01/02)") {
  const std::vector<efapp::SavedTab> tabs = {
      sample(),
      {"My Field", efapp::kNoPreset, "adaptive-dormand-prince", false, {}, {{2, 0.5, -0.25}}},
      {"Empty Configuration 1", efapp::kNoPreset, "", false, {}, {{1, 0.0, 0.0}}},
  };
  const efapp::ParsedSession r = efapp::parseSession(efapp::serializeSession(tabs));
  CHECK(r.skippedTabs == 0);
  CHECK(r.skippedCharges == 0);
  REQUIRE(r.value.size() == 3);
  CHECK(r.value[0].title == "Dipole 1");
  CHECK(r.value[0].preset == 0);
  CHECK(r.value[0].method == "euler");
  REQUIRE(r.value[0].charges.size() == 2);
  CHECK(r.value[0].charges[1].charge == -1);
  CHECK(r.value[1].title == "My Field");
  CHECK(r.value[1].preset == efapp::kNoPreset);
  CHECK(r.value[1].method == "adaptive-dormand-prince");
  CHECK(r.value[1].charges[0].x == doctest::Approx(0.5));
  CHECK(r.value[2].preset == efapp::kNoPreset);
}

TEST_CASE("invalid content is skipped and counted; foreign sections ignored (SES-01)") {
  efapp::ParsedSession r = efapp::parseSession("[tab \"No charges\"]\n");
  CHECK(r.value.empty());
  CHECK(r.skippedTabs == 1);

  r = efapp::parseSession("[tab \"Bad\"]\ncharge = 1, 0\ncharge = 2, 1, 1\n");
  REQUIRE(r.value.size() == 1);
  CHECK(r.skippedCharges == 1);
  REQUIRE(r.value[0].charges.size() == 1);

  r = efapp::parseSession("[configuration \"X\"]\ncharge = 1, 0, 0\n");
  CHECK(r.value.empty());  // foreign section ignored
}

TEST_CASE("file round-trip and missing-file default (SES-01)") {
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-session";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "session.ini";
  CHECK(efapp::loadSession(file).empty());

  std::string err;
  REQUIRE(efapp::saveSession(file, {sample()}, err));
  CHECK(err.empty());
  const std::vector<efapp::SavedTab> loaded = efapp::loadSession(file);
  REQUIRE(loaded.size() == 1);
  CHECK(loaded[0].title == "Dipole 1");
  CHECK(loaded[0].method == "euler");
  REQUIRE(loaded[0].charges.size() == 2);

  std::error_code ec;
  std::filesystem::remove(file, ec);
  std::filesystem::remove(tmp, ec);
}

TEST_CASE("the session records the current collection (v2.2 COL-04)") {
  const std::vector<efapp::SavedTab> tabs = {sample()};
  const efapp::ParsedSession r = efapp::parseSession(efapp::serializeSession(tabs, "My Collection"));
  CHECK(r.collection == "My Collection");
  REQUIRE(r.value.size() == 1);

  // Files from before collections parse as the Original collection.
  const efapp::ParsedSession legacy = efapp::parseSession(efapp::serializeSession(tabs));
  CHECK(legacy.collection == "Original");

  // The collection key survives the file round-trip.
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-session-col";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "session.ini";
  std::string err;
  REQUIRE(efapp::saveSession(file, tabs, "My Collection", err));
  const efapp::ParsedSession loaded = efapp::parseSession(efapp::serializeSession(efapp::loadSessionFull(file).tabs, efapp::loadSessionFull(file).collection));
  CHECK(loaded.collection == "My Collection");
  std::error_code ec;
  std::filesystem::remove(file, ec);
  std::filesystem::remove(tmp, ec);
}

TEST_CASE("autocalc is persisted per tab (v2.2 CALC-03)") {
  std::vector<efapp::SavedTab> tabs = {sample()};
  tabs[0].autoRecalc = true;
  const efapp::ParsedSession r = efapp::parseSession(efapp::serializeSession(tabs));
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].autoRecalc);

  efapp::ParsedSession r2 = efapp::parseSession("[tab \"X\"]\nautocalc = true\ncharge = 1, 0, 0\n");
  REQUIRE(r2.value.size() == 1);
  CHECK(r2.value[0].autoRecalc);
}
