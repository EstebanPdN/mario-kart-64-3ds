#pragma once

#include <algorithm>
#include <cstdint>

#ifndef BE16SWAP
#define BE16SWAP(value) __builtin_bswap16(static_cast<uint16_t>(value))
#endif

namespace Ship::Math {
inline float clamp(float value, float minimum, float maximum) {
    return std::clamp(value, minimum, maximum);
}
} // namespace Ship::Math
