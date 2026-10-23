#ifdef MK64_3DS_NATIVE_KART_LAB
#include "native_kart_lab_3ds.h"
#endif
#ifdef MK64_3DS_NATIVE_LUIGI_LAB
#include "native_course_lab_3ds.hpp"
extern "C" bool Mk64NativeLuigiLabTick();
#endif
#include "game_data_3ds.h"
#include "improved_texture_pack_3ds.hpp"
#include "improved_resources_3ds.h"
#include "audio_runtime_3ds.h"
#include "auto_mode_3ds.h"
#include "autodump_lab_3ds.h"
#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "game_runtime_3ds.h"
#include "game_state_3ds.h"
#include "input_3ds.h"
#include "performance_trace_3ds.hpp"
#include "diagnostic_export_3ds.hpp"
#include "audio_ndsp_3ds.h"
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"
#include "texture_mode_3ds.hpp"
#include "race_clock_3ds.hpp"
#include "loading_screen_3ds.h"
#include "startup_trace_3ds.h"
#include "heap_policy_3ds.hpp"
#include "updater.h"

#include <3ds.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <malloc.h>

extern "C" {
extern int32_t gMenuSelection;
extern int32_t gGamestate, gRaceState;
extern int16_t gCurrentCourseId;
extern float gCourseTimer;
void initialize_memory_pool(void);
int Mk64MemoryArena3DSIsReady(void);
void audio_init(void);
void osInitialize(void);
void sound_init(void);
void thread5_game_loop(void);
void thread5_iteration(void);
void CVarSetInteger(const char*, int32_t);
}

extern "C" {
// Match the CIA exheader. Four MiB was unnecessarily reserved by 3DSX builds
// and reduced the heap available to the first-run installer on Old 3DS.
uint32_t __stacksize__ = 1 * 1024 * 1024;

// Conservative fallback if the early resource-limit query fails. The wrapped
// libctru heap entrypoint selects 20 MiB for <=80 MiB launches before either
// heap is allocated; expanded New 3DS launches keep this 24 MiB reservation.
uint32_t __ctru_linear_heap_size = 24 * 1024 * 1024;
}

