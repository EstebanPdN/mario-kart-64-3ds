#ifndef MK64_CPU_ITEM_TARGET_3DS_INC_H
#define MK64_CPU_ITEM_TARGET_3DS_INC_H
#include "solo_modes_3ds.h"
// Current race positions, never the fixed principal-rival IDs or human flags.
static bool Mk64ItemLeader3DS(s32 id) {
    return id >= 0 && id < NUM_PLAYERS &&
        (gPlayerPositionLUT[0] == id || gPlayerPositionLUT[1] == id);
}

static s32 Mk64CpuItemTarget3DS(s32 ownerId, s32 direction) {
    if (ownerId < 0 || ownerId >= NUM_PLAYERS) return -1;
    Player* owner = &gPlayers[ownerId];
    Vec3f forward = {0.0f, 0.0f, 1.0f};
    const f32 range = Mk64SoloBattle3DS() ? 450.0f : 180.0f;
    f32 closest = range * range;
    s32 target = -1;
    func_802B64C4(forward, owner->rotation[1] + owner->unk_0C0);
    for (s32 i = 0; i < NUM_PLAYERS; ++i) {
        Player* other = &gPlayers[i];
        // Race attacks deliberately select only the leading pair. Arena
        // opponents have no race rank, so retain its ordinary proximity policy.
        if ((gModeSelection != BATTLE && !Mk64ItemLeader3DS(i)) ||
            i == ownerId || !(other->type & PLAYER_EXISTS) ||
            (other->type & (PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) ||
            gLapCountByPlayerId[i] >= 3 || (other->effects & (BOO_EFFECT | STAR_EFFECT))) continue;
        f32 x = other->pos[0] - owner->pos[0];
        f32 y = other->pos[1] - owner->pos[1];
        f32 z = other->pos[2] - owner->pos[2];
        f32 along = x * forward[0] + z * forward[2];
        f32 across = x * forward[2] - z * forward[0];
        f32 distance = x*x + z*z;
        // Ranking is not permission to shoot through bridges or across walls.
        const f32 cone = Mk64SoloBattle3DS() ? 0.65f : 0.4f;
        if (y*y > 25.0f*25.0f || (direction > 0 && along <= 0) ||
            (direction < 0 && along >= 0) ||
            across*across > (20.0f + cone*fabsf(along))*(20.0f + cone*fabsf(along))) continue;
        if (Mk64SoloBattle3DS() && !Mk64SoloBattle3DSClearShot(ownerId,i)) continue;
        if (distance < closest) { closest = distance; target = i; }
    }
    return target;
}
static f32 Mk64CpuTripleShellSpeed3DS(s32 ownerId, s32 parentType) {
    if (!Mk64NewItems3DSEnabled() || parentType != ACTOR_TRIPLE_GREEN_SHELL ||
        ownerId < 0 || ownerId >= NUM_PLAYERS ||
        !(gPlayers[ownerId].type & PLAYER_CPU) || (gPlayers[ownerId].type & PLAYER_HUMAN)) return 8.0f;
    s32 target = Mk64CpuItemTarget3DS(ownerId, 0);
    if (target < 0) return 8.0f;
    Player* owner = &gPlayers[ownerId];
    Vec3f axis = {0.0f, 0.0f, 1.0f};
    func_802B64C4(axis, owner->rotation[1] + owner->unk_0C0);
    f32 along = (gPlayers[target].pos[0] - owner->pos[0])*axis[0] +
        (gPlayers[target].pos[2] - owner->pos[2])*axis[2];
    return along < 0.0f ? -8.0f : 8.0f;
}
#endif
