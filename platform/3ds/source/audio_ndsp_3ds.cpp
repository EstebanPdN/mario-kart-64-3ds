#include "audio_ndsp_3ds.h"

#include <3ds.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace {

constexpr int kChannel = 0;
// Six waves match the proven 3DS ports' safety margin and let the producer
// absorb course-start SD/GPU stalls without immediately starving NDSP.
constexpr size_t kBufferCount = 6;
constexpr size_t kFramesPerBuffer = 2048;
constexpr size_t kChannels = 2;

struct AudioBuffer {
    ndspWaveBuf wave = {};
    int16_t* samples = nullptr;
};

std::array<AudioBuffer, kBufferCount> sBuffers;
std::atomic<uint32_t> sQueuedBuffers{ 0 };
std::atomic<uint32_t> sDroppedBuffers{ 0 };
LightLock sBufferLock;
bool sInitialized = false;

class BufferLockGuard {
  public:
    BufferLockGuard() { LightLock_Lock(&sBufferLock); }
    ~BufferLockGuard() { LightLock_Unlock(&sBufferLock); }

    BufferLockGuard(const BufferLockGuard&) = delete;
    BufferLockGuard& operator=(const BufferLockGuard&) = delete;
};

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
    // From this point on Shutdown must balance ndspInit even when a later
    // linear allocation fails.
    LightLock_Init(&sBufferLock);
    sInitialized = true;

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
    ndspChnWaveBufClear(kChannel);
    ndspChnSetInterp(kChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(kChannel, static_cast<float>(sampleRate));
    ndspChnSetFormat(kChannel, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(kChannel, mix);
    ndspChnSetPaused(kChannel, false);
    return true;
}

extern "C" void Mk64Audio3DSShutdown(void) {
    if (sInitialized) {
        ndspChnSetPaused(kChannel, true);
        ndspChnWaveBufClear(kChannel);
        // Stop libctru's NDSP worker and unregister its APT/DSP hooks before
        // releasing any wave memory or letting the process tear services down.
        ndspExit();
        sInitialized = false;
    }
    for (auto& buffer : sBuffers) {
        if (buffer.samples != nullptr) {
            linearFree(buffer.samples);
            buffer.samples = nullptr;
        }
        std::memset(&buffer.wave, 0, sizeof(buffer.wave));
    }
    sQueuedBuffers.store(0, std::memory_order_relaxed);
    sDroppedBuffers.store(0, std::memory_order_relaxed);
}

extern "C" void Mk64Audio3DSSetPaused(bool paused) {
    if (sInitialized) {
        ndspChnSetPaused(kChannel, paused);
    }
}

extern "C" uint32_t Mk64Audio3DSBufferedFrames(void) {
    if (!sInitialized) {
        return 0;
    }

    // Audio synthesis can queue a wave from core 1/2 while frame pacing and
    // the developer overlay inspect the margin on core 0. Keep our wave
    // resets/additions and those snapshots serialized; libctru remains the
    // sole writer of the asynchronous status/sequence fields between calls.
    BufferLockGuard lock;

    // A PLAYING wave has already been consumed up to the channel's current
    // sample position. Counting its full size made the producer believe it
    // had almost one extra game tick buffered and delayed refills precisely
    // when a heavy race frame needed the safety margin most.
    const u16 playingSequence = ndspChnGetWaveBufSeq(kChannel);
    const u32 playingPosition = ndspChnGetSamplePos(kChannel);
    uint32_t frames = 0;
    for (const auto& buffer : sBuffers) {
        if (buffer.wave.status == NDSP_WBUF_QUEUED) {
            frames += buffer.wave.nsamples;
        } else if (buffer.wave.status == NDSP_WBUF_PLAYING) {
            if (buffer.wave.sequence_id == playingSequence &&
                playingPosition < buffer.wave.nsamples) {
                frames += buffer.wave.nsamples - playingPosition;
            } else {
                // Status and sequence are updated by NDSP. If they crossed
                // between the two snapshots, conservatively retain the wave
                // instead of reporting a false underrun.
                frames += buffer.wave.nsamples;
            }
        }
    }
    return frames;
}

extern "C" uint32_t Mk64Audio3DSQueuedCount(void) {
    return sQueuedBuffers.load(std::memory_order_relaxed);
}

extern "C" uint32_t Mk64Audio3DSDroppedCount(void) {
    return sDroppedBuffers.load(std::memory_order_relaxed);
}

extern "C" bool Mk64Audio3DSHasReusableBuffer(void) {
    if (!sInitialized) return false;
    BufferLockGuard lock;
    return std::any_of(sBuffers.begin(), sBuffers.end(),
                       [](const AudioBuffer& candidate) {
                           return IsReusable(candidate.wave);
                       });
}

extern "C" bool Mk64Audio3DSQueueStereoS16(const int16_t* samples, size_t frameCount) {
    if (!sInitialized || samples == nullptr || frameCount == 0 || frameCount > kFramesPerBuffer) {
        sDroppedBuffers.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    BufferLockGuard lock;
    auto buffer = std::find_if(sBuffers.begin(), sBuffers.end(),
                               [](const AudioBuffer& candidate) { return IsReusable(candidate.wave); });
    if (buffer == sBuffers.end()) {
        sDroppedBuffers.fetch_add(1, std::memory_order_relaxed);
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
    ndspChnSetPaused(kChannel, false);
    sQueuedBuffers.fetch_add(1, std::memory_order_relaxed);
    return true;
}
