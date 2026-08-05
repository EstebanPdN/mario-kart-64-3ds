#include "engine/HM_Intro.h"
#include "engine/registry/RegisterContent.h"
#include "port/audio/HMAS.h"

HarbourMastersIntro::HarbourMastersIntro() = default;
void HarbourMastersIntro::HM_InitIntro() {}
void HarbourMastersIntro::HM_TickIntro() {}
void HarbourMastersIntro::HM_DrawIntro() {}

void RegisterTracks(Registry<TrackInfo>&) {}
void RegisterActors(Registry<ActorInfo, const SpawnParams&>&) {}

extern "C" {

void HMAS_Play(HMAS_ChannelId, HMAS_AudioId, bool) {}
void HMAS_Stop(HMAS_ChannelId) {}
bool HMAS_IsPlaying(HMAS_ChannelId) { return false; }
void HMAS_SetPitch(HMAS_ChannelId, float) {}
void HMAS_SetVolume(HMAS_ChannelId, float) {}
void HMAS_SetPause(HMAS_ChannelId, bool) {}
void HMAS_AddEffect(HMAS_ChannelId, HMAS_EffectType, HMAS_EffectTransition, uint32_t, float) {}
bool HMAS_IsIDRegistered(HMAS_AudioId) { return false; }

void freecam_loop() {}
void freecam_update_controller() {}
void moon_jump() {}
void render_collision() {}

}
