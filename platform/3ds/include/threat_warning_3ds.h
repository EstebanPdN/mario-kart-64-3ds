#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Mk64ThreatWarning3DS {
    int32_t source;
    float bearing; // -1 left, 0 directly behind, +1 right, in racer-heading axes.
    uint8_t item; // Native item-window texture index.
} Mk64ThreatWarning3DS;
// Read-only simulation-thread scan. No resource loads, actor or audio mutation.
bool Mk64ThreatWarning3DSRead(Mk64ThreatWarning3DS* warning);
#ifdef __cplusplus
}
#endif
