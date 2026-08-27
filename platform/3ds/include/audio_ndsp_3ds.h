#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Mk64Audio3DSInit(uint32_t sampleRate);
void Mk64Audio3DSShutdown(void);
void Mk64Audio3DSSetPaused(bool paused);
uint32_t Mk64Audio3DSBufferedFrames(void);
uint32_t Mk64Audio3DSQueuedCount(void);
uint32_t Mk64Audio3DSDroppedCount(void);
int16_t* Mk64Audio3DSAcquireStereoS16(size_t frameCount, uint32_t* bufferToken);
void Mk64Audio3DSReleaseStereoS16(uint32_t bufferToken);
bool Mk64Audio3DSCommitStereoS16(uint32_t bufferToken, size_t frameCount);
bool Mk64Audio3DSQueueStereoS16(const int16_t* samples, size_t frameCount);

#ifdef __cplusplus
}
#endif
