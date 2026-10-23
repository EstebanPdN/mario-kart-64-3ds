#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void Mk64NativeKartLabLoad(void);
void Mk64NativeKartCaptureReset(void);
typedef enum Mk64NativeCharacterKind {
    MK64_NATIVE_BODY, MK64_NATIVE_GHOST, MK64_NATIVE_TRAIL,
    MK64_NATIVE_REFLECTION, MK64_NATIVE_GROUND_SHADOW, MK64_NATIVE_CREDITS_SHADOW,
    MK64_NATIVE_CHARACTER_KINDS
} Mk64NativeCharacterKind;
void Mk64NativeShadowEnqueue(const void* anchor,const void* matrixKey,const float* model,
    const void* vertices,const char* texture,uint32_t renderMode,uint8_t alpha,Mk64NativeCharacterKind kind);
void Mk64NativeKartEnqueue(const void* anchor,const void* matrixKey,const float* model,
    const void* vertices,const char* texture,const void* palette,const uint8_t colors[8],
    uint32_t renderMode,bool blend,Mk64NativeCharacterKind kind);
#ifdef __cplusplus
}
#endif
