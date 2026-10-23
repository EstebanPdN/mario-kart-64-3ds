#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Main-thread exclusive game preparation spans. Nesting subtracts child time
// from its parent, including recursive calls in the same category. Pair tokens
// in LIFO order on every return path. A zero token is a no-op outside a tick.
typedef enum Mk64PerfGameSection3DS {
    MK64_PERF_GAME_PHYSICS,
    MK64_PERF_GAME_COLLISION,
    MK64_PERF_GAME_PLAYERS,
    MK64_PERF_GAME_ACTORS,
    MK64_PERF_GAME_CAMERA,
    MK64_PERF_GAME_VISUAL,
    MK64_PERF_GAME_SECTION_COUNT
} Mk64PerfGameSection3DS;
uint32_t Mk64Perf3DSGameSectionBegin(Mk64PerfGameSection3DS section);
void Mk64Perf3DSGameSectionEnd(uint32_t token);

#ifdef __cplusplus
}
#endif
