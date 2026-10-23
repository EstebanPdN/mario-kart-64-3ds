#pragma once
#include "native_particle_contract_3ds.h"
#ifdef __cplusplus
extern "C" {
#endif
void Mk64NativeParticleLoad(void);
void Mk64NativeParticleReset(void);
void Mk64NativeParticleGpuFrameReady(void);
void Mk64NativeParticleSaveStats(unsigned frame);
void Mk64NativeScheduleParticle(const void* anchor,unsigned packet);
#ifdef __cplusplus
}
namespace Fast {class Interpreter;class GfxRenderingAPICitro3D;}
void Mk64NativeParticleBeginBatch(Fast::GfxRenderingAPICitro3D*);
void Mk64NativeParticleEndBatch(Fast::GfxRenderingAPICitro3D*);
void Mk64NativeParticleDraw(Fast::Interpreter*,unsigned packet);
#endif
