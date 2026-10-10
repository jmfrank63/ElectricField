#pragma once
// Port of CElectricFieldDoc::GetData / GetDataFromThreads (ElectricFieldDoc.cpp:139-228): equipotential de-duplication
// ("a very ugly method of getting rid of duplicates") + Bezier adjust, in WORLD units, as pure functions.
#include <atomic>
#include <vector>

#include "ef/bezier.hpp"
#include "ef/charge_system.hpp"
#include "ef/tracer.hpp"

namespace ef {

struct FieldResult {
  std::vector<FieldLine> electric;       // non-empty lines only, Bezier-adjusted, ordered by seed index
  std::vector<PotentialLine> potential;  // de-duplicated, Bezier-adjusted
};

// De-duplication only (no Bezier). `raw` must already be in deterministic job order (D-04); the sort is stable so ties
// (equal potentials) keep job order.
std::vector<PotentialLine> dedupEquipotentials(std::vector<PotentialLine> raw, const ChargeSystem& field, const ComputeParams& p);

// raw lines in job order -> final result (drops empty sink lines, de-dups, adjusts for Bezier).
// `stop` (optional) is polled between stages and per line: once it reads true the function returns early with an
// unspecified PARTIAL result that the caller must discard (cooperative cancel of the finalization, WR-02).
FieldResult finalize(std::vector<FieldLine> rawElectric, std::vector<PotentialLine> rawPotential, const ChargeSystem& field,
                     const ComputeParams& p, const std::atomic<bool>* stop = nullptr);

}  // namespace ef
