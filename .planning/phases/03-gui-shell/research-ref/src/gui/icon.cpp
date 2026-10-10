#include "gui/icon.hpp"

namespace efgui {
namespace {
const unsigned char k256[] = {
#embed "../../assets/ElectricField-256.png"
};
const unsigned char k96[] = {
#embed "../../assets/ElectricField-96.png"
};
}  // namespace
const unsigned char* icon256Png() { return k256; }
std::size_t icon256PngSize() { return sizeof(k256); }
const unsigned char* icon96Png() { return k96; }
std::size_t icon96PngSize() { return sizeof(k96); }
}  // namespace efgui
