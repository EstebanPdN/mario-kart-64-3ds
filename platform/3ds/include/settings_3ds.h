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

typedef enum Mk64RenderDistance3DS {
    MK64_RENDER_DISTANCE_3DS_LOW = 0,
    MK64_RENDER_DISTANCE_3DS_NORMAL = 1,
    MK64_RENDER_DISTANCE_3DS_HIGH = 2,
} Mk64RenderDistance3DS;

typedef enum Mk64HudLayout3DS {
    MK64_HUD_LAYOUT_3DS_CLEAN = 0,
    MK64_HUD_LAYOUT_3DS_MK7 = 1,
    MK64_HUD_LAYOUT_3DS_MKDS = 2,
    MK64_HUD_LAYOUT_3DS_MKDS_2 = 3,
    MK64_HUD_LAYOUT_3DS_CLASSIC = 4,
} Mk64HudLayout3DS;

Mk64HudLayout3DS Mk64Settings3DSGetHudLayout(void);
void Mk64Settings3DSSetHudLayout(Mk64HudLayout3DS layout);

// Settings live in memory after the initial load. Setters never perform file
// I/O; call Save after a user-confirmed change or during a clean shutdown.
// Call before Load; fresh/reset defaults use the detected hardware profile.
void Mk64Settings3DSSetHardwareModel(bool isNewModel);
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

Mk64RenderDistance3DS Mk64Settings3DSGetRenderDistance(void);
void Mk64Settings3DSSetRenderDistance(Mk64RenderDistance3DS distance);

Mk64DisplayFilter3DS Mk64Settings3DSGetDisplayFilter(void);
void Mk64Settings3DSSetDisplayFilter(Mk64DisplayFilter3DS filter);

uint8_t Mk64Settings3DSGetTurboMultiplier(void);
void Mk64Settings3DSSetTurboMultiplier(uint8_t multiplier);

uint16_t Mk64Settings3DSGetMasterVolumePercent(void);
void Mk64Settings3DSSetMasterVolumePercent(uint16_t percent);

bool Mk64Settings3DSGetShowFpsEnabled(void);
bool Mk64Settings3DSGetShowLoadingScreens(void);
void Mk64Settings3DSSetShowLoadingScreens(bool enabled);
void Mk64Settings3DSSetShowFpsEnabled(bool enabled);

bool Mk64Settings3DSGetOverlayEnabled(void);
void Mk64Settings3DSSetOverlayEnabled(bool enabled);

#ifdef __cplusplus
}
#endif
