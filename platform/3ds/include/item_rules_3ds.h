#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Latched on scene/race initialization, never changes for live actors.
bool Mk64NewItems3DSEnabled(void);
void Mk64NewItems3DSBeginRace(void);
void Mk64NewItems3DSLightningUsed(void);
// Called only by a real box collision; itemId 0 rolls normally, balloon overrides.
bool Mk64CpuItemBox3DSPickup(int playerId, int itemId);
float Mk64CpuSeekBox3DS(int playerId, int nearest, float nativeFactor);
void Mk64CpuTraffic3DSAvoid(int playerId);
bool Mk64CpuTraffic3DSAvoiding(int playerId);
bool Mk64CpuLeader3DSRegulate(int playerId, float targetSpeed);
bool Mk64CpuKeepCornerThrottle3DS(int playerId);
float Mk64CpuDrive3DSLimitForce(void* racer, float nativeForce);
void Mk64CpuItemPenaltyTick3DS(int playerId, bool hit);
void Mk64ShellWarning3DSStop(void* actor);
void Mk64ItemHit3DSTick(void* racer);
bool Mk64ItemHit3DSTry(void* racer);
bool Mk64ItemHit3DSProtected(void* racer);
#ifdef __cplusplus
}
#endif
