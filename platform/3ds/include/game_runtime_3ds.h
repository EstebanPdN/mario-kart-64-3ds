#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64Graphics3DSInit(void);
void Mk64Graphics3DSShutdown(void);
bool WindowIsRunning(void);
bool Mk64Graphics3DSResolvedNewModel(void);
uint32_t Mk64Graphics3DSResolvedOutputWidth(void);
bool Mk64Graphics3DSUsesIntermediatePresentation(void);

#ifdef __cplusplus
}
#endif
