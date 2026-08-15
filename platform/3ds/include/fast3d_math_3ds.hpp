#pragma once

namespace mk64_3ds {

inline float Fast3DAspectCorrection(float width, float height) {
    return width > 0.0f ? (4.0f / 3.0f) * height / width : 1.0f;
}

// Returns a value with the same sign as the perspective-divided screen-space
// cross product. Face culling only consumes that sign, so the homogeneous form
// removes three divisions from every culled triangle.
inline float Fast3DProjectedCross(float x1, float y1, float w1,
                                  float x2, float y2, float w2,
                                  float x3, float y3, float w3) {
    const float dx1 = x1 * w2 - x2 * w1;
    const float dy1 = y1 * w2 - y2 * w1;
    const float dx2 = x3 * w2 - x2 * w3;
    const float dy2 = y3 * w2 - y2 * w3;
    float cross = dx1 * dy2 - dy1 * dx2;
    if ((w1 < 0.0f) != (w3 < 0.0f)) {
        cross = -cross;
    }
    return cross;
}

} // namespace mk64_3ds
