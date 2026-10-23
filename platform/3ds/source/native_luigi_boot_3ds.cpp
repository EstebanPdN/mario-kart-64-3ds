// Native resource initialization; bounded experiments may additionally force a race.
#include "native_course_lab_3ds.hpp"
#ifdef MK64_3DS_NATIVE_KART_LAB
#include "native_kart_lab_3ds.h"
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
#include "native_particle_lab_3ds.h"
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
#include <stdexcept>
#endif
#include "engine/World.h"
#include "engine/Cup.h"
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
#include "native_vegetation_lab_3ds.h"
#include "engine/Matrix.h"
#include "port/interpolation/FrameInterpolation.h"
#include <cstring>
extern "C" Mtx* Mk64NativeObjectMatrixRecord(const float* model) {
    Mat4 copy;std::memcpy(copy,model,sizeof copy);Mtx* key=GetEffectMatrix();
    FrameInterpolation_RecordMatrixMtxFToMtx(reinterpret_cast<MtxF*>(copy),key);
    guMtxF2L(copy,key);return key;
}
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
extern "C" const void* Mk64NativeParticleCompareLastMatrix() {
    const auto& matrices=GetWorld()->world_mtx.Objects;
    if(matrices.empty())throw std::runtime_error("Native particle comparison matrix missing");
    return &matrices.back();
}
#endif
#endif
#include "auto_mode_3ds.h"
#ifdef MK64_3DS_NATIVE_CACHE_LAB
extern "C" void Mk64NativeCacheProbe3DS();
#endif
extern "C" {
#include "main.h"
#include "menus.h"
#include "menu_items.h"
#include "racing/race_logic.h"
void TrackBrowser_SetTrackFromCup();
void Mk64NativeLuigiLabLoad();
void Mk64Auto3DSLabEnable();
}
extern "C" void Mk64NativeLuigiLabBoot() {
#ifndef MK64_3DS_NATIVE_FULL_GAME
    Mk64NativeLuigiLabLoad();
#endif
#ifdef MK64_3DS_NATIVE_KART_LAB
    Mk64NativeKartLabLoad();
#endif
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
    Mk64NativeVegetationLoad();
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
    Mk64NativeParticleLoad();
#endif
#ifdef MK64_3DS_NATIVE_CACHE_LAB
    Mk64NativeCacheProbe3DS();
#endif
#ifndef MK64_3DS_NATIVE_FULL_GAME
    static Cup cup("native-luigi-lab","Native Luigi Lab",std::vector<std::string>(4,"mk:luigi_raceway"));
    auto* world=GetWorld();world->SetCurrentCup(&cup);world->SetCupIndex(MUSHROOM_CUP);world->SetCurrentCup(&cup);
    gPlayerCount=1;gPlayerCountSelection1=1;gScreenModeSelection=SCREEN_MODE_1P;
    gModeSelection=GRAND_PRIX;gCCSelection=CC_50;gDemoMode=0;gIsGamePaused=0;
    gCharacterSelections[0]=MARIO;gCupSelection=MUSHROOM_CUP;gCourseIndexInCup=0;gIsMirrorMode=0;
    TrackBrowser_SetTrackFromCup();
#ifndef MK64_3DS_NATIVE_PLAYTEST
    Mk64Auto3DSLabEnable();
#endif
    gGamestateNext=RACING;
#endif
}
