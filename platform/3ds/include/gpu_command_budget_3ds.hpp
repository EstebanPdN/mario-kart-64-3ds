#pragma once

#include <cstdint>
#include <stdexcept>

namespace mk64_3ds {
// Results can exceed Citro3D's default 256 KiB even on a mandatory keyframe.
// Allocate once at renderer startup, before calculating archive RAM headroom.
constexpr std::uint32_t kGpuCommandBufferBytes = 1024U * 1024U;
// Leave a separate tail for UI, scaled presentation, and frame finalization.
constexpr std::uint32_t kGpuCommandTailWords = (256U * 1024U) / sizeof(std::uint32_t);
// A Fast3D batch may dirty shader, uniform, texture, and TEV state together.
constexpr std::uint32_t kGpuCommandDrawWords = (16U * 1024U) / sizeof(std::uint32_t);

inline bool GpuCommandRoom(std::uint32_t capacityWords, std::uint32_t offsetWords,
                           std::uint32_t requiredWords) {
    // GPUCMD_Split can change the current buffer base/size. Use its current
    // remaining words, not the previous frame's C3D_GetCmdBufUsage ratio.
    return offsetWords <= capacityWords && requiredWords <= capacityWords - offsetWords;
}

class GpuCommandPressure : public std::length_error {
public:
    GpuCommandPressure() : std::length_error("3DS GPU command buffer pressure") {}
};
}
