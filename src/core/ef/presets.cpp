#include "ef/presets.hpp"

namespace ef {

const std::array<Preset, kPresetCount>& presets() {
  // Order = AddDocTemplate order in ElectricField.cpp:113-185 = ConfigDocs.cpp order. Values: ConfigDocs.cpp.
  static const std::array<Preset, kPresetCount> table{{
      {"dipole", "Dipole", {{{+1, -2, 0}, {-1, 2, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
      {"two-equal-charges", "Two Equal Charges", {{{+1, -2, 0}, {+1, 2, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
      {"two-unequal-charges-same-sign", "Two Unequal Charges Same Sign", {{{+1, -2, 0}, {+2, 2, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
      {"two-unequal-charges-different-signs", "Two Unequal Charges Different Signs", {{{+2, -2, 0}, {-1, 2, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
      {"quadrupole", "Quadrupole", {{{+1, -1.5, -1.5}, {-1, 1.5, -1.5}, {-1, -1.5, 1.5}, {+1, 1.5, 1.5}}}, 4},
      {"four-charges-same-sign", "Four Charges Same Sign", {{{+1, -3, 0}, {+1, -1, 0}, {+1, 1, 0}, {+1, 3, 0}}}, 4},
      {"two-dipoles-same-direction", "Two Dipoles Same Direction", {{{+1, -3, 0}, {-1, -1, 0}, {+1, 1, 0}, {-1, 3, 0}}}, 4},
      {"two-dipoles-opposed-direction", "Two Dipoles Opposed Direction", {{{+1, -3, 0}, {-1, -1, 0}, {-1, 1, 0}, {+1, 3, 0}}}, 4},
      {"parallel-dipoles", "Parallel Dipoles", {{{+1, -1.5, -1.5}, {-1, 1.5, -1.5}, {+1, -1.5, 1.5}, {-1, 1.5, 1.5}}}, 4},
  }};
  return table;
}

ChargeSystem makeSystem(const Preset& p) {
  ChargeSystem s;
  for (int i = 0; i < p.count; ++i) {
    const PresetCharge& pc = p.charges[static_cast<std::size_t>(i)];
    Charge c;
    c.charge = pc.charge;
    c.position = Vec2(pc.x, pc.y);
    s.charges.push_back(c);
  }
  return s;
}

}  // namespace ef
