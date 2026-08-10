#include "audio_ndsp_3ds.h"

#include <3ds.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {

constexpr int kChannel = 0;
constexpr size_t kBufferCount = 4;
constexpr size_t kFramesPerBuffer = 2048;
constexpr size_t kChannels = 2;

struct AudioBuffer {
    ndspWaveBuf wave = {};
    int16_t* samples = nullptr;
};

std::array<AudioBuffer, kBufferCount> sBuffers;
bool sInitialized = false;

bool IsReusable(const ndspWaveBuf& wave) {
    return wave.status == NDSP_WBUF_FREE || wave.status == NDSP_WBUF_DONE;
}

void ResetWave(AudioBuffer& buffer, size_t frameCount) {
    std::memset(&buffer.wave, 0, sizeof(buffer.wave));
    buffer.wave.data_vaddr = buffer.samples;
    buffer.wave.nsamples = static_cast<u32>(frameCount);
    buffer.wave.looping = false;
}

} // namespace

extern "C" bool Mk64Audio3DSInit(uint32_t sampleRate) {
    if (sInitialized) {
        return true;
    }
    if (R_FAILED(ndspInit())) {
        return false;
    }

    for (auto& buffer : sBuffers) {
        buffer.samples = static_cast<int16_t*>(
            linearAlloc(kFramesPerBuffer * kChannels * sizeof(int16_t)));
        if (buffer.samples == nullptr) {
            Mk64Audio3DSShutdown();
            return false;
        }
        std::memset(buffer.samples, 0, kFramesPerBuffer * kChannels * sizeof(int16_t));
        ResetWave(buffer, 0);
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(kChannel);
    ndspChnSetInterp(kChannel, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(kChannel, static_cast<float>(sampleRate));
    ndspChnSetFormat(kChannel, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(kChannel, mix);
    sInitialized = true;
    return true;
}

extern "C" void Mk64Audio3DSShutdown(void) {
    if (sInitialized) {
        ndspChnWaveBufClear(kChannel);
    }
    for (auto& buffer : sBuffers) {
        if (buffer.samples != nullptr) {
            linearFree(buffer.samples);
            buffer.samples = nullptr;
        }
        std::memset(&buffer.wave, 0, sizeof(buffer.wave));
    }
    if (sInitialized) {
        ndspExit();
    }
    sInitialized = false;
}

extern "C" uint32_t Mk64Audio3DSBufferedFrames(void) {
    if (!sInitialized) {
        return 0;
    }

    uint32_t frames = 0;
    for (const auto& buffer : sBuffers) {
        if (buffer.wave.status == NDSP_WBUF_QUEUED || buffer.wave.status == NDSP_WBUF_PLAYING) {
            frames += buffer.wave.nsamples;
        }
    }
    return frames;
}

extern "C" bool Mk64Audio3DSQueueStereoS16(const int16_t* samples, size_t frameCount) {
    if (!sInitialized || samples == nullptr || frameCount == 0 || frameCount > kFramesPerBuffer) {
        return false;
    }

    auto buffer = std::find_if(sBuffers.begin(), sBuffers.end(),
                               [](const AudioBuffer& candidate) { return IsReusable(candidate.wave); });
    if (buffer == sBuffers.end()) {
        return false;
    }

    const size_t byteCount = frameCount * kChannels * sizeof(int16_t);
    std::memcpy(buffer->samples, samples, byteCount);
    // DSP_FlushDataCache is disproportionately expensive when the application
    // CPU limit is high. The kernel cache operation provides the coherency
    // NDSP needs without burning a material part of an Old 3DS frame.
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, reinterpret_cast<u32>(buffer->samples),
                             static_cast<u32>(byteCount));
    ResetWave(*buffer, frameCount);
    ndspChnWaveBufAdd(kChannel, &buffer->wave);
    return true;
}
