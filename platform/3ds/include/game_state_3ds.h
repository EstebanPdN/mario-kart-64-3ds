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
    MK64_BOTTOM_UI_STANDING_COUNT = 5,
};

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

    float courseTimerSeconds;
    int8_t currentLap;
    int8_t totalLaps;
    int16_t currentItem;
    uint8_t activeRacerCount;
    uint8_t standingCount;
    int8_t standingPlayerIds[MK64_BOTTOM_UI_STANDING_COUNT];
    int8_t standingCharacterIds[MK64_BOTTOM_UI_STANDING_COUNT];
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
    uint8_t minimapRed;
    uint8_t minimapGreen;
    uint8_t minimapBlue;
} Mk64BottomUIGameState3DS;

void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* snapshot);
void Mk64GameState3DSSetTopHudEnabled(bool enabled);
void Mk64GameState3DSApplyTurbo(bool active, uint8_t multiplier);

#ifdef __cplusplus
}
#endif
