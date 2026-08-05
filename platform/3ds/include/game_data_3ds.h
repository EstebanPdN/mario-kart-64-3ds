#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Mk64GameData3DSStatus {
    MK64_GAME_DATA_READY = 0,
    MK64_GAME_DATA_MISSING_ROM,
    MK64_GAME_DATA_BAD_ROM,
    MK64_GAME_DATA_ERROR,
} Mk64GameData3DSStatus;

typedef struct Mk64GameData3DSResult {
    Mk64GameData3DSStatus status;
    const char* archivePath;
    char message[384];
} Mk64GameData3DSResult;

Mk64GameData3DSResult Mk64GameData3DSEnsure(void);

#ifdef __cplusplus
}
#endif
