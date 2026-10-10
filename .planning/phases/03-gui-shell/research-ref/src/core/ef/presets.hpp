#pragma once
#include <array>
#include <cstddef>
#include <string_view>

#include "ef/charge_system.hpp"

namespace ef {

inline constexpr std::size_t kPresetCount = 9;

struct PresetCharge {
  int charge;
  double x;
  double y;
};

struct Preset {
  std::string_view slug;         // file-name safe, stable
  std::string_view displayName;  // ElectricField.rc string table (docName field)
  std::array<PresetCharge, 4> charges;
  int count;
};

const std::array<Preset, kPresetCount>& presets();
ChargeSystem makeSystem(const Preset& p);

}  // namespace ef
