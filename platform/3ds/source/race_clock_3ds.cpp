#include "race_clock_3ds.hpp"
#include "performance_trace_3ds.hpp"
#include "auto_mode_3ds.h"
#include <cmath>
extern "C" {
extern float gCourseTimer;
extern uint16_t D_8015011E, gIsGamePaused;
extern int gGamestate;
extern int gTickLogic;
void Mk64BottomUI3DSResetTurbo(void);
}
namespace {
mk64_3ds::RaceClock sClock;
int sAdvanceRemaining = 0;
double sAdvanceValue = 0, sAdvanceStep = 0;
}
extern "C" void Mk64RaceClock3DSReset() {
    sClock.Reset(); sAdvanceRemaining = 0;
    // RACE_INIT is authoritative even when retry re-enters the same course
    // without a different menu snapshot or course ID. Pause is not a reset.
    // An AUTO session is continuous across cup courses and failure retries.
    // A new process still starts unlatched; manual race entry keeps its reset.
    if (!Mk64Auto3DSActive()) Mk64BottomUI3DSResetTurbo();
}
extern "C" void Mk64RaceClock3DSFreeze() { sClock.Sync(mk64_3ds::PerformanceNow(), false); }
extern "C" void Mk64RaceClock3DSSetSpeed(unsigned speed) {
    sAdvanceRemaining = 0;
    sClock.SetRate(mk64_3ds::PerformanceNow(), speed);
}
extern "C" void Mk64RaceClock3DSObserve() {
    const bool active = gGamestate == 4 && D_8015011E && !gIsGamePaused;
    const auto baseline = static_cast<uint64_t>(std::max(0.0f, gCourseTimer) * 1000000.0);
    sClock.Sync(mk64_3ds::PerformanceNow(), active, baseline);
    sAdvanceRemaining = 0;
    if (active && gTickLogic > 2) {
        // Spread this accelerated image's elapsed time over its logic ticks.
        // Giving every substep the same timer makes arena roulette/steering
        // deduplication incorrectly discard all but the first tick at x5.
        sAdvanceRemaining = gTickLogic;
        sAdvanceValue = std::max(0.0f, gCourseTimer);
        const double target = sClock.Read(mk64_3ds::PerformanceNow()) / 1000000.0;
        sAdvanceStep = std::max(0.0, target - sAdvanceValue) / sAdvanceRemaining;
    }
}
extern "C" float Mk64RaceClock3DSAdvance(float nativeTime) {
    const auto now = mk64_3ds::PerformanceNow();
    sClock.Sync(now, true, static_cast<uint64_t>(std::max(0.0f, nativeTime) * 1000000.0));
    if (sAdvanceRemaining > 0) {
        --sAdvanceRemaining;
        sAdvanceValue += sAdvanceStep;
        return static_cast<float>(sAdvanceValue);
    }
    return static_cast<float>(sClock.Read(now) / 1000000.0);
}
extern "C" uint32_t Mk64RaceClock3DSDisplay(uint32_t frozenTime, bool frozen) {
    if (frozen || !sClock.Running()) return frozenTime;
    return static_cast<uint32_t>(std::min<uint64_t>(sClock.Read(mk64_3ds::PerformanceNow()) / 10000, 599999));
}
