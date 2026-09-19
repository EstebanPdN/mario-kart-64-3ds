#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Startup only: the worker owns software framebuffers, never a Citro3D frame.
void Mk64Loading3DSStart(const char* archivePath);
void Mk64Loading3DSPauseDisplay(void);
void Mk64Loading3DSResumeDisplay(void);
void Mk64Loading3DSStop(void);
bool Mk64Loading3DSActive(void);
#ifdef __cplusplus
}
#endif
