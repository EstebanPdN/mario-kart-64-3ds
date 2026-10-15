#pragma once
#include <array>
#include <cstdint>
#include <utility>

namespace mk64_3ds {
// Separate from native race RNG: rolling a cup cannot alter item/AI randomness.
struct RandomCupPlan {
    uint32_t state;
    std::array<uint8_t,4> tracks{};
    uint8_t character = 0;
    explicit RandomCupPlan(uint64_t seed) : state(uint32_t(seed ^ (seed >> 32))) {
        if (!state) state = 0x6d2b79f5;
        std::array<uint8_t,16> candidates{8,9,6,11,10,5,1,0,14,12,7,2,18,4,3,13};
        for (unsigned i=0;i<tracks.size();++i) {
            const unsigned chosen=i+Next()%(candidates.size()-i);
            std::swap(candidates[i],candidates[chosen]);
            tracks[i]=candidates[i];
        }
        character=Next()%8;
    }
    uint32_t Next() { state^=state<<13;state^=state>>17;state^=state<<5;return state; }
    bool NextMirror() { return (Next() & 1) != 0; }
};
}
