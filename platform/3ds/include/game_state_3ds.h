#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64GameState3DSInit(void);
void Mk64GameState3DSPrepareRaceResources(void);
bool Mk64RandomCup3DSStart(bool randomDriverAndSpeed, uint64_t seed);
void Mk64RandomCup3DSPrepareRace(void);
void Mk64RandomCup3DSReset(void);
bool Mk64RandomCup3DSActive(void);
int Mk64RandomCup3DSMenuKind(void); // 0 unavailable, 1 Game Select, 2 Map Select.
bool Mk64GameState3DSSkipCeremony(void);
void Mk64MenuBackTitle3DSBegin(void);
int Mk64MenuBackTitle3DSFadeIn(int defaultDuration);
// Read the same live vertex colors as the native RETRY/QUIT menu.
bool Mk64GameState3DSGetPauseTextColors(bool selected, uint32_t* top, uint32_t* bottom);

enum {
    MK64_BOTTOM_UI_RACER_COUNT = 8,
    MK64_BOTTOM_UI_STANDING_COUNT = 8,
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
    uint16_t portraitHitAngle;
} Mk64BottomUIRacer3DS;

typedef struct Mk64BottomUIGameState3DS {
    float topHudOpacity;
    bool dataGrid, dataCourse, dataHasGhost, dataHasRecords;
    int dataIndex, dataSubmenu, dataConfirm, dataEraseOption;
    const char* trackLength;
    uint32_t dataTimes[6];
    int32_t gameState;
    int32_t gameMode;
    int32_t menuSelection;
    int32_t mainMenuSelection;
    int8_t selectedDriver, selectedLevel;
    bool gameSelectVisible;
    float gameSelectOptionReveal, gameSelectDataReveal;
    bool racing;
    uint32_t hudAnimationTick;
    uint32_t menuFlashTick;
    bool demo;
    bool autoMode, autoCinematic, autoMenuAvailable;
    int8_t observedPlayer;
    bool multiplayer;
    bool paused;
    bool mirrorMode;
    int8_t topHudRenderMode;
    bool raceFinished;
    bool topFinishPlaceVisible;
    bool finishPortraitTransfer, finishPortraitLineup;
    uint16_t finishPortraitPhase;
    uint32_t lapDurationCentiseconds[3];

    float courseTimerSeconds;
    uint32_t courseTimerCentiseconds;
    int8_t currentLap;
    int8_t totalLaps;
    int16_t currentItem;
    bool itemWindowVisible;
    float itemWindowSlideY;
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

const char* Mk64GameState3DSDataPreview(int index);
const char* TrackBrowser_GetMinimapTextureByIdx(size_t index);
const char* Mk64GameState3DSDataName(int index);
void Mk64GameState3DSDataMove(int delta);
void Mk64GameState3DSDataAction(int action);
void Mk64GameState3DSDataConfirm(int direction, bool activate, bool back);
void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* snapshot);
// Post-simulation presentation refresh; no resource loading or animation ticks.
void Mk64GameState3DSRefreshStandings(Mk64BottomUIGameState3DS* snapshot);
void Mk64GameState3DSUpdateHudAnimations(void);
void Mk64GameState3DSSetTopHudEnabled(bool enabled);
void Mk64GameState3DSApplyTurbo(bool active, uint8_t multiplier);
bool Mk64GameState3DSRaceControlsActive(void);
int Mk64GameState3DSGetTopHudRenderMode(void);
bool Mk64GameState3DSPerformPauseAction(Mk64PauseAction3DS action);

#ifdef __cplusplus
}
#endif
