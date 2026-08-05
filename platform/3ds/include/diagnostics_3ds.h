#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mk64DiagnosticsInput3DS {
    uint32_t heldMask;
    int16_t circleX;
    int16_t circleY;
    int16_t cstickX;
    int16_t cstickY;
} Mk64DiagnosticsInput3DS;

bool Mk64Diagnostics3DSStart(void);
void Mk64Diagnostics3DSStop(void);
bool Mk64Diagnostics3DSOwnsHid(void);
bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS* input);

void Mk64Diagnostics3DSCheckpoint(const char* stage);
void Mk64Diagnostics3DSSetStage(const char* stage);
void Mk64Diagnostics3DSFailure(const char* stage, const char* reason);
void Mk64Diagnostics3DSSetResource(const char* path, size_t loadedCount);
void Mk64Diagnostics3DSSetArchiveEntryCount(size_t entryCount);
void Mk64Diagnostics3DSSetGameArena(const void* base, size_t capacity);
void Mk64Diagnostics3DSSetDisplayList(const void* base, size_t size);
void Mk64Diagnostics3DSSetFrame(uint64_t frame, unsigned presentation);
void Mk64Diagnostics3DSGfxWatchdog(const void* command, uint32_t word0, uint32_t word1,
                                   size_t commandCount);

#ifdef __cplusplus
}
#endif
