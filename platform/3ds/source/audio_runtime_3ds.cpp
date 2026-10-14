#include "audio_runtime_3ds.h"
#include "game_runtime_3ds.h"

#include "audio_ndsp_3ds.h"
#include "diagnostics_3ds.h"
#include "settings_3ds.h"
#include "performance_trace_3ds.hpp"
#include "startup_trace_3ds.h"

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
#include <cstdio>

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
// Refill begins below a roughly 67 ms target. A final 896-frame block may
// transiently overshoot that threshold; the six-wave pool provides capacity
// without making a third synthesis block part of the normal policy.
constexpr uint32_t kTargetBufferedFrames = kStereoFramesPerGameFrame * 2;
constexpr uint32_t kMaxSynthesisBlocksPerPump = 1;
constexpr size_t kAudioWorkerStackSize = 32u * 1024u;

bool sReady = false;
bool sGameStateReady = false;
bool sSuspended = false;
uint32_t sSynthesisBlockCount = 0;
uint32_t sPumpCallCount = 0;
uint32_t sMultiBlockPumpCount = 0;
uint32_t sQueueFailureCount = 0;
uint32_t sObservedEmptyTransitionCount = 0;
bool sLoggedFirstSignal = false;
bool sAudioPrimed = false;
bool sEmptyObservedActive = false;
Thread sWorkerThread = nullptr;
LightEvent sAudioWakeEvent;
std::atomic<bool> sAudioWorkerBusy{ false };
std::atomic<bool> sWorkerRunning{ false };
std::atomic<bool> sPaused{ false };
std::atomic<uint16_t> sVolumePercent{ 100 };
int sWorkerCore = -1;
bool sCpuLimitChanged = false;
__3ds_u32 sPreviousCpuLimit = 0;

template <int32_t Numerator, int32_t Denominator>
void ApplyGain(int16_t* samples, size_t sampleCount) {
    for (size_t index = 0; index < sampleCount; ++index) {
        int32_t scaled = static_cast<int32_t>(samples[index]) * Numerator / Denominator;
        if (scaled > INT16_MAX) {
            scaled = INT16_MAX;
        } else if (scaled < INT16_MIN) {
            scaled = INT16_MIN;
        }
        samples[index] = static_cast<int16_t>(scaled);
    }
}

