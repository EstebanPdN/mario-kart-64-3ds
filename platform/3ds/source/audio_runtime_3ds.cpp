#include "audio_runtime_3ds.h"

#include "audio_ndsp_3ds.h"
#include "diagnostics_3ds.h"
#include "settings_3ds.h"

#include "audio/data.h"
#include "audio/heap.h"
#include "audio/load.h"

#include <libultraship/bridge/audiobridge.h>

// libultraship's N64 compatibility headers intentionally define u8/u16/u32
// with the original game's ABI. Keep libctru's fixed-width aliases private so
// both APIs can coexist in this translation unit on devkitARM.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

#include <array>
#include <atomic>
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
constexpr uint32_t kTargetBufferedFrames = kStereoFramesPerGameFrame * 2;
constexpr size_t kAudioWorkerStackSize = 64u * 1024u;
constexpr int32_t kAudioWorkerPriority = 0x18;

bool sReady = false;
uint32_t sPumpCount = 0;
bool sLoggedFirstSignal = false;
Thread sWorkerThread = nullptr;
LightEvent sWorkerStart;
LightEvent sWorkerDone;
std::atomic<bool> sWorkerRunning{ false };
std::atomic<bool> sJobOutstanding{ false };
std::atomic<bool> sPaused{ false };
std::atomic<uint16_t> sVolumePercent{ 100 };
int sWorkerCore = -1;
bool sCpuLimitChanged = false;
__3ds_u32 sPreviousCpuLimit = 0;

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

