#include "jumbotron_3ds.h"

#include "system_3ds.h"

#include <3ds.h>

#include <array>
#include <cstddef>

namespace {

constexpr unsigned kNativeScreenWidth = 320;
constexpr unsigned kCaptureLeft = 88;
constexpr unsigned kCaptureTop = 72;
constexpr unsigned kTileWidth = 64;
constexpr unsigned kTileHeight = 32;
constexpr unsigned kCaptureWidth = kTileWidth * 2;
constexpr unsigned kCaptureHeight = kTileHeight * 3;
constexpr unsigned kTopScreenLogicalWidth = 400;
constexpr unsigned kTopScreenHeight = 240;

uint16_t PackBigEndianRgba5551(const uint8_t* bgr) {
    const uint16_t pixel = static_cast<uint16_t>(((bgr[2] >> 3) << 11) |
                                                 ((bgr[1] >> 3) << 6) |
                                                 ((bgr[0] >> 3) << 1) | 1u);
    return __builtin_bswap16(pixel);
}

} // namespace

extern "C" void Mk64Jumbotron3DSUpdate(uint16_t* topLeft, uint16_t* topRight,
                                         uint16_t* middleLeft, uint16_t* middleRight,
                                         uint16_t* bottomLeft, uint16_t* bottomRight) {
    // Match the original N64 jumbotron's half-rate refresh cadence while
    // avoiding an unnecessary framebuffer readback on every other frame.
    static bool updateThisFrame = true;
    const bool shouldUpdate = updateThisFrame;
    updateThisFrame = !updateThisFrame;
    if (!shouldUpdate) return;

    const std::array<uint16_t*, 6> targets = {
        topLeft, topRight, middleLeft, middleRight, bottomLeft, bottomRight,
    };
    for (uint16_t* target : targets) {
        if (target == nullptr) return;
    }

    uint16_t physicalWidth = 0;
    uint16_t physicalHeight = 0;
    const uint8_t* framebuffer =
        gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &physicalWidth, &physicalHeight);
    if (framebuffer == nullptr || physicalWidth != kTopScreenHeight ||
        (physicalHeight != kTopScreenLogicalWidth &&
         physicalHeight != kTopScreenLogicalWidth * 2)) {
        return;
    }

    const unsigned horizontalScale = physicalHeight / kTopScreenLogicalWidth;
    const unsigned cropLeft =
        (physicalHeight - kNativeScreenWidth * horizontalScale) / 2;
    const unsigned firstColumn =
        cropLeft + kCaptureLeft * horizontalScale;
    const unsigned lastColumn =
        cropLeft + (kCaptureLeft + kCaptureWidth - 1) * horizontalScale;
    const uint8_t* invalidateBegin =
        framebuffer + static_cast<size_t>(firstColumn) * physicalWidth * 3u;
    const size_t invalidateBytes =
        static_cast<size_t>(lastColumn - firstColumn + 1) * physicalWidth * 3u;
    if (!Mk64System3DSInvalidateDataCache(invalidateBegin, invalidateBytes)) {
        return;
    }

    for (unsigned y = 0; y < kCaptureHeight; ++y) {
        const unsigned tileRow = y / kTileHeight;
        const unsigned tileY = y % kTileHeight;
        const unsigned logicalY = kCaptureTop + y;
        const unsigned rotatedY = physicalWidth - 1u - logicalY;
        for (unsigned x = 0; x < kCaptureWidth; ++x) {
            const unsigned tileColumn = x / kTileWidth;
            const unsigned tileX = x % kTileWidth;
            const unsigned logicalX =
                cropLeft + (kCaptureLeft + x) * horizontalScale;
            const uint8_t* pixel =
                framebuffer +
                (static_cast<size_t>(logicalX) * physicalWidth + rotatedY) * 3u;
            targets[tileRow * 2 + tileColumn][tileY * kTileWidth + tileX] =
                PackBigEndianRgba5551(pixel);
        }
    }
}
