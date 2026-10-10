#pragma once
#include "ef/method.hpp"

namespace ef {

// Replaces every read of theApp.options.* in the core. Value type, passed explicitly.
struct ComputeParams {
  unsigned pixelsPerUnit = 100;       // Options::distanceUnitLength (fixed internal constant)
  double chargeRadiusPx = 25.0;       // Options::chargeRadius (float 25.0f in MFC) -> 0.25 world units
  unsigned linesPerUnitCharge = 16;   // Options::numLinesOnUnitCharge
  bool calculateEquipotentials = true;
  double potentialInterval = 0.1;
  Method method = Method::Euler;

  // r = chargeRadius / distanceUnitLength  (StartCalculating, FieldLinesCalculator.cpp:363)
  double seedRadius() const { return chargeRadiusPx / static_cast<double>(pixelsPerUnit); }
  // Charge::HitCharge  (Charge.h:25): chargeRadius / 1.5 / distanceUnitLength
  double hitRadius() const { return chargeRadiusPx / 1.5 / static_cast<double>(pixelsPerUnit); }
};

}  // namespace ef
