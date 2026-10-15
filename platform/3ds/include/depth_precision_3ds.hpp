#pragma once

namespace mk64_3ds {
// Convert before PICA's float24 input/ALU rounding: subtracting almost equal
// clip Z/W on that ALU destroys far-depth separation (trees, bridges, water).
// The result is a small homogeneous depth in [-w,0], not divided depth.
inline float PicaClipDepth(float z, float w) { return (z - w) * 0.5f; }

inline float PicaObjectClipDepth(const float* object, const float matrix[4][4], float w) {
    const float z = object[0] * matrix[0][2] + object[1] * matrix[1][2] +
                    object[2] * matrix[2][2] + matrix[3][2];
    return PicaClipDepth(z, w);
}
// Reversed depth uses GREATER: a decal moves towards the camera, not behind
// its backing polygon. Avoid the old -0.001 bias (thousands of depth units).
constexpr float PicaDecalBias(bool decal) { return decal ? 0x1p-22f : 0.0f; }
constexpr float PicaPrimitiveDepth(float depth) {
    return 1.0f - (depth < 0.0f ? 0.0f : depth > 1.0f ? 1.0f : depth);
}
}
