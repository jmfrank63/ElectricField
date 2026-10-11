#pragma once
#include <cstddef>
#include <vector>

#include "ef/charge_system.hpp"

namespace ef {

struct Seed {
  std::size_t chargeIndex = 0;
  int charge = 0;               // value of the charge the seed belongs to
  int totalCharge = 0;
  bool hasDifferentSigns = false;
  double angle = 0;             // actual seed angle
  bool first = false;           // first seed of its charge == only one allowed to spawn equipotentials
  bool isSource = false;        // sign(totalCharge) == sign(charge): lines are recorded
  Vec2 point;
};

// FieldLinesCalculator::StartCalculating (FieldLinesCalculator.cpp:223-259), without the thread start.
std::vector<Seed> planSeeds(const ChargeSystem& field, const ComputeParams& p);

// FieldLinesCalculator::GetNumberOfElectricFieldLines (FieldLinesCalculator.cpp:261), with the unsigned-wrap defect fixed (std::abs).
int expectedLineCount(const ChargeSystem& field, const ComputeParams& p);

}  // namespace ef
