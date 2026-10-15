// Live collidable World objects, not HUD/decorative object slots. Lane steering
// only: never brake, move a racer, disable collision or change item penalties.
#include "engine/objects/Penguin.h"
#include "engine/objects/Thwomp.h"
#include "engine/objects/Mole.h"
#include "cpu_hit_state_3ds.inc.h"
static bool sMk64ObstacleStepActive3DS[NUM_PLAYERS];
static float sMk64ObstacleSavedStep3DS[NUM_PLAYERS];
static void Mk64CpuObstacleStep3DS(int id, bool avoiding) {
    auto& instruction=gPlayerTrackPositionFactorInstruction[id];
    if (avoiding) {
        if (!sMk64ObstacleStepActive3DS[id]) sMk64ObstacleSavedStep3DS[id]=instruction.step;
        sMk64ObstacleStepActive3DS[id]=true;
        if (instruction.step < .04f) instruction.step=.04f;
    } else if (sMk64ObstacleStepActive3DS[id]) {
        instruction.step=sMk64ObstacleSavedStep3DS[id];
        sMk64ObstacleStepActive3DS[id]=false;
    }
}
static bool Mk64CpuObstacleEligible3DS(int id) {
    if (id < 0 || id >= NUM_PLAYERS ||
        !(Mk64NewItems3DSEnabled() || Mk64Auto3DSActive()) || gDemoMode || IsPodiumCeremony() ||
        (gModeSelection != GRAND_PRIX && gModeSelection != VERSUS)) return false;
    const Player* player = &gPlayers[id];
    return (player->type & (PLAYER_EXISTS | PLAYER_CPU)) == (PLAYER_EXISTS | PLAYER_CPU) &&
        !(player->type & (PLAYER_HUMAN | PLAYER_START_SEQUENCE | PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
        !(player->effects & BOO_EFFECT) && !Mk64RacerHitMotion3DS(player) &&
        !(player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER)) && !player->lakituProps &&
        !bStopAICrossing[id] && gLapCountByPlayerId[id] < 3;
}
static bool Mk64CpuObstacle3DSAvoid(int id, bool wasAvoiding) {
    if (!Mk64CpuObstacleEligible3DS(id)) return false;
    const int path = gPathIndexByPlayerId[id];
    if (path < 0 || path >= 4) return false;
    const int count = gPathCountByPathIndex[path], point = gNearestPathPointByPlayerId[id];
    if (count < 3 || point >= count || !gTrackPaths[path] ||
        !gTrackLeftPaths[path] || !gTrackRightPaths[path]) return false;
    World* world = GetWorld();
    if (!world) return false;
    struct Hazard { float factor, halfWidth, radius; } hazards[32];
    int used = 0;
    const Player* player = &gPlayers[id];
    for (const auto& entry : world->Objects) {
        if (!entry || entry->bPendingDestroy || entry->_objectIndex < 0 || entry->_objectIndex >= OBJECT_LIST_SIZE) continue;
        if (!dynamic_cast<const OPenguin*>(entry.get()) && !dynamic_cast<const OThwomp*>(entry.get()) &&
            !dynamic_cast<const OMole*>(entry.get())) continue;
        const Object& object = gObjectList[entry->_objectIndex];
        if (!object.state || !(object.status & 0x200) || fabsf(object.pos[1]-player->pos[1]) > 20.0f) continue;
        float dx=object.pos[0]-player->pos[0], dz=object.pos[2]-player->pos[2];
        if (dx*dx+dz*dz > 280.0f*280.0f) continue;
        // Brief bounded prediction for sliding penguins; no long straight-line
        // extrapolation of their curved/circular path.
        float vx=object.velocity[0]*8.0f, vz=object.velocity[2]*8.0f;
        if (vx > 15) vx=15; if (vx < -15) vx=-15;
        if (vz > 15) vz=15; if (vz < -15) vz=-15;
        float x=object.pos[0]+vx, z=object.pos[2]+vz;
        int nearest=point, ahead=0; float best=1.0e20f;
        const int limit=count < 33 ? count-1 : 32;
        for (int step=0; step<=limit; ++step) {
            int index=(point+step)%count;
            const auto& p=gTrackPaths[path][index];
            float px=x-p.x, pz=z-p.z;
            float d=px*px+pz*pz;
            if (d<best) {best=d;nearest=index;ahead=step;}
        }
        const auto& prev=gTrackPaths[path][(point+count-1)%count];
        const auto& next=gTrackPaths[path][(point+1)%count];
        if (!ahead && dx*(next.x-prev.x)+dz*(next.z-prev.z) < -10.0f) continue;
        const auto& left=gTrackLeftPaths[path][nearest];
        const auto& right=gTrackRightPaths[path][nearest];
        float wx=right.x-left.x, wz=right.z-left.z, width2=wx*wx+wz*wz;
        if (width2 < 4.0f || fabsf(object.pos[1]-gTrackPaths[path][nearest].y) > 20.0f) continue;
        float factor=2.0f*((x-left.x)*wx+(z-left.z)*wz)/width2-1.0f;
        float half=0.5f*sqrtf(width2);
        float radius=player->boundingBoxSize+object.boundingBoxSize+10.0f;
        // Outside the local road segment or on a different bend/level.
        float lx=left.x+(factor+1)*wx*.5f, lz=left.z+(factor+1)*wz*.5f;
        if (fabsf(factor)>1.2f || (x-lx)*(x-lx)+(z-lz)*(z-lz)>80.0f*80.0f) continue;
        hazards[used++]={factor,half,radius};
        if (used==32) break;
    }
    if (!used) return false;
    float baseline=wasAvoiding ? gPlayerTrackPositionFactorInstruction[id].unkC :
        gPlayerTrackPositionFactorInstruction[id].target;
    if (baseline < -.8f) baseline=-.8f; if (baseline > .8f) baseline=.8f;
    bool blocked=false;
    for (int i=0;i<used;++i)
        if (fabsf(baseline-hazards[i].factor)*hazards[i].halfWidth < hazards[i].radius) blocked=true;
    if (!blocked) return false;
    float separation=CM_GetProps()->AIMinimumSeparation;
    if (separation < .3f) separation=.3f; if (separation > .8f) separation=.8f;
    float lanes[6]={baseline,-separation,0,separation,-.8f,.8f};
    float bestScore=1.0e20f, goal=baseline;
    for (float lane : lanes) {
        float danger=0;
        for (int i=0;i<used;++i) {
            float overlap=hazards[i].radius-fabsf(lane-hazards[i].factor)*hazards[i].halfWidth;
            if (overlap>0) danger+=overlap;
        }
        // Clearance dominates; stick to a safe previous side instead of
        // alternating across the obstacle every update. Native lane changes
        // provide the smooth motion; this writes no speed/force/position.
        float score=danger*100.0f+fabsf(lane-gTrackPositionFactor[id])+
            .6f*fabsf(lane-gPlayerTrackPositionFactorInstruction[id].target);
        if (score<bestScore) {bestScore=score;goal=lane;}
    }
    gPlayerTrackPositionFactorInstruction[id].target=goal;
    return true;
}
