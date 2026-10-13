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

TEST_CASE("grid settings round-trip with the intersections flag (v2.3 GRID, v2.5 GRID-06)") {
  std::vector<efapp::SavedTab> tabs;
  efapp::SavedTab t = sample();
  t.grid.active = true;
  t.grid.visible = false;
  t.grid.polar = false;
  t.grid.intersectionsOnly = true;
  t.grid.origin = {1.25, -2.0};
  t.grid.spacing = 0.75;
  t.grid.angleStepDeg = 12.5;
  tabs.push_back(t);
  const efapp::ParsedSession r = efapp::parseSession(efapp::serializeSession(tabs));
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].grid.active);
  CHECK_FALSE(r.value[0].grid.visible);
  CHECK(r.value[0].grid.intersectionsOnly);
  CHECK(r.value[0].grid.origin.X == doctest::Approx(1.25));
  CHECK(r.value[0].grid.origin.Y == doctest::Approx(-2.0));
  CHECK(r.value[0].grid.spacing == doctest::Approx(0.75));
  CHECK(r.value[0].grid.angleStepDeg == doctest::Approx(12.5));

  // A pre-v2.5 session (7 fields, no intersections flag) parses: the flag falls back to false.
  const std::string legacy = "[tab \"Dipole 1\"]\npreset = 0\ngrid = 1,1,0,0,0,0.5,15\ncharge = 1, -2, 0\n";
  const efapp::ParsedSession g = efapp::parseSession(legacy);
  REQUIRE(g.value.size() == 1);
  CHECK(g.value[0].grid.active);
  CHECK_FALSE(g.value[0].grid.intersectionsOnly);
  CHECK(g.value[0].grid.spacing == doctest::Approx(0.5));
  CHECK(g.value[0].grid.angleStepDeg == doctest::Approx(15.0));
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

// v2.6 phase 3: the session file writes the same extended 6-field charge line as the collections
// (symmetric grammar, pinned decision): `charge = value, x, y, sizePx, id, name`, names comma-free
// raw UTF-8, empty name as a trailing empty field.
TEST_CASE("extended 6-field charge lines round-trip size, id and name losslessly (v2.6 phase 3)") {
  std::vector<efapp::SavedTab> tabs;
  efapp::SavedTab t = sample();
  t.charges = {{1, -2.0, 0.0, 40.0f, 1, "My Charge"}, {-1, 2.0, 0.0, 0.0f, 2, "Größerer Test"}, {2, 0.5, -0.25, 3.0f, 3, "Has, Comma"}};
  tabs.push_back(t);
  const std::string text = efapp::serializeSession(tabs);
  const efapp::ParsedSession r = efapp::parseSession(text);
  CHECK(r.skippedTabs == 0);
  CHECK(r.skippedCharges == 0);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 3);
  CHECK(r.value[0].charges[0].sizePx == doctest::Approx(40.0));
  CHECK(r.value[0].charges[0].id == 1);
  CHECK(r.value[0].charges[0].name == "My Charge");
  CHECK(r.value[0].charges[1].sizePx == 0.0f);
  CHECK(r.value[0].charges[1].id == 2);
  CHECK(r.value[0].charges[1].name == "Größerer Test");  // non-ASCII passes through byte-for-byte
  CHECK(r.value[0].charges[2].id == 3);
  CHECK(r.value[0].charges[2].name == "Has Comma");  // the model's ',' came back stripped

  // serialize -> parse -> serialize is stable (the second write is byte-identical to the first).
  CHECK(efapp::serializeSession(r.value, "Original") == efapp::serializeSession(tabs, "Original"));
}

