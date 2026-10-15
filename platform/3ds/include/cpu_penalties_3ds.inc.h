#include "item_rules_3ds.h"
#include "cpu_hit_state_3ds.inc.h"

static bool Mk64CpuFullPenalty3DS(Player* player, bool visible) {
    if (!Mk64NewItems3DSEnabled() || !(player->type & PLAYER_CPU) ||
        (player->type & (PLAYER_HUMAN | PLAYER_CINEMATIC_MODE))) return false;
    // Include pending hits: clear the offscreen shortcut flag BEFORE apply_effect
    // consumes the trigger, not one tick after banana/squish already took it.
    if (Mk64CpuItemPenalty3DS(player) || player->kartHopVelocity > 0.0f) return true;
    if (visible) return false; // The normal visible branch already has full physics.

    // Offscreen shortcut never refreshes its tyre materials. Refresh using the
    // same four contact samples as the player, then use full physics only where
    // a penalty/hazard exists. Ordinary asphalt retains the inexpensive AI path.
    check_bounding_collision(&player->collision, player->boundingBoxSize,
                             player->pos[0], player->pos[1], player->pos[2]);
    func_80029B4C(player, player->pos[0], player->pos[1], player->pos[2]);
    for (s32 tyre = 0; tyre < 4; ++tyre) {
        u8 surface = player->tyres[tyre].surfaceType;
        if (surface < 15 && (D_800E2E90[player->characterId][surface] > 0.0f ||
                            D_800E2EB0[player->characterId][surface] > 0.0f)) return true;
    }
    s32 id = player - gPlayerOne;
    return (player->oobProps & (UNDER_OOB_OR_FLUID_LEVEL | PASS_OOB_OR_FLUID_LEVEL)) ||
        gPlayerWaterLevel[id] - player->tyres[BACK_LEFT].baseHeight >= 3.5f ||
        gPlayerWaterLevel[id] - player->tyres[BACK_RIGHT].baseHeight >= 3.5f ||
        func_802ABDB8(player->collision.meshIndexZX) != 0 ||
        player->surfaceType == OUT_OF_BOUNDS;
}
