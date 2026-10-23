#pragma once
#include "native_particle_contract_3ds.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// Diagnostic-only dual capture. The original producer records the matrix once;
// the native packet borrows that stable key and owns all other draw inputs.
const void* Mk64NativeParticleCompareLastMatrix(void);
void Mk64NativeParticleEmitCompared(const void* begin,const void* end,
    const Mk64NativeParticle3DS*,Mk64NativeParticleQuad,const void* matrixKey);
#ifdef __cplusplus
}
void Mk64NativeParticleUseEagerExits(bool enabled);
uint64_t Mk64NativeParticleExitCount();
void Mk64NativeParticleUseReference(bool enabled);
bool Mk64NativeParticleIsReference();
void Mk64NativeParticleValidateComparison(const void* commands,size_t bytes);
const void* Mk64NativeParticleComparedEnd(unsigned packet,const void* begin);
void Mk64NativeParticleReferenceReached(unsigned packet);
uint64_t Mk64NativeParticleReferenceCount();
uint64_t Mk64NativeParticleDrawCount();
unsigned Mk64NativeParticlePacketCount();
uint64_t Mk64NativeParticleBatchCount();
uint64_t Mk64NativeParticleGeometryCleanCount();
#endif
