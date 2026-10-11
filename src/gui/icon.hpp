#pragma once
// App icon: the 256x256 PNG stored inside ElectricField/res/ElectricField.ico (extracted byte-for-byte) and a 96x96 resample
// (About box), embedded with Clang's #embed. Files live under assets/ so they survive the MFC tree deletion (D-14).
#include <cstddef>

namespace efgui {
const unsigned char* icon256Png();
std::size_t icon256PngSize();
const unsigned char* icon96Png();
std::size_t icon96PngSize();
}  // namespace efgui
