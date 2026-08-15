#pragma once

#include <cstdint>

namespace mk64_3ds {

constexpr std::uint8_t kAdaptivePresentationHealthyTicksToEnable = 45;
constexpr std::uint8_t kAdaptivePresentationProbeTicksToEnable = 8;
constexpr std::uint8_t kAdaptivePresentationPressureCooldownTicks = 15;
constexpr std::uint8_t kAdaptivePresentationFailedProbeCooldownTicks = 60;

enum AdaptivePresentationPressure : std::uint32_t {
    AdaptivePressureNone = 0,
    AdaptivePressureNoPriorFrame = 1u << 0,
    AdaptivePressureAudioLow = 1u << 1,
    AdaptivePressureSlowTick = 1u << 2,
    AdaptivePressureResourceActivity = 1u << 3,
    AdaptivePressureTextureUpload = 1u << 4,
    AdaptivePressureCitro3DBusy = 1u << 5,
    AdaptivePressureRecovery = 1u << 6,
};

struct AdaptivePresentationInputs {
    bool hasPriorTopFrame = false;
    std::uint32_t audioBufferedFrames = 0;
    std::uint32_t audioLowWaterFrames = 0;
    std::uint32_t audioRecoveryFrames = 0;
    bool keyframeHeadroom = false;
    bool previousTickSlow = false;
    bool resourceActivity = false;
    bool textureUploadActivity = false;
    bool citro3DBusy = false;
};

struct AdaptivePresentationState {
    bool midpointEnabled = false;
    std::uint8_t healthyRecoveryTicks = 0;
    std::uint8_t midpointProbeTicks = 0;
    std::uint8_t cooldownTicks = 0;
};

struct AdaptivePresentationDecision {
    bool renderMidpoint = false;
    std::uint32_t pressureMask = AdaptivePressureNone;
    std::uint8_t healthyRecoveryTicks = 0;
};

// A midpoint is optional; the following keyframe is not. Require sustained
// keyframe headroom, then validate several midpoints before committing to the
// optional midpoint path. A failed validation gets a longer cooldown to avoid
// oscillating between an expensive probe and an overloaded keyframe.
inline AdaptivePresentationDecision UpdateAdaptivePresentation(
    AdaptivePresentationState* state, const AdaptivePresentationInputs& inputs) {
    AdaptivePresentationDecision decision = {};
    if (state == nullptr) {
        return decision;
    }

    if (!inputs.hasPriorTopFrame) {
        decision.pressureMask |= AdaptivePressureNoPriorFrame;
    }
    if (inputs.audioBufferedFrames <= inputs.audioLowWaterFrames) {
        decision.pressureMask |= AdaptivePressureAudioLow;
    }
    if (inputs.previousTickSlow) {
        decision.pressureMask |= AdaptivePressureSlowTick;
    }
    if (inputs.resourceActivity) {
        decision.pressureMask |= AdaptivePressureResourceActivity;
    }
    if (inputs.textureUploadActivity) {
        decision.pressureMask |= AdaptivePressureTextureUpload;
    }
    if (inputs.citro3DBusy) {
        decision.pressureMask |= AdaptivePressureCitro3DBusy;
    }

    const bool lacksRecoveryMargin = inputs.audioBufferedFrames < inputs.audioRecoveryFrames;
    if (decision.pressureMask != AdaptivePressureNone || lacksRecoveryMargin) {
        const bool failedMidpoint = state->midpointEnabled || state->midpointProbeTicks != 0;
        state->midpointEnabled = false;
        state->healthyRecoveryTicks = 0;
        state->midpointProbeTicks = 0;
        if (failedMidpoint) {
            state->cooldownTicks = kAdaptivePresentationFailedProbeCooldownTicks;
        } else if (decision.pressureMask != AdaptivePressureNone &&
                   state->cooldownTicks < kAdaptivePresentationPressureCooldownTicks) {
            state->cooldownTicks = kAdaptivePresentationPressureCooldownTicks;
        }
        if (lacksRecoveryMargin && decision.pressureMask == AdaptivePressureNone) {
            decision.pressureMask = AdaptivePressureRecovery;
        }
        return decision;
    }

    if (state->cooldownTicks != 0) {
        --state->cooldownTicks;
        decision.pressureMask = AdaptivePressureRecovery;
        return decision;
    }

    if (state->midpointEnabled) {
        decision.renderMidpoint = true;
        decision.healthyRecoveryTicks = state->healthyRecoveryTicks;
        return decision;
    }

    if (state->midpointProbeTicks != 0) {
        if (state->midpointProbeTicks < kAdaptivePresentationProbeTicksToEnable) {
            ++state->midpointProbeTicks;
        }
        decision.renderMidpoint = true;
        if (state->midpointProbeTicks >= kAdaptivePresentationProbeTicksToEnable) {
            state->midpointEnabled = true;
            state->midpointProbeTicks = 0;
        }
        decision.healthyRecoveryTicks = state->healthyRecoveryTicks;
        return decision;
    }

    // Meeting the 30 Hz deadline is not enough evidence that a second display
    // list will fit before the next required keyframe. Only keyframe-only
    // samples with a measured midpoint-sized margin may build recovery credit.
    if (!inputs.keyframeHeadroom) {
        state->healthyRecoveryTicks = 0;
        decision.pressureMask = AdaptivePressureRecovery;
        return decision;
    }

    if (state->healthyRecoveryTicks < kAdaptivePresentationHealthyTicksToEnable) {
        ++state->healthyRecoveryTicks;
    }
    if (state->healthyRecoveryTicks >= kAdaptivePresentationHealthyTicksToEnable) {
        state->midpointProbeTicks = 1;
        decision.renderMidpoint = true;
    } else {
        decision.pressureMask = AdaptivePressureRecovery;
    }
    decision.healthyRecoveryTicks = state->healthyRecoveryTicks;
    return decision;
}

} // namespace mk64_3ds
