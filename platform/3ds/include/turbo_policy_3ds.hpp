#pragma once
#include <algorithm>

namespace mk64_3ds {
// Hold-only C-Stick activation. Pause and modal input suspend Turbo immediately.
struct TurboPolicy3DS {
    void Reset() {}
    int Update(int multiplier, bool paused, bool captured, bool /*shoulderPressed*/,
               bool cstickHeld) {
        multiplier = std::clamp(multiplier, 2, 5);
        return !paused && !captured && cstickHeld ? multiplier : 1;
    }
};
}
