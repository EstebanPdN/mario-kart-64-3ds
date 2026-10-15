#include "lakitu_sprite_3ds.h"
#include "resource_runtime_3ds.h"

#include <array>
#include <cstring>

namespace {
constexpr size_t kFrameCount = 32;
constexpr size_t kPixels = 56 * 72;
// Immutable once published: raw display-list pointers and the GPU cache must
// never refer to an LRU entry or a buffer overwritten by another animation.
// At each light change the native timer advances the palette one tick before
// the next frame. Retain those two boundary combinations as well.
alignas(16) std::array<std::array<uint8_t, kPixels * 2>, kFrameCount + 2> sFrames{};
std::array<std::array<uint8_t, 512>, 3> sPalettes{};
bool sReady = false;
}

extern "C" bool Mk64LakituCountdown3DSPrepare(const char* const* frames,
                                              size_t count, const char* const* palettes,
                                              size_t paletteCount) {
    if (sReady) return true;
    if (!frames || !palettes || count != kFrameCount || paletteCount != 3) return false;
    for (size_t i = 0; i < sFrames.size(); ++i) {
        Mk64TextureResource3DS palette{}, frame{};
        const size_t frameIndex = i < count ? i : (i == count ? 7 : 23);
        const size_t paletteIndex = i < count ? (i < 8 ? 0 : (i < 24 ? 1 : 2))
                                              : (i == count ? 1 : 2);
        if (!palettes[paletteIndex] || !frames[frameIndex] ||
            !Mk64Resource3DSGetTexture(palettes[paletteIndex], &palette) || !palette.data || palette.size != 512)
            return false;
        // Resolving the frame may evict the palette. Copy it first.
        auto& paletteBytes = sPalettes[paletteIndex];
        std::memcpy(paletteBytes.data(), palette.data, paletteBytes.size());
        if (!Mk64Resource3DSGetTexture(frames[frameIndex], &frame) || !frame.data ||
            frame.type != 4 || frame.width != 56 || frame.height != 72 || frame.size != kPixels)
            return false;
        for (size_t pixel = 0; pixel < kPixels; ++pixel) {
            const size_t entry = static_cast<size_t>(frame.data[pixel]) * 2;
            // Keep the original big-endian RGBA5551 value, including alpha.
            sFrames[i][pixel * 2] = paletteBytes[entry];
            sFrames[i][pixel * 2 + 1] = paletteBytes[entry + 1];
        }
    }
    sReady = true;
    return true;
}

extern "C" const uint8_t* Mk64LakituCountdown3DSFrame(int index, const uint8_t* activePalette) {
    if (!sReady || !activePalette || index < 0 || index >= static_cast<int>(kFrameCount)) return nullptr;
    // Match the native active TLUT, not a guessed timer phase. Unknown palettes
    // retain the original CI8 fallback rather than publishing incorrect colors.
    for (size_t palette = 0; palette < sPalettes.size(); ++palette) {
        if (std::memcmp(activePalette, sPalettes[palette].data(), 512) != 0) continue;
        const size_t expected = index < 8 ? 0 : (index < 24 ? 1 : 2);
        if (palette == expected) return sFrames[static_cast<size_t>(index)].data();
        if (index == 7 && palette == 1) return sFrames[kFrameCount].data();
        if (index == 23 && palette == 2) return sFrames[kFrameCount + 1].data();
        break;
    }
    return nullptr;
}
