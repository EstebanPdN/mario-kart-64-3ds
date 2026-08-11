#include "audio_runtime_3ds.h"

#include "audio_ndsp_3ds.h"
#include "diagnostics_3ds.h"
#include "settings_3ds.h"

#include "audio/data.h"
#include "audio/heap.h"
#include "audio/load.h"

#include <libultraship/bridge/audiobridge.h>

#include <array>
#include <cstdint>

extern "C" void create_next_audio_buffer(int16_t* samples, uint32_t sampleCount);

namespace {
// MK64's audio session preset is 0x68b0 (26,800 Hz). Matching NDSP to the
// synthesis rate avoids both pitch error and a steady underrun at 30 Hz.
constexpr uint32_t kSampleRate = 26800;
constexpr uint32_t kSamplesPerSynthesisFrame = 448;
constexpr uint32_t kSynthesisFramesPerGameFrame = 2;
constexpr uint32_t kStereoFramesPerGameFrame =
    kSamplesPerSynthesisFrame * kSynthesisFramesPerGameFrame;
constexpr uint32_t kStereoSamplesPerGameFrame = kStereoFramesPerGameFrame * 2;
bool sReady = false;
uint32_t sPumpCount = 0;
bool sLoggedFirstSignal = false;

template <int32_t Numerator, int32_t Denominator>
void ApplyGain(std::array<int16_t, kStereoSamplesPerGameFrame>& samples) {
    for (int16_t& sample : samples) {
        int32_t scaled = static_cast<int32_t>(sample) * Numerator / Denominator;
        if (scaled > INT16_MAX) {
            scaled = INT16_MAX;
        } else if (scaled < INT16_MIN) {
            scaled = INT16_MIN;
        }
        sample = static_cast<int16_t>(scaled);
    }
}

void ApplyMasterVolume(std::array<int16_t, kStereoSamplesPerGameFrame>& samples) {
    // Select once per buffer so the inner loop uses only constant power-of-two
    // divisions; ARM11 does not have a hardware integer divide instruction.
    switch (Mk64Settings3DSGetMasterVolumePercent()) {
        case 25:
            ApplyGain<1, 4>(samples);
            break;
        case 50:
            ApplyGain<1, 2>(samples);
            break;
        case 75:
            ApplyGain<3, 4>(samples);
            break;
        case 150:
            ApplyGain<3, 2>(samples);
            break;
        case 200:
            ApplyGain<2, 1>(samples);
            break;
        case 100:
        default:
            break;
    }
}

void LogAudioState() {
    uint32_t activePlayers = 0;
    uint32_t activeNotes = 0;
    for (int i = 0; i < SEQUENCE_PLAYERS; ++i) {
        if (gSequencePlayers[i].enabled) {
            ++activePlayers;
        }
    }
    if (gNotes != nullptr && gMaxSimultaneousNotes > 0) {
        for (int i = 0; i < gMaxSimultaneousNotes; ++i) {
            if (gNotes[i].noteSubEu.enabled) {
                ++activeNotes;
            }
        }
    }
    Mk64Diagnostics3DSAudioState(static_cast<uint32_t>(gAudioResetStatus),
                                 static_cast<uint32_t>(gAudioResetPresetIdToLoad),
                                 static_cast<uint32_t>(gSequenceCount), activePlayers, activeNotes,
                                 static_cast<uint32_t>(gAudioErrorFlags));
}

}

extern "C" bool Mk64GameAudio3DSInit() {
    sReady = Mk64Audio3DSInit(kSampleRate);
    return sReady;
}

extern "C" void Mk64GameAudio3DSSetPaused(bool paused) {
    if (sReady) {
        Mk64Audio3DSSetPaused(paused);
    }
}

extern "C" void Mk64GameAudio3DSPump() {
    if (!sReady || Mk64Audio3DSBufferedFrames() > kStereoFramesPerGameFrame * 2) {
        return;
    }

    std::array<int16_t, kStereoSamplesPerGameFrame> samples = {};
    create_next_audio_buffer(samples.data(), kSamplesPerSynthesisFrame);
    create_next_audio_buffer(samples.data() + kSamplesPerSynthesisFrame * 2,
                             kSamplesPerSynthesisFrame);
    ApplyMasterVolume(samples);
    ++sPumpCount;
    // Inspect frequently enough to diagnose startup, but do not synchronously
    // flush an SD log every game tick while the intro is intentionally silent.
    const bool inspectSignal = sPumpCount <= 4u ||
                               (!sLoggedFirstSignal && (sPumpCount % 30u) == 0u) ||
                               (sPumpCount % 180u) == 0u;
    uint32_t peak = 0;
    uint32_t nonzero = 0;
    if (inspectSignal) {
        for (const int16_t sample : samples) {
            const int32_t value =
                sample < 0 ? -static_cast<int32_t>(sample) : static_cast<int32_t>(sample);
            if (value != 0) ++nonzero;
            if (static_cast<uint32_t>(value) > peak) peak = static_cast<uint32_t>(value);
        }
    }
    Mk64Audio3DSQueueStereoS16(samples.data(), kStereoFramesPerGameFrame);
    const bool firstSignal = inspectSignal && peak != 0u && !sLoggedFirstSignal;
    if (firstSignal) sLoggedFirstSignal = true;
    if (inspectSignal || firstSignal) {
        Mk64Diagnostics3DSAudio(sPumpCount, Mk64Audio3DSBufferedFrames(), peak, nonzero,
                                Mk64Audio3DSQueuedCount(), Mk64Audio3DSDroppedCount());
        LogAudioState();
    }
}

extern "C" void Mk64GameAudio3DSShutdown() {
    Mk64Audio3DSShutdown();
    sReady = false;
    sPumpCount = 0;
    sLoggedFirstSignal = false;
}

extern "C" uint32_t GameEngine_GetSampleRate() {
    return kSampleRate;
}

extern "C" uint32_t GameEngine_GetSamplesPerFrame() {
    return kStereoSamplesPerGameFrame;
}

extern "C" void SetAudioChannels(AudioChannelsSetting) {
    // NDSP output is fixed to stereo for the vanilla 3DS runtime.
}
