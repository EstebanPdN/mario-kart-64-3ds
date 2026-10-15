#ifndef MK64_CPU_HIT_STATE_3DS_INC_H
#define MK64_CPU_HIT_STATE_3DS_INC_H
#include "item_rules_3ds.h"
#include <math.h>
static bool Mk64RacerHitMotion3DS(const Player* player) {
    const u32 effects = 0x80 | 0x40 | 0x400 | 0x800 | 0x01000000 |
        HIT_BY_ITEM_EFFECT | HIT_EFFECT | 0x20000 | 0x10000;
    return (player->effects & effects) || (player->kartProps & DRIVING_SPINOUT);
}
static bool Mk64CpuItemPenalty3DS(Player* player) {
    const u32 effects = 0x80 | 0x40 | 0x400 | 0x800 | 0x01000000 |
        HIT_BY_ITEM_EFFECT | HIT_EFFECT | LIGHTNING_EFFECT | 0x20000 | 0x10000;
    const u32 triggers = HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER;
    return (player->effects & effects) || (player->triggers & triggers) ||
        (player->kartProps & DRIVING_SPINOUT);
}
static bool Mk64CpuKeepHitHeight3DS(Player* player) {
    return Mk64NewItems3DSEnabled() && (player->type & PLAYER_CPU) &&
        !(player->type & (PLAYER_HUMAN | PLAYER_CINEMATIC_MODE)) &&
        (Mk64CpuItemPenalty3DS(player) || player->kartHopVelocity > 0.0f);
}
#endif
