#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64Graphics3DSInit(void);
void Mk64Graphics3DSShutdown(void);
bool WindowIsRunning(void);

typedef enum Mk64PerformanceProfile3DS {
    MK64_PERFORMANCE_PROFILE_OLD_3DS = 0,
    MK64_PERFORMANCE_PROFILE_NEW_3DS = 1,
} Mk64PerformanceProfile3DS;

Mk64PerformanceProfile3DS Mk64Graphics3DSResolvedPerformanceProfile(void);
uint32_t Mk64Graphics3DSBottomHudRefreshDivisor(void);
size_t Mk64Graphics3DSTextureCacheCapacity(void);
bool Mk64Graphics3DSResolvedNewModel(void);
uint32_t Mk64Graphics3DSResolvedOutputWidth(void);
bool Mk64Graphics3DSUsesIntermediatePresentation(void);

#ifdef __cplusplus
}
#endif
