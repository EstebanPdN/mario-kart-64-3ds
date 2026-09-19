#include "game_data_3ds.h"
#include "audio_runtime_3ds.h"
#include "bottom_ui_3ds.h"
#include "updater.h"
#include "diagnostics_3ds.h"
#include "game_runtime_3ds.h"
#include "game_state_3ds.h"
#include "input_3ds.h"
#include "performance_trace_3ds.hpp"
#include "audio_ndsp_3ds.h"
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"
#include "loading_screen_3ds.h"

#include <3ds.h>

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <malloc.h>

extern "C" {
extern int32_t gMenuSelection;
extern int32_t gGamestate, gRaceState;
extern float gCourseTimer;
void initialize_memory_pool(void);
int Mk64MemoryArena3DSIsReady(void);
void audio_init(void);
void osInitialize(void);
void sound_init(void);
void thread5_game_loop(void);
void thread5_iteration(void);
}

extern "C" {
// Match the CIA exheader. Four MiB was unnecessarily reserved by 3DSX builds
// and reduced the heap available to the first-run installer on Old 3DS.
uint32_t __stacksize__ = 1 * 1024 * 1024;

// E4 hardware evidence captured std::bad_alloc with roughly 14.5 MiB still
// free in the linear heap but only 1.8 MiB, badly fragmented, in the ordinary
// C/C++ heap. The 24 MiB reservation still leaves several MiB above the E4
// linear working set, including the larger New 3DS texture-cache profile,
// while returning another 4 MiB to strings, maps and other ordinary objects.
uint32_t __ctru_linear_heap_size = 24 * 1024 * 1024;
}

namespace {
constexpr int32_t kLogoIntroMenu = 8;
constexpr uint64_t kSimulationRate = 30;
aptHookCookie sAptHook;
bool sResumePending = false;
void AppletTransition(APT_HookType event, void*) {
    if (event == APTHOOK_ONSUSPEND || event == APTHOOK_ONSLEEP) {
        Mk64Diagnostics3DSCheckpoint("apt-suspend-enter");
        Mk64Diagnostics3DSSetAptSuspended(true);
        Mk64GameAudio3DSSuspend();
        Mk64Diagnostics3DSCheckpoint("apt-suspend-ready");
        sResumePending = true;
    }
}

void CloseServices(void*) {
    Mk64Diagnostics3DSCheckpoint("close-updater");
    Updater_Shutdown();
    Mk64Diagnostics3DSCheckpoint("close-audio");
    Mk64GameAudio3DSShutdown();
    Mk64Diagnostics3DSCheckpoint("close-gsp");
    gfxExit();
    Mk64Diagnostics3DSCheckpoint("close-diagnostics");
    Mk64Diagnostics3DSStop();
}

[[noreturn]] void CloseProcess() {
    // Bound all potentially blocking SDK joins and service IPC as one unit.
    // On timeout, the kernel destroys all threads without unmapping live stacks.
    Thread closer = threadCreate(CloseServices, nullptr, 64u * 1024u, 0x30, -2, false);
    if (closer == nullptr || R_FAILED(threadJoin(closer, 3000000000ULL))) svcExitProcess();
    threadFree(closer);
    std::_Exit(0);
}

void ArchiveLoadProgress(unsigned percent) {
    if (Mk64Loading3DSActive()) return;
    static unsigned previous = 101;
    if (percent == previous) return;
    previous = percent;
    Mk64BottomUI3DSShowLoadingProgress("LOADING GAME", "LOADING RESOURCES", percent);
}

[[noreturn]] void TerminateHandler() noexcept {
    const char* reason = "std::terminate without an active exception";
    if (std::current_exception() != nullptr) {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::bad_alloc&) {
            reason = "uncaught std::bad_alloc";
        } catch (const std::exception& exception) {
            reason = exception.what();
        } catch (...) {
            reason = "uncaught non-standard C++ exception";
        }
    }
    Mk64Loading3DSStop();
    Mk64Diagnostics3DSEmergency(reason);
    std::_Exit(1);
}

