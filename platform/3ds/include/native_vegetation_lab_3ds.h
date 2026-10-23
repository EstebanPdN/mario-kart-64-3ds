#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void Mk64NativeVegetationLoad(void);
void Mk64NativeVegetationReset(void);
void Mk64NativeVegetationEnqueue(unsigned root,const void* anchor,const float* model);
void Mk64NativeVegetationSaveStats(unsigned frame);
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
void Mk64NativeItemBoxState(const void* anchor,uint32_t clear,uint32_t set,uint32_t mode,unsigned resetScale);
void Mk64NativeScheduleObjectState(const void* anchor,unsigned packet);
#endif
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
void Mk64NativeShellEnqueue(const void* anchor,const float* model,unsigned species,unsigned frame,unsigned reverse);
void Mk64NativeProjectilePalette(const void* anchor,unsigned species);
#endif
// The shared migration scheduler preserves ordering across packet families.
void Mk64NativeScheduleVegetation(const void* anchor,unsigned packet);
#ifdef __cplusplus
}
namespace Fast {class Interpreter;class GfxRenderingAPICitro3D;}
void Mk64NativeVegetationBeginBatch(Fast::GfxRenderingAPICitro3D*);
void Mk64NativeVegetationEndBatch(Fast::GfxRenderingAPICitro3D*);
void Mk64NativeVegetationDraw(Fast::Interpreter*,unsigned packet);
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
void Mk64NativeObjectStateApply(Fast::Interpreter*,unsigned packet);
#endif
#endif
