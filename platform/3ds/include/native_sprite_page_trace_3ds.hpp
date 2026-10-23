#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds::native_scene {
// Bounded RAM-only evidence for actual character page requests. Full process
// diagnostics can replay the same sequence at different cache capacities.
// Lower 16 bits identify a pack page, bits 16..23 give active capacity, and
// bit 31 marks a decode. No allocation, clock read, or file I/O on a draw.
struct NativeSpritePageTrace {
    uint64_t requests=0;
    std::array<uint32_t,1024> events{};
    void Record(uint32_t page,unsigned capacity,bool decoded) {
        events[requests&1023]=page|(uint32_t(capacity)<<16)|(decoded?0x80000000u:0);
        ++requests;
    }
};
static_assert(sizeof(NativeSpritePageTrace)==4104);
inline NativeSpritePageTrace gNativeSpritePageTrace;
}
