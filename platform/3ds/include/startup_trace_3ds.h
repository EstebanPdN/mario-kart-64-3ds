#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Stage strings must have static lifetime. Only the main thread sets stages.
void Mk64Startup3DSStart(void);
void Mk64Startup3DSStage(const char* stage) __attribute__((weak));
void Mk64Startup3DSFailure(const char* stage) __attribute__((weak));
void Mk64Startup3DSDisplayTimeout(void) __attribute__((weak));
void Mk64Startup3DSLoaderPhase(const char* stage) __attribute__((weak));
bool Mk64Startup3DSActive(void) __attribute__((weak));
void Mk64Startup3DSFrameSubmitted(void) __attribute__((weak));
bool Mk64Startup3DSHasSubmittedFrames(void);
void Mk64Startup3DSAsync(void);
void Mk64Startup3DSGameState(int gameState, int menu);
void Mk64Startup3DSStop(void);
#ifdef __cplusplus
}
#endif
static inline void Mk64StartupStage(const char* stage) {
    if (Mk64Startup3DSStage) Mk64Startup3DSStage(stage);
}
static inline void Mk64StartupFailure(const char* stage) {
    if (Mk64Startup3DSFailure) Mk64Startup3DSFailure(stage);
    else Mk64StartupStage(stage);
}
static inline void Mk64StartupLoaderPhase(const char* stage) {
    if (Mk64Startup3DSLoaderPhase) Mk64Startup3DSLoaderPhase(stage);
}
