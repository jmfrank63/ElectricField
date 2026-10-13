#pragma once
#include <algorithm>

#include "ef/method.hpp"

namespace ef {

// Replaces every read of theApp.options.* in the core. Value type, passed explicitly.
struct ComputeParams {
  unsigned pixelsPerUnit = 100;       // Options::distanceUnitLength (fixed internal constant)
  double chargeRadiusPx = 25.0;       // Options::chargeRadius (float 25.0f in MFC) -> 0.25 world units
  unsigned linesPerUnitCharge = 16;   // Options::numLinesOnUnitCharge
  bool calculateEquipotentials = true;
  double potentialInterval = 0.1;
  unsigned numThreads = 4;            // Options::numThreads (Options.cpp:22)
  Method method = Method::Euler;
  // v2.6 phase 2: px per world unit the computation should adapt to, in LOGICAL px: the GUI stamps its
  // zoom-relative scale (100 * zoom, display-scale-independent), so kRefPpu = 100 means "the default
  // view at every DPI" and the px constants below (kSeedPx, the hit target, kRingPx) are logical px -
  // the same space as DrawStyle::chargeRadiusPx. 0/unset = legacy; ef-cli never sets it, so parity
  // output is bit-identical.
  double viewScale = 0;

  // Adaptation engages ONLY above the reference ppu: at/below it every radius is exactly the legacy value.
  static constexpr double kRefPpu = 100;
  static constexpr double kSeedPx = 2.0;  // the seed circle is ~2 px on screen when zoomed in
  // Floor = 10x the near-field step 0.001: keeps h/r <= 0.1 for the fixed-step methods (load-bearing).
  static constexpr double kMinSeedWorld = 0.01;

  bool adapts() const { return viewScale > kRefPpu; }

  // Charge radius in world units: chargeRadiusPx / pixelsPerUnit == 0.25 with the defaults (D-03).
  double chargeRadiusWorld() const { return chargeRadiusPx / static_cast<double>(pixelsPerUnit); }
  // r = chargeRadius / distanceUnitLength  (StartCalculating, FieldLinesCalculator.cpp:231);
  // zoom-adapted above kRefPpu (the drawn dot must cover the floored seed circle: viewScale <= 25/0.01
  // with the default 25 px dot - the applied Options charge size caps the GUI's zoom ceiling instead).
  double seedRadius() const {
    if (!adapts()) return chargeRadiusWorld();
    return std::clamp(kSeedPx / viewScale, kMinSeedWorld, chargeRadiusWorld());
  }
  // Charge::HitCharge  (Charge.h:25): chargeRadius / 1.5 / distanceUnitLength; zoom-adapted above
  // kRefPpu so endpoints stay behind the drawn dot (kHitPx = chargeRadiusPx / 1.5 px on screen). Two
  // clamps: the legacy value above (25/1.5/100 with the defaults, so the adapted radius shrinks
  // continuously out of it) and the seed circle below - a line is BORN on the seed circle, so a hit
  // radius above it would end the line on its own charge at the first step (the MFC numbers keep
  // seed = 1.5 * hit; at the ceiling the seed is the binding constraint and endpoints stay in the dot).
  double hitRadius() const {
    const double legacy = chargeRadiusPx / 1.5 / static_cast<double>(pixelsPerUnit);
    if (!adapts()) return legacy;
    return std::min(std::clamp(chargeRadiusPx / 1.5 / viewScale, kMinSeedWorld, legacy), seedRadius());
  }
};

}  // namespace ef
