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

int ShowError(const char* message) {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    std::printf("Mario Kart 64 3DS\n\n%s\n\nPress START to exit.\n", message);
    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) break;
        gspWaitForVBlank();
    }
    gfxExit();
    return 1;
}
}

int main() {
    const Mk64GameData3DSResult data = Mk64GameData3DSEnsure();
    if (data.status != MK64_GAME_DATA_READY || data.archivePath == nullptr) {
        return ShowError(data.message);
    }
    Mk64Diagnostics3DSStart();
    Mk64Diagnostics3DSCheckpoint("game-data-ready");
    Mk64Diagnostics3DSCheckpoint("resource-runtime-init");
    if (!Mk64Resource3DSInit(data.archivePath)) {
        Mk64Diagnostics3DSCheckpoint("resource-runtime-init-failed");
        Mk64Diagnostics3DSStop();
        return ShowError("mk64.o2r could not be opened or is not a supported archive.");
    }
    Mk64Diagnostics3DSCheckpoint("resource-runtime-ready");
    Mk64Diagnostics3DSCheckpoint("graphics-init");
    if (!Mk64Graphics3DSInit()) {
        Mk64Diagnostics3DSCheckpoint("graphics-init-failed");
        Mk64Resource3DSShutdown();
        Mk64Diagnostics3DSStop();
        return ShowError("The native Citro3D renderer could not be initialized.");
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
        return ShowError("The vanilla game state could not be initialized.");
    }
    Mk64Diagnostics3DSCheckpoint("game-state-ready");

    Mk64Diagnostics3DSCheckpoint("audio-init");
    audio_init();
    sound_init();
    Mk64GameAudio3DSInit();
    Mk64Diagnostics3DSCheckpoint("audio-ready");

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-init");
    thread5_game_loop();
    Mk64Diagnostics3DSCheckpoint("vanilla-loop-ready");

    while (WindowIsRunning()) {
        Mk64Diagnostics3DSSetStage("game-loop-audio");
        Mk64GameAudio3DSPump();
        Mk64Diagnostics3DSSetStage("game-loop-iteration");
        thread5_iteration();
    }

    Mk64Diagnostics3DSCheckpoint("shutdown");
    Mk64GameAudio3DSShutdown();
    Mk64Graphics3DSShutdown();
    Mk64Resource3DSShutdown();
    Mk64Diagnostics3DSStop();
    std::_Exit(0);
}