namespace {
constexpr int32_t kLogoIntroMenu = 8;
constexpr uint64_t kSimulationRate = 30;
aptHookCookie sAptHook;
bool sResumePending = false;
std::atomic_flag sMemoryRecoveryActive = ATOMIC_FLAG_INIT;

void MemoryPressureHandler() {
    // Resource decoding already retries its own malloc failures, but wrapper
    // objects (shared_ptr control blocks, strings and cache nodes) allocate
    // after decoding has returned. Give every C++ allocation one process-wide
    // chance to surrender the next optional cache tier, then retry. The
    // resource owner reports exactly which cache was released.
    if (!sMemoryRecoveryActive.test_and_set(std::memory_order_acq_rel)) {
        const bool released = Mk64Resource3DSReclaimPerformanceCache();
        sMemoryRecoveryActive.clear(std::memory_order_release);
        if (released) {
            Mk64Diagnostics3DSCheckpoint(Mk64Resource3DSLastReclaimedCache());
            return;
        }
    }
    throw std::bad_alloc();
}

void AppletTransition(APT_HookType event, void*) {
    if (event == APTHOOK_ONSUSPEND || event == APTHOOK_ONSLEEP) {
        Mk64Startup3DSSetSuspended(true);
        Mk64RaceClock3DSFreeze();
        Mk64Diagnostics3DSSetAptSuspended(true);
        // Release the system-core allocation before any diagnostic logging.
        Mk64GameAudio3DSSuspend();
        Mk64StartupStage("apt-suspend-enter");
        Mk64Diagnostics3DSCheckpoint("apt-suspend-enter");
        Mk64StartupStage("apt-suspend-ready");
        Mk64Diagnostics3DSCheckpoint("apt-suspend-ready");
        sResumePending = true;
    }
}

void CloseServices(void*) {
    // No graphics/resource jobs run after the main loop has stopped. Close
    // optional artwork handles before the updater releases the RomFS mount.
    mk64_3ds::improved::GlobalPack().Close();
    Mk64StartupStage("close-updater");
    Updater_Shutdown();
    Mk64StartupStage("close-audio");
    Mk64Diagnostics3DSCheckpoint("close-audio");
    Mk64GameAudio3DSShutdown();
    Mk64StartupStage("close-gsp");
    Mk64Diagnostics3DSCheckpoint("close-gsp");
    gfxExit();
    Mk64StartupStage("close-diagnostics");
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

bool ApplyArtworkModeChange(Mk64UiTextures3DS& observedMode) {
    const auto requestedMode = Mk64Settings3DSGetUiTextures();
    if (requestedMode == observedMode) return false;
    mk64_3ds::DiagnosticExportPause pause(mk64_3ds::PerformancePause::ResourceLoad);
    // This is between presentations and before the next scene recording.
    // Fence the submitted GPU frame, then release Fast3D's texture/RDP owners
    // before Configure can replace catalog storage or reclaim optional caches.
    // The live logic scene remains owned; this resets graphics state only.
    Mk64Graphics3DSResetForTrackLoad();
    const bool hadCatalog = mk64_3ds::improved::GlobalPack().IsOpen();
    const bool configured = Mk64Improved3DSConfigure(requestedMode);
    if (!configured && hadCatalog) {
        // Catalog publication is transactional. Restore the setting (including
        // the saved value changed by Options) instead of rendering a partial mode.
        Mk64Settings3DSSetUiTextures(observedMode);
        const bool restored = Mk64Improved3DSConfigure(observedMode);
        const bool saved = Mk64Settings3DSSave();
        Mk64Diagnostics3DSCheckpoint(restored ? "improved-mode-change-rejected-catalog-retained"
            : "improved-mode-change-rejected-catalog-restore-failed");
        if (!saved) Mk64Diagnostics3DSCheckpoint("improved-mode-rollback-save-failed");
        return true; // Same observed setting prevents automatic retry every tick.
    }
    observedMode = requestedMode;
    // Missing optional artwork is distinct from failure to expand a valid
    // compact catalog. Keep the existing Original resource fallback in that case.
    const std::size_t budget = mk64_3ds::ArchiveResidencyBudget(
        envGetHeapSize(), static_cast<std::size_t>(mallinfo().uordblks));
    const bool resident = configured && Mk64Improved3DSPrepare(budget, Mk64Graphics3DSResolvedNewModel());
    char state[224];
    std::snprintf(state, sizeof(state),
        "improved-mode-change mode=%u catalog=%s resident=%u metadata=%lu compressed=%lu pages=%lu budget=%lu",
        static_cast<unsigned>(observedMode), configured ? "ready" : "absent-original-fallback", resident ? 1U : 0U,
        static_cast<unsigned long>(Mk64Improved3DSMetadataBytes()), static_cast<unsigned long>(Mk64Improved3DSResidentBytes()),
        static_cast<unsigned long>(Mk64Improved3DSPageBytes()), static_cast<unsigned long>(budget));
    Mk64Diagnostics3DSCheckpoint(state);
    return true;
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
    // _Exit runs libctru's heap teardown hooks. If another library-owned
    // worker is still scheduled, it can execute once more with an unmapped
    // heap-backed stack and replace the useful fatal record with a misleading
    // Arm11 data-abort dump. Terminate all process threads atomically instead.
    svcExitProcess();
    __builtin_unreachable();
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
    Mk64Startup3DSMarkEntry(); // Include diagnostics-before-loader work in the trace.
    std::set_terminate(TerminateHandler);
    std::set_new_handler(MemoryPressureHandler);
    Mk64Diagnostics3DSStart();
    Mk64Startup3DSStart();
    char heapProfile[128];
    std::snprintf(heapProfile, sizeof(heapProfile), "heap-partition ordinary=%lu linear=%lu archive-reserve=%lu",
                  static_cast<unsigned long>(envGetHeapSize()),
                  static_cast<unsigned long>(envGetLinearHeapSize()),
                  static_cast<unsigned long>(mk64_3ds::kArchiveReserveBytes));
    Mk64Diagnostics3DSCheckpoint(heapProfile);
    Mk64StartupStage("loading-screen-init");
    Mk64Diagnostics3DSCheckpoint("loading-screen-init");
    Mk64Loading3DSStart(nullptr);
    Mk64StartupStage("loading-screen-ready");
    Mk64Diagnostics3DSCheckpoint("loading-screen-ready");
    Mk64Settings3DSSetHardwareModel(Mk64Diagnostics3DSIsNewModel());
    Mk64StartupStage("settings-load");
    Mk64Diagnostics3DSCheckpoint("settings-load");
    Mk64Settings3DSLoad();
    Mk64Resource3DSSetImprovedTexturePolicy([](const char* name) {
        return mk64_3ds::ImprovedResourceTexture(Mk64Settings3DSGetUiTextures(), name);
    });
    Mk64StartupStage("settings-ready");
    Mk64Diagnostics3DSCheckpoint("settings-ready");
    Mk64StartupStage("game-data-init");
    Mk64Diagnostics3DSCheckpoint("game-data-init");
    const Mk64GameData3DSResult data = Mk64GameData3DSEnsure();
    if (data.status != MK64_GAME_DATA_READY || data.archivePath == nullptr) {
        Mk64StartupStage("game-data-failed");
        Mk64Diagnostics3DSCheckpoint("game-data-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError(data.message);
    }
    Mk64StartupStage("game-data-ready");
    Mk64Diagnostics3DSCheckpoint("game-data-ready");
    // Updater owns the persistent RomFS mount. Native artwork must not try
    // its one-time pack open before that mount is ready. Extraction is done.
    Mk64StartupStage("updater-init-enter");
    Updater_Init(argc > 0 ? argv[0] : nullptr);
    Mk64StartupStage("updater-init-returned");
    mk64_3ds::improved::GlobalPack().SetReleaseCallback(Mk64Graphics3DSEvictSourceTexture);
    // Compact inactive aliases before any UI/archive allocation. Failure keeps
    // the prior valid catalog and existing Original artwork fallback available.
    const bool artworkConfigured = Mk64Improved3DSConfigure(Mk64Settings3DSGetUiTextures());
    Mk64Diagnostics3DSCheckpoint(artworkConfigured ? "improved-catalog-configured"
        : "improved-catalog-configure-failed-fallback-retained");
    {
        const auto hintProbe = mk64_3ds::improved::LocalPack().Load("@hint_random",192,29);
        Mk64Diagnostics3DSCheckpoint(hintProbe ? "improved-texture-pack-ready"
                                              : "improved-texture-pack-unavailable");
    }
    Mk64Loading3DSStart(data.archivePath);

    // First-run extraction needs the regular heap for Torch's ROM buffer and
    // per-file YAML data. Reserve the vanilla arena only after mk64.o2r is
    // ready, but still before the resource index and Citro3D allocate memory.
    initialize_memory_pool();
    if (!Mk64MemoryArena3DSIsReady()) {
        Mk64StartupStage("game-arena-init-failed");
        Mk64Diagnostics3DSCheckpoint("game-arena-init-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError("Not enough application memory for the 8 MiB game arena.");
    }
    Mk64StartupStage("game-arena-ready");
    Mk64Diagnostics3DSCheckpoint("game-arena-ready");

    Mk64StartupStage("resource-runtime-init");
    Mk64Diagnostics3DSCheckpoint("resource-runtime-init");
    if (!Mk64Resource3DSInit(data.archivePath)) {
        Mk64StartupStage("resource-runtime-init-failed");
        Mk64Diagnostics3DSCheckpoint("resource-runtime-init-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError("mk64.o2r could not be opened or is not a supported archive.");
    }
    Mk64StartupStage("resource-runtime-ready");
    Mk64Diagnostics3DSCheckpoint("resource-runtime-ready");
    Mk64Loading3DSPauseDisplay();
    Mk64StartupStage("graphics-init");
    Mk64Diagnostics3DSCheckpoint("graphics-init");
    if (!Mk64Graphics3DSInit()) {
        Mk64StartupStage("graphics-init-failed");
        Mk64Diagnostics3DSCheckpoint("graphics-init-failed");
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The native Citro3D renderer could not be initialized.");
    }
    Mk64StartupStage("graphics-ready");
    Mk64Diagnostics3DSCheckpoint("graphics-ready");
    Mk64Loading3DSResumeDisplay();

    Mk64StartupStage("libultra-init");
    Mk64Diagnostics3DSCheckpoint("libultra-init");
    osInitialize();
    Mk64Input3DSInit();
    Mk64StartupStage("game-state-init");
    Mk64Diagnostics3DSCheckpoint("game-state-init");
    if (!Mk64GameState3DSInit()) {
        Mk64StartupStage("game-state-init-failed");
        Mk64Diagnostics3DSCheckpoint("game-state-init-failed");
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The vanilla game state could not be initialized.");
    }
    Mk64StartupStage("game-state-ready");
    Mk64Diagnostics3DSCheckpoint("game-state-ready");
    // Use the existing hold-to-look-behind enhancement for the dedicated X.
    CVarSetInteger("gLookBehind", 1);

    Mk64StartupStage("audio-init");
    Mk64Diagnostics3DSCheckpoint("audio-init");
    audio_init();
    sound_init();
    if (Mk64GameAudio3DSInit()) {
        Mk64StartupStage("audio-ready");
        Mk64Diagnostics3DSCheckpoint("audio-ready");
    } else {
        Mk64StartupStage("audio-init-failed");
        Mk64Diagnostics3DSCheckpoint("audio-init-failed");
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("DSP audio could not start. Dump DSP firmware with a current homebrew setup, then try again.");
    }

    Mk64StartupStage("bottom-ui-init");
    Mk64Diagnostics3DSCheckpoint("bottom-ui-init");
    if (!Mk64BottomUI3DSInit()) {
        Mk64StartupStage("bottom-ui-init-failed");
        Mk64Diagnostics3DSCheckpoint("bottom-ui-init-failed");
        Mk64GameAudio3DSShutdown();
        Mk64Loading3DSStop();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The bottom-screen interface could not be initialized.");
    }
    Mk64StartupStage("bottom-ui-ready");
    Mk64Diagnostics3DSCheckpoint("bottom-ui-ready");

    // Allocate the recorder after GPU/UI initialization and before archive
    // admission. Full-capacity ordinary fallback is charged to that budget.
    mk64_3ds::PerformanceInitialize();
    char recorder[192];
    std::snprintf(recorder, sizeof(recorder), "performance-storage pool=%s payload=%lu reserved=%lu linear-free=%lu linear-reserve=%lu",
        mk64_3ds::PerformanceStorageKind(), static_cast<unsigned long>(mk64_3ds::PerformanceStorageBytes()),
        static_cast<unsigned long>(mk64_3ds::PerformanceStorageReservedBytes()),
        static_cast<unsigned long>(linearSpaceFree()), static_cast<unsigned long>(mk64_3ds::kPerformanceLinearReserve));
    Mk64Diagnostics3DSCheckpoint(recorder);

    // Retain the compressed archive, not every decoded/GPU texture. CIA
    // metadata requests expanded memory on both Old and New models. Reserve
    // headroom for later course objects and transient decompression buffers.
    const auto used = static_cast<size_t>(mallinfo().uordblks);
    const size_t heapSize = envGetHeapSize();
    constexpr size_t reserve = mk64_3ds::kArchiveReserveBytes;
    size_t budget = mk64_3ds::ArchiveResidencyBudget(heapSize, used);
#ifdef MK64_3DS_NATIVE_KART_LAB
    // The ordinary heap cannot retain both complete archives on Old. Native
    // animation pages replace its largest measured render-time I/O source;
    // reserve their loading budget instead of filling it with the legacy ZIP.
    if(!Mk64Graphics3DSResolvedNewModel())budget=0;
#endif
    Mk64Diagnostics3DSMemory("before-archive-residency", Mk64Resource3DSLoadedCount(), 0, 0, 0, 0, 0);
    Mk64StartupStage("archive-ram-loading");
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
    Mk64StartupStage("archive-residency-complete");

    // This is additional allowance after the O2R commit, with the same 8 MiB
    // reserve. Do not reuse the earlier, pre-residency budget for artwork.
    const std::size_t artworkBudget = mk64_3ds::ArchiveResidencyBudget(
        envGetHeapSize(), static_cast<std::size_t>(mallinfo().uordblks));
    const bool artworkResident = Mk64Improved3DSPrepare(artworkBudget, Mk64Graphics3DSResolvedNewModel());
    char artworkMemory[192];
    std::snprintf(artworkMemory, sizeof(artworkMemory),
        "improved-memory resident=%u metadata=%lu compressed=%lu pages=%lu budget=%lu",
        artworkResident ? 1U : 0U, static_cast<unsigned long>(Mk64Improved3DSMetadataBytes()),
        static_cast<unsigned long>(Mk64Improved3DSResidentBytes()), static_cast<unsigned long>(Mk64Improved3DSPageBytes()),
        static_cast<unsigned long>(artworkBudget));
    Mk64Diagnostics3DSCheckpoint(artworkMemory);

    Mk64Loading3DSStop();

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    Mk64StartupStage("vanilla-loop-init");
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-init");
    thread5_game_loop();
    // Its rendering_init() submits a blank frame before func_800C5CB8().
    // That frame must not start synthesis against half-initialized game state.
    Mk64GameAudio3DSFinishInitialization();
#ifdef MK64_3DS_AUTODUMP_LAB
    Mk64Lab3DSBoot();
#endif
#ifdef MK64_3DS_NATIVE_LUIGI_LAB
    Mk64NativeLuigiLabBoot();
#endif
    Mk64StartupStage("vanilla-loop-ready");
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-ready");

    aptHook(&sAptHook, AppletTransition, nullptr);
    uint64_t nextSimulationDeadline = svcGetSystemTick();
    uint64_t deadlineRemainder = 0;
    bool suppressNextPresentation = false;
    bool startupTraceSampled = false;
    auto observedTextureMode = Mk64Settings3DSGetUiTextures();
    std::uint32_t courseGeneration = 0;
    bool previousRacing = false;
    std::int16_t previousCourse = -1;
    std::int32_t previousRaceState = -1;
    while (WindowIsRunning() && !Updater_ShouldClose()) {
#if defined(MK64_3DS_NATIVE_LUIGI_LAB) && !defined(MK64_3DS_NATIVE_FULL_GAME)
        if(!Mk64NativeLuigiLabTick())break;
#endif
        // Coarse coverage begins before barriers, APT and laboratory work.
        // Intentional export/suspend/terminal iterations are abandoned below.
        mk64_3ds::PerformanceBegin();
        auto& perf = mk64_3ds::PerformanceCurrent();
        // Also protect the first tick and exceptional/early renderer returns.
        // APT callbacks and all UI/game logic must own the audio globals.
        { mk64_3ds::PerformanceTimer timer(perf.logic_barrier_us); Mk64GameAudio3DSBeginLogic(); }
        Mk64StartupStage("apt-poll-enter");
        { mk64_3ds::PerformanceTimer timer(perf.events_us); Mk64Graphics3DSPollEvents(); }
        Mk64StartupStage("apt-poll-returned");
        if (!WindowIsRunning()) break;
        if (sResumePending) {
            Mk64Startup3DSSetSuspended(false);
            // aptMainLoop has now finished restoring DSP and GPU ownership.
            sResumePending = false;
            Mk64GameAudio3DSResume();
            Mk64Diagnostics3DSSetAptSuspended(false);
            Mk64Graphics3DSResumeAfterDiagnosticPause();
            Mk64BottomUI3DSResetFps();
            mk64_3ds::PerformanceResume(mk64_3ds::PerformancePause::Suspend, perf.start_us);
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            Mk64StartupStage("apt-resume-ready");
            Mk64Diagnostics3DSCheckpoint("apt-resume-ready");
            continue;
        }
        if (Mk64Diagnostics3DSServiceDumpIfRequested()) {
            Mk64RaceClock3DSFreeze();
            Mk64Graphics3DSResumeAfterDiagnosticPause();
            Mk64BottomUI3DSResetFps();
            mk64_3ds::PerformanceResume(mk64_3ds::PerformancePause::Diagnostic, perf.start_us);
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
        if (Mk64Diagnostics3DSIsPaused()) {
            mk64_3ds::PerformanceAbandon();
            Mk64RaceClock3DSFreeze();
            Mk64Diagnostics3DSSetStage("diagnostic-dump-paused");
            svcSleepThread(16000000LL);
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
#ifdef MK64_3DS_AUTODUMP_LAB
        int labAction;
        { mk64_3ds::PerformanceTimer timer(perf.controller_us); labAction = Mk64Lab3DSTick(); }
        if (labAction != 0) {
            Mk64RaceClock3DSFreeze();
            mk64_3ds::PerformanceAbandon();
            if (labAction != MK64_LAB_EXPORTED) Mk64GameAudio3DSSetPaused(true);
            nextSimulationDeadline = svcGetSystemTick(); deadlineRemainder = 0;
            suppressNextPresentation = false;
            if (labAction == MK64_LAB_EXIT) break;
            continue;
        }
#endif
        if (ApplyArtworkModeChange(observedTextureMode)) {
            nextSimulationDeadline = svcGetSystemTick(); deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
        Mk64StartupStage("first-iteration-enter");
        Mk64RaceClock3DSObserve();
        Mk64Diagnostics3DSSetStage("game-loop-iteration");
        Mk64StartupStage("bottom-ui-prepare-enter");
        { mk64_3ds::PerformanceTimer timer(perf.prepare_us); Mk64BottomUI3DSPrepareFrame(); }
        Mk64StartupStage("bottom-ui-prepare-returned");
        // Options can change mode inside PrepareFrame. Apply that transaction
        // before game/renderer code can observe the new policy with old metadata.
        if (ApplyArtworkModeChange(observedTextureMode)) {
            nextSimulationDeadline = svcGetSystemTick(); deadlineRemainder = 0;
            suppressNextPresentation = false;
            continue;
        }
        perf.game_state = static_cast<uint32_t>(gGamestate);
        const bool racing = gGamestate == 4;
        if (racing && (!previousRacing || previousCourse != gCurrentCourseId ||
            (previousRaceState >= 3 && gRaceState < 3))) ++courseGeneration;
        previousRacing = racing; previousCourse = gCurrentCourseId;
        previousRaceState = gRaceState;
        mk64_3ds::PerformanceCourse(courseGeneration, static_cast<std::uint16_t>(gCurrentCourseId));
        mk64_3ds::PerformanceRaceState(gGamestate == 4, static_cast<uint32_t>(gRaceState),
            gCourseTimer > 0.0f ? static_cast<uint32_t>(gCourseTimer * 1000.0f) : 0);
        Mk64Auto3DSBenchmarkTick();
        perf.scale = Mk64Settings3DSGetRenderScalePercent();
        perf.distance = Mk64Settings3DSGetRenderDistance();
        perf.layout = Mk64Settings3DSGetHudLayout();
        perf.filter = Mk64Settings3DSGetDisplayFilter();
        perf.profile = Mk64Graphics3DSResolvedNewModel();
        perf.width = Mk64Graphics3DSResolvedOutputWidth();
        Mk64Graphics3DSSuppressNextPresentation(suppressNextPresentation);
        suppressNextPresentation = false;
        { mk64_3ds::PerformanceTimer timer(perf.iteration_us); thread5_iteration(); }
        mk64_3ds::PerformanceStateAfter(static_cast<std::uint32_t>(gGamestate), static_cast<std::uint32_t>(gRaceState),
            gCourseTimer > 0 ? static_cast<std::uint32_t>(gCourseTimer * 1000.0f) : 0);
        Mk64Startup3DSGameState(gGamestate, gMenuSelection);
        Mk64StartupStage("first-iteration-returned");
        // Do not enter the audio worker or pacer if game logic closes the window.
        if (!WindowIsRunning()) break;
        Mk64Diagnostics3DSSetStage("game-loop-audio");
        Mk64StartupStage("audio-pump-enter");
        { mk64_3ds::PerformanceTimer timer(perf.audio_pump_us); Mk64GameAudio3DSPump(); }
        Mk64StartupStage("audio-pump-returned");
        if (!startupTraceSampled && gGamestate != 7 && Mk64Startup3DSHasSubmittedFrames()) {
            startupTraceSampled = true;
            Mk64StartupStage("startup-trace-switch-to-sampling");
            Mk64Diagnostics3DSCheckpoint("startup-trace-switch-to-sampling");
            Mk64Diagnostics3DSBufferRuntimeLog();
            // Keep evidence through the logo/menu without per-stage SD waits.
            // Joining this optional worker here could terminate a healthy game
            // when filesystem IPC takes longer than the shutdown deadline.
            Mk64Startup3DSAsync();
            nextSimulationDeadline = svcGetSystemTick();
            deadlineRemainder = 0;
        }
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
    mk64_3ds::PerformanceAbandon();

    // WindowIsRunning becomes false after aptMainLoop reports the HOME-menu
    // close request. Settings are persisted on every change. Join the audio
    // and diagnostics workers while NDSP/HID and their stacks are still mapped,
    // then keep the immediate exit that avoids GPU/resource teardown after
    // Citro3D has disabled its VBlank callbacks during the APT transition.
    aptUnhook(&sAptHook);
    CloseProcess();
}

extern "C" void userAppExit() {
    Mk64Startup3DSStop();
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
