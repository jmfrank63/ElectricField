#include "ef/finalizer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ef {

std::vector<PotentialLine> dedupEquipotentials(std::vector<PotentialLine> raw, const ChargeSystem& field, const ComputeParams& p) {
  if (raw.empty()) return raw;

  bool hasDifferentSigns = false;
  field.totalCharge(hasDifferentSigns);

  // in this case there cannot be duplicates
  if (field.charges.size() == 2 && hasDifferentSigns) return raw;

  // sort lines by potential (stable: ties keep the deterministic job order, D-04)
  std::stable_sort(raw.begin(), raw.end(), [](const PotentialLine& a, const PotentialLine& b) { return a.potential < b.potential; });

  double potential = 0;
  std::vector<PotentialLine> equipotentials;
  std::vector<PotentialLine> out;

  // pick a point with no symmetric arrangement of charges around it (D-05: same reference point as MFC)
  const Vec2 ref(-7., 5.5);
  const double interval = p.potentialInterval;
  const double px = static_cast<double>(p.pixelsPerUnit);

  for (const auto& line : raw) {
    // all lines that are in an interval smaller than a third of the potential interval are considered at the same potential
    if (std::abs(potential - line.potential) < interval / 3. || 0 == potential) {
      potential = line.potential;
      equipotentials.push_back(line);
    } else {
      // all lines at a certain potential are in 'equipotentials': sort them by distance of the weight centre to ref
      std::stable_sort(equipotentials.begin(), equipotentials.end(), [&ref](const PotentialLine& a, const PotentialLine& b) {
        return (ref - a.weightCenter).Length() < (ref - b.weightCenter).Length();
      });

      for (auto& equiline : equipotentials) {
        if (!out.empty()) {
          // weight centres more than 2*chargeRadius apart => different lines, not duplicates of the same one
          if ((out.back().weightCenter - equiline.weightCenter).Length() * px > 2. * p.chargeRadiusPx ||
              std::abs(potential - out.back().potential) > interval * 2. / 3.)
            out.push_back(std::move(equiline));
        } else {
          out.push_back(std::move(equiline));
        }
      }
      equipotentials.clear();

      potential = line.potential;
      equipotentials.push_back(line);
    }
  }

  // the last group is appended as is (original behaviour)
  out.insert(out.end(), std::make_move_iterator(equipotentials.begin()), std::make_move_iterator(equipotentials.end()));
  return out;
}

FieldResult finalize(std::vector<FieldLine> rawElectric, std::vector<PotentialLine> rawPotential, const ChargeSystem& field,
                     const ComputeParams& p, const std::atomic<bool>* stop) {
  const auto stopped = [stop] { return stop != nullptr && stop->load(std::memory_order_relaxed); };
  FieldResult r;
  for (auto& l : rawElectric)
    if (!l.points.empty()) r.electric.push_back(std::move(l));  // sink jobs record nothing; they only spawn equipotentials
  if (stopped()) return r;
  r.potential = dedupEquipotentials(std::move(rawPotential), field, p);
  for (auto& l : r.electric) {
    if (stopped()) return r;
    adjustForBezier(l.points);
  }
  for (auto& l : r.potential) {
    if (stopped()) return r;
    adjustForBezier(l.points);
  }
  return r;
}

}  // namespace ef
