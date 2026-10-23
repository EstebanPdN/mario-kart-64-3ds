#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool Mk64Auto3DSActive(void);
bool Mk64Auto3DSRaceInputLocked(void);
bool Mk64Auto3DSMenuAvailable(void);
bool Mk64Auto3DSSelect(void);
void Mk64Auto3DSReset(void);
void Mk64Auto3DSMenuInput(bool confirm, bool back);
// AUTO cup presentation/retry only; never simulates global controller input.
bool Mk64Auto3DSGrandPrixSequence(void);
// Local A-edge equivalent for a specific native result menu; never writes pads.
enum { MK64_AUTO_CONFIRM_GP = 0, MK64_AUTO_CONFIRM_RETRY, MK64_AUTO_CONFIRM_SOLO,
       MK64_AUTO_CONFIRM_CEREMONY, MK64_AUTO_CONFIRM_COUNT };
bool Mk64Auto3DSMenuConfirm(int menu, bool ready);
void Mk64Auto3DSPrepareCeremony(void);
void Mk64Auto3DSPreparePlayers(void);
void Mk64Auto3DSObserveInput(int direction, bool toggleCamera);
int Mk64Auto3DSObservedPlayer(void);
bool Mk64Auto3DSCinematic(void);
bool Mk64Auto3DSWatchedAudio(int playerId);
bool Mk64Auto3DSFinishAudio(int playerId);
bool Mk64Auto3DSCameraTick(void* camera);
// Requests one lightweight trace dump at 30 s and one at the finish of every
// AUTO race, so whole AUTO cups measure each course on hardware.
void Mk64Auto3DSBenchmarkTick(void);
int Mk64CpuItem3DSHud(int playerId);
bool Mk64CpuItem3DSRolling(int playerId);
#ifdef __cplusplus
}
#endif
