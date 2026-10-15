// Shared CPU pace, anchored to racing humans (selected driver in AUTO),
// never the next CPU or the currently observed camera.
// Fixed state only; native driving/terrain and vertical hit motion are retained.
#include "diagnostics_3ds.h"
#include "auto_mode_3ds.h"
static f32 sMk64LeaderRouteLength3DS;
static f32 sMk64PaceSampleTime3DS = -1.0f;
static f32 sMk64HumanPace3DS[NUM_PLAYERS], sMk64HumanProgress3DS[NUM_PLAYERS];
static f32 sMk64HumanIdleSince3DS[NUM_PLAYERS];
static f32 sMk64CpuPaceTime3DS[NUM_PLAYERS], sMk64CpuPaceTarget3DS[NUM_PLAYERS];
static f32 sMk64CpuPaceBase3DS[NUM_PLAYERS];
static f32 sMk64CpuForcePace3DS[NUM_PLAYERS];
static f32 sMk64CpuForceBlend3DS[NUM_PLAYERS];
static f32 sMk64CpuStalledSince3DS[NUM_PLAYERS];
static f32 sMk64CpuStallOriginX3DS[NUM_PLAYERS], sMk64CpuStallOriginZ3DS[NUM_PLAYERS];
static bool sMk64CpuRecoveryLogged3DS[NUM_PLAYERS];
static bool sMk64CpuExcessForceLogged3DS[NUM_PLAYERS];
static u32 sMk64CpuRecoveryLogCount3DS;
static void Mk64CpuLeaderReset3DS(void) {
    sMk64LeaderRouteLength3DS = 0.0f;
    sMk64PaceSampleTime3DS = -1.0f;
    sMk64CpuRecoveryLogCount3DS = 0;
    for (s32 i=0; i<NUM_PLAYERS; ++i) {
        sMk64HumanPace3DS[i] = sMk64HumanProgress3DS[i] = 0.0f;
        sMk64HumanIdleSince3DS[i] = -1.0f;
        sMk64CpuPaceTime3DS[i] = sMk64CpuStalledSince3DS[i] = -1.0f;
        sMk64CpuPaceTarget3DS[i] = 0.0f;
        sMk64CpuPaceBase3DS[i] = 0.0f;
        sMk64CpuForcePace3DS[i] = 0.0f;
        sMk64CpuForceBlend3DS[i] = 0.0f;
        sMk64CpuRecoveryLogged3DS[i] = false;
        sMk64CpuExcessForceLogged3DS[i] = false;
    }
}
static bool Mk64CpuLeaderEligible3DS(s32 id) {
    return (Mk64NewItems3DSEnabled() || Mk64Auto3DSActive()) && id >= 0 && id < NUM_PLAYERS &&
        (gModeSelection == GRAND_PRIX || gModeSelection == VERSUS) &&
        !gDemoMode && !IsPodiumCeremony() &&
        (gPlayers[id].type & PLAYER_EXISTS) && (gPlayers[id].type & PLAYER_CPU) &&
        !(gPlayers[id].type & (PLAYER_HUMAN | PLAYER_CINEMATIC_MODE | PLAYER_START_SEQUENCE)) &&
        gLapCountByPlayerId[id] < 3;
}
bool Mk64CpuKeepCornerThrottle3DS(int playerId) {
    if (!Mk64CpuLeaderEligible3DS(playerId) || bStopAICrossing[playerId]) return false;
    const Player* player = &gPlayers[playerId];
    return !Mk64RacerHitMotion3DS(player) && !player->lakituProps &&
        !(player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER));
}
static f32 Mk64CpuRouteLength3DS(void) {
    if (sMk64LeaderRouteLength3DS > 0.0f) return sMk64LeaderRouteLength3DS;
    s32 count = gPathCountByPathIndex[0];
    if (!gTrackPaths[0] || count < 2) return 0.0f;
    for (s32 i=0; i<count; ++i) {
        TrackPathPoint* a = &gTrackPaths[0][i];
        TrackPathPoint* b = &gTrackPaths[0][(i+1)%count];
        f32 x=b->x-a->x, y=b->y-a->y, z=b->z-a->z;
        sMk64LeaderRouteLength3DS += sqrtf(x*x+y*y+z*z);
    }
    return sMk64LeaderRouteLength3DS;
}
static bool Mk64PaceHuman3DS(s32 id) {
    const bool driver = (gPlayers[id].type & PLAYER_HUMAN) || (Mk64Auto3DSActive() && id == 0);
    return driver && (gPlayers[id].type & PLAYER_EXISTS) &&
        !(gPlayers[id].type & (PLAYER_CINEMATIC_MODE | PLAYER_START_SEQUENCE | PLAYER_INVISIBLE_OR_BOMB)) &&
        gLapCountByPlayerId[id] < 3;
}
static void Mk64CpuSampleHumanPace3DS(f32 baseline) {
    f32 now = gCourseTimer;
    if (now == sMk64PaceSampleTime3DS) return;
    f32 dt = now - sMk64PaceSampleTime3DS;
    bool first = sMk64PaceSampleTime3DS < 0.0f || dt <= 0.0f || dt > 1.0f;
    for (s32 i=0; i<NUM_PLAYERS; ++i) {
        if (!Mk64PaceHuman3DS(i)) continue;
        Player* p = &gPlayers[i];
        f32 progress = gCourseCompletionPercentByPlayerId[i];
        if (first) {
            sMk64HumanPace3DS[i] = baseline;
            sMk64HumanIdleSince3DS[i] = -1.0f;
        } else if (!Mk64RacerHitMotion3DS(p) && !p->lakituProps) {
            // Sample cruising, not the slow acceleration out of a hit, water,
            // lightning or a sharp corner. Such losses must not drag the whole
            // field towards walking pace even after the tumble flag has ended.
            f32 speed = sqrtf(p->velocity[0]*p->velocity[0] + p->velocity[2]*p->velocity[2]);
            if (speed > baseline*1.3f) speed = baseline*1.3f;
            if (speed >= baseline*0.85f)
                sMk64HumanPace3DS[i] += (speed-sMk64HumanPace3DS[i]) * dt/(4.0f+dt);
            // Low speed alone is not inactivity: a player still making route
            // progress must not suddenly release every CPU's pace constraint.
            bool idle = speed < baseline*0.25f && !(progress > sMk64HumanProgress3DS[i]);
            if (!idle) sMk64HumanIdleSince3DS[i] = -1.0f;
            else if (sMk64HumanIdleSince3DS[i] < 0.0f) sMk64HumanIdleSince3DS[i] = now;
        } else {
            // Being knocked out/fished up is not deliberately refusing to race.
            sMk64HumanIdleSince3DS[i] = -1.0f;
        }
        sMk64HumanProgress3DS[i] = progress;
    }
    sMk64PaceSampleTime3DS = now;
}
// No extra braking penalty. Hit time must not count towards a stalled driver.
void Mk64CpuItemPenaltyTick3DS(int playerId, bool hit) {
    if (playerId >= 0 && playerId < NUM_PLAYERS && hit) sMk64CpuStalledSince3DS[playerId] = -1.0f;
}
static void Mk64CpuRecoverSteering3DS(s32 id, Player* player, f32 horizontal) {
    // Native apply_cpu_turn multiplies steering by a speed table whose first
    // entry is zero. A car facing a barrier can therefore never turn away at
    // rest. Only assist genuinely stalled, grounded cars; never erase a hit,
    // fish-up, crossing wait or change position/path progress/vertical motion.
    if (horizontal >= 0.8f || Mk64RacerHitMotion3DS(player) || player->lakituProps ||
        (player->effects & 8) || player->kartHopVelocity > 0.0f) {
        sMk64CpuStalledSince3DS[id] = -1.0f;
        sMk64CpuRecoveryLogged3DS[id] = false;
        return;
    }
    f32 movedX=player->pos[0]-sMk64CpuStallOriginX3DS[id], movedZ=player->pos[2]-sMk64CpuStallOriginZ3DS[id];
    if (sMk64CpuStalledSince3DS[id] < 0.0f ||
        movedX*movedX+movedZ*movedZ > player->boundingBoxSize*player->boundingBoxSize) {
        sMk64CpuStalledSince3DS[id] = gCourseTimer;
        sMk64CpuStallOriginX3DS[id] = player->pos[0];
        sMk64CpuStallOriginZ3DS[id] = player->pos[2];
    }
    if (gCourseTimer - sMk64CpuStalledSince3DS[id] < 1.0f) return;
    s32 path = gPathIndexByPlayerId[id];
    if (path < 0 || path >= 4 || !gTrackPaths[path]) return;
    s32 count = gPathCountByPathIndex[path], nearest = gNearestPathPointByPlayerId[id];
    if (count < 3 || nearest < 0 || nearest >= count) return;
    if (!sMk64CpuRecoveryLogged3DS[id] && sMk64CpuRecoveryLogCount3DS < 32) {
        // Buffered runtime log only (no race-time SD write); bounded per race.
        char detail[224];
        snprintf(detail, sizeof(detail), "id=%d time=%.2f path=%d point=%d speed=%.3f engine=%.2f force=%.2f effects=%08lx triggers=%08lx walls=%.2f/%.2f",
            (int)id, gCourseTimer, (int)path, (int)nearest, horizontal,
            player->currentSpeed, player->kartPropulsionStrength,
            (unsigned long)player->effects, (unsigned long)player->triggers,
            player->collision.surfaceDistance[0], player->collision.surfaceDistance[1]);
        Mk64Diagnostics3DSCpuDrive(detail);
        sMk64CpuRecoveryLogged3DS[id] = true;
        ++sMk64CpuRecoveryLogCount3DS;
    }
    TrackPathPoint* point = &gTrackPaths[path][(nearest+2)%count];
    Vec3f direction = { point->x-player->pos[0], 0.0f, point->z-player->pos[2] };
    // Project a target through a contacting wall onto its free side/tangent.
    // Native response subtracts normal*negative penetration: unk48/unk54
    // point towards the free side of the wall.
    for (s32 wall=0; wall<2; ++wall) {
        if (player->collision.surfaceDistance[wall] >= 0.0f) continue;
        f32* outward = wall ? player->collision.unk54 : player->collision.unk48;
        f32 nx=outward[0], nz=outward[2], length=nx*nx+nz*nz;
        if (length < 0.1f) continue;
        f32 into = direction[0]*nx + direction[2]*nz;
        if (into < 0.0f) {
            direction[0] -= into*nx/length;
            direction[2] -= into*nz/length;
        }
        direction[0] += nx*player->boundingBoxSize*2.0f;
        direction[2] += nz*player->boundingBoxSize*2.0f;
    }
    if (direction[0]*direction[0]+direction[2]*direction[2] < 1.0f) return;
    Vec3f target = {player->pos[0]+direction[0], player->pos[1], player->pos[2]+direction[2]};
    s16 turn = (s16)(-get_angle_between_two_vectors(player->pos, target)-player->rotation[1]);
    if (turn > 364) turn=364;
    if (turn < -364) turn=-364;
    // Use the same yaw increment consumed by native full movement. Do not
    // inject velocity or bypass collision. Moving AI immediately keeps its turn.
    player->unk_078 = turn;
}
bool Mk64CpuLeader3DSRegulate(int playerId, float targetSpeed) {
    if (!Mk64CpuLeaderEligible3DS(playerId)) return false;
    Player* player = &gPlayers[playerId];
    if (bStopAICrossing[playerId]) {
        sMk64CpuStalledSince3DS[playerId] = -1.0f;
        return false; // keep train/crossing safety
    }
    s32 cc = gCCSelection;
    if (cc < 0 || cc > 3) return false;
    f32 normal = CM_GetProps()->NormalTargetSpeed[cc];
    f32 curve = CM_GetProps()->CurveTargetSpeed[cc];
    f32 maximum = normal > curve ? normal : curve;
    if (!(normal > 0.0f) || !(maximum > 0.0f)) return false;
    f32 base = targetSpeed > 0.0f ? targetSpeed : normal;
    // Both visible/full and offscreen/cheap AI reach this controller. Native
    // 8.333 is emitted only by the cheap branch, so preserving it alone never
    // helped the followed car. Supply the selected rival's demand here too.
    const bool autoRace = Mk64Auto3DSActive();
    const bool roadDemand = base >= normal*0.9f;
    if (base > maximum) base = normal;
    bool selectedRival = autoRace && playerId == 0 && player->currentRank > 0 && roadDemand &&
        !Mk64RacerHitMotion3DS(player) && !player->lakituProps &&
        !(player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER));
    if (selectedRival && base < 8.333333f) base = 8.333333f;
    Mk64CpuSampleHumanPace3DS(normal);
    s32 anchor = -1;
    for (s32 i=0; i<NUM_PLAYERS; ++i) {
        if (!Mk64PaceHuman3DS(i) ||
            !(gCourseCompletionPercentByPlayerId[i] >= -1.0f && gCourseCompletionPercentByPlayerId[i] <= 4.0f) ||
            (sMk64HumanIdleSince3DS[i] >= 0.0f && gCourseTimer-sMk64HumanIdleSince3DS[i] >= 8.0f)) continue;
        if (anchor < 0 || gCourseCompletionPercentByPlayerId[i] > gCourseCompletionPercentByPlayerId[anchor]) anchor=i;
    }
    f32 desired = base;
    f32 gap = 0.0f;
    sMk64CpuForceBlend3DS[playerId] = 0.0f;
    if (anchor >= 0 && anchor != playerId) {
        gap = (gCourseCompletionPercentByPlayerId[playerId]-gCourseCompletionPercentByPlayerId[anchor]) * Mk64CpuRouteLength3DS();
        s32 rank = player->currentRank;
        if (rank < 0) rank=0;
        if (rank >= NUM_PLAYERS) rank=NUM_PLAYERS-1;
        f32 corridor = 100.0f-40.0f*rank;
        if (gap > corridor-80.0f && gap < 3.0f*sMk64LeaderRouteLength3DS) {
            f32 blend = (gap-corridor+80.0f)/80.0f;
            if (blend > 1.0f) blend=1.0f;
            blend = blend*blend*(3.0f-2.0f*blend);
            sMk64CpuForceBlend3DS[playerId] = blend;
            // Every ahead CPU shares one human anchor (no 1st/2nd relay), but
            // never wait at half speed for a damaged/slow player. At most 10%
            // below sampled healthy cruise, and 16% below native road demand.
            // Item losses remain real; skill and boxes still decide recovery.
            // Do not herd all seven cars onto one positive lead: then even
            // the last CPU keeps pulling ahead of a recovering human. A loose
            // rank-relative corridor straddles the human, with identical speed
            // floors/ceilings for everyone (not the native main-rival bonus).
            f32 following = sMk64HumanPace3DS[anchor] + (corridor-gap)*(normal/500.0f);
            f32 floor = sMk64HumanPace3DS[anchor]*0.90f;
            if (floor < base*0.84f) floor=base*0.84f;
            if (following < floor) following=floor;
            if (following > base) following=base;
            desired += (following-base)*blend;
        }
    }
    f32 dt = gCourseTimer - sMk64CpuPaceTime3DS[playerId];
    if (sMk64CpuPaceTime3DS[playerId] < 0.0f || dt < 0.0f || dt > 1.0f) {
        sMk64CpuPaceTarget3DS[playerId] = base;
        sMk64CpuForcePace3DS[playerId] = base*1.3f;
        dt = 0.0f;
    } else if (sMk64CpuPaceBase3DS[playerId] > 0.0f) {
        // Smooth the pack restriction, NOT the underlying road/curve demand.
        // Otherwise leaving a slow/off-track target or resuming the selected
        // rival assist takes many seconds after a hit. Native acceleration
        // still restores the engine; no speed or force is injected here.
        f32 ratio = base / sMk64CpuPaceBase3DS[playerId];
        sMk64CpuPaceTarget3DS[playerId] *= ratio;
        sMk64CpuForcePace3DS[playerId] *= ratio;
    }
    sMk64CpuPaceBase3DS[playerId] = base;
    sMk64CpuPaceTime3DS[playerId] = gCourseTimer;
    f32 change = desired-sMk64CpuPaceTarget3DS[playerId], step=normal*0.04f*dt;
    if (change > step) change=step;
    if (change < -step) change=-step;
    sMk64CpuPaceTarget3DS[playerId] += change;
    desired = sMk64CpuPaceTarget3DS[playerId];
    // Native mushroom/ramp effects rewrite currentSpeed AFTER this controller.
    // Bound the final force instead of trying to overwrite their timers/speed.
    // Real boosts retain headroom nearby or behind, fading out when far ahead.
    f32 boostBlend = (gap-100.0f)/200.0f;
    if (boostBlend < 0.0f) boostBlend=0.0f;
    if (boostBlend > 1.0f) boostBlend=1.0f;
    boostBlend=boostBlend*boostBlend*(3.0f-2.0f*boostBlend);
    f32 boostPace = desired+(base*1.3f-desired)*(1.0f-boostBlend);
    f32 forceChange=boostPace-sMk64CpuForcePace3DS[playerId], forceStep=normal*0.08f*dt;
    if (forceChange > forceStep) forceChange=forceStep;
    if (forceChange < -forceStep) forceChange=-forceStep;
    sMk64CpuForcePace3DS[playerId] += forceChange;
    // The native engine/topSpeed clamp otherwise makes an 8.333 demand almost
    // indistinguishable from regular cruise. Reuse its bounded 380-force rival
    // assist, not a velocity multiplier or a modified character topSpeed.
    if (selectedRival) player->effects |= 0x200000;
    else player->effects &= ~0x200000;
    f32 horizontal = sqrtf(player->velocity[0]*player->velocity[0] + player->velocity[2]*player->velocity[2]);
    Mk64CpuRecoverSteering3DS(playerId, player, horizontal);
    // Let the native force function decay residual CPU boost; do not cut it
    // instantaneously on one tick. Never create a new rival/rank speed bonus.
    if (player->effects & (BOOST_EFFECT | STAR_EFFECT)) {
        player_accelerate_alternative(player);
        return true; // final native force applies the same soft pack envelope
    }
    // World velocity and currentSpeed are different units. Native propulsion
    // is currentSpeed^2/25, with asphalt drag 0.12*kartFriction. Approach the
    // corresponding engine demand, instead of braking the engine to zero
    // while inertia still reports high world speed. Do not compensate terrain
    // losses: native per-tyre/turn/water/lightning penalties remain intact.
    f32 engine = sqrtf(desired * (0.12f*player->kartFriction) * 25.0f);
    if (engine > player->topSpeed) engine=player->topSpeed;
    if (player->currentSpeed > engine) {
        f32 lift=player->currentSpeed-engine;
        if (lift > 0.35f) lift=0.35f;
        player_decelerate_alternative(player, lift);
    } else {
        player_accelerate_alternative(player);
        // Acceleration still uses native character tables; trim only its small
        // overshoot and refresh the force every tick, including at cruise.
        if (player->currentSpeed > engine) player_decelerate_alternative(player, player->currentSpeed-engine);
        player->kartProps |= THROTTLE;
    }
    return true; // bypass rank-distance withholding and FAST/MAX/SLOW overrides
}