TEST_CASE("empty name trailing field and legacy/skip arities in the session file (v2.6 phase 3)") {
  // An empty name writes a trailing empty field and parses back empty.
  std::vector<efapp::SavedTab> tabs;
  efapp::SavedTab t = sample();
  t.charges = {{1, 0, 0, 0.0f, 1, ""}};
  tabs.push_back(t);
  const std::string text = efapp::serializeSession(tabs);
  CHECK(text.find("charge = 1, 0, 0, 0, 1, \n") != std::string::npos);
  const efapp::ParsedSession r = efapp::parseSession(text);
  REQUIRE(r.value.size() == 1);
  REQUIRE(r.value[0].charges.size() == 1);
  CHECK(r.value[0].charges[0].id == 1);
  CHECK(r.value[0].charges[0].name.empty());

  // Legacy 3-field lines keep parsing with defaults; 4/5-field lines are skipped and counted.
  const std::string mixed = "[tab \"T\"]\n"
                            "charge = 1, 0, 0\n"
                            "charge = 2, 1, 1, 25\n"
                            "charge = 2, 1, 1, 25, 7\n"
                            "charge = -1, 2, 2, 10, 4, Named\n";
  const efapp::ParsedSession m = efapp::parseSession(mixed);
  CHECK(m.skippedCharges == 2);
  REQUIRE(m.value.size() == 1);
  REQUIRE(m.value[0].charges.size() == 2);
  CHECK(m.value[0].charges[0].sizePx == 0.0f);
  CHECK(m.value[0].charges[0].id == 0);
  CHECK(m.value[0].charges[0].name.empty());
  CHECK(m.value[0].charges[1].sizePx == doctest::Approx(10.0));
  CHECK(m.value[0].charges[1].id == 4);
  CHECK(m.value[0].charges[1].name == "Named");

  // WR-01: the extended size field obeys the mutator contract - 0 (inherit) or 2..120 px; anything
  // else is a malformed line, skipped and counted, symmetric with the collection parser.
  const std::string sizes = "[tab \"Sizes\"]\n"
                            "charge = 1, 0, 0, 0, 1, Zero\n"       // 0 = inherit: legal
                            "charge = 1, 1, 0, 2, 2, Floor\n"      // the 2 px floor: legal
                            "charge = 1, 2, 0, 120, 3, Ceiling\n"  // the 120 px ceiling: legal
                            "charge = 1, 3, 0, -5, 4, Negative\n"  // negative: malformed
                            "charge = 1, 4, 0, 1e30, 5, Huge\n";   // screen-filling dot: malformed
  const efapp::ParsedSession s = efapp::parseSession(sizes);
  CHECK(s.skippedCharges == 2);
  REQUIRE(s.value.size() == 1);
  REQUIRE(s.value[0].charges.size() == 3);
  CHECK(s.value[0].charges[0].sizePx == 0.0f);
  CHECK(s.value[0].charges[1].sizePx == doctest::Approx(2.0));
  CHECK(s.value[0].charges[2].sizePx == doctest::Approx(120.0));

  // Pre-v2.6 files (3-field lines) still parse exactly as before.
  const efapp::ParsedSession legacy = efapp::parseSession("[tab \"Dipole 1\"]\ncharge = 1, -2, 0\ncharge = -1, 2, 0\n");
  CHECK(legacy.skippedCharges == 0);
  REQUIRE(legacy.value.size() == 1);
  REQUIRE(legacy.value[0].charges.size() == 2);
  CHECK(legacy.value[0].charges[1].charge == -1);
  CHECK(legacy.value[0].charges[1].id == 0);
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

  // v2.6 REC-02: hasCollection is true only for a genuinely stored, non-empty collection line - a blank
  // or absent line parses as the "Original" default but must stay distinguishable (a legacy pointer
  // carries exactly `collection = <name>` and nothing else; a fresh sandbox pointer is absent entirely).
  CHECK(r.hasCollection);
  CHECK(legacy.hasCollection);  // serializeSession always writes the line, so this one is genuinely stored
  const efapp::ParsedSession blank = efapp::parseSession("collection =\n[tab \"Dipole 1\"]\ncharge = 1, -2, 0\n");
  CHECK(blank.collection == "Original");
  CHECK_FALSE(blank.hasCollection);
  const efapp::ParsedSession absent = efapp::parseSession("[tab \"Dipole 1\"]\npreset = 0\ncharge = 1, -2, 0\n");
  CHECK(absent.collection == "Original");
  CHECK_FALSE(absent.hasCollection);
  const efapp::ParsedSession spaced = efapp::parseSession("collection =   \n[tab \"Dipole 1\"]\ncharge = 1, -2, 0\n");
  CHECK(spaced.collection == "Original");
  CHECK_FALSE(spaced.hasCollection);  // trailing spaces trim to a blank value, same as `collection =`

  // The collection key survives the file round-trip.
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-session-col";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "session.ini";
  std::string err;
  REQUIRE(efapp::saveSession(file, tabs, "My Collection", err));
  const efapp::ParsedSession loaded = efapp::parseSession(efapp::serializeSession(efapp::loadSessionFull(file).tabs, efapp::loadSessionFull(file).collection));
  CHECK(loaded.collection == "My Collection");
  // ... and the flag survives too (stored line / blank line, the blank being what the quit dump of
  // collection-less preset tabs writes - REC-02 must not seed from it).
  CHECK(efapp::loadSessionFull(file).hasCollection);
  REQUIRE(efapp::saveSession(file, tabs, std::string(), err));
  const efapp::SavedSession blankFull = efapp::loadSessionFull(file);
  CHECK(blankFull.collection == "Original");
  CHECK_FALSE(blankFull.hasCollection);
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

// h22: the session persists each tab's source configuration so Save Configuration can still rename
// the entry in place after a restart. Additive and optional: absent lines keep the title derivation.
TEST_CASE("the tab's source configuration round-trips (h22)") {
  std::vector<efapp::SavedTab> tabs = {sample()};
  tabs[0].source = "My Dipole";  // spaces intact
  efapp::ParsedSession r = efapp::parseSession(efapp::serializeSession(tabs));
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].source == "My Dipole");

  // Double quotes are stripped on serialize (free-form renamed titles may contain them).
  tabs[0].source = "Renamed \"Field\"";
  r = efapp::parseSession(efapp::serializeSession(tabs));
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].source == "Renamed Field");

  // An empty source emits no line and parses back empty (backward compatible with today's files).
  tabs[0].source.clear();
  const std::string text = efapp::serializeSession(tabs);
  CHECK(text.find("source =") == std::string::npos);
  r = efapp::parseSession(text);
  REQUIRE(r.value.size() == 1);
  CHECK(r.value[0].source.empty());

  // Legacy-format text without any source line parses exactly as before.
  const std::string legacy = "[tab \"Dipole 1\"]\npreset = 0\nmethod = euler\ncharge = 1, -2, 0\ncharge = -1, 2, 0\n"
                             "[tab \"My Field\"]\npreset = none\ncharge = 2, 0.5, 0\n";
  const efapp::ParsedSession l = efapp::parseSession(legacy);
  CHECK(l.skippedTabs == 0);
  REQUIRE(l.value.size() == 2);
  CHECK(l.value[0].title == "Dipole 1");
  CHECK(l.value[0].source.empty());
  CHECK(l.value[1].title == "My Field");
  CHECK(l.value[1].source.empty());
}

