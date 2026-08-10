#include <common_structs.h>

#include "port/Engine.h"

#include <cstdint>

// MK64's simulation remains at its native 30 Hz. The 3DS renderer presents
// one matrix-interpolated frame between each pair of simulation frames so the
// top LCD receives a genuine 60 Hz presentation without speeding up gameplay.
uint32_t GameEngine::GetInterpolationFPS() {
    return 60;
}

uint32_t GameEngine::GetInterpolationFrameCount() {
    return 2;
}

extern "C" uint32_t GameEngine_GetInterpolationFrameCount() {
    return GameEngine::GetInterpolationFrameCount();
}