float Mk64CpuDrive3DSLimitForce(void* racer, float nativeForce) {
    Player* p=(Player*)racer;
    s32 id=p-gPlayerOne;
    if (!Mk64CpuLeaderEligible3DS(id) || bStopAICrossing[id] ||
        Mk64RacerHitMotion3DS(p) || p->lakituProps ||
        (p->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER)) ||
        sMk64CpuPaceTime3DS[id] < 0.0f ||
        gCourseTimer-sMk64CpuPaceTime3DS[id] < 0.0f ||
        gCourseTimer-sMk64CpuPaceTime3DS[id] > 0.15f) return nativeForce;
    bool boost=p->effects & (BOOST_EFFECT | STAR_EFFECT | BOOST_RAMP_ASPHALT_EFFECT | BOOST_RAMP_WOOD_EFFECT);
    f32 pace=boost ? sMk64CpuForcePace3DS[id] : sMk64CpuPaceTarget3DS[id];
    f32 ceiling=pace*(0.12f*p->kartFriction);
    if (sMk64CpuForceBlend3DS[id] > 0.9f && nativeForce > ceiling*1.65f &&
        !sMk64CpuExcessForceLogged3DS[id]) {
        char detail[224];
        snprintf(detail, sizeof(detail), "force-envelope id=%d time=%.2f speed=%.2f pace=%.2f native=%.1f ceiling=%.1f effects=%08lx triggers=%08lx engine=%.1f extra=%.1f/%.1f boost=%.1f tyres=%u/%u/%u/%u",
            (int)id,gCourseTimer,p->speed,pace,nativeForce,ceiling,
            (unsigned long)p->effects,(unsigned long)p->triggers,p->currentSpeed,
            p->unk_0E8,p->unk_0E4,p->boostPower,
            (unsigned)p->tyres[0].surfaceType,(unsigned)p->tyres[1].surfaceType,
            (unsigned)p->tyres[2].surfaceType,(unsigned)p->tyres[3].surfaceType);
        Mk64Diagnostics3DSCpuDrive(detail);
        sMk64CpuExcessForceLogged3DS[id]=true;
    }
    // Only remove excess forward force. Never add terrain compensation, touch
    // world velocity/height, cancel a hit or change a human/OFF force result.
    return ceiling > 0.0f && nativeForce > ceiling
        ? nativeForce+(ceiling-nativeForce)*sMk64CpuForceBlend3DS[id] : nativeForce;
}
