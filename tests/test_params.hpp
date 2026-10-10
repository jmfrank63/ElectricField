#pragma once
#include "ef/params.hpp"

// Debug builds (O0 + UBSan) run the tracer 10-30x slower than ReleaseFast; the equipotential jobs dominate the cost
// (they scale with 1/potentialInterval, not with linesPerUnitCharge). Tests that sweep many presets use these lighter
// parameters; full MFC defaults are exercised by ef-cli in the 3-OS parity job (ReleaseFast).
inline ef::ComputeParams lightParams() {
  ef::ComputeParams p;
  p.linesPerUnitCharge = 4;
  p.potentialInterval = 0.5;
  p.numThreads = 4;
  return p;
}
