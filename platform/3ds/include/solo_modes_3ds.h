#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool Mk64SoloVs3DS(void);
bool Mk64SoloBattle3DS(void);
int Mk64SoloMode3DS(int playerRow, int modeRow);
signed char* Mk64SoloSubMode3DS(int playerRow, int modeRow);
int Mk64SoloModeLimit3DS(int playerRow);
int Mk64SoloLevelLimit3DS(int playerRow, int modeRow, bool extra);
void Mk64SoloBattle3DSSpawn(void);
void Mk64SoloVs3DSSpawn(float* x, float* z, float height);
void Mk64SoloBattle3DSReset(void);
void Mk64SoloBattle3DSTick(int playerId);
void Mk64SoloBattle3DSItems(int playerId);
void Mk64SoloBattle3DSCamera(void* camera);
bool Mk64SoloBattle3DSSpectating(void);
int Mk64SoloBattle3DSObservedPlayer(void);
bool Mk64SoloBattle3DSFarView(void);
void Mk64SoloBattle3DSObserveInput(int direction, bool toggleDistance);
bool Mk64SoloBattle3DSClearShot(int ownerId, int targetId);
bool Mk64SoloBattle3DSWriteDiagnostics(const char* directory);
#ifdef __cplusplus
}
#endif
