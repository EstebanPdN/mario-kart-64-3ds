#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mk64_3ds::native_scene {

// Version 1 is a little-endian cache format. Conversion happens outside the
// render loop. Position/UV retain source integer precision; color is RGBA8.
struct Vertex {
    std::int16_t position[3];
    std::int16_t uv[2];
    std::uint8_t color[4];
    std::uint16_t reserved;
};
static_assert(sizeof(Vertex) == 16);
static_assert(offsetof(Vertex, uv) == 6 && offsetof(Vertex, color) == 10);

struct MeshView {
    const std::uint8_t* vertices = nullptr;
    const std::uint8_t* indices = nullptr;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
};

inline std::uint32_t ReadLE32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

// Validate before allocation/upload. Views borrow their input; they never own
// a file or reference a display list. Failed validation clears the result.
inline bool ValidateMesh(const void* bytes, std::size_t size, MeshView& result) {
    result = {};
    constexpr std::size_t kHeaderBytes = 16;
    constexpr std::size_t kMaximumBytes = 8u * 1024u * 1024u;
    if (!bytes || size < kHeaderBytes || size > kMaximumBytes) return false;
    const auto* p = static_cast<const std::uint8_t*>(bytes);
    if (std::memcmp(p, "NCM1", 4) != 0 || ReadLE32(p + 12) != sizeof(Vertex)) return false;
    const std::uint32_t vertexCount = ReadLE32(p + 4);
    const std::uint32_t indexCount = ReadLE32(p + 8);
    if (!vertexCount || vertexCount > 65535 || !indexCount || indexCount % 3) return false;
    const std::size_t vertexBytes = std::size_t(vertexCount) * sizeof(Vertex);
    // Subtract available bytes before multiplying untrusted index counts.
    if (vertexBytes > size - kHeaderBytes) return false;
    const std::size_t indexBytes = size - kHeaderBytes - vertexBytes;
    if (indexBytes % 2 || indexBytes / 2 != indexCount) return false;
    const auto* indices = p + kHeaderBytes + vertexBytes;
    for (std::size_t i = 0; i < indexCount; ++i) {
        const auto index = std::uint32_t(indices[2*i]) | (std::uint32_t(indices[2*i+1]) << 8);
        if (index >= vertexCount) return false;
    }
    result = {p + kHeaderBytes, indices, vertexCount, indexCount};
    return true;
}

} // namespace mk64_3ds::native_scene
