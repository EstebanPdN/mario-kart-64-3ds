#pragma once
#include <algorithm>
#include <cstdint>

namespace mk64_3ds {
// Monotonic wall time, owned by the game thread. No worker mutates game globals.
class RaceClock {
public:
    void Reset() { elapsed = anchor = 0; initialized = running = false; rate = 1; }
    void SetRate(uint64_t now, unsigned speed) {
        speed = std::clamp(speed, 1U, 5U);
        if (speed == rate) return;
        elapsed = Read(now); anchor = now; rate = speed;
    }
    void Sync(uint64_t now, bool active, uint64_t initial = 0) {
        if (!initialized) { elapsed = initial; initialized = true; }
        if (running && !active) elapsed = Read(now);
        if (!running && active) anchor = now;
        running = active;
    }
    uint64_t Read(uint64_t now) const {
        return elapsed + (running && now >= anchor ? (now - anchor) * rate : 0);
    }
    bool Running() const { return running; }
private:
    uint64_t elapsed = 0, anchor = 0;
    bool initialized = false, running = false;
    unsigned rate = 1;
};
}

extern "C" {
void Mk64RaceClock3DSReset();
void Mk64RaceClock3DSObserve();
void Mk64RaceClock3DSFreeze();
void Mk64RaceClock3DSSetSpeed(unsigned speed);
float Mk64RaceClock3DSAdvance(float nativeTime);
uint32_t Mk64RaceClock3DSDisplay(uint32_t frozenTime, bool frozen);
}
