#include "adaptive_presentation_3ds.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t kFramesPerTick = 896;

mk64_3ds::AdaptivePresentationInputs HealthyInputs() {
    mk64_3ds::AdaptivePresentationInputs inputs = {};
    inputs.hasPriorTopFrame = true;
    inputs.audioBufferedFrames = kFramesPerTick * 2;
    inputs.audioLowWaterFrames = kFramesPerTick;
    inputs.audioRecoveryFrames = kFramesPerTick * 2;
    return inputs;
}

} // namespace

int main() {
    mk64_3ds::AdaptivePresentationState state = {};

    auto inputs = HealthyInputs();
    inputs.hasPriorTopFrame = false;
    auto decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureNoPriorFrame) != 0);

    inputs = HealthyInputs();
    for (int tick = 0; tick < 3; ++tick) {
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
        assert(!decision.renderMidpoint);
        assert(decision.pressureMask == mk64_3ds::AdaptivePressureRecovery);
    }
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(decision.renderMidpoint);

    inputs.audioBufferedFrames = kFramesPerTick + 1;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(decision.renderMidpoint);

    inputs.audioBufferedFrames = kFramesPerTick;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureAudioLow) != 0);

    inputs = HealthyInputs();
    inputs.audioBufferedFrames = inputs.audioRecoveryFrames - 1;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert(decision.pressureMask == mk64_3ds::AdaptivePressureRecovery);

    inputs.audioBufferedFrames = inputs.audioRecoveryFrames;
    for (int tick = 0; tick < 4; ++tick) {
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    }
    assert(decision.renderMidpoint);

    inputs.resourceActivity = true;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureResourceActivity) != 0);

    inputs = HealthyInputs();
    inputs.audioBufferedFrames = inputs.audioLowWaterFrames + 1;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert(decision.pressureMask == mk64_3ds::AdaptivePressureRecovery);

    inputs = HealthyInputs();
    inputs.previousTickSlow = true;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureSlowTick) != 0);

    inputs = HealthyInputs();
    for (int tick = 0; tick < 4; ++tick) {
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    }
    assert(decision.renderMidpoint);

    std::puts("adaptive presentation policy: ok");
    return 0;
}
