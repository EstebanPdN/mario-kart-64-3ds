#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The bottom-screen UI shares the Citro3D frame owned by the Fast3D renderer.
 * Initialize it only after Citro3D, call PrepareFrame before the vanilla game
 * polls its controllers, and call Draw while the Citro3D frame is still open.
 */
bool Mk64BottomUI3DSInit(void);
void Mk64BottomUI3DSShutdown(void);
void Mk64BottomUI3DSPrepareFrame(void);
void Mk64BottomUI3DSRecordPresentation(void);
void Mk64BottomUI3DSDrawTopFps(void* existingTopTarget);
void Mk64BottomUI3DSDraw(void* existingTopTarget);
bool Mk64BottomUI3DSConsumeC2DUsage(void);

/* Input-layer boundary used by input_3ds.cpp. */
uint32_t Mk64BottomUI3DSFilterGameKeys(uint32_t heldKeys);
bool Mk64BottomUI3DSConsumesCStick(void);

/* Diagnostic/read-only state for integration and tests. */
bool Mk64BottomUI3DSIsModalOpen(void);
float Mk64BottomUI3DSGetCurrentFps(void);
float Mk64BottomUI3DSGetAverageFps(void);

#ifdef __cplusplus
}
#endif