void ApplyMasterVolume(int16_t* samples, size_t sampleCount, uint16_t volumePercent) {
    // Select once per buffer so the inner loop uses only constant power-of-two
    // divisions; ARM11 does not have a hardware integer divide instruction.
    switch (volumePercent) {
        case 25:
            ApplyGain<1, 4>(samples, sampleCount);
            break;
        case 50:
            ApplyGain<1, 2>(samples, sampleCount);
            break;
        case 75:
            ApplyGain<3, 4>(samples, sampleCount);
            break;
        case 150:
            ApplyGain<3, 2>(samples, sampleCount);
            break;
        case 200:
            ApplyGain<2, 1>(samples, sampleCount);
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
    return Mk64Audio3DSBufferedFrames() < kTargetBufferedFrames;
}

bool SynthesizeAndQueue(uint16_t volumePercent) {
    // create_next_audio_buffer advances the music and SFX timeline. Check the
    // sole-producer wave pool first so an already-full queue cannot consume
    // audio state that NDSP will never play.
    uint32_t bufferToken = 0;
    int16_t* samples =
        Mk64Audio3DSAcquireStereoS16(kStereoFramesPerGameFrame, &bufferToken);
    if (samples == nullptr) return false;
    create_next_audio_buffer(samples, kSamplesPerSynthesisFrame);
    create_next_audio_buffer(samples + kSamplesPerSynthesisFrame * 2,
                             kSamplesPerSynthesisFrame);
    ApplyMasterVolume(samples, kStereoSamplesPerGameFrame, volumePercent);
    const uint32_t nextBlock = sSynthesisBlockCount + 1u;

    // Inspect frequently enough to diagnose startup, but keep signal scans
    // out of the normal hot path.
    const bool inspectSignal = nextBlock <= 4u ||
                               (!sLoggedFirstSignal && (nextBlock % 30u) == 0u) ||
                               (nextBlock % 180u) == 0u;
    uint32_t peak = 0;
    uint32_t nonzero = 0;
    if (inspectSignal) {
        for (size_t index = 0; index < kStereoSamplesPerGameFrame; ++index) {
            const int16_t sample = samples[index];
            const int32_t value =
                sample < 0 ? -static_cast<int32_t>(sample) : static_cast<int32_t>(sample);
            if (value != 0) ++nonzero;
            if (static_cast<uint32_t>(value) > peak) peak = static_cast<uint32_t>(value);
        }
    }
    if (!Mk64Audio3DSCommitStereoS16(bufferToken, kStereoFramesPerGameFrame)) {
        Mk64Audio3DSReleaseStereoS16(bufferToken);
        return false;
    }
    sSynthesisBlockCount = nextBlock;
    const bool firstSignal = inspectSignal && peak != 0u && !sLoggedFirstSignal;
    if (firstSignal) sLoggedFirstSignal = true;
    if (inspectSignal || firstSignal) {
        Mk64Diagnostics3DSAudio(sSynthesisBlockCount, Mk64Audio3DSBufferedFrames(), peak, nonzero,
                                Mk64Audio3DSQueuedCount(), Mk64Audio3DSDroppedCount());
        LogAudioState();
    }
    return true;
}

void FinishPumpTelemetry(uint32_t bufferedBefore, uint32_t blocksThisPump,
                         bool queueFailedThisPump) {
    const uint32_t bufferedAfter = Mk64Audio3DSBufferedFrames();
    if (bufferedAfter >= kStereoFramesPerGameFrame) sAudioPrimed = true;
    if (bufferedAfter != 0) sEmptyObservedActive = false;
    if (blocksThisPump > 1) ++sMultiBlockPumpCount;
    const bool sampledCatchup = blocksThisPump > 1u &&
                                (sMultiBlockPumpCount <= 4u ||
                                 (sMultiBlockPumpCount % 60u) == 0u);
    const bool sampledFailure = queueFailedThisPump &&
                                (sQueueFailureCount <= 4u ||
                                 (sQueueFailureCount % 30u) == 0u);
    const bool shouldLog = sPumpCallCount <= 4u || (sPumpCallCount % 180u) == 0u ||
                           sampledCatchup || sampledFailure;
    if (shouldLog) {
        Mk64Diagnostics3DSAudioPump(sPumpCallCount, sSynthesisBlockCount,
                                    bufferedBefore, bufferedAfter, blocksThisPump,
                                    sMultiBlockPumpCount, sQueueFailureCount,
                                    sObservedEmptyTransitionCount);
    }
}

void AudioWorkerMain(void*) {
    while (sWorkerRunning.load(std::memory_order_acquire)) {
        LightEvent_Wait(&sAudioWakeEvent);
        if (!sWorkerRunning.load(std::memory_order_acquire)) break;

        sAudioWorkerBusy.store(true, std::memory_order_release);

        while (sWorkerRunning.load(std::memory_order_acquire) &&
               !sPaused.load(std::memory_order_relaxed) &&
               NeedsSynthesis()) {
            const uint16_t volume = sVolumePercent.load(std::memory_order_relaxed);
            if (!SynthesizeAndQueue(volume)) {
                break;
            }
        }

        sAudioWorkerBusy.store(false, std::memory_order_release);
    }
}

void RestoreCpuLimit() {
    if (sCpuLimitChanged) {
        APT_SetAppCpuTimeLimit(sPreviousCpuLimit);
        sCpuLimitChanged = false;
    }
}

// Stop the producer before APT hands DSP/GPU ownership to HOME. Never free
// its stack or restore the shared-core budget while the thread is alive.
void StopAudioWorker() {
    sWorkerRunning.store(false, std::memory_order_release);
    if (sWorkerThread != nullptr) {
        LightEvent_Signal(&sAudioWakeEvent);
        if (R_FAILED(threadJoin(sWorkerThread, 500ULL * 1000ULL * 1000ULL))) {
            svcExitProcess();
        }
        threadFree(sWorkerThread);
        sWorkerThread = nullptr;
    }
    sAudioWorkerBusy.store(false, std::memory_order_release);
    sWorkerCore = -1;
    RestoreCpuLimit();
}

bool StartAudioWorker() {
    const bool isNewModel = Mk64Graphics3DSResolvedPerformanceProfile() ==
                            MK64_PERFORMANCE_PROFILE_NEW_3DS;
    sWorkerCore = isNewModel ? 2 : 1;

    if (!isNewModel) {
        __3ds_u32 currentLimit = 0;
        const bool hadLimit = R_SUCCEEDED(APT_GetAppCpuTimeLimit(&currentLimit));
        if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(30))) {
            sPreviousCpuLimit = hadLimit ? currentLimit : 0;
            sCpuLimitChanged = true;
            Mk64Diagnostics3DSCheckpoint("audio-core1-limit-30");
        }
    }

    LightEvent_Init(&sAudioWakeEvent, RESET_ONESHOT);
    sWorkerRunning.store(true, std::memory_order_release);
    sAudioWorkerBusy.store(false, std::memory_order_release);

    int32_t prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    int32_t workerPrio = prio + 1;
    if (workerPrio < 0x1A) workerPrio = 0x1A;
    if (workerPrio > 0x3E) workerPrio = 0x3E;

    sWorkerThread = threadCreate(AudioWorkerMain, nullptr, kAudioWorkerStackSize,
                                 workerPrio, sWorkerCore, false);
    if (sWorkerThread == nullptr && sWorkerCore != 0) {
        // Fallback automatically to Core 0 if Core 1 thread creation fails
        sWorkerCore = 0;
        sWorkerThread = threadCreate(AudioWorkerMain, nullptr, kAudioWorkerStackSize,
                                     workerPrio, sWorkerCore, false);
    }

    if (sWorkerThread == nullptr) {
        sWorkerRunning.store(false, std::memory_order_release);
        sWorkerCore = -1;
        RestoreCpuLimit();
        Mk64Diagnostics3DSCheckpoint("audio-worker-create-failed-sync-fallback");
        return false;
    }

    Mk64Diagnostics3DSCheckpoint(isNewModel ? "audio-worker-core2" : (sWorkerCore == 1 ? "audio-worker-core1" : "audio-worker-core0"));
    return true;
}

} // namespace

