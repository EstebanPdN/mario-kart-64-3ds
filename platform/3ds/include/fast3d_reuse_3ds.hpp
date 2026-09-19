#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace mk64_3ds {

struct Fast3DReuseStats {
    std::uint32_t matrixHits = 0;
    std::uint32_t matrixMisses = 0;
    std::uint32_t positionHits = 0;
    std::uint32_t positionMisses = 0;
};
inline Fast3DReuseStats gFast3DReuseStats;

// Pointer bits select a bucket only. A hit always validates every input byte
// and the complete calculation state. Cached copies own their data; no pointer
// from an earlier course or resource lifetime is ever dereferenced.
template<class Input, class State, class Output, std::size_t MaxCount,
         std::size_t Slots>
class Fast3DExactBlockCache {
    static_assert(MaxCount != 0 && Slots != 0 && (Slots & (Slots - 1)) == 0);
    static_assert(std::is_trivially_copyable_v<Input> &&
                  std::is_trivially_copyable_v<State> &&
                  std::is_trivially_copyable_v<Output>);
    struct Entry {
        std::uintptr_t address = 0;
        std::size_t count = 0;
        State state{};
        std::array<Input, MaxCount> source{};
        std::array<Output, MaxCount> result{};
    };
    std::array<Entry, Slots> entries_{};

    static std::size_t Bucket(const Input* source) {
        const auto address = reinterpret_cast<std::uintptr_t>(source);
        return ((address >> 4) ^ (address >> 12)) & (Slots - 1);
    }

  public:
    const Output* Find(const Input* source, std::size_t count,
                       const State& state) const {
        if (source == nullptr || count == 0 || count > MaxCount) return nullptr;
        const auto& entry = entries_[Bucket(source)];
        if (entry.address != reinterpret_cast<std::uintptr_t>(source) ||
            entry.count != count ||
            std::memcmp(&entry.state, &state, sizeof(State)) != 0 ||
            std::memcmp(entry.source.data(), source, count * sizeof(Input)) != 0) {
            return nullptr;
        }
        return entry.result.data();
    }

    void Store(const Input* source, std::size_t count, const State& state,
               const Output* result) {
        if (source == nullptr || result == nullptr || count == 0 ||
            count > MaxCount) return;
        auto& entry = entries_[Bucket(source)];
        // The result can be copied from a previously returned entry. memmove
        // makes that supported even for a bucket collision.
        std::memmove(entry.result.data(), result, count * sizeof(Output));
        std::memcpy(entry.source.data(), source, count * sizeof(Input));
        std::memcpy(&entry.state, &state, sizeof(State));
        entry.address = reinterpret_cast<std::uintptr_t>(source);
        entry.count = count;
    }

    void Clear() {
        for (auto& entry : entries_) entry.count = 0;
    }
};

// Decode all 16 fixed-point elements using unsigned bit assembly. The original
// signed left shift is undefined for negative matrix components.
inline void DecodeFast3DFixedMatrix(float output[4][4], const std::int32_t* input) {
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; col += 2) {
            const auto integer = static_cast<std::uint32_t>(input[row * 2 + col / 2]);
            const auto fraction = static_cast<std::uint32_t>(input[8 + row * 2 + col / 2]);
            const std::uint32_t bits[2] = {
                (integer & 0xffff0000U) | (fraction >> 16),
                (integer << 16) | (fraction & 0xffffU)
            };
            std::int32_t values[2];
            std::memcpy(values, bits, sizeof(values));
            output[row][col] = values[0] * (1.0f / 65536.0f);
            output[row][col + 1] = values[1] * (1.0f / 65536.0f);
        }
    }
}

inline void LoadFast3DFixedMatrix(float output[4][4], const std::int32_t* input) {
    static Fast3DExactBlockCache<std::int32_t, std::uint8_t, float, 16, 32> cache;
    constexpr std::uint8_t state = 0;
    if (const auto* found = cache.Find(input, 16, state)) {
        std::memcpy(output, found, sizeof(float) * 16);
        ++gFast3DReuseStats.matrixHits;
    } else {
        DecodeFast3DFixedMatrix(output, input);
        cache.Store(input, 16, state, &output[0][0]);
        ++gFast3DReuseStats.matrixMisses;
    }
}

struct Fast3DPositionDepth {
    float z;
    float w;
};

// Only the two depth columns contribute to these outputs. Lighting, UVs,
// viewport, aspect, fog and x/y transformation remain live in their existing
// paths; they are deliberately never replayed from this cache.
template<class Vertex>
inline const Fast3DPositionDepth* BeginFast3DPositionBlock(
        const Vertex* vertices, std::size_t count, const float matrix[4][4]) {
    constexpr std::size_t maxCount = 32;
    // Small sprite/actor loads cost less to evaluate directly than to compare
    // and copy a whole cache entry. Keep caching for full geometry blocks.
    if (vertices == nullptr || count < 16 || count > maxCount) return nullptr;
    struct DepthState { float columns[4][2]; };
    DepthState state{};
    for (std::size_t row = 0; row < 4; ++row) {
        state.columns[row][0] = matrix[row][2];
        state.columns[row][1] = matrix[row][3];
    }
    static Fast3DExactBlockCache<Vertex, DepthState, Fast3DPositionDepth,
                                maxCount, 64> cache;
    static std::array<Fast3DPositionDepth, maxCount> scratch;
    if (const auto* found = cache.Find(vertices, count, state)) {
        ++gFast3DReuseStats.positionHits;
        return found;
    }
    ++gFast3DReuseStats.positionMisses;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* ob = vertices[i].v.ob;
        scratch[i].z = ob[0] * matrix[0][2] + ob[1] * matrix[1][2] +
                       ob[2] * matrix[2][2] + matrix[3][2];
        scratch[i].w = ob[0] * matrix[0][3] + ob[1] * matrix[1][3] +
                       ob[2] * matrix[2][3] + matrix[3][3];
    }
    cache.Store(vertices, count, state, scratch.data());
    return scratch.data();
}

} // namespace mk64_3ds
