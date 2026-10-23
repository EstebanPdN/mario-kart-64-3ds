#pragma once
#include "fast3d_reuse_3ds.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace mk64_3ds {
// Only object XYZ and the Z/W columns determine these two outputs. Retaining
// UV/color/normal/flag bytes both wastes storage and rejects valid reuse after
// texture scroll or shade updates. Every relevant mutable input is still read.
class Fast3DCompactPositionCache {
    struct Entry {
        std::uintptr_t source = 0;
        std::size_t count = 0;
        float columns[4][2]{};
        std::array<std::array<std::int16_t, 3>, 32> positions;
        std::array<Fast3DPositionDepth, 32> output;
    };
    std::array<Entry, 64> entries_{};
public:
    template<class Vertex>
    const Fast3DPositionDepth* Load(const Vertex* vertices, std::size_t count,
                                  const float matrix[4][4]) {
        static_assert(sizeof(vertices->v.ob) == 3 * sizeof(std::int16_t), "Fast3D object coordinate layout changed");
        if (vertices == nullptr || count < 16 || count > 32) return nullptr;
        const auto address = reinterpret_cast<std::uintptr_t>(vertices);
        auto& entry = entries_[((address >> 4) ^ (address >> 12)) & 63];
        float columns[4][2];
        for (std::size_t row = 0; row < 4; ++row) {
            columns[row][0] = matrix[row][2];
            columns[row][1] = matrix[row][3];
        }
        bool hit = entry.source == address && entry.count == count &&
                   std::memcmp(entry.columns, columns, sizeof(columns)) == 0;
        for (std::size_t i = 0; hit && i < count; ++i)
            hit = std::memcmp(entry.positions[i].data(), vertices[i].v.ob, 3 * sizeof(std::int16_t)) == 0;
        if (hit) {
            ++gFast3DReuseStats.positionHits;
            return entry.output.data();
        }
        ++gFast3DReuseStats.positionMisses;
        for (std::size_t i = 0; i < count; ++i) {
            const auto* ob = vertices[i].v.ob;
            entry.output[i].z = ob[0] * matrix[0][2] + ob[1] * matrix[1][2] +
                                ob[2] * matrix[2][2] + matrix[3][2];
            entry.output[i].w = ob[0] * matrix[0][3] + ob[1] * matrix[1][3] +
                                ob[2] * matrix[2][3] + matrix[3][3];
            std::memcpy(entry.positions[i].data(), ob, 3 * sizeof(std::int16_t));
        }
        std::memcpy(entry.columns, columns, sizeof(columns));
        entry.source = address;
        entry.count = count;
        return entry.output.data();
    }
};
template<class Vertex>
inline const Fast3DPositionDepth* BeginFast3DCompactPositionBlock(
        const Vertex* vertices, std::size_t count, const float matrix[4][4]) {
    static Fast3DCompactPositionCache cache;
    return cache.Load(vertices, count, matrix);
}
} // namespace mk64_3ds
