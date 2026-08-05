#include "audio_runtime_3ds.h"

#include "audio_ndsp_3ds.h"

#include <libultraship/bridge/audiobridge.h>

#include <array>
#include <cstdint>

extern "C" void create_next_audio_buffer(int16_t* samples, uint32_t sampleCount);

namespace {
constexpr uint32_t kSampleRate = 32000;
constexpr uint32_t kSamplesPerSynthesisFrame = 448;
constexpr uint32_t kSynthesisFramesPerGameFrame = 2;
constexpr uint32_t kStereoFramesPerGameFrame =
    kSamplesPerSynthesisFrame * kSynthesisFramesPerGameFrame;
constexpr uint32_t kStereoSamplesPerGameFrame = kStereoFramesPerGameFrame * 2;
bool sReady = false;
}

extern "C" bool Mk64GameAudio3DSInit() {
#if defined(MK64_3DS_ENABLE_NDSP_AUDIO)
    sReady = Mk64Audio3DSInit(kSampleRate);
#else
    sReady = false;
#endif
    return sReady;
}

extern "C" void Mk64GameAudio3DSPump() {
    if (!sReady || Mk64Audio3DSBufferedFrames() > kStereoFramesPerGameFrame * 2) {
        return;
    }

    std::array<int16_t, kStereoSamplesPerGameFrame> samples = {};
    create_next_audio_buffer(samples.data(), kSamplesPerSynthesisFrame);
    create_next_audio_buffer(samples.data() + kSamplesPerSynthesisFrame * 2,
                             kSamplesPerSynthesisFrame);
    Mk64Audio3DSQueueStereoS16(samples.data(), kStereoFramesPerGameFrame);
}

extern "C" void Mk64GameAudio3DSShutdown() {
    Mk64Audio3DSShutdown();
    sReady = false;
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
