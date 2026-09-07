#include "performance_trace_3ds.hpp"
#include "adaptive_presentation_3ds.hpp"
#include <cassert>
#include <cstdio>

int main() {
    mk64_3ds::PerformanceHistory<4> history;
    assert(history.Size() == 0);
    for (unsigned i = 1; i <= 11; ++i) {
        mk64_3ds::PerformanceTick tick;
        tick.tick = i; tick.epoch = i < 8 ? 0 : 1;
        tick.presents = i % 3; tick.suppressed = tick.presents == 0;
        history.Push(tick);
    }
    assert(history.Size() == 4);
    for (unsigned i = 0; i < 4; ++i) {
        assert(history.At(i).tick == i + 8);
        assert(history.At(i).epoch == 1);
        assert(history.At(i).presents == (i+8)%3);
    }
    // Constant rendering work, no upload pressure. Reproduce the audio queue
    // sawtooth independently of a ROM, GPU or emulator frame limiter.
    mk64_3ds::AdaptivePresentationState state;
    state.midpointEnabled = true;
    mk64_3ds::AdaptivePresentationInputs inputs;
    inputs.hasPriorTopFrame = true;
    inputs.audioLowWaterFrames = 896; inputs.audioRecoveryFrames = 1344;
    inputs.keyframeHeadroom = true;
    unsigned midpoints = 0;
    for (unsigned tick = 0; tick < 600; ++tick) {
        inputs.audioBufferedFrames = tick % 40 == 0 ? 1200 : 1792;
        midpoints += mk64_3ds::UpdateAdaptivePresentation(&state, inputs).renderMidpoint;
    }
    assert(midpoints == 600);
    inputs.audioBufferedFrames = 896;
    assert(!mk64_3ds::UpdateAdaptivePresentation(&state, inputs).renderMidpoint);
    assert(!state.midpointEnabled);
    std::printf("history wrap/epoch and constant-work audio sawtooth: ok; recorder=%zu bytes\n",
                sizeof(mk64_3ds::PerformanceHistory<256>) + sizeof(mk64_3ds::PerformanceTick));
}
