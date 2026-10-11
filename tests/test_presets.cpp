// The 9 preset charge tables are a committed snapshot of the original MFC ConfigDocs.cpp (the MFC tree was
// deleted in Phase 4). src/core/ef/presets.cpp must reproduce these values exactly; the QA-05 visual
// baselines in docs/baseline/mfc/ were rendered by the original app from the same charges.
#include "doctest/doctest.h"

#include <cstddef>

#include "ef/presets.hpp"

using namespace ef;

namespace {

struct Expect {
  int charge;
  double x, y;
};

struct DocExpect {
  const char* slug;
  int count;
  Expect charges[4];
};

constexpr DocExpect kSnapshot[9] = {
    {"dipole", 2, {{1, -2.0, 0.0}, {-1, 2.0, 0.0}, {0, 0.0, 0.0}, {0, 0.0, 0.0}}},
    {"two-equal-charges", 2, {{1, -2.0, 0.0}, {1, 2.0, 0.0}, {0, 0.0, 0.0}, {0, 0.0, 0.0}}},
    {"two-unequal-charges-same-sign", 2, {{1, -2.0, 0.0}, {2, 2.0, 0.0}, {0, 0.0, 0.0}, {0, 0.0, 0.0}}},
    {"two-unequal-charges-different-signs", 2, {{2, -2.0, 0.0}, {-1, 2.0, 0.0}, {0, 0.0, 0.0}, {0, 0.0, 0.0}}},
    {"quadrupole", 4, {{1, -1.5, -1.5}, {-1, 1.5, -1.5}, {-1, -1.5, 1.5}, {1, 1.5, 1.5}}},
    {"four-charges-same-sign", 4, {{1, -3.0, 0.0}, {1, -1.0, 0.0}, {1, 1.0, 0.0}, {1, 3.0, 0.0}}},
    {"two-dipoles-same-direction", 4, {{1, -3.0, 0.0}, {-1, -1.0, 0.0}, {1, 1.0, 0.0}, {-1, 3.0, 0.0}}},
    {"two-dipoles-opposed-direction", 4, {{1, -3.0, 0.0}, {-1, -1.0, 0.0}, {-1, 1.0, 0.0}, {1, 3.0, 0.0}}},
    {"parallel-dipoles", 4, {{1, -1.5, -1.5}, {-1, 1.5, -1.5}, {1, -1.5, 1.5}, {-1, 1.5, 1.5}}},
};

}  // namespace

TEST_CASE("nine presets, in the MFC document-template order, with the .rc display names") {
  static_assert(kPresetCount == 9, "");
  const char* names[] = {"Dipole", "Two Equal Charges", "Two Unequal Charges Same Sign",
                         "Two Unequal Charges Different Signs", "Quadrupole", "Four Charges Same Sign",
                         "Two Dipoles Same Direction", "Two Dipoles Opposed Direction", "Parallel Dipoles"};
  for (std::size_t i = 0; i < kPresetCount; ++i) CHECK(presets()[i].displayName == names[i]);
}

TEST_CASE("preset charges equal the committed snapshot of the MFC ConfigDocs.cpp") {
  static_assert(kPresetCount == sizeof(kSnapshot) / sizeof(kSnapshot[0]), "");
  for (std::size_t i = 0; i < kPresetCount; ++i) {
    CAPTURE(presets()[i].slug);
    CHECK(presets()[i].slug == kSnapshot[i].slug);
    REQUIRE(presets()[i].count == kSnapshot[i].count);
    for (int k = 0; k < kSnapshot[i].count; ++k) {
      CHECK(presets()[i].charges[static_cast<std::size_t>(k)].charge == kSnapshot[i].charges[static_cast<std::size_t>(k)].charge);
      CHECK(presets()[i].charges[static_cast<std::size_t>(k)].x == doctest::Approx(kSnapshot[i].charges[static_cast<std::size_t>(k)].x));
      CHECK(presets()[i].charges[static_cast<std::size_t>(k)].y == doctest::Approx(kSnapshot[i].charges[static_cast<std::size_t>(k)].y));
    }
  }
}
