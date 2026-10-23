#pragma once
#include <algorithm>
#include <cstdint>

namespace mk64_3ds {
// Citro3D starts its CPU timer on successful FrameBegin, before our pending
// framebuffer swap has cleared. Only that later wait is inside the SDK timer;
// subtracting the entire BeginFrame wait would also remove real recording work.
inline float ActiveRecordingMilliseconds(float sdkMilliseconds,
                                         std::uint32_t pendingPresentWaitUs) {
    return std::max(0.0f, sdkMilliseconds - pendingPresentWaitUs / 1000.0f);
}
}