[[noreturn]] void ExitWithError(const char* message) {
    Mk64Loading3DSStop();
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    std::printf("Mario Kart 64 3DS\n\n%s\n\nPress START to exit.\n", message);
    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) break;
        gspWaitForVBlank();
    }
    gfxExit();
    std::_Exit(1);
}
}

int main(int argc, char** argv) {
    std::set_terminate(TerminateHandler);
    Mk64Diagnostics3DSStart();
    Mk64Diagnostics3DSCheckpoint("loading-screen-init");
    Mk64Loading3DSStart(nullptr);
    Mk64Diagnostics3DSCheckpoint("loading-screen-ready");
    Mk64Settings3DSSetHardwareModel(Mk64Diagnostics3DSIsNewModel());
    Mk64Diagnostics3DSCheckpoint("settings-load");
    Mk64Settings3DSLoad();
    Mk64Diagnostics3DSCheckpoint("settings-ready");
    Mk64Diagnostics3DSCheckpoint("game-data-init");
    const Mk64GameData3DSResult data = Mk64GameData3DSEnsure();
    if (data.status != MK64_GAME_DATA_READY || data.archivePath == nullptr) {
        Mk64Diagnostics3DSCheckpoint("game-data-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError(data.message);
    }
    Mk64Diagnostics3DSCheckpoint("game-data-ready");
    Mk64Loading3DSStart(data.archivePath);

    // First-run extraction needs the regular heap for Torch's ROM buffer and
    // per-file YAML data. Reserve the vanilla arena only after mk64.o2r is
    // ready, but still before the resource index and Citro3D allocate memory.
    initialize_memory_pool();
    if (!Mk64MemoryArena3DSIsReady()) {
        Mk64Diagnostics3DSCheckpoint("game-arena-init-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError("Not enough application memory for the 8 MiB game arena.");
    }
    Mk64Diagnostics3DSCheckpoint("game-arena-ready");

    Mk64Diagnostics3DSCheckpoint("resource-runtime-init");
    if (!Mk64Resource3DSInit(data.archivePath)) {
        Mk64Diagnostics3DSCheckpoint("resource-runtime-init-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError("mk64.o2r could not be opened or is not a supported archive.");
    }
    Mk64Diagnostics3DSCheckpoint("resource-runtime-ready");
    Mk64Loading3DSPauseDisplay();
    Mk64Diagnostics3DSCheckpoint("graphics-init");
    if (!Mk64Graphics3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("graphics-init-failed");
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The native Citro3D renderer could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("graphics-ready");
    Mk64Loading3DSResumeDisplay();

    Mk64Diagnostics3DSCheckpoint("libultra-init");
    osInitialize();
    Mk64Input3DSInit();
    Mk64Diagnostics3DSCheckpoint("game-state-init");
    if (!Mk64GameState3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("game-state-init-failed");
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The vanilla game state could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("game-state-ready");

    Mk64Diagnostics3DSCheckpoint("audio-init");
    audio_init();
    sound_init();
    if (Mk64GameAudio3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("audio-ready");
    } else {
        Mk64Diagnostics3DSCheckpoint("audio-init-failed");
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("DSP audio could not start. Dump DSP firmware with a current homebrew setup, then try again.");
    }

    Mk64Diagnostics3DSCheckpoint("bottom-ui-init");
    if (!Mk64BottomUI3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("bottom-ui-init-failed");
        Mk64GameAudio3DSShutdown();
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The bottom-screen interface could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("bottom-ui-ready");

    // Retain the compressed archive, not every decoded/GPU texture. CIA
    // metadata requests expanded memory on both Old and New models. Reserve
    // headroom for later course objects and transient decompression buffers.
    const auto used = static_cast<size_t>(mallinfo().uordblks);
    const size_t heapSize = envGetHeapSize();
    constexpr size_t reserve = 8u * 1024u * 1024u;
    const size_t budget = heapSize > used && heapSize - used > reserve
        ? heapSize - used - reserve : 0;
    Mk64Diagnostics3DSMemory("before-archive-residency", Mk64Resource3DSLoadedCount(), 0, 0, 0, 0, 0);
    Mk64Diagnostics3DSCheckpoint("archive-ram-loading");
    const bool resident = Mk64Resource3DSMakeResident(budget, ArchiveLoadProgress);
    char residency[192];
    std::snprintf(residency, sizeof(residency),
        "archive-residency %s heap=%lu used=%lu reserve=%lu budget=%lu required=%lu reads=%llu",
        Mk64Resource3DSResidencyStatus(), static_cast<unsigned long>(heapSize),
        static_cast<unsigned long>(used), static_cast<unsigned long>(reserve),
        static_cast<unsigned long>(budget), static_cast<unsigned long>(Mk64Resource3DSResidentRequiredBytes()),
        static_cast<unsigned long long>(Mk64Resource3DSPhysicalReadCalls()));
    Mk64Diagnostics3DSCheckpoint(residency);
    if (!resident && !Mk64Resource3DSResidencyMemoryLimited()) {
        Mk64Diagnostics3DSFailure("archive-ram-loading", Mk64Resource3DSResidencyStatus());
        Mk64Loading3DSStop();
        Mk64BottomUI3DSShutdown();
        Mk64GameAudio3DSShutdown();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("Could not read mk64.o2r.\nCheck the archive and SD card.\nDetails: sd:/3ds/MK64/dump/runtime.log");
    }
    // RAM residency is an optimization, not a requirement to open the game.
    // Preserve the validated reader on unusually constrained launches; partial
    // resident allocations have already been discarded transactionally.
    Mk64Diagnostics3DSCheckpoint(resident ? "archive-ram-ready-sd-closed" : "archive-streaming-memory-fallback");
    Mk64Diagnostics3DSMemory("after-archive-residency", Mk64Resource3DSLoadedCount(), 0, 0, 0, 0, 0);
    Mk64Diagnostics3DSBufferRuntimeLog();

    Mk64Loading3DSStop();

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-init");
    thread5_game_loop();
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-ready");

    Updater_Init(argc > 0 ? argv[0] : nullptr);
    aptHook(&sAptHook, AppletTransition, nullptr);
    uint64_t nextSimulationDeadline = svcGetSystemTick();
    uint64_t deadlineRemainder = 0;
    bool suppressNextPresentation = false;
    while (WindowIsRunning() && !Updater_ShouldClose()) {
        Mk64Graphics3DSPollEvents();
        if (!WindowIsRunning()) break;
        if (sResumePending) {
            // aptMainLoop has now finished restoring DSP and GPU ownership.
            sResumePending = false;
            Mk64GameAudio3DSResume();
            Mk64Diagnostics3DSSetAptSuspended(false);
            Mk64Graphics3DSResumeAfterDiagnosticPause();
            Mk64BottomUI3DSResetFps();
            mk64_3ds::PerformanceResume();
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            Mk64Diagnostics3DSCheckpoint("apt-resume-ready");
        }
        if (Mk64Diagnostics3DSServiceDumpIfRequested()) {
            Mk64Graphics3DSResumeAfterDiagnosticPause();
            Mk64BottomUI3DSResetFps();
            mk64_3ds::PerformanceResume();
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
        if (Mk64Diagnostics3DSIsPaused()) {
            Mk64Diagnostics3DSSetStage("diagnostic-dump-paused");
            svcSleepThread(16000000LL);
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
        mk64_3ds::PerformanceBegin();
        Mk64Diagnostics3DSSetStage("game-loop-iteration");
        auto& perf = mk64_3ds::PerformanceCurrent();
        { mk64_3ds::PerformanceTimer timer(perf.prepare_us); Mk64BottomUI3DSPrepareFrame(); }
        perf.game_state = static_cast<uint32_t>(gGamestate);
        mk64_3ds::PerformanceRaceState(gGamestate == 4, static_cast<uint32_t>(gRaceState),
            gCourseTimer > 0.0f ? static_cast<uint32_t>(gCourseTimer * 1000.0f) : 0);
        perf.scale = Mk64Settings3DSGetRenderScalePercent();
        perf.distance = Mk64Settings3DSGetRenderDistance();
        perf.layout = Mk64Settings3DSGetHudLayout();
        perf.filter = Mk64Settings3DSGetDisplayFilter();
        perf.profile = Mk64Graphics3DSResolvedNewModel();
        perf.width = Mk64Graphics3DSResolvedOutputWidth();
        Mk64Graphics3DSSuppressNextPresentation(suppressNextPresentation);
        suppressNextPresentation = false;
        { mk64_3ds::PerformanceTimer timer(perf.iteration_us); thread5_iteration(); }
        // Do not enter the audio worker or pacer if game logic closes the window.
        if (!WindowIsRunning()) break;
        Mk64Diagnostics3DSSetStage("game-loop-audio");
        { mk64_3ds::PerformanceTimer timer(perf.audio_pump_us); Mk64GameAudio3DSPump(); }
        perf.audio_after = Mk64Audio3DSBufferedFrames();

        // Keep the original 30 Hz simulation clock exact. If rendering falls
        // behind, the following tick may omit only its presentation so logic,
        // input and audio can recover instead of making the whole game run in
        // slow motion. Long loading/diagnostic stalls reset the clock rather
        // than replaying seconds of stale input.
        nextSimulationDeadline += SYSCLOCK_ARM11 / kSimulationRate;
        deadlineRemainder += SYSCLOCK_ARM11 % kSimulationRate;
        if (deadlineRemainder >= kSimulationRate) {
            ++nextSimulationDeadline;
            deadlineRemainder -= kSimulationRate;
        }
        const uint64_t now = svcGetSystemTick();
        if (now < nextSimulationDeadline) {
            const uint64_t remainingTicks = nextSimulationDeadline - now;
            const int64_t remainingNanoseconds = static_cast<int64_t>(
                remainingTicks * 1000000000ULL / SYSCLOCK_ARM11);
            if (remainingNanoseconds > 0) {
                mk64_3ds::PerformanceTimer timer(perf.sleep_us);
                svcSleepThread(remainingNanoseconds);
            }
        } else {
            const uint64_t lateness = now - nextSimulationDeadline;
            perf.lateness_us = static_cast<uint32_t>(std::min<uint64_t>(lateness * 1000000ULL / SYSCLOCK_ARM11, UINT32_MAX));
            const uint64_t tickTicks = SYSCLOCK_ARM11 / kSimulationRate;
            if (lateness > tickTicks * 3U) {
                perf.pacer_reset = 1;
                nextSimulationDeadline = now;
                deadlineRemainder = 0;
            } else if (lateness >= tickTicks / 2U) {
                suppressNextPresentation = true;
            }
        }
        mk64_3ds::PerformanceEnd();
    }

    // WindowIsRunning becomes false after aptMainLoop reports the HOME-menu
    // close request. Settings are persisted on every change. Join the audio
    // and diagnostics workers while NDSP/HID and their stacks are still mapped,
    // then keep the immediate exit that avoids GPU/resource teardown after
    // Citro3D has disabled its VBlank callbacks during the APT transition.
    aptUnhook(&sAptHook);
    CloseProcess();
}

extern "C" void userAppExit() {
    Mk64Loading3DSStop();
    // libctru invokes this hook before hidExit() unmaps HID shared memory.
    // Quiesce the audio worker before waiting on the diagnostics HID poller so
    // it cannot keep using services while process teardown is in progress.
    Mk64GameAudio3DSAbortForProcessExit();
    Mk64Diagnostics3DSAbortForProcessExit();
    // _Exit still invokes libctru's heap unmapping. Its default __appExit
    // does not stop GSP: the event thread's heap-backed stack would disappear
    // while it is running (hardware dump 114). gfxExit joins that thread
    // before the heap is released, and skips its VBlank wait after APT has
    // returned GPU ownership to HOME. Do not run the renderer destructors here.
    gfxExit();
}
