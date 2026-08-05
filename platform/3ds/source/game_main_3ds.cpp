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
uint32_t __stacksize__ = 4 * 1024 * 1024;
}

namespace {
constexpr int32_t kLogoIntroMenu = 8;
constexpr std::array<const char*, 3> kArchivePaths = {
    "sdmc:/3ds/mk64-3ds/mk64.o2r",
    "sdmc:/3ds/spaghettikart/mk64.o2r",
    "sdmc:/mk64.o2r",
};

const char* FindArchive() {
    for (const char* path : kArchivePaths) {
        if (FILE* file = std::fopen(path, "rb")) {
            std::fclose(file);
            return path;
        }
    }
    return nullptr;
}

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
    const char* archivePath = FindArchive();
    if (archivePath == nullptr) {
        return ShowError("mk64.o2r was not found. Place your legally generated archive in\n/3ds/mk64-3ds/mk64.o2r on the SD card.");
    }
    if (!Mk64Resource3DSInit(archivePath)) {
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
