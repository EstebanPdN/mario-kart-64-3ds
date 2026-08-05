#include "game_data_3ds.h"
#include "audio_runtime_3ds.h"
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
    if (!Mk64Resource3DSInit(data.archivePath)) {
        return ShowError("mk64.o2r could not be opened or is not a supported archive.");
    }
    if (!Mk64Graphics3DSInit()) {
        Mk64Resource3DSShutdown();
        return ShowError("The native Citro3D renderer could not be initialized.");
    }

    osInitialize();
    Mk64Input3DSInit();
    if (!Mk64GameState3DSInit()) {
        Mk64Graphics3DSShutdown();
        Mk64Resource3DSShutdown();
        return ShowError("The vanilla game state could not be initialized.");
    }

    audio_init();
    sound_init();
    Mk64GameAudio3DSInit();

    // Skip the desktop-only Harbour Masters splash and enter the stock logo.
    gMenuSelection = kLogoIntroMenu;
    thread5_game_loop();

    while (WindowIsRunning()) {
        Mk64GameAudio3DSPump();
        thread5_iteration();
    }

    Mk64GameAudio3DSShutdown();
    Mk64Graphics3DSShutdown();
    Mk64Resource3DSShutdown();
    std::_Exit(0);
}