void ApplyMasterVolume(std::array<int16_t, kStereoSamplesPerGameFrame>& samples,
                       uint16_t volumePercent) {
    // Select once per buffer so the inner loop uses only constant power-of-two
    // divisions; ARM11 does not have a hardware integer divide instruction.
    switch (volumePercent) {
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

bool NeedsSynthesis() {
    return Mk64Audio3DSBufferedFrames() <= kTargetBufferedFrames;
}

void SynthesizeAndQueue(uint16_t volumePercent) {
    std::array<int16_t, kStereoSamplesPerGameFrame> samples = {};
    create_next_audio_buffer(samples.data(), kSamplesPerSynthesisFrame);
    create_next_audio_buffer(samples.data() + kSamplesPerSynthesisFrame * 2,
                             kSamplesPerSynthesisFrame);
    ApplyMasterVolume(samples, volumePercent);
    ++sPumpCount;

    // Inspect frequently enough to diagnose startup, but keep signal scans
    // out of the normal hot path.
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

void AudioWorkerMain(void*) {
    while (sWorkerRunning.load(std::memory_order_acquire)) {
        LightEvent_Wait(&sWorkerStart);
        if (!sWorkerRunning.load(std::memory_order_acquire)) break;

        // Game logic has already submitted its audio commands. Only this
        // worker consumes them, and the main thread waits for completion
        // before beginning the next logic tick, so libultraship's emulated
        // N64 message queues are never accessed concurrently across ticks.
        SynthesizeAndQueue(sVolumePercent.load(std::memory_order_relaxed));
        LightEvent_Signal(&sWorkerDone);
    }
}

void RestoreCpuLimit() {
    if (sCpuLimitChanged) {
        APT_SetAppCpuTimeLimit(sPreviousCpuLimit);
        sCpuLimitChanged = false;
    }
}

bool StartAudioWorker() {
    bool isNewModel = false;
    APT_CheckNew3DS(&isNewModel);
    sWorkerCore = isNewModel ? 2 : -1;

    if (!isNewModel) {
        __3ds_u32 currentLimit = 0;
        const bool hadLimit = R_SUCCEEDED(APT_GetAppCpuTimeLimit(&currentLimit));
        static constexpr std::array<__3ds_u32, 4> kCore1Limits = { 80, 70, 50, 30 };
        for (const __3ds_u32 candidate : kCore1Limits) {
            if (R_FAILED(APT_SetAppCpuTimeLimit(candidate))) continue;
            // A successful Set has already changed process state even if the
            // verification IPC happens to fail. Record it immediately so
            // every fallback/shutdown path can restore the previous limit.
            sPreviousCpuLimit = hadLimit ? currentLimit : 0;
            sCpuLimitChanged = !hadLimit || candidate != currentLimit;
            __3ds_u32 actual = candidate;
            if (R_SUCCEEDED(APT_GetAppCpuTimeLimit(&actual)) && actual == 0) {
                RestoreCpuLimit();
                continue;
            }
            sWorkerCore = 1;
            break;
        }
    }

    if (sWorkerCore < 0) {
        RestoreCpuLimit();
        Mk64Diagnostics3DSCheckpoint("audio-worker-sync-fallback");
        return false;
    }

    LightEvent_Init(&sWorkerStart, RESET_ONESHOT);
    LightEvent_Init(&sWorkerDone, RESET_ONESHOT);
    sWorkerRunning.store(true, std::memory_order_release);
    sWorkerThread = threadCreate(AudioWorkerMain, nullptr, kAudioWorkerStackSize,
                                 kAudioWorkerPriority, sWorkerCore, false);
    if (sWorkerThread == nullptr) {
        sWorkerRunning.store(false, std::memory_order_release);
        sWorkerCore = -1;
        RestoreCpuLimit();
        Mk64Diagnostics3DSCheckpoint("audio-worker-create-failed-sync-fallback");
        return false;
    }

    Mk64Diagnostics3DSCheckpoint(isNewModel ? "audio-worker-core2" : "audio-worker-core1");
    return true;
}

bool ScheduleWorkerJob() {
    if (sWorkerThread == nullptr || !sWorkerRunning.load(std::memory_order_acquire) ||
        sPaused.load(std::memory_order_acquire)) {
        return false;
    }
    bool expected = false;
    if (!sJobOutstanding.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }
    sVolumePercent.store(Mk64Settings3DSGetMasterVolumePercent(), std::memory_order_relaxed);
    LightEvent_Signal(&sWorkerStart);
    return true;
}

void WaitForWorkerJob() {
    if (!sJobOutstanding.load(std::memory_order_acquire)) return;
    LightEvent_Wait(&sWorkerDone);
    sJobOutstanding.store(false, std::memory_order_release);
}

}

extern "C" bool Mk64GameAudio3DSInit() {
    sReady = Mk64Audio3DSInit(kSampleRate);
    if (sReady) {
        sPaused.store(false, std::memory_order_relaxed);
        sJobOutstanding.store(false, std::memory_order_relaxed);
        StartAudioWorker();
    }
    return sReady;
}

extern "C" void Mk64GameAudio3DSSetPaused(bool paused) {
    if (sReady) {
        sPaused.store(paused, std::memory_order_release);
        if (paused) WaitForWorkerJob();
        Mk64Audio3DSSetPaused(paused);
    }
}

extern "C" void Mk64GameAudio3DSBeginFrame() {
    if (!sReady || sPaused.load(std::memory_order_acquire) || !NeedsSynthesis()) return;
    // Starting immediately before the display-list interpreter overlaps the
    // expensive mixer with CPU/GPU rendering instead of appending it to the
    // critical path after every rendered frame.
    ScheduleWorkerJob();
}

extern "C" void Mk64GameAudio3DSPump() {
    if (!sReady) return;

    const bool jobWasStartedDuringRender = sJobOutstanding.load(std::memory_order_acquire);
    if (jobWasStartedDuringRender) WaitForWorkerJob();
    if (sPaused.load(std::memory_order_acquire) || jobWasStartedDuringRender ||
        !NeedsSynthesis()) {
        return;
    }

    if (ScheduleWorkerJob()) {
        WaitForWorkerJob();
    } else {
        // Old 3DS systems that cannot reserve core 1 still retain the safe
        // single-threaded path rather than losing audio entirely.
        SynthesizeAndQueue(Mk64Settings3DSGetMasterVolumePercent());
    }
}

extern "C" void Mk64GameAudio3DSShutdown() {
    if (sWorkerThread != nullptr) {
        WaitForWorkerJob();
        sWorkerRunning.store(false, std::memory_order_release);
        LightEvent_Signal(&sWorkerStart);
        threadJoin(sWorkerThread, U64_MAX);
        threadFree(sWorkerThread);
        sWorkerThread = nullptr;
    }
    sWorkerCore = -1;
    RestoreCpuLimit();
    Mk64Audio3DSShutdown();
    sReady = false;
    sPumpCount = 0;
    sLoggedFirstSignal = false;
    sPaused.store(false, std::memory_order_relaxed);
    sJobOutstanding.store(false, std::memory_order_relaxed);
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
