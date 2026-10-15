#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Countdown only. Original CI8 + TLUT pixels, expanded into stable RGBA16
// storage before drawing; the finish flag keeps its existing rendering path.
bool Mk64LakituCountdown3DSPrepare(const char* const* frames,
                                  size_t frameCount, const char* const* palettes,
                                  size_t paletteCount);
const uint8_t* Mk64LakituCountdown3DSFrame(int index, const uint8_t* activePalette);

#ifdef __cplusplus
}
#endif
