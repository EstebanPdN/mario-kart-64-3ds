#pragma once

#include <cstdint>

namespace mk64_3ds {

constexpr std::uint8_t kAdaptivePresentationHealthyTicksToEnable = 8;

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
    bool previousTickSlow = false;
    bool resourceActivity = false;
    bool textureUploadActivity = false;
    bool citro3DBusy = false;
};

struct AdaptivePresentationState {
    bool midpointEnabled = false;
    std::uint8_t healthyRecoveryTicks = 0;
};

struct AdaptivePresentationDecision {
    bool renderMidpoint = false;
    std::uint32_t pressureMask = AdaptivePressureNone;
    std::uint8_t healthyRecoveryTicks = 0;
};

// A midpoint is optional; the following keyframe is not. Disable immediately
// on pressure, then require several ticks with a higher audio margin before
// spending CPU/GPU time on interpolating and decoding a second display list.
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

    if (decision.pressureMask != AdaptivePressureNone) {
        state->midpointEnabled = false;
        state->healthyRecoveryTicks = 0;
        return decision;
    }

    if (state->midpointEnabled) {
        decision.renderMidpoint = true;
        decision.healthyRecoveryTicks = state->healthyRecoveryTicks;
        return decision;
    }

    if (inputs.audioBufferedFrames < inputs.audioRecoveryFrames) {
        state->healthyRecoveryTicks = 0;
        decision.pressureMask = AdaptivePressureRecovery;
        return decision;
    }

    if (state->healthyRecoveryTicks < kAdaptivePresentationHealthyTicksToEnable) {
        ++state->healthyRecoveryTicks;
    }
    if (state->healthyRecoveryTicks >= kAdaptivePresentationHealthyTicksToEnable) {
        state->midpointEnabled = true;
        decision.renderMidpoint = true;
    } else {
        decision.pressureMask = AdaptivePressureRecovery;
    }
    decision.healthyRecoveryTicks = state->healthyRecoveryTicks;
    return decision;
}

} // namespace mk64_3ds
