#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace mk64_3ds {
// Native frames matching the owner's 32-frame, 30 ms Lakitu flag animation.
// Only names/format logic ship with the port; pixels come from the local O2R.
struct LoadingAnimation {
    static constexpr unsigned width = 72, height = 56, frames = 32, frameMs = 30;
    std::array<uint8_t, 512> palette{};
    std::array<uint8_t, width * height * frames> pixels{};
};
inline uint32_t LoadingU32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline bool LoadingTexture(const std::vector<uint8_t>& bytes, unsigned type,
                           unsigned width, unsigned height, void* out, size_t size) {
    if (bytes.size() != 80 + size || LoadingU32(bytes.data()) != 0 ||
        LoadingU32(bytes.data() + 4) != 0x4f544558 || LoadingU32(bytes.data() + 8) != 0 ||
        LoadingU32(bytes.data() + 64) != type || LoadingU32(bytes.data() + 68) != width ||
        LoadingU32(bytes.data() + 72) != height || LoadingU32(bytes.data() + 76) != size) return false;
    std::memcpy(out, bytes.data() + 80, size);
    return true;
}
template<class Read> bool LoadAnimationFromArchive(LoadingAnimation& animation, Read read) {
    std::vector<uint8_t> bytes;
    if (!read("textures/common_data/common_tlut_lakitu_checkered_flag", bytes) ||
        !LoadingTexture(bytes, 2, 16, 16, animation.palette.data(), animation.palette.size())) return false;
    for (unsigned i = 0; i < LoadingAnimation::frames; ++i) {
        char name[96];
        std::snprintf(name, sizeof(name), "textures/other_textures/gTextureLakituCheckeredFlag%02u", i + 1);
        if (!read(name, bytes) || !LoadingTexture(bytes, 4, LoadingAnimation::width,
            LoadingAnimation::height, animation.pixels.data() + i * LoadingAnimation::width * LoadingAnimation::height,
            LoadingAnimation::width * LoadingAnimation::height)) return false;
    }
    return true;
}
inline uint32_t LoadingChecksum(const LoadingAnimation& animation) {
    uint32_t hash = 2166136261u;
    for (uint8_t byte : animation.palette) hash = (hash ^ byte) * 16777619u;
    for (uint8_t byte : animation.pixels) hash = (hash ^ byte) * 16777619u;
    return hash;
}
inline std::array<uint8_t, 32> LoadingCacheHeader(uint64_t sourceSize, uint64_t sourceTime, uint32_t checksum) {
    std::array<uint8_t, 32> header{};
    std::memcpy(header.data(), "MK64LD01", 8);
    for (unsigned i = 0; i < 8; ++i) { header[8+i] = sourceSize >> (i*8); header[16+i] = sourceTime >> (i*8); }
    for (unsigned i = 0; i < 4; ++i) header[24+i] = checksum >> (i*8);
    return header;
}
inline bool ReadLoadingCache(FILE* file, uint64_t sourceSize, uint64_t sourceTime, LoadingAnimation& animation) {
    if (!file) return false;
    std::array<uint8_t, 32> header{};
    if (std::fread(header.data(), 1, header.size(), file) != header.size() ||
        std::fread(&animation, 1, sizeof(animation), file) != sizeof(animation) || std::fgetc(file) != EOF) return false;
    return header == LoadingCacheHeader(sourceSize, sourceTime, LoadingChecksum(animation));
}
inline bool WriteLoadingCache(FILE* file, uint64_t sourceSize, uint64_t sourceTime, const LoadingAnimation& animation) {
    if (!file) return false;
    const auto header = LoadingCacheHeader(sourceSize, sourceTime, LoadingChecksum(animation));
    return std::fwrite(header.data(), 1, header.size(), file) == header.size() &&
        std::fwrite(&animation, 1, sizeof(animation), file) == sizeof(animation);
}
// BGR8 framebuffer columns are rotated 90 degrees on both 3DS screens.
inline void DrawLoadingAnimation(const LoadingAnimation& animation, uint64_t elapsedMs,
                                uint8_t* framebuffer, unsigned screenWidth, unsigned screenHeight) {
    if (!framebuffer || screenHeight != 240 || (screenWidth != 400 && screenWidth != 800)) return;
    const unsigned scaleX = screenWidth / 400;
    const unsigned left = (screenWidth - LoadingAnimation::width * scaleX) / 2;
    const unsigned top = (screenHeight - LoadingAnimation::height) / 2;
    const unsigned frame = (elapsedMs / LoadingAnimation::frameMs) % LoadingAnimation::frames;
    for (unsigned y = 0; y < LoadingAnimation::height; ++y) {
        for (unsigned x = 0; x < LoadingAnimation::width; ++x) {
            const unsigned index = animation.pixels[(frame * LoadingAnimation::height + y) * LoadingAnimation::width + x];
            const uint16_t rgba = uint16_t(animation.palette[index*2]) << 8 | animation.palette[index*2+1];
            const bool opaque = rgba & 1;
            for (unsigned copy = 0; copy < scaleX; ++copy) {
                auto* p = framebuffer + ((left + x*scaleX + copy) * screenHeight + screenHeight - 1 - (top+y)) * 3;
                p[0] = opaque ? ((rgba >> 1) & 31) * 255 / 31 : 0;
                p[1] = opaque ? ((rgba >> 6) & 31) * 255 / 31 : 0;
                p[2] = opaque ? ((rgba >> 11) & 31) * 255 / 31 : 0;
            }
        }
    }
}
static_assert(sizeof(LoadingAnimation) == 129536);
}
