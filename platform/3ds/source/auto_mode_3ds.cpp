#include "auto_mode_3ds.h"
#include "solo_modes_3ds.h"
#include "auto_confirm_3ds.hpp"
#include <array>
#include <cmath>
#include <libultraship.h>
extern "C" {
#include "main.h"
#include "menus.h"
#include "menu_items.h"
#include "camera.h"
#include "code_800029B0.h"
#include "code_80005FD0.h"
#include "racing/race_logic.h"
#include "audio/external.h"
#include "sounds.h"
void func_8001A220(s32 playerId, s32 cameraId);
}

namespace {
// Session state only, never stored in the settings/save file.
bool sEnabled = false, sCinematic = false, sCameraDirty = true;
int sObserved = 0, sShotTicks = 0;
std::array<mk64_3ds::AutoConfirmPulse3DS, MK64_AUTO_CONFIRM_COUNT> sConfirmPulses{};
bool AutoModeAvailable() {
    return gModeSelection == GRAND_PRIX || gModeSelection == VERSUS || gModeSelection == BATTLE;
}
int AutoPlayerCount() { return gModeSelection == BATTLE ? 4 : NUM_PLAYERS; }
}
#include "auto_audio_3ds.inc.hpp"
extern "C" bool Mk64Auto3DSActive() {
    // Results may change the viewport selection to two; controller count stays one.
    return sEnabled && !gDemoMode && AutoModeAvailable() && gPlayerCount == 1;
}
extern "C" bool Mk64Auto3DSGrandPrixSequence() {
    return Mk64Auto3DSActive() && gModeSelection == GRAND_PRIX && !gIsGamePaused;
}
extern "C" bool Mk64Auto3DSMenuConfirm(int menu, bool ready) {
    if (menu < 0 || menu >= MK64_AUTO_CONFIRM_COUNT) return false;
    const bool eligible = ready && Mk64Auto3DSActive() && !gIsGamePaused &&
        !is_screen_being_faded() &&
        (menu == MK64_AUTO_CONFIRM_SOLO ? (gModeSelection == VERSUS || gModeSelection == BATTLE)
                                      : gModeSelection == GRAND_PRIX);
    return sConfirmPulses[menu].Tick(eligible, osGetTime());
}
extern "C" void Mk64Auto3DSPrepareCeremony() {
    if (!Mk64Auto3DSActive() || gModeSelection != GRAND_PRIX) return;
    // Presentation only, after final results: no points/save data are edited.
    // Swap rather than duplicate the chosen driver on the podium and outside it.
    for (int rank = 0; rank < NUM_PLAYERS; ++rank) {
        if (gCharacterIdByGPOverallRank[rank] != gCharacterSelections[0]) continue;
        const auto fourth = gCharacterIdByGPOverallRank[3];
        gCharacterIdByGPOverallRank[3] = gCharacterIdByGPOverallRank[rank];
        gCharacterIdByGPOverallRank[rank] = fourth;
        break;
    }
}
extern "C" bool Mk64Auto3DSRaceInputLocked() {
    // RACING also contains the finish/retry menu. Those screens must retain
    // their native A and cursor input; only the live race is a spectator lock.
    return Mk64Auto3DSActive() && gGamestate == RACING &&
        gRaceState < RACE_FINISHED &&
        (gModeSelection == BATTLE || gLapCountByPlayerId[0] < 3);
}
extern "C" bool Mk64Auto3DSMenuAvailable() {
    return gGamestate != RACING && gMenuSelection == CHARACTER_SELECT_MENU &&
        AutoModeAvailable() && gPlayerCount == 1 &&
        gPlayerSelectMenuSelection == PLAYER_SELECT_MENU_MAIN && !is_screen_being_faded();
}
extern "C" bool Mk64Auto3DSSelect() {
    if (!Mk64Auto3DSMenuAvailable()) return false;
    const int grid = gCharacterGridSelections[0];
    if (grid < 1 || grid > 8) return false;
    gCharacterSelections[0] = sCharacterGridOrder[grid - 1];
    // Confirm through the native menu animation/voice, not a second menu route.
    sEnabled = true;
    sObserved = 0;
    sCinematic = false;
    sCameraDirty = true;
    sShotTicks = 0;
    return true;
}
extern "C" void Mk64Auto3DSReset() {
    sConfirmPulses = {};
    Mk64AutoAudio3DSReset();
    sEnabled = false;
    sObserved = sShotTicks = 0;
    sCinematic = false;
    sCameraDirty = true;
}
extern "C" void Mk64Auto3DSMenuInput(bool confirm, bool back) {
    if (gGamestate == RACING) return;
    if (gMenuSelection == MAIN_MENU || gMenuSelection == START_MENU ||
        (gMenuSelection == CHARACTER_SELECT_MENU &&
         (back || (confirm && gPlayerSelectMenuSelection == PLAYER_SELECT_MENU_MAIN))))
        Mk64Auto3DSReset();
}
extern "C" void Mk64Auto3DSPreparePlayers() {
    if (!Mk64Auto3DSActive() || gGamestate != RACING) return;
    sConfirmPulses = {};
    for (int id = 0; id < AutoPlayerCount(); ++id) {
        if (!(gPlayers[id].type & PLAYER_EXISTS)) continue;
        // Pure CPU, including slot zero: modern item pickup must not see a
        // pretend human. Retain native character-specific physics/stat tables.
        gPlayers[id].type &= ~PLAYER_HUMAN;
        gPlayers[id].type |= PLAYER_CPU;
    }
    Mk64AutoAudio3DSReset();
    sObserved = sShotTicks = 0;
    sCameraDirty = true;
}
extern "C" int Mk64Auto3DSObservedPlayer() {
    return Mk64Auto3DSActive() ? sObserved : Mk64SoloBattle3DSObservedPlayer();
}
extern "C" bool Mk64Auto3DSCinematic() { return Mk64Auto3DSActive() && gModeSelection!=BATTLE && sCinematic; }
extern "C" bool Mk64Auto3DSWatchedAudio(int id) {
    return (Mk64Auto3DSRaceInputLocked() && id == sObserved) ||
        (Mk64SoloBattle3DSSpectating() && id == Mk64SoloBattle3DSObservedPlayer());
}
extern "C" bool Mk64Auto3DSFinishAudio(int id) {
    // The selected driver (slot zero), not the spectator camera, owns the cup
    // result. Its delayed native finish voice outlives the live-race input lock.
    return id == 0 && Mk64Auto3DSActive() && gGamestate == RACING &&
        (gModeSelection == BATTLE ? gRaceState == RACE_FINISHED : gLapCountByPlayerId[0] >= 3);
}
extern "C" void Mk64Auto3DSObserveInput(int direction, bool toggleCamera) {
    if (!Mk64Auto3DSRaceInputLocked() || gIsGamePaused) return;
    if (direction) {
        const int count = AutoPlayerCount();
        for (int attempt = 0; attempt < count; ++attempt) {
            sObserved = (sObserved + (direction < 0 ? count - 1 : 1)) % count;
            if (gPlayers[sObserved].type & PLAYER_EXISTS) break;
        }
        sCameraDirty = true;
    }
    if (toggleCamera) {
        if(gModeSelection==BATTLE)Mk64SoloBattle3DSObserveInput(0,true);
        else { sCinematic = !sCinematic; sCameraDirty = true; }
    }
}
extern "C" bool Mk64Auto3DSCameraTick(void* rawCamera) {
    if (!Mk64Auto3DSActive() || gGamestate != RACING || !rawCamera) return false;
    Camera* camera = static_cast<Camera*>(rawCamera);
    if (camera != gScreenContexts[0].raceCamera) return false;
    if (!(gPlayers[sObserved].type & PLAYER_EXISTS)) {
        // A consumed bomb no longer has a body/camera subject. Keep watching
        // the survivors, never resurrect it or give it a controller.
        for (int id = 0; id < AutoPlayerCount(); ++id) {
            if (gPlayers[id].type & PLAYER_EXISTS) {
                sObserved = id; sCameraDirty = true; break;
            }
        }
    }
    Player* player = &gPlayers[sObserved];
    gScreenContexts[0].player = player; // Section/water rendering must follow this car too.
    Mk64AutoAudio3DSTick();
    if (gIsGamePaused) return true;
    if (sCameraDirty) {
        camera_init(player->pos, player->rotation[1], gModeSelection == BATTLE ? 9 : sCinematic ? 3 : 1, camera->cameraId);
        camera->playerId = sObserved;
        D_801646CC = 0; // Native automatic driver changes are replaced by left/right.
        D_801646C8 = 0;
        D_801646D0[camera->cameraId].unk0 = 0;
        if (sCinematic && gModeSelection != BATTLE) {
            func_8001A220(sObserved, camera->cameraId);
            camera_start_cinematic_shot(sObserved, camera->cameraId);
        }
        sCameraDirty = false;
        sShotTicks = 0;
    }
    camera->playerId = sObserved;
    if (gModeSelection == BATTLE) {
        // Race-demo shots dereference racing waypoints, which arenas lack.
        // Near/far are both static gameplay follow views: arenas have no
        // race-demo cinematics. Keep HUD/audio and viewport zero in either.
        if (player->lakituProps & (LAKITU_RETRIEVAL | HELD_BY_LAKITU)) {
            func_8001E8E8(camera, player, camera->cameraId);
        } else {
            const bool farView=Mk64SoloBattle3DSFarView();
            camera->unk_30[0] = 0.0f;
            camera->unk_30[1] = farView ? 24.0f : 11.6f;
            camera->unk_30[2] = farView ? -65.0f : -38.5f;
            camera->unk_3C[2] = 19.2f;
            func_8001EA0C(camera, player, camera->cameraId);
        }
    } else if (sCinematic) {
        if (++sShotTicks >= 250) {
            func_8001A450(sObserved, camera->cameraId, sObserved);
            sShotTicks = 0;
        }
        u16 mode = 3;
        func_8001A588(&mode, camera, player, camera->cameraId, camera->cameraId);
    } else if (player->lakituProps & (LAKITU_RETRIEVAL | HELD_BY_LAKITU)) {
        func_8001E8E8(camera, player, camera->cameraId);
    } else {
        func_8001E45C(camera, player, camera->cameraId);
    }
    return true;
}
