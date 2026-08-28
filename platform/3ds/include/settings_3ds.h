#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Mk64AspectRatio3DS {
    MK64_ASPECT_RATIO_3DS_WIDE = 0,
    MK64_ASPECT_RATIO_3DS_ORIGINAL = 1,
} Mk64AspectRatio3DS;

typedef enum Mk64DisplayFilter3DS {
    MK64_DISPLAY_FILTER_3DS_BILINEAR = 0,
    MK64_DISPLAY_FILTER_3DS_BLUR = 1,
    MK64_DISPLAY_FILTER_3DS_CRT = 2,
} Mk64DisplayFilter3DS;

// Settings live in memory after the initial load. Setters never perform file
// I/O; call Save after a user-confirmed change or during a clean shutdown.
void Mk64Settings3DSLoad(void);
bool Mk64Settings3DSSave(void);
void Mk64Settings3DSResetDefaults(void);

Mk64AspectRatio3DS Mk64Settings3DSGetAspectRatio(void);
void Mk64Settings3DSSetAspectRatio(Mk64AspectRatio3DS aspectRatio);

bool Mk64Settings3DSGetTopHudEnabled(void);
void Mk64Settings3DSSetTopHudEnabled(bool enabled);

uint16_t Mk64Settings3DSGetResolutionWidth(void);
void Mk64Settings3DSSetResolutionWidth(uint16_t width);

uint8_t Mk64Settings3DSGetRenderScalePercent(void);
void Mk64Settings3DSSetRenderScalePercent(uint8_t percent);

Mk64DisplayFilter3DS Mk64Settings3DSGetDisplayFilter(void);
void Mk64Settings3DSSetDisplayFilter(Mk64DisplayFilter3DS filter);

uint8_t Mk64Settings3DSGetTurboMultiplier(void);
void Mk64Settings3DSSetTurboMultiplier(uint8_t multiplier);

uint16_t Mk64Settings3DSGetMasterVolumePercent(void);
void Mk64Settings3DSSetMasterVolumePercent(uint16_t percent);

bool Mk64Settings3DSGetShowFpsEnabled(void);
void Mk64Settings3DSSetShowFpsEnabled(bool enabled);

bool Mk64Settings3DSGetOverlayEnabled(void);
void Mk64Settings3DSSetOverlayEnabled(bool enabled);

#ifdef __cplusplus
}
#endif
