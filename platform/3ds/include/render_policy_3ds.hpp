#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mk64_3ds {
constexpr uint8_t NormalizeRenderScale(long percent) {
    return static_cast<uint8_t>((std::clamp(percent, 50L, 100L) + 2) / 5 * 5);
}

constexpr uint8_t RenderScaleFromTouch(int x) {
    return static_cast<uint8_t>(50 + (std::clamp(x, 146, 246) - 146 + 5) / 10 * 5);
}

// Normal leaves range to the stock projection/PVS, with NO second W cutoff
// or artificial 75%-horizon fade. Low is an explicit reduced-distance option.
inline float RenderDistanceEnd(float courseFar, int preset) {
    if (!std::isfinite(courseFar) || courseFar <= 0 || preset != 0) return 0;
    return courseFar * 0.75f;
}

constexpr bool CullDistantTriangle(float a, float b, float c, float end,
                                    bool rectangle, bool depthTest) {
    // Retain intersecting triangles; removing them at their centre opens holes
    // in long track polygons. Orthographic UI and sky are never candidates.
    return end > 0 && !rectangle && depthTest && a > end && b > end && c > end;
}
}
