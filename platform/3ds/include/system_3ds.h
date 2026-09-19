#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t Mk64System3DSGetTick(void);
uint64_t Mk64System3DSTicksPerSecond(void);
/* Make CPU-written linear memory visible to PICA200/NDSP without a service
 * round trip when the current title permissions allow the direct ARM11 SVC. */
bool Mk64System3DSCleanDataCache(const void* address, size_t size);
const char* Mk64System3DSDataCacheMode(void);
/* Capture the individual linear-memory allocations made by a library initializer.
 * The game executable wraps linearAlloc only while this window is active. */
void Mk64System3DSBeginLinearAllocationCapture(void);
bool Mk64System3DSEndLinearAllocationCapture(const void** address, size_t* size);
size_t Mk64System3DSCapturedLinearAllocationSize(void);
bool Mk64System3DSCleanCapturedLinearAllocations(void);

#ifdef __cplusplus
}
#endif
