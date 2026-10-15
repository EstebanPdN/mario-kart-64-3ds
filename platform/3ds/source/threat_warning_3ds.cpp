#include "threat_warning_3ds.h"
#include "auto_mode_3ds.h"
#include "item_rules_3ds.h"
#include "port/Game.h"
extern "C" {
#include "main.h"
#include "code_80005FD0.h"
#include "defines.h"
#include "racing/race_logic.h"
#include "racing/math_util.h"
#include "threat_warning_3ds.inc.h"
}
#include <cmath>
#include <limits>

extern "C" bool Mk64ThreatWarning3DSRead(Mk64ThreatWarning3DS* warning) {
    if (!warning) return false;
    *warning = {};
    if (!Mk64NewItems3DSEnabled() || gGamestate != RACING || gDemoMode ||
        gIsGamePaused || gPlayerCountSelection1 != 1 || Mk64Auto3DSCinematic()) return false;
    const int id = Mk64Auto3DSActive() ? Mk64Auto3DSObservedPlayer() : 0;
    if (!Mk64ThreatListener3DS(id)) return false;
    const Player& listener = gPlayers[id];
    Vec3f forward = {0, 0, 1};
    func_802B64C4(forward, listener.rotation[1] + listener.unk_0C0);
    float best = std::numeric_limits<float>::infinity();
    float bestX = 0, bestZ = 0;
    auto consider = [&](const f32* pos, float range, float height, uint8_t item, int32_t source) {
        const float x = pos[0]-listener.pos[0], y = pos[1]-listener.pos[1], z = pos[2]-listener.pos[2];
        const float along = x*forward[0] + z*forward[2];
        const float distance = x*x+y*y+z*z;
        if (!std::isfinite(distance) || along >= 0 || distance >= best ||
            (range > 0 && distance > range*range) || (height > 0 && y*y > height*height)) return;
        best = distance; bestX = x; bestZ = z;
        warning->item = item; warning->source = source;
    };
    for (int other = 0; other < NUM_PLAYERS; ++other) {
        const Player& racer = gPlayers[other];
        if (other != id && (racer.type & PLAYER_EXISTS) &&
            !(racer.type & (PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
            (racer.effects & STAR_EFFECT) && gLapCountByPlayerId[other] < 3)
            consider(racer.pos, kMk64ThreatStarRange3DS, kMk64ThreatStarHeight3DS, 10, 0x10000000+other);
    }
    const size_t count = CM_GetActorSize();
    for (size_t i = 0; i < count; ++i) {
        const Actor* actor = CM_GetActor(i);
        if (!actor || !(actor->flags & 0x8000) ||
            (actor->type != ACTOR_RED_SHELL && actor->type != ACTOR_BLUE_SPINY_SHELL)) continue;
        const ShellActor* shell = reinterpret_cast<const ShellActor*>(actor);
        if (shell->unk_16 <= 0 || (shell->state != BLUE_SHELL_LOCK_ON &&
            shell->state != GREEN_SHELL_HIT_A_RACER)) continue;
        if (shell->type == ACTOR_BLUE_SPINY_SHELL) {
            if (shell->playerId != id) consider(shell->pos, 0, 0, 7, static_cast<int32_t>(i));
        } else if (shell->targetPlayer == id) {
            consider(shell->pos, kMk64ThreatRedRange3DS, 0, 5, static_cast<int32_t>(i));
        }
    }
    if (!warning->item) return false;
    // Native guLookAt uses up x backward as screen-right. With a +Z
    // heading, -world-X is screen-right, not +world-X. Keep rear eligibility
    // unchanged; only the horizontal HUD bearing uses this camera handedness.
    const float right = bestZ*forward[0]-bestX*forward[2];
    const float rear = -(bestX*forward[0]+bestZ*forward[2]);
    warning->bearing = std::atan2(right, rear) / 1.5707963267948966f;
    return true;
}
