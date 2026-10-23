#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace mk64_3ds {
// Keep the interpreter's operation order, near-eye rule, negative-W rule,
// clamp and truncation. This is the RSP fog alpha, not the distance option.
inline std::uint8_t OriginalFogAlpha(float z, float w, float mul, float offset) {
    if (std::fabs(w) < 0.001f) w = 0.001f;
    float inverse = 1.0f / w;
    if (inverse < 0.0f) inverse = std::numeric_limits<std::int16_t>::max();
    const float fog = std::clamp(z * inverse * mul + offset, 0.0f, 255.0f);
    return std::isfinite(fog) ? static_cast<std::uint8_t>(fog) : 0;
}
inline float OriginalFogCoordinate(float normalizedAlpha) {
    return (normalizedAlpha * 255.0f + 0.5f) * (1.0f / 256.0f);
}
inline std::uint8_t OriginalFogVertex(const float object[3], const float matrix[4][4],
                                      float mul, float offset) {
    const float z = object[0] * matrix[0][2] + object[1] * matrix[1][2] +
                    object[2] * matrix[2][2] + matrix[3][2];
    const float w = object[0] * matrix[0][3] + object[1] * matrix[1][3] +
                    object[2] * matrix[2][3] + matrix[3][3];
    return OriginalFogAlpha(z, w, mul, offset);
}
} // namespace mk64_3ds
