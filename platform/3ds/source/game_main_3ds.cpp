#include "game_data_3ds.h"
#include "audio_runtime_3ds.h"
#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "game_runtime_3ds.h"
#include "game_state_3ds.h"
#include "input_3ds.h"
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"

#include <3ds.h>

#include <array>
#include <cstdio>
#include <cstdlib>

extern "C" {
extern int32_t gMenuSelection;
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

// libctru otherwise reserves 32 MiB for the linear heap and only 24 MiB for
// ordinary allocations in the 64 MiB application region. Hardware dumps have
// reached roughly 17.3 MiB of linear use as resource caches fill, while both
// Torch and the game approach the ordinary-heap ceiling. A conservative 28 MiB
// linear reservation keeps more than 10 MiB of observed headroom and returns
// 4 MiB to the fragmentation-prone C/C++ heap.
uint32_t __ctru_linear_heap_size = 28 * 1024 * 1024;
}

namespace {
constexpr int32_t kLogoIntroMenu = 8;
constexpr uint64_t kSimulationRate = 30;

[[noreturn]] void ExitWithError(const char* message) {
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

int main() {
    Mk64Diagnostics3DSStart();
    Mk64Diagnostics3DSCheckpoint("game-data-init");
    const Mk64GameData3DSResult data = Mk64GameData3DSEnsure();
    if (data.status != MK64_GAME_DATA_READY || data.archivePath == nullptr) {
        Mk64Diagnostics3DSCheckpoint("game-data-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError(data.message);
    }
    Mk64Diagnostics3DSCheckpoint("game-data-ready");
    Mk64Settings3DSLoad();

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
    Mk64Diagnostics3DSCheckpoint("graphics-init");
    if (!Mk64Graphics3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("graphics-init-failed");
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The native Citro3D renderer could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("graphics-ready");

    Mk64Diagnostics3DSCheckpoint("libultra-init");
    osInitialize();
    Mk64Input3DSInit();
    Mk64Diagnostics3DSCheckpoint("game-state-init");
    if (!Mk64GameState3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("game-state-init-failed");
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
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("DSP audio could not start. Dump DSP firmware with a current homebrew setup, then try again.");
    }

    Mk64Diagnostics3DSCheckpoint("bottom-ui-init");
    if (!Mk64BottomUI3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("bottom-ui-init-failed");
        Mk64GameAudio3DSShutdown();
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        ExitWithError("The bottom-screen interface could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("bottom-ui-ready");

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-init");
    thread5_game_loop();
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-ready");

    uint64_t nextSimulationDeadline = svcGetSystemTick();
    uint64_t deadlineRemainder = 0;
    while (WindowIsRunning()) {
        if (Mk64Diagnostics3DSServiceDumpIfRequested()) {
            continue;
        }
        if (Mk64Diagnostics3DSIsPaused()) {
            Mk64Diagnostics3DSSetStage("diagnostic-dump-paused");
            svcSleepThread(16000000LL);
            continue;
        }
        Mk64Diagnostics3DSSetStage("game-loop-iteration");
        Mk64BottomUI3DSPrepareFrame();
        thread5_iteration();
        // HandleEvents() runs inside the display-list iteration and is where
        // aptMainLoop() observes HOME -> Close Software. Do not enter the
        // audio worker wait or frame pacer after that close request.
        if (!WindowIsRunning()) break;
        Mk64Diagnostics3DSSetStage("game-loop-audio");
        Mk64GameAudio3DSPump();

        // Midpoint presentation is adaptive, but gameplay remains a stable
        // 30 Hz clock. When a pressured frame omits the optional midpoint,
        // wait outside Citro3D rather than submitting a dummy GPU frame. The
        // remainder accumulator keeps the 30 Hz deadline exact over time.
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
            if (remainingNanoseconds > 0) svcSleepThread(remainingNanoseconds);
        } else {
            // Never compress the next logic interval after a slow resource or
            // upload frame. Resume the 30 Hz clock from the time actually
            // reached instead of attempting even one catch-up tick.
            nextSimulationDeadline = now;
            deadlineRemainder = 0;
        }
    }

    // WindowIsRunning becomes false after aptMainLoop reports the HOME-menu
    // close request. Settings are persisted on every change. Join the audio
    // and diagnostics workers while NDSP/HID and their stacks are still mapped,
    // then keep the immediate exit that avoids GPU/resource teardown after
    // Citro3D has disabled its VBlank callbacks during the APT transition.
    Mk64GameAudio3DSShutdown();
    Mk64Diagnostics3DSStop();
    std::_Exit(0);
}

extern "C" void userAppExit() {
    // libctru invokes this hook before hidExit() unmaps HID shared memory.
    // Stop the diagnostics HID poller first so abnormal exits cannot leave it
    // dereferencing that mapping while process services are torn down.
    Mk64Diagnostics3DSAbortForProcessExit();
    Mk64GameAudio3DSAbortForProcessExit();
}
