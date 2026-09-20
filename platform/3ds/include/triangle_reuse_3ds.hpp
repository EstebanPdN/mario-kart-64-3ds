#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mk64_3ds {

// Only consecutive, resolved triangle handlers share state. Every other
// command (including calls, returns, vertex loads and ucode changes) breaks
// the run. Direct interpreter calls never opt in implicitly.
struct TriangleRun {
    uint64_t generation = 1;
    bool active = false;
    void Invalidate() { ++generation; active = false; }
};

struct TriangleCommandScope {
    TriangleRun& run;
    TriangleCommandScope(TriangleRun& value, bool pureTriangle) : run(value) {
        if (!pureTriangle) run.Invalidate();
        run.active = pureTriangle;
    }
    ~TriangleCommandScope() { run.active = false; }
};

struct TriangleReuseCounters {
    uint64_t materialHits = 0, materialMisses = 0;
    uint64_t vertexHits = 0, vertexMisses = 0;
};
inline TriangleReuseCounters gTriangleReuseCounters;

// Maximum interpreter stride: XYZW, two UV/clamp pairs, fog, grayscale,
// and seven RGBA combiner inputs. BSS only; no frame-time allocations.
template <size_t Slots> class TriangleVertexCache {
public:
    static constexpr size_t kMaxFloats = 48;
    bool Copy(size_t slot, uint64_t generation, uint8_t mode, float* output, size_t& count) const {
        if (slot >= Slots) return false;
        const auto& entry = entries_[slot];
        if (!entry.count || entry.generation != generation || entry.mode != mode) return false;
        count = entry.count;
        std::memcpy(output, entry.data, count * sizeof(float));
        return true;
    }
    void Store(size_t slot, uint64_t generation, uint8_t mode, const float* data, size_t count) {
        if (slot >= Slots || count > kMaxFloats) return;
        auto& entry = entries_[slot];
        entry.generation = generation;
        entry.mode = mode;
        entry.count = static_cast<uint8_t>(count);
        std::memcpy(entry.data, data, count * sizeof(float));
    }
private:
    struct Entry {
        uint64_t generation = 0;
        float data[kMaxFloats]{};
        uint8_t count = 0, mode = 0;
    };
    std::array<Entry, Slots> entries_{};
};
} // namespace mk64_3ds
