// Ground-travelling MK64 shells: road-centre traversal, then a local attack.
// No heap allocations/sidecar cache. unk_16 is unused by the legacy red/blue
// actor; it holds the path+1 only while launched. shellId becomes a lifetime
// after the triple parent has detached it, as in the original moving state.
#include <stdlib.h>
#include "item_rules_3ds.h"
#include "cpu_item_target_3ds.inc.h"
#include "threat_warning_3ds.inc.h"
// Positive unk_16 packs path+1 in the low byte, warning in bit 8 and pursuit in bit 9.
// Constructors reset it; backwards (-1) and OFF actors never use the bit.
static void Mk64RoadShellWarning3DS(struct ShellActor* shell) {
    if (!Mk64NewItems3DSEnabled() ||
        (shell->type != ACTOR_BLUE_SPINY_SHELL && shell->type != ACTOR_RED_SHELL)) return;
    bool locked = false;
    if (shell->unk_16 > 0 &&
        (shell->state == BLUE_SHELL_LOCK_ON || shell->state == GREEN_SHELL_HIT_A_RACER)) {
        if (shell->type == ACTOR_BLUE_SPINY_SHELL) {
            // Passing blue shells threaten every racer behind whom they travel,
            // not only the current leader. Audio retains positional attenuation.
            for (s32 id = 0; id < NUM_PLAYERS; ++id)
                if (id != shell->playerId && Mk64ThreatBehind3DS(id, shell->pos, 0.0f, 0.0f)) {
                    locked = true;
                    break;
                }
        } else {
            locked = Mk64ThreatBehind3DS(shell->targetPlayer, shell->pos, kMk64ThreatRedRange3DS, 0.0f);
        }
    }
    bool sounding = shell->unk_16 >= 0 && (shell->unk_16 & 0x100);
    if (locked && !sounding) {
        func_800C9D80(shell->pos, shell->velocity, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
        shell->unk_16 |= 0x100;
    } else if (!locked && sounding) {
        func_800C9EF4(shell->pos, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
        shell->unk_16 &= ~0x100;
    }
}
void Mk64ShellWarning3DSStop(void* actor) {
    struct ShellActor* shell = (struct ShellActor*)actor;
    if (Mk64NewItems3DSEnabled() &&
        (shell->type == ACTOR_RED_SHELL || shell->type == ACTOR_BLUE_SPINY_SHELL) &&
        shell->unk_16 > 0 && (shell->unk_16 & 0x100)) {
        func_800C9EF4(shell->pos, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
        shell->unk_16 &= ~0x100;
    }
}
static bool Mk64RoadShellVictim3DS(s32 id, s32 owner) {
    return id >= 0 && id < NUM_PLAYERS && id != owner &&
        (gPlayers[id].type & PLAYER_EXISTS) &&
        !(gPlayers[id].type & (PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
        gLapCountByPlayerId[id] < 3 && !(gPlayers[id].effects & BOO_EFFECT);
}
static s32 Mk64RoadShellTarget3DS(struct ShellActor* shell) {
    s32 owner = shell->playerId;
    if (owner < 0 || owner >= NUM_PLAYERS) return -1;
    if (shell->type == ACTOR_BLUE_SPINY_SHELL &&
        (gPlayers[owner].type & PLAYER_CPU) && !(gPlayers[owner].type & PLAYER_HUMAN)) {
        // Blue still favours the leading pair. Red must NOT skip the racer ahead.
        for (s32 rank = 0; rank < 2; ++rank) {
            s32 id = gPlayerPositionLUT[rank];
            if (Mk64RoadShellVictim3DS(id, owner)) return id;
        }
        return -1;
    }
    s32 end = shell->type == ACTOR_BLUE_SPINY_SHELL ? NUM_PLAYERS : gPlayers[owner].currentRank;
    if (end > NUM_PLAYERS) end = NUM_PLAYERS;
    // Red targets the next eligible racer ahead; blue targets the leader.
    for (s32 rank = shell->type == ACTOR_BLUE_SPINY_SHELL ? 0 : end - 1;
         rank >= 0 && rank < end; rank += shell->type == ACTOR_BLUE_SPINY_SHELL ? 1 : -1) {
        s32 id = gPlayerPositionLUT[rank];
        if (Mk64RoadShellVictim3DS(id, owner)) return id;
    }
    return -1;
}

static s32 Mk64RoadShellNearest3DS(struct ShellActor* shell, s32 path) {
    f32 best = 1.0e30f;
    s32 point = 0;
    s32 count = gPathCountByPathIndex[path];
    s32 ownerPath = gPathIndexByPlayerId[shell->playerId];
    s32 ownerPoint = gNearestPathPointByPlayerId[shell->playerId];
    for (s32 i = 0; i < count; ++i) {
        if (ownerPath == path) {
            s32 gap = abs(i-ownerPoint);
            if (gap > count/2) gap = count-gap;
            if (gap > 8) continue; // Never join the opposite side of a hairpin.
        }
        TrackPathPoint* p = &gTrackPaths[path][i];
        f32 x = p->x - shell->pos[0], y = p->y + shell->boundingBoxSize - shell->pos[1];
        f32 z = p->z - shell->pos[2];
        // Height distinguishes crossing roads/bridges.
        f32 distance = x*x + y*y + z*z;
        if (distance < best) { best = distance; point = i; }
    }
    return point;
}

static bool Mk64UpdateRoadShell3DS(struct ShellActor* shell) {
    if (!Mk64NewItems3DSEnabled()) return false;
    if (gModeSelection == BATTLE || shell->state == HELD_SHELL ||
        shell->state == RELEASED_SHELL || shell->state == TRIPLE_RED_SHELL ||
        shell->state == DESTROYED_SHELL) return false;
    if (shell->playerId < 0 || shell->playerId >= NUM_PLAYERS) {
        destroy_destructable_actor((struct Actor*)shell); return true;
    }
    if (shell->state == MOVING_SHELL && shell->unk_16 == 0) {
        Vec3f axis = {0,0,1};
        Player* owner = &gPlayers[shell->playerId];
        func_802B64C4(axis, owner->rotation[1] + owner->unk_0C0);
        // Preserve the deliberate backwards throw and its ordinary collision.
        if (shell->velocity[0]*axis[0] + shell->velocity[2]*axis[2] < 0) {
            shell->unk_16 = -1; return false;
        }
        s32 target = Mk64RoadShellTarget3DS(shell);
        s32 path = gPathIndexByPlayerId[target >= 0 ? target : shell->playerId];
        if (path < 0 || path >= 4 || !gTrackPaths[path] || gPathCountByPathIndex[path] < 2) {
            path = gPathIndexByPlayerId[shell->playerId];
        }
        if (path < 0 || path >= 4 || !gTrackPaths[path] || gPathCountByPathIndex[path] < 2) {
            destroy_destructable_actor((struct Actor*)shell); return true;
        }
        s32 nearest = Mk64RoadShellNearest3DS(shell, path);
        TrackPathPoint* join = &gTrackPaths[path][nearest];
        f32 joinX=join->x-shell->pos[0], joinY=join->y+shell->boundingBoxSize-shell->pos[1], joinZ=join->z-shell->pos[2];
        if (path != gPathIndexByPlayerId[shell->playerId] &&
            joinX*joinX+joinY*joinY+joinZ*joinZ > 50.0f*50.0f) {
            // A target on a different canyon branch is not a licence to cut
            // across the canyon. Keep this route until roads actually meet.
            path=gPathIndexByPlayerId[shell->playerId];
            if (path>=4 || !gTrackPaths[path] || gPathCountByPathIndex[path]<2) {
                destroy_destructable_actor((struct Actor*)shell); return true;
            }
            nearest=Mk64RoadShellNearest3DS(shell,path);
        }
        shell->unk_16 = path + 1;
        shell->pathIndex = nearest;
        shell->targetPlayer = target;
        shell->shellId = 900.0f; // 30 seconds at MK64's logic rate; bounded occupancy.
        shell->state = shell->type == ACTOR_BLUE_SPINY_SHELL ? BLUE_SHELL_LOCK_ON : GREEN_SHELL_HIT_A_RACER;
        bool registered = false;
        s32 actorIndex = CM_FindActorIndex(shell);
        for (s32 i=0;i<NUM_PLAYERS;++i) {
            if (gUnexpiredActorsList[i].unkC && gUnexpiredActorsList[i].actorIndex == actorIndex) registered = true;
        }
        if (shell->type == ACTOR_BLUE_SPINY_SHELL) {
            if (!registered) add_blue_shell_in_unexpired_actor_list(actorIndex);
        } else if (!registered) add_red_shell_in_unexpired_actor_list(actorIndex);
    }
    if (shell->unk_16 <= 0) return false;
    s32 path = (shell->unk_16 & 0xFF) - 1;
    if (path < 0 || path >= 4 || !gTrackPaths[path] || gPathCountByPathIndex[path] < 2 ||
        shell->pathIndex >= gPathCountByPathIndex[path] || --shell->shellId <= 0) {
        destroy_destructable_actor((struct Actor*)shell); return true;
    }
    if (shell->shellId <= 870.0f) shell->flags &= ~0x1000; // Original owner grace.
    shell->rotVelocity += 0x71C;
    bool pursuing = (shell->unk_16 & 0x200) != 0;
    // Red keeps its selected racer through the local dive. Blue always
    // rechecks the live lead: overtaking its old target must not divert it.
    s32 target = pursuing && shell->type == ACTOR_RED_SHELL &&
        Mk64RoadShellVictim3DS(shell->targetPlayer, shell->playerId)
        ? shell->targetPlayer : Mk64RoadShellTarget3DS(shell);
    if (target != shell->targetPlayer || target < 0) {
        shell->unk_16 &= ~0x200;
        pursuing = false;
    }
    shell->targetPlayer = target;
    Mk64RoadShellWarning3DS(shell);
    Vec3f old = {shell->pos[0], shell->pos[1], shell->pos[2]};
    bool attack = false;
    f32 travelSpeed = shell->type == ACTOR_BLUE_SPINY_SHELL ? 12.0f : 8.0f;
    if (target >= 0) {
        Player* victim = &gPlayers[target];
        f32 moving = sqrtf(victim->velocity[0]*victim->velocity[0] + victim->velocity[2]*victim->velocity[2]);
        if (victim->speed > moving) moving = victim->speed;
        // The old 16-unit cap could make a shell slower than a boosted racer.
        f32 catchSpeed = moving*1.25f + 2.0f;
        if (catchSpeed > travelSpeed) travelSpeed = catchSpeed;
        if (travelSpeed > 32.0f) travelSpeed = 32.0f;
    }
    if (target >= 0 && gPathIndexByPlayerId[target] == path) {
        Player* victim = &gPlayers[target];
        f32 x = victim->pos[0]-old[0], y = victim->pos[1]-old[1], z = victim->pos[2]-old[2];
        s32 gap = abs((s32)gNearestPathPointByPlayerId[target] - (s32)shell->pathIndex);
        s32 count = gPathCountByPathIndex[path];
        if (gap > count/2) gap = count-gap;
        // Only dive locally on the same stretch of road, never across a
        // hairpin, an overpass or the wall between adjacent track sections.
        attack = (pursuing || x*x + z*z < 70.0f*70.0f) && y*y < 25.0f*25.0f && gap <= 4;
        if (attack) {
            shell->unk_16 |= 0x200; // Distance threshold starts pursuit, never cancels it.
            f32 ground = victim->pos[1] - victim->boundingBoxSize + shell->boundingBoxSize;
            y = ground-old[1];
            f32 separation = sqrtf(x*x + z*z);
            f32 lead = separation / 40.0f;
            if (lead > 2.0f) lead = 2.0f;
            x += victim->velocity[0]*lead;
            z += victim->velocity[2]*lead;
            f32 distance = sqrtf(x*x + y*y + z*z);
            f32 scale = distance > travelSpeed ? travelSpeed/distance : 1.0f;
            shell->pos[0] += x*scale; shell->pos[1] += y*scale; shell->pos[2] += z*scale;
            // Pursuit used to freeze pathIndex. On a moving target it then
            // failed the local-road guard and steered back to an old waypoint.
            // Keep a bounded local road cursor at the shell's actual position.
            f32 best = 1.0e30f;
            s32 nearest = shell->pathIndex;
            for (s32 offset=-8; offset<=8; ++offset) {
                s32 index = ((s32)shell->pathIndex + offset + count*8) % count;
                TrackPathPoint* point = &gTrackPaths[path][index];
                f32 dx=point->x-shell->pos[0], dy=point->y+shell->boundingBoxSize-shell->pos[1];
                f32 dz=point->z-shell->pos[2];
                f32 distance2=dx*dx+dy*dy+dz*dz;
                if (distance2 < best) { best=distance2; nearest=index; }
            }
            shell->pathIndex=(nearest+1)%count;
        }
    }
    if (!attack) {
        // Spend one movement budget along consecutive centre-line points;
        // corner turns never become a long straight line through a wall.
        f32 budget = travelSpeed;
        for (s32 steps=0; steps<16 && budget>0.0f; ++steps) {
            TrackPathPoint* point = &gTrackPaths[path][shell->pathIndex];
            f32 x=point->x-shell->pos[0], y=point->y+shell->boundingBoxSize-shell->pos[1];
            f32 z=point->z-shell->pos[2];
            f32 distance=sqrtf(x*x+y*y+z*z);
            if (distance <= budget) {
                shell->pos[0]=point->x; shell->pos[1]=point->y+shell->boundingBoxSize; shell->pos[2]=point->z;
                budget -= distance;
                shell->pathIndex=(shell->pathIndex+1)%gPathCountByPathIndex[path];
                if (target >= 0) {
                    s32 nextPath=gPathIndexByPlayerId[target];
                    if (nextPath!=path && nextPath<4 && gTrackPaths[nextPath] &&
                        gPathCountByPathIndex[nextPath]>=2) {
                        // Only switch routes at an actual shared centre-line
                        // point. Scan on point arrival, never per-frame/heap.
                        for (s32 join=0;join<gPathCountByPathIndex[nextPath];++join) {
                            TrackPathPoint* other=&gTrackPaths[nextPath][join];
                            if (other->trackSectionId!=point->trackSectionId) continue;
                            f32 dx=other->x-point->x,dy=other->y-point->y,dz=other->z-point->z;
                            if (dx*dx+dy*dy+dz*dz>16.0f) continue;
                            path=nextPath;shell->unk_16=(shell->unk_16 & 0x300) | (path+1);
                            shell->pathIndex=(join+1)%gPathCountByPathIndex[path];
                            break;
                        }
                    }
                }
            } else {
                f32 scale=budget/distance;
                shell->pos[0]+=x*scale; shell->pos[1]+=y*scale; shell->pos[2]+=z*scale;
                break;
            }
        }
    }
    for (s32 i=0;i<3;++i) shell->velocity[i]=shell->pos[i]-old[i];
    // Actor-vs-kart/item collisions still run in the normal world pass.
    // Terrain does not kill a shell following known road-centre geometry.
    return true;
}
