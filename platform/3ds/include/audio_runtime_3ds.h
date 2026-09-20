#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64GameAudio3DSInit(void);
// Enable synthesis only after the vanilla startup has initialized audio state.
void Mk64GameAudio3DSFinishInitialization(void);
// Return ownership of shared game audio state to the main thread.
void Mk64GameAudio3DSBeginLogic(void);
void Mk64GameAudio3DSSetPaused(bool paused);
void Mk64GameAudio3DSBeginFrame(void);
void Mk64GameAudio3DSSuspend(void);
void Mk64GameAudio3DSResume(void);
void Mk64GameAudio3DSPump(void);
void Mk64GameAudio3DSShutdown(void);
void Mk64GameAudio3DSAbortForProcessExit(void);

#ifdef __cplusplus
}
#endif
