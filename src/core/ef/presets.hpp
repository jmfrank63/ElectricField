#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ef/charge_system.hpp"

namespace ef {

inline constexpr std::size_t kPresetCount = 9;

// The one transport type for collections/sessions/API. The trailing identity/draw fields mirror the
// persisted line order (value, x, y, sizePx, id, name - pinned) so a positional 6-field initializer
// reads exactly like the file line; defaults keep the 9-entry preset table and every legacy literal
// compiling (v2.6 phase 3).
struct PresetCharge {
  int charge;
  double x;
  double y;
  float sizePx = 0.f;
  std::uint64_t id = 0;
  std::string name{};
};

// v2.6 phase 3: the Charge -> PresetCharge conversion used by every save/merge/dump site, so the
// identity fields can never be silently dropped by a new positional rebuild.
inline PresetCharge toPresetCharge(const Charge& c) {
  PresetCharge p;
  p.charge = c.charge;
  p.x = c.position.X;
  p.y = c.position.Y;
  p.id = c.id;
  p.name = c.name;
  p.sizePx = c.sizePx;
  return p;
}

struct Preset {
  std::string_view slug;         // file-name safe, stable
  std::string_view displayName;  // ElectricField.rc string table (docName field)
  std::array<PresetCharge, 4> charges;
  int count;
};

const std::array<Preset, kPresetCount>& presets();
ChargeSystem makeSystem(const Preset& p);

}  // namespace ef
