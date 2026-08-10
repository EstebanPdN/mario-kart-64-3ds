#include "game_data_3ds.h"
#include "audio_runtime_3ds.h"
#include "diagnostics_3ds.h"
#include "game_runtime_3ds.h"
#include "game_state_3ds.h"
#include "input_3ds.h"
#include "resource_runtime_3ds.h"

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
}

namespace {
constexpr int32_t kLogoIntroMenu = 8;

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
    Mk64Diagnostics3DSCheckpoint("game-arena-init");

    // Hold the vanilla game's arena before O2R and Citro3D allocate from the
    // much smaller Old 3DS application heap. setup_game_memory() resets this
    // same arena later; this early call exists solely to make the reservation
    // deterministic and to turn allocation failure into a visible error.
    initialize_memory_pool();
    if (!Mk64MemoryArena3DSIsReady()) {
        Mk64Diagnostics3DSCheckpoint("game-arena-init-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError("Not enough application memory for the 8 MiB game arena.");
    }
    Mk64Diagnostics3DSCheckpoint("game-arena-ready");

    const Mk64GameData3DSResult data = Mk64GameData3DSEnsure();
    if (data.status != MK64_GAME_DATA_READY || data.archivePath == nullptr) {
        Mk64Diagnostics3DSCheckpoint("game-data-failed");
        Mk64Diagnostics3DSStop();
        ExitWithError(data.message);
    }
    Mk64Diagnostics3DSCheckpoint("game-data-ready");
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

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-init");
    thread5_game_loop();
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-ready");

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
        thread5_iteration();
        Mk64Diagnostics3DSSetStage("game-loop-audio");
        Mk64GameAudio3DSPump();
    }

    Mk64Diagnostics3DSCheckpoint("shutdown");
    Mk64GameAudio3DSShutdown();
    Mk64Graphics3DSShutdown();
    Mk64Resource3DSShutdown();
    Mk64Diagnostics3DSStop();
    std::_Exit(0);
}
