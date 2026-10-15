#pragma once
#include <cstdint>

namespace mk64_3ds {
// Wall-clock A edges, independent of frame rate, Turbo and native menu counters.
struct AutoConfirmPulse3DS {
    uint64_t next = 0;
    bool armed = false;
    bool Tick(bool eligible, uint64_t now) {
        if (!eligible) { *this = {}; return false; }
        if (!armed) { armed = true; next = now + 500; return false; }
        if (now < next) return false;
        next = now + 500; // Never issue catch-up bursts after a load/stall.
        return true;
    }
};
}
