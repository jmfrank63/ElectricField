#include "doctest/doctest.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "ef/presets.hpp"

using namespace ef;

TEST_CASE("nine presets, in the MFC document-template order, with the .rc display names") {
  static_assert(kPresetCount == 9, "");
  const char* names[] = {"Dipole", "Two Equal Charges", "Two Unequal Charges Same Sign",
                         "Two Unequal Charges Different Signs", "Quadrupole", "Four Charges Same Sign",
                         "Two Dipoles Same Direction", "Two Dipoles Opposed Direction", "Parallel Dipoles"};
  for (std::size_t i = 0; i < kPresetCount; ++i) CHECK(presets()[i].displayName == names[i]);
}

TEST_CASE("preset charges equal ElectricField/ConfigDocs.cpp (parsed from the MFC source)") {
  // The test runs with cwd = repository root (build.zig: run_tests.setCwd). Phase 4 deletes ElectricField/ and this check.
  std::ifstream in("ElectricField/ConfigDocs.cpp");
  if (!in.good()) {
    MESSAGE("ElectricField/ConfigDocs.cpp not found - skipping MFC cross-check");
    return;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  // split into OnNewDocument bodies and collect (charge, x, y) in push_back order
  std::vector<std::vector<PresetCharge>> docs;
  const std::regex token(R"(IMPLEMENT_DYNCREATE\(|charge\.charge\s*=\s*(-?\d+)\s*;|charge\.position\s*=\s*Vector2D<double>\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\)\s*;|push_back\(charge\))");
  PresetCharge cur{1, 0, 0};
  for (auto it = std::sregex_iterator(src.begin(), src.end(), token); it != std::sregex_iterator(); ++it) {
    const std::smatch& m = *it;
    if (m.str(0).rfind("IMPLEMENT_DYNCREATE", 0) == 0) docs.emplace_back();
    else if (m[1].matched) cur.charge = std::stoi(m[1]);
    else if (m[2].matched) { cur.x = std::stod(m[2]); cur.y = std::stod(m[3]); }
    else docs.back().push_back(cur);
  }
  // the first IMPLEMENT_DYNCREATE match inside ConfigDocs.cpp is the first document; there are 9
  REQUIRE(docs.size() == kPresetCount);
  for (std::size_t i = 0; i < kPresetCount; ++i) {
    CAPTURE(presets()[i].slug);
    REQUIRE(static_cast<int>(docs[i].size()) == presets()[i].count);
    for (std::size_t k = 0; k < docs[i].size(); ++k) {
      CHECK(docs[i][k].charge == presets()[i].charges[k].charge);
      CHECK(docs[i][k].x == doctest::Approx(presets()[i].charges[k].x));
      CHECK(docs[i][k].y == doctest::Approx(presets()[i].charges[k].y));
    }
  }
}