extern "C" bool Mk64GameAudio3DSInit() {
    sGameStateReady = false;
    sSuspended = false;
    sReady = Mk64Audio3DSInit(kSampleRate);
    if (sReady) {
        sSynthesisBlockCount = 0;
        sPumpCallCount = 0;
        sMultiBlockPumpCount = 0;
        sQueueFailureCount = 0;
        sObservedEmptyTransitionCount = 0;
        sLoggedFirstSignal = false;
        sAudioPrimed = false;
        sEmptyObservedActive = false;
        sPaused.store(false, std::memory_order_relaxed);
        sAudioWorkerBusy.store(false, std::memory_order_relaxed);
        StartAudioWorker();
    }
    return sReady;
}

extern "C" void Mk64GameAudio3DSFinishInitialization() {
    sGameStateReady = sReady;
}

extern "C" void Mk64GameAudio3DSBeginLogic() {
    // 100% asynchronous: zero blocking on Core 0.
}

extern "C" void Mk64GameAudio3DSSetPaused(bool paused) {
    if (sReady) {
        sPaused.store(paused, std::memory_order_release);
        Mk64Audio3DSSetPaused(paused);
    }
}

extern "C" void Mk64GameAudio3DSSuspend() {
    if (!sReady || sSuspended) return;
    sSuspended = true;
    StopAudioWorker();
    Mk64Audio3DSSetPaused(true);
}

extern "C" void Mk64GameAudio3DSResume() {
    if (!sReady || !sSuspended) return;
    sSuspended = false;
    StartAudioWorker();
    Mk64Audio3DSSetPaused(sPaused.load(std::memory_order_acquire));
}

extern "C" void Mk64GameAudio3DSBeginFrame() {
    if (!sReady || !sGameStateReady || sSuspended || sPaused.load(std::memory_order_relaxed)) return;
    sVolumePercent.store(Mk64Settings3DSGetMasterVolumePercent(), std::memory_order_relaxed);
    if (!sAudioWorkerBusy.load(std::memory_order_relaxed) && NeedsSynthesis()) {
        LightEvent_Signal(&sAudioWakeEvent);
    }
}

extern "C" void Mk64GameAudio3DSPump() {
    if (!sReady || !sGameStateReady || sSuspended) return;

    ++sPumpCallCount;
    const uint32_t bufferedBefore = Mk64Audio3DSBufferedFrames();
    if (sAudioPrimed && bufferedBefore == 0 && !sEmptyObservedActive) {
        ++sObservedEmptyTransitionCount;
        sEmptyObservedActive = true;
    } else if (bufferedBefore != 0) {
        sEmptyObservedActive = false;
    }

    sVolumePercent.store(Mk64Settings3DSGetMasterVolumePercent(), std::memory_order_relaxed);

    if (sPaused.load(std::memory_order_relaxed)) {
        FinishPumpTelemetry(bufferedBefore, 0, false);
        return;
    }

    if (sWorkerThread != nullptr) {
        // Asynchronous fire-and-forget: signal Core 1 worker if idle and synthesis needed
        if (!sAudioWorkerBusy.load(std::memory_order_relaxed) && bufferedBefore < kTargetBufferedFrames) {
            LightEvent_Signal(&sAudioWakeEvent);
        }
    } else {
        // Fallback only if worker thread could not be created
        if (bufferedBefore < kTargetBufferedFrames) {
            SynthesizeAndQueue(sVolumePercent.load(std::memory_order_relaxed));
        }
    }

    FinishPumpTelemetry(bufferedBefore, 0, false);
}

extern "C" void Mk64GameAudio3DSShutdown() {
    sGameStateReady = false;
    StopAudioWorker();
    Mk64Audio3DSShutdown();
    sReady = false;
    sSuspended = false;
    sSynthesisBlockCount = 0;
    sPumpCallCount = 0;
    sMultiBlockPumpCount = 0;
    sQueueFailureCount = 0;
    sObservedEmptyTransitionCount = 0;
    sLoggedFirstSignal = false;
    sAudioPrimed = false;
    sEmptyObservedActive = false;
    sPaused.store(false, std::memory_order_relaxed);
    sAudioWorkerBusy.store(false, std::memory_order_relaxed);
}

extern "C" void Mk64GameAudio3DSAbortForProcessExit() {
    sGameStateReady = false;
    sPaused.store(true, std::memory_order_release);
    sWorkerRunning.store(false, std::memory_order_release);
    if (sWorkerThread != nullptr) {
        LightEvent_Signal(&sAudioWakeEvent);
        if (R_SUCCEEDED(threadJoin(sWorkerThread, 250ULL * 1000ULL * 1000ULL))) {
            threadFree(sWorkerThread);
        } else {
            svcExitProcess();
        }
        sWorkerThread = nullptr;
    }
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
