#include "audio_runtime_3ds.h"
#include "game_runtime_3ds.h"

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
// Refill begins below a roughly 67 ms target. A final 896-frame block may
// transiently overshoot that threshold; the six-wave pool provides capacity
// without making a third synthesis block part of the normal policy.
constexpr uint32_t kTargetBufferedFrames = kStereoFramesPerGameFrame * 2;
constexpr uint32_t kMaxSynthesisBlocksPerPump = 2;
constexpr size_t kAudioWorkerStackSize = 64u * 1024u;
constexpr int32_t kAudioWorkerPriority = 0x18;

bool sReady = false;
uint32_t sSynthesisBlockCount = 0;
uint32_t sPumpCallCount = 0;
uint32_t sMultiBlockPumpCount = 0;
uint32_t sQueueFailureCount = 0;
uint32_t sObservedEmptyTransitionCount = 0;
bool sLoggedFirstSignal = false;
bool sAudioPrimed = false;
bool sEmptyObservedActive = false;
Thread sWorkerThread = nullptr;
LightEvent sWorkerStart;
LightEvent sWorkerDone;
std::atomic<bool> sWorkerRunning{ false };
std::atomic<bool> sJobOutstanding{ false };
std::atomic<bool> sLastJobQueued{ false };
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
        LightEvent_Wait(&sWorkerStart);
        if (!sWorkerRunning.load(std::memory_order_acquire)) break;

        // Game logic has already submitted its audio commands. Only this
        // worker consumes them, and the main thread waits for completion
        // before beginning the next logic tick, so libultraship's emulated
        // N64 message queues are never accessed concurrently across ticks.
        sLastJobQueued.store(
            SynthesizeAndQueue(sVolumePercent.load(std::memory_order_relaxed)),
            std::memory_order_release);
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
    const bool isNewModel = Mk64Graphics3DSResolvedPerformanceProfile() ==
                            MK64_PERFORMANCE_PROFILE_NEW_3DS;
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
    sLastJobQueued.store(false, std::memory_order_relaxed);
    LightEvent_Signal(&sWorkerStart);
    return true;
}

bool WaitForWorkerJob() {
    if (!sJobOutstanding.load(std::memory_order_acquire)) return true;
    LightEvent_Wait(&sWorkerDone);
    sJobOutstanding.store(false, std::memory_order_release);
    return sLastJobQueued.load(std::memory_order_acquire);
}

}

extern "C" bool Mk64GameAudio3DSInit() {
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
        sJobOutstanding.store(false, std::memory_order_relaxed);
        sLastJobQueued.store(false, std::memory_order_relaxed);
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

    ++sPumpCallCount;
    const uint32_t bufferedBefore = Mk64Audio3DSBufferedFrames();
    if (sAudioPrimed && bufferedBefore == 0 && !sEmptyObservedActive) {
        // This is a Pump-time observation, not proof that NDSP remained
        // empty long enough to produce an audible hardware underrun.
        ++sObservedEmptyTransitionCount;
        sEmptyObservedActive = true;
    } else if (bufferedBefore != 0) {
        sEmptyObservedActive = false;
    }

    const bool jobWasStartedDuringRender = sJobOutstanding.load(std::memory_order_acquire);
    const bool renderJobQueued = !jobWasStartedDuringRender || WaitForWorkerJob();
    uint32_t synthesizedThisPump = jobWasStartedDuringRender && renderJobQueued ? 1u : 0u;
    if (sPaused.load(std::memory_order_acquire)) {
        FinishPumpTelemetry(bufferedBefore, synthesizedThisPump, false);
        return;
    }
    if (!renderJobQueued) {
        ++sQueueFailureCount;
        FinishPumpTelemetry(bufferedBefore, synthesizedThisPump, true);
        return;
    }

    // Refill to a bounded high-water mark, not just one block per visual
    // iteration. A long course-start frame can consume more than 33 ms of
    // queued sound; producing the missing blocks here keeps NDSP real-time
    // while all audio-global access remains complete before the next game
    // logic tick. At steady 30 Hz this loop still produces exactly one block.
    bool queueFailedThisPump = false;
    while (NeedsSynthesis() && synthesizedThisPump < kMaxSynthesisBlocksPerPump) {
        bool queued = false;
        if (ScheduleWorkerJob()) {
            queued = WaitForWorkerJob();
        } else {
            // Old 3DS systems that cannot reserve core 1 retain the safe
            // single-threaded path rather than losing audio entirely.
            queued = SynthesizeAndQueue(Mk64Settings3DSGetMasterVolumePercent());
        }
        // Synthesis advances the game's audio timeline. If NDSP has no
        // reusable wave, stop immediately so one pressured frame can never
        // advance and discard several consecutive music/SFX blocks.
        if (!queued) {
            ++sQueueFailureCount;
            queueFailedThisPump = true;
            break;
        }
        ++synthesizedThisPump;
    }
    FinishPumpTelemetry(bufferedBefore, synthesizedThisPump, queueFailedThisPump);
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
    sSynthesisBlockCount = 0;
    sPumpCallCount = 0;
    sMultiBlockPumpCount = 0;
    sQueueFailureCount = 0;
    sObservedEmptyTransitionCount = 0;
    sLoggedFirstSignal = false;
    sAudioPrimed = false;
    sEmptyObservedActive = false;
    sPaused.store(false, std::memory_order_relaxed);
    sJobOutstanding.store(false, std::memory_order_relaxed);
    sLastJobQueued.store(false, std::memory_order_relaxed);
}

extern "C" void Mk64GameAudio3DSAbortForProcessExit() {
    // aptMainLoop() becoming false means the OS is terminating this process.
    // During that transition NDSP/service IPC and an outstanding worker can
    // stop replying, so the normal unbounded teardown is unsafe. Stop issuing
    // new work and wake the worker, then let _Exit release process-owned
    // services and memory without waiting on either endpoint.
    sPaused.store(true, std::memory_order_release);
    sWorkerRunning.store(false, std::memory_order_release);
    if (sWorkerThread != nullptr) {
        LightEvent_Signal(&sWorkerStart);
        if (R_SUCCEEDED(threadJoin(sWorkerThread, 250ULL * 1000ULL * 1000ULL))) {
            threadFree(sWorkerThread);
        } else {
            threadDetach(sWorkerThread);
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
