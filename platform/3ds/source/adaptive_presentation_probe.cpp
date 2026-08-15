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
    inputs.keyframeHeadroom = true;
    return inputs;
}

void ExpectRecovery(const mk64_3ds::AdaptivePresentationDecision& decision) {
    assert(!decision.renderMidpoint);
    assert(decision.pressureMask == mk64_3ds::AdaptivePressureRecovery);
}

void DrainCooldown(mk64_3ds::AdaptivePresentationState* state) {
    auto inputs = HealthyInputs();
    std::uint32_t drainedTicks = 0;
    while (state->cooldownTicks != 0) {
        ExpectRecovery(mk64_3ds::UpdateAdaptivePresentation(state, inputs));
        ++drainedTicks;
        assert(drainedTicks <= mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks);
    }
}

void RecoverToFirstProbe(mk64_3ds::AdaptivePresentationState* state) {
    assert(state->cooldownTicks == 0);
    auto inputs = HealthyInputs();
    for (std::uint8_t tick = 1;
         tick < mk64_3ds::kAdaptivePresentationHealthyTicksToEnable; ++tick) {
        ExpectRecovery(mk64_3ds::UpdateAdaptivePresentation(state, inputs));
    }
    const auto decision = mk64_3ds::UpdateAdaptivePresentation(state, inputs);
    assert(decision.renderMidpoint);
    assert(decision.pressureMask == mk64_3ds::AdaptivePressureNone);
    assert(state->midpointProbeTicks == 1);
    assert(!state->midpointEnabled);
}

void CompleteProbe(mk64_3ds::AdaptivePresentationState* state) {
    auto inputs = HealthyInputs();
    for (std::uint8_t tick = 1; tick < mk64_3ds::kAdaptivePresentationProbeTicksToEnable;
         ++tick) {
        const auto decision = mk64_3ds::UpdateAdaptivePresentation(state, inputs);
        assert(decision.renderMidpoint);
        assert(decision.pressureMask == mk64_3ds::AdaptivePressureNone);
    }
    assert(state->midpointEnabled);
    assert(state->midpointProbeTicks == 0);
}

bool AdvanceUntilMidpoint(mk64_3ds::AdaptivePresentationState* state,
                          const mk64_3ds::AdaptivePresentationInputs& inputs,
                          std::uint32_t maximumTicks) {
    for (std::uint32_t tick = 0; tick < maximumTicks; ++tick) {
        if (mk64_3ds::UpdateAdaptivePresentation(state, inputs).renderMidpoint) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    static_assert(sizeof(mk64_3ds::AdaptivePresentationState) <= 4);
    assert(!mk64_3ds::UpdateAdaptivePresentation(nullptr, HealthyInputs()).renderMidpoint);

    mk64_3ds::AdaptivePresentationState state = {};
    auto inputs = HealthyInputs();
    inputs.hasPriorTopFrame = false;
    auto decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureNoPriorFrame) != 0);
    assert(state.cooldownTicks == mk64_3ds::kAdaptivePresentationPressureCooldownTicks);

    DrainCooldown(&state);
    inputs = HealthyInputs();
    inputs.keyframeHeadroom = false;
    for (std::uint32_t tick = 0;
         tick < mk64_3ds::kAdaptivePresentationHealthyTicksToEnable * 2U; ++tick) {
        ExpectRecovery(mk64_3ds::UpdateAdaptivePresentation(&state, inputs));
    }
    assert(state.healthyRecoveryTicks == 0);
    assert(state.midpointProbeTicks == 0);

    RecoverToFirstProbe(&state);
    CompleteProbe(&state);

    // A merely non-critical audio level is not enough margin for an optional
    // second display list, even though it remains above the hard low-water mark.
    inputs = HealthyInputs();
    inputs.audioBufferedFrames = inputs.audioLowWaterFrames + 1;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    ExpectRecovery(decision);
    assert(state.cooldownTicks == mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks);

    inputs.audioBufferedFrames = inputs.audioLowWaterFrames;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureAudioLow) != 0);

    inputs = HealthyInputs();
    inputs.audioBufferedFrames = inputs.audioRecoveryFrames - 1;
    ExpectRecovery(mk64_3ds::UpdateAdaptivePresentation(&state, inputs));

    inputs = HealthyInputs();
    DrainCooldown(&state);
    RecoverToFirstProbe(&state);

    inputs.resourceActivity = true;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureResourceActivity) != 0);
    assert(state.cooldownTicks == mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks);

    inputs = HealthyInputs();
    inputs.previousTickSlow = true;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureSlowTick) != 0);

    // A workload that becomes slow just before the long recovery threshold
    // never spends time on an optional midpoint.
    state = {};
    std::uint32_t oscillatingMidpoints = 0;
    inputs = HealthyInputs();
    for (std::uint32_t tick = 0; tick < 240; ++tick) {
        inputs.previousTickSlow =
            tick % mk64_3ds::kAdaptivePresentationHealthyTicksToEnable ==
            mk64_3ds::kAdaptivePresentationHealthyTicksToEnable - 1;
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
        oscillatingMidpoints += decision.renderMidpoint ? 1u : 0u;
    }
    assert(oscillatingMidpoints == 0);
    assert(!state.midpointEnabled);

    // If a trial midpoint overloads the following keyframe, wait two seconds
    // before collecting a fresh sustained-headroom window.
    state = {};
    inputs = HealthyInputs();
    std::uint32_t failedProbeMidpoints = 0;
    for (std::uint32_t cycle = 0; cycle < 3; ++cycle) {
        assert(AdvanceUntilMidpoint(&state, inputs, 120));
        ++failedProbeMidpoints;
        inputs.citro3DBusy = true;
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
        assert(!decision.renderMidpoint);
        assert(state.cooldownTicks == mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks);
        inputs.citro3DBusy = false;
    }
    assert(failedProbeMidpoints == 3);
    assert(!state.midpointEnabled);

    // Genuine sustained headroom survives validation and keeps requesting a
    // midpoint until a real pressure signal arrives.
    DrainCooldown(&state);
    RecoverToFirstProbe(&state);
    CompleteProbe(&state);
    for (std::uint32_t tick = 0; tick < 180; ++tick) {
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
        assert(decision.renderMidpoint);
    }

    inputs.textureUploadActivity = true;
    decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    assert(!decision.renderMidpoint);
    assert((decision.pressureMask & mk64_3ds::AdaptivePressureTextureUpload) != 0);

    state = {};
    inputs = HealthyInputs();
    for (std::uint8_t tick = 0;
         tick < mk64_3ds::kAdaptivePresentationHealthyTicksToEnable; ++tick) {
        decision = mk64_3ds::UpdateAdaptivePresentation(&state, inputs);
    }
    assert(decision.renderMidpoint);
    assert(!state.midpointEnabled);

    std::puts("adaptive presentation policy: ok");
    return 0;
}