// 261012-h26: the session pointer's recents list - `recent = <path>` lines, newest first, persisted with
// the collection tag. The pointer file has no tabs; the per-collection tab files never write any.
TEST_CASE("recents round-trip in the session pointer (261012-h26)") {
  const std::vector<std::string> recents = {"/tmp/ef/Second.ini", "/tmp/ef/First With Spaces.ini"};
  const std::string text = efapp::serializeSession({}, "Original", recents);
  const efapp::ParsedSession r = efapp::parseSession(text);
  CHECK(r.collection == "Original");
  CHECK(r.value.empty());
  REQUIRE(r.recents.size() == 2);
  CHECK(r.recents[0] == "/tmp/ef/Second.ini");
  CHECK(r.recents[1] == "/tmp/ef/First With Spaces.ini");  // spaces intact, one entry per line

  // The file round-trip preserves them (saveSession with recents / loadSessionFull).
  const auto tmp = std::filesystem::temp_directory_path() / "ef-test-session-recents";
  std::filesystem::create_directories(tmp);
  const auto file = tmp / "session.ini";
  std::string err;
  REQUIRE(efapp::saveSession(file, {}, "Pointer", recents, err));
  const efapp::SavedSession loaded = efapp::loadSessionFull(file);
  CHECK(loaded.collection == "Pointer");
  REQUIRE(loaded.recents.size() == 2);
  CHECK(loaded.recents[0] == "/tmp/ef/Second.ini");
  std::error_code ec;
  std::filesystem::remove(file, ec);
  std::filesystem::remove(tmp, ec);

  // Legacy text without recents parses to an empty list.
  CHECK(efapp::parseSession(efapp::serializeSession({sample()}, "X")).recents.empty());

  // v2.6 REC-02: seeding the pointer's collection into the recents list. Entries are full paths, so
  // membership is compared against the key itself.
  const std::string key = "/tmp/ef/Collections/Original.ini";
  CHECK(efapp::seedRecents({}, key) == std::vector<std::string>{key});  // empty list -> the key alone

  const std::vector<std::string> withKey = {"/tmp/ef/Second.ini", key};
  CHECK(efapp::seedRecents(withKey, key) == withKey);  // already present -> unchanged, order intact

  CHECK(efapp::seedRecents(withKey, std::string()) == withKey);  // empty key -> nothing to seed, list unchanged

  const std::vector<std::string> seeded = efapp::seedRecents({"/tmp/ef/Second.ini", "/tmp/ef/Third.ini"}, key);
  REQUIRE(seeded.size() == 3);
  CHECK(seeded[0] == key);  // the pointer's collection is by definition the most recently opened
  CHECK(seeded[1] == "/tmp/ef/Second.ini");
  CHECK(seeded[2] == "/tmp/ef/Third.ini");

  std::vector<std::string> full;
  for (int i = 0; i < 10; ++i) full.push_back("/tmp/ef/R" + std::to_string(i) + ".ini");
  const std::vector<std::string> capped = efapp::seedRecents(full, key);
  REQUIRE(capped.size() == efapp::kMaxRecentCollections);
  CHECK(capped[0] == key);
  CHECK(capped[1] == "/tmp/ef/R0.ini");
  CHECK(capped.back() == "/tmp/ef/R8.ini");  // the oldest of the previous ten fell off

  // v2.6 (phase 1 review): the cap is unconditional - the parser admits up to 100 recent lines, so an
  // over-cap pointer (hand-edited / corrupt-but-parseable) comes back capped whether or not the key
  // was inserted.
  std::vector<std::string> overCap;
  for (int i = 0; i < 12; ++i) overCap.push_back("/tmp/ef/N" + std::to_string(i) + ".ini");
  const std::vector<std::string> cappedInsert = efapp::seedRecents(overCap, key);
  REQUIRE(cappedInsert.size() == efapp::kMaxRecentCollections);
  CHECK(cappedInsert[0] == key);  // inserted at the front, then capped with drop
  CHECK(cappedInsert[1] == "/tmp/ef/N0.ini");
  CHECK(cappedInsert.back() == "/tmp/ef/N8.ini");

  std::vector<std::string> overCapPresent = overCap;
  overCapPresent[5] = key;  // key already in the list: order intact, nothing re-inserted, the tail beyond the cap falls off
  const std::vector<std::string> cappedPresent = efapp::seedRecents(overCapPresent, key);
  REQUIRE(cappedPresent.size() == efapp::kMaxRecentCollections);
  CHECK(cappedPresent[0] == "/tmp/ef/N0.ini");
  CHECK(cappedPresent[5] == key);
  CHECK(cappedPresent.back() == "/tmp/ef/N9.ini");
}
