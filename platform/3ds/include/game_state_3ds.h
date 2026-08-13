#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64GameState3DSInit(void);

enum {
    MK64_BOTTOM_UI_RACER_COUNT = 8,
    MK64_BOTTOM_UI_STANDING_COUNT = 4,
};

typedef enum Mk64TopHudRenderMode3DS {
    MK64_TOP_HUD_RENDER_FULL = -1,
    MK64_TOP_HUD_RENDER_NONE = 0,
    MK64_TOP_HUD_RENDER_LAP_PROGRESS = 1,
    MK64_TOP_HUD_RENDER_SPEEDOMETER = 2,
    MK64_TOP_HUD_RENDER_POSITION_LAP = 3,
} Mk64TopHudRenderMode3DS;

typedef enum Mk64PauseAction3DS {
    MK64_PAUSE_ACTION_CONTINUE = 0,
    MK64_PAUSE_ACTION_QUIT = 1,
} Mk64PauseAction3DS;

typedef struct Mk64BottomUIRacer3DS {
    bool active;
    int8_t characterId;
    int8_t rank;
    float worldX;
    float worldZ;
    int16_t rotationY;
} Mk64BottomUIRacer3DS;

typedef struct Mk64BottomUIGameState3DS {
    int32_t gameState;
    int32_t gameMode;
    int32_t menuSelection;
    int32_t mainMenuSelection;
    bool gameSelectVisible;
    bool racing;
    bool paused;
    bool mirrorMode;
    int8_t topHudRenderMode;
    bool raceFinished;

    float courseTimerSeconds;
    uint32_t courseTimerCentiseconds;
    int8_t currentLap;
    int8_t totalLaps;
    int16_t currentItem;
    bool itemWindowVisible;
    uint8_t itemTextureIndex;
    uint8_t activeRacerCount;
    uint8_t standingCount;
    int8_t standingPlayerIds[MK64_BOTTOM_UI_STANDING_COUNT];
    int8_t standingCharacterIds[MK64_BOTTOM_UI_STANDING_COUNT];
    int8_t standingLapCounts[MK64_BOTTOM_UI_STANDING_COUNT];
    bool standingUnknown[MK64_BOTTOM_UI_STANDING_COUNT];
    float standingNativeX[MK64_BOTTOM_UI_STANDING_COUNT];
    float standingNativeY[MK64_BOTTOM_UI_STANDING_COUNT];
    float standingNativeDirection[MK64_BOTTOM_UI_STANDING_COUNT];
    uint8_t standingAlpha;
    uint8_t playerBorderRed;
    uint8_t playerBorderGreen;
    uint8_t playerBorderBlue;
    bool currentPlaceVisible;
    uint8_t currentPlaceIndex;
    uint8_t currentPlaceGreen;
    float currentPlaceScale;
    int16_t currentPlaceNativeX;
    int16_t currentPlaceNativeY;
    Mk64BottomUIRacer3DS racers[MK64_BOTTOM_UI_RACER_COUNT];

    size_t trackIndex;
    const char* trackName;
    const char* mainBackgroundTexture;
    const char* coursePreviewTexture;
    const char* minimapTexture;
    int16_t minimapWidth;
    int16_t minimapHeight;
    int32_t minimapPlayerX;
    int32_t minimapPlayerY;
    float minimapPlayerScale;
    float minimapFinishlineX;
    float minimapFinishlineY;
    uint8_t minimapRed;
    uint8_t minimapGreen;
    uint8_t minimapBlue;
} Mk64BottomUIGameState3DS;

void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* snapshot);
void Mk64GameState3DSSetTopHudEnabled(bool enabled);
void Mk64GameState3DSApplyTurbo(bool active, uint8_t multiplier);
bool Mk64GameState3DSRaceControlsActive(void);
bool Mk64GameState3DSCycleHiddenTopHud(void);
int Mk64GameState3DSGetTopHudRenderMode(void);
bool Mk64GameState3DSPerformPauseAction(Mk64PauseAction3DS action);

#ifdef __cplusplus
}
#endif
