#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mk64DiagnosticsInput3DS {
    uint32_t heldMask;
    uint32_t downMask;
    uint32_t upMask;
    int16_t circleX;
    int16_t circleY;
    int16_t cstickX;
    int16_t cstickY;
    uint16_t touchX;
    uint16_t touchY;
    bool touchHeld;
} Mk64DiagnosticsInput3DS;

bool Mk64Diagnostics3DSStart(void);
void Mk64Diagnostics3DSStop(void);
void Mk64Diagnostics3DSAbortForProcessExit(void);
void Mk64Diagnostics3DSEmergency(const char* reason);
bool Mk64Diagnostics3DSOwnsHid(void);
bool Mk64Diagnostics3DSIsPaused(void);
bool Mk64Diagnostics3DSServiceDumpIfRequested(void);
bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS* input);
bool Mk64Diagnostics3DSConsumeInput(Mk64DiagnosticsInput3DS* input);
bool Mk64Diagnostics3DSRequestDump(void);
bool Mk64Diagnostics3DSIsNewModel(void);
bool Mk64Diagnostics3DSSupportsWideMode(void);
const char* Mk64Diagnostics3DSGetSystemModelName(void);

void Mk64Diagnostics3DSCheckpoint(const char* stage);
void Mk64Diagnostics3DSSetStage(const char* stage);
void Mk64Diagnostics3DSFailure(const char* stage, const char* reason);
void Mk64Diagnostics3DSSetResource(const char* path, size_t loadedCount);
void Mk64Diagnostics3DSSetArchiveEntryCount(size_t entryCount);
void Mk64Diagnostics3DSCoursePrefetch(size_t dependencies, size_t loadedEntries,
                                      size_t chargedBytes, size_t budgetSkips,
                                      size_t unavailableEntries);
void Mk64Diagnostics3DSKartPrefetch(size_t attemptedEntries, size_t loadedEntries,
                                    size_t loadedBytes, size_t duplicateEntries,
                                    size_t unavailableEntries);
void Mk64Diagnostics3DSSetGameArena(const void* base, size_t capacity);
void Mk64Diagnostics3DSSetDisplayList(const void* base, size_t size);
void Mk64Diagnostics3DSSetFrame(uint64_t frame, unsigned presentation);
void Mk64Diagnostics3DSAudio(uint32_t synthesisBlock, uint32_t bufferedFrames, uint32_t peak,
                             uint32_t nonzeroSamples, uint32_t queuedBuffers,
                             uint32_t droppedBuffers);
void Mk64Diagnostics3DSAudioPump(uint32_t pumpCalls, uint32_t synthesisBlocks,
                                 uint32_t bufferedBefore, uint32_t bufferedAfter,
                                 uint32_t blocksThisPump, uint32_t multiBlockPumps,
                                 uint32_t queueFailures, uint32_t observedEmptyTransitions);
void Mk64Diagnostics3DSAudioState(uint32_t resetStatus, uint32_t resetPreset,
                                  uint32_t sequenceCount, uint32_t activePlayers,
                                  uint32_t activeNotes, uint32_t audioErrors);
void Mk64Diagnostics3DSPerformance(uint32_t frame, uint32_t fps2Tenths,
                                   uint32_t fps10Tenths, uint32_t drawCalls,
                                   uint32_t triangles, uint32_t textureUploads,
                                   uint32_t textureKilobytes, uint32_t vertexKilobytes,
                                   size_t loadedResources, uint32_t processingHundredths,
                                   uint32_t drawingHundredths, uint32_t commandPermille,
                                   uint32_t linearHeapFlushFrames);
void Mk64Diagnostics3DSMemory(const char* label, size_t loadedResources, size_t textureSlots,
                              size_t initializedTextures, size_t textureBytes,
                              size_t shaderPrograms, size_t clipScratchBytes);
void Mk64Diagnostics3DSGfxWatchdog(const void* command, uint32_t word0, uint32_t word1,
                                   size_t commandCount);

#ifdef __cplusplus
}
#endif
