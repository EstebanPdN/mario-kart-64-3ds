#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace mk64_3ds {
// One DrawTriangles owns all conversion state. Repeated triangle corners can
// reuse their already converted output only while EVERY source float matches.
// This cache never crosses draw, shader, clipping, native-matrix or UV fences.
template <class Packed, std::size_t MaxFloats, std::size_t Slots = 16>
class PackedVertexConversionCache {
    static_assert((Slots & (Slots - 1)) == 0 && Slots != 0);
    static_assert(std::is_trivially_copyable_v<Packed>);
    struct Entry {
        std::array<float, MaxFloats> source;
        Packed packed;
        bool valid = false;
    };
    std::array<Entry, Slots> entries_;
    Entry* pending_ = nullptr;
    std::size_t count_;
public:
    explicit PackedVertexConversionCache(std::size_t count) : count_(count) {}
    bool Copy(const float* source, Packed& packed) {
        pending_ = nullptr;
        if (count_ < 4 || count_ > MaxFloats) return false;
        std::uint32_t x, y, w;
        std::memcpy(&x, source, 4);
        std::memcpy(&y, source + 1, 4);
        std::memcpy(&w, source + 3, 4);
        // Rotate the words separately before avalanching. A plain xor of
        // odd products loses both sign bits when X and Y change together;
        // centred quad corners would then collide in every low-bit bucket.
        std::uint32_t mixed = x ^ ((y << 11) | (y >> 21)) ^ ((w << 22) | (w >> 10));
        mixed ^= mixed >> 16;
        mixed *= 0x7feb352dU;
        mixed ^= mixed >> 15;
        mixed *= 0x846ca68bU;
        mixed ^= mixed >> 16;
        auto& entry = entries_[mixed & (Slots - 1)];
        if (entry.valid && std::memcmp(source, entry.source.data(), count_ * sizeof(float)) == 0) {
            packed = entry.packed;
            return true;
        }
        pending_ = &entry;
        return false;
    }
    void Store(const float* source, const Packed& packed) {
        if (pending_ == nullptr) return;
        std::memcpy(pending_->source.data(), source, count_ * sizeof(float));
        pending_->packed = packed;
        pending_->valid = true;
        pending_ = nullptr;
    }
};
} // namespace mk64_3ds
