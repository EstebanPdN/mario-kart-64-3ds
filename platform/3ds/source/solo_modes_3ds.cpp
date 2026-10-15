#include "solo_modes_3ds.h"
#include "arena_navigation_3ds.hpp"
#include "item_rules_3ds.h"
#include "auto_mode_3ds.h"
#include <cstdio>
#include <libultraship.h>
#include "port/Game.h"
extern "C" {
#include "main.h"
#include "menus.h"
#include "spawn_players.h"
#include "code_800029B0.h"
#include "code_80005FD0.h"
#include "player_controller.h"
#include "render_player.h"
#include "camera.h"
#include "racing/race_logic.h"
#include "racing/math_util.h"
#include "racing/actors.h"
#include "code_80057C60.h"
}
namespace {
signed char sSoloLevels[4] = {};
constexpr int kSoloModes[4] = { GRAND_PRIX, TIME_TRIALS, VERSUS, BATTLE };
ArenaNavigation3DS::Graph sArena;
struct Bot {
    ArenaNavigation3DS::Point target{}, objective{}, last{}, recoveryTarget{};
    float nextPlan=0, nextSteer=0, lastSteer=-1, lastItemTime=-1;
    float stuckSince=0, escapeUntil=0, fightSince=-1, repositionUntil=0;
    float bestRemaining=1e30f, progressUntil=0, avoidBoxUntil=0;
    float reverseUntil=0;
    float motionAt=-1, recoveryUntil=0, nextRecovery=0;
    ArenaNavigation3DS::Point motionPosition{}, motionTarget{};
    int recoveryAttempt=0;
    uint8_t driveReason=0, facingFailure=0, momentumFailure=0;
    bool edgeRisk=false;
    bool routeBlocked=false;
    bool recoveringEdge=false;
    bool controlSuspended=false;
    int wander=0, goal=-1, box=-1, roamGoal=-1, opponent=-1, avoidBox=-1;
};
Bot sBots[4];
struct BattleSample {
    float time=0, speed=0, stalled=0;
    ArenaNavigation3DS::Point position{}, velocity{}, target{};
    uint32_t type=0, effects=0, lakitu=0;
    int16_t yaw=0;
    uint8_t reason=0, facing=0, momentum=0, recovering=0;
};
struct BattleTrace { std::array<BattleSample,64> samples{}; unsigned next=0, count=0; float time=-1; };
BattleTrace sTrace[4];
bool sBattleFarView=false;
int sBattleObserved=-1;
// No file I/O during gameplay. The paused Developer dump exports this bounded
// history, including early native hit/rescue returns that RAM arenas omit.
struct TraceOnReturn {
    int id;
    ~TraceOnReturn() {
        auto& trace=sTrace[id];
        if(trace.time>=0 && gCourseTimer>=trace.time && gCourseTimer-trace.time<.1f)return;
        const auto& p=gPlayers[id];const auto& bot=sBots[id];
        auto& sample=trace.samples[trace.next];
        sample={gCourseTimer,p.speed,bot.stuckSince,
            {p.pos[0],p.pos[1],p.pos[2]},{p.velocity[0],p.velocity[1],p.velocity[2]},bot.target,
            uint32_t(p.type),uint32_t(p.effects),uint32_t(p.lakituProps),p.rotation[1],
            bot.driveReason,bot.facingFailure,bot.momentumFailure,uint8_t(bot.recoveringEdge)};
        trace.next=(trace.next+1)%trace.samples.size();trace.count=std::min(trace.count+1,64u);trace.time=gCourseTimer;
    }
};
bool sArenaReady=false;
ArenaNavigation3DS::Point Position(const float* p) { return {p[0],p[1],p[2]}; }
void BrakeAtEdge(Player& p) {
    player_decelerate_alternative(&p,8.f);
    p.velocity[0]*=.75f;p.velocity[2]*=.75f;
}
int Reposition(int start,ArenaNavigation3DS::Point pos,int id) {
    int goal=-1;float best=-1e30f;
    for(int node=0;node<sArena.count;++node) {
        const auto point=sArena.nodes[node].center;
        const float travel=ArenaNavigation3DS::Distance2(pos,point);
        if(travel<220*220 || travel>900*900 || !sArena.Reachable(start,node))continue;
        float separation=600*600;
        for(int other=0;other<4;++other)if(other!=id && (gPlayers[other].type&PLAYER_EXISTS) &&
            gPlayerBalloonCount[other]>=0)
            separation=std::min(separation,ArenaNavigation3DS::Distance2(point,Position(gPlayers[other].pos)));
        // Spread among distinct sectors, instead of selecting the same nearby
        // triangle and immediately rejoining a two-kart circular dogfight.
        const float sector=(id==1?point.x:id==2?point.z:-point.x)*80;
        const float score=separation-travel*.25f+sector;
        if(score>best){best=score;goal=node;}
    }
    return goal;
}
void BuildArena() {
    sArena.Reset();
    for (unsigned i=0;i<gCollisionMeshCount;++i) {
        const auto& t=gCollisionMesh[i];
        // flags' low byte is a section ID, not a walkability mask. All four
        // original arenas use section 0xFF, which the former 0x18 test rejected.
        if (t.surfaceType == OUT_OF_BOUNDS || !t.vtx1 || !t.vtx2 || !t.vtx3) continue;
        auto point=[](const Vtx* v) -> ArenaNavigation3DS::Point {
            return {float(v->v.ob[0]),float(v->v.ob[1]),float(v->v.ob[2])};
        };
        if(t.normalY>=.5f) sArena.Add(point(t.vtx1),point(t.vtx2),point(t.vtx3));
        else if(std::fabs(t.normalY)<.5f) sArena.AddWall(point(t.vtx1),point(t.vtx2),point(t.vtx3));
    }
    sArena.Connect();
    sArenaReady=true;
}
}
extern "C" bool Mk64SoloVs3DS() {
    return !gDemoMode && gPlayerCount == 1 && gModeSelection == VERSUS;
}
extern "C" bool Mk64SoloBattle3DS() {
    return !gDemoMode && gPlayerCount == 1 && gModeSelection == BATTLE;
}
extern "C" int Mk64SoloMode3DS(int players, int mode) {
    if (players == 0) return kSoloModes[mode >= 0 && mode < 4 ? mode : 0];
    return gGameModePlayerSelection[players][mode];
}
extern "C" signed char* Mk64SoloSubMode3DS(int players, int mode) {
    // Keep the original GP/TT storage visible to existing menu integrations.
    // Only the two added rows need sidecar storage; the native ABI is [4][3].
    if (players == 0 && mode >= 2 && mode < 4) return &sSoloLevels[mode];
    return &gGameModeSubMenuColumn[players][mode];
}
extern "C" int Mk64SoloModeLimit3DS(int players) {
    return players == 0 ? 3 : gPlayerModeSelection[players];
}
extern "C" int Mk64SoloLevelLimit3DS(int players, int mode, bool extra) {
    const int game = Mk64SoloMode3DS(players, mode);
    return game == BATTLE ? 0 : game == TIME_TRIALS ? 1 : extra ? 3 : 2;
}
extern "C" void Mk64SoloVs3DSSpawn(float* x, float* z, float height) {
    for (int id=0;id<8;++id) {
        D_80165270[id]=7-id;
        const int character=(gCharacterSelections[0]+id)%8;
        spawn_player(&gPlayers[id],id,x[7-id],z[7-id],height,32768,character,
            PLAYER_EXISTS|PLAYER_START_SEQUENCE|(id==0?PLAYER_HUMAN:PLAYER_CPU));
    }
    func_80039AE4();
}
extern "C" void Mk64SoloBattle3DSSpawn() {
    // Call only the native four-player spawn routine, not the split-screen
    // dispatcher. Camera/player counts remain one throughout the session.
    float x[8] = {}, z[8] = {};
    float radius = IsBigDonut() ? 575 : IsSkyscraper() ? 400 : IsDoubleDeck() ? 160 : 200;
    float height = IsBigDonut() ? 200 : IsSkyscraper() ? 480 : IsDoubleDeck() ? 55 : 5;
    z[0] = radius; z[1] = -radius; x[2] = -radius; x[3] = radius;
    const signed char chosen = gCharacterSelections[0];
    signed char opponents[7];
    for (int i=0,next=0;i<8;++i)if(i!=chosen)opponents[next++]=i;
    for (int i=0;i<3;++i) {
        const int pick=i+random_int(7-i);
        std::swap(opponents[i],opponents[pick]);
        gCharacterSelections[i+1]=opponents[i];
    }
    spawn_players_4p_battle(x, z, height);
    for (int i = 1; i < 4; ++i) {
        gPlayers[i].type &= ~PLAYER_HUMAN;
        gPlayers[i].type |= PLAYER_CPU;
    }
    for (int i = 4; i < 8; ++i) gPlayers[i].type &= ~PLAYER_EXISTS;
    Mk64SoloBattle3DSReset();
}
extern "C" void Mk64SoloBattle3DSReset() {
    sArena.Reset(); sArenaReady=false;
    sBattleFarView=false; sBattleObserved=-1;
    func_8001AB00(); // Battle bypasses init_players; clear roulette/actor strategies on every retry.
    D_801646CC=0;
    for (int i=0;i<4;++i) {
        sBots[i]=Bot{};
        sTrace[i]=BattleTrace{};
        sBots[i].last=Position(gPlayers[i].pos);
        sBots[i].wander=i;
        sBots[i].nextPlan=i*.08f;
        gLapCountByPlayerId[i]=0;
        gNumPathPointsTraversed[i]=0;
        bStopAICrossing[i]=0;
    }
    // Collision geometry is ready at spawn; keep graph construction outside
    // live gameplay so the first combat frame does not inherit a loading hitch.
    BuildArena();
}
extern "C" void Mk64SoloBattle3DSTick(int id) {
    if (id<0 || id>3 || (id==0 && !Mk64Auto3DSActive()) || !Mk64SoloBattle3DS()) return;
    auto& p=gPlayers[id]; auto& bot=sBots[id];
    TraceOnReturn trace{id};
    bot.driveReason=0;bot.facingFailure=bot.momentumFailure=0;
    const bool bomb = p.type & PLAYER_UNKNOWN_0x40;
    const bool contactAttack = bomb || (p.effects & STAR_EFFECT);
    if (!(p.type&PLAYER_EXISTS) || !(p.type&PLAYER_CPU) || p.type&PLAYER_START_SEQUENCE ||
        (!bomb && p.type&(PLAYER_CINEMATIC_MODE|PLAYER_INVISIBLE_OR_BOMB)) ||
        gRaceState!=RACE_IN_PROGRESS || gIsGamePaused ||
        p.lakituProps&(LAKITU_RETRIEVAL|HELD_BY_LAKITU|LAKITU_SCENE)) {
        bot.controlSuspended=true;
        bot.driveReason=1;
        return;
    }
    // Native physics may call this twice for one simulation image. Inventory
    // timers/roulette must advance once, not repeat with each physics substep.
    if (!bomb && bot.lastItemTime!=gCourseTimer) {
        Mk64SoloBattle3DSItems(id);bot.lastItemTime=gCourseTimer;
    }
    // Native hit bodies retain control until recovery. Steering or
    // accelerating over a hit would recreate the old CPU penalty bug.
    if (p.effects&(HIT_BY_ITEM_EFFECT|HIT_EFFECT|0x80|0x40|0x400|0x20000|0x01000000)) {
        bot.controlSuspended=true;
        bot.driveReason=2;
        return;
    }
    if (bot.controlSuspended) {
        // Recovery may put the body on another floor. Never resume a stale
        // portal, edge target or retreat inherited from before the hit/fall.
        bot.controlSuspended=false;bot.nextPlan=bot.nextSteer=0;
        bot.recoveringEdge=bot.routeBlocked=false;bot.roamGoal=-1;
        bot.stuckSince=0;bot.last=Position(p.pos);
        bot.motionAt=-1;bot.nextRecovery=bot.recoveryUntil=0;
        bot.reverseUntil=0;p.kartProps&=~BACK_UP;
    }
    const int itemBranch = cpu_ItemStrategy[id].branch;
    const bool shells = itemBranch == CPU_STRATEGY_HOLD_GREEN_SHELL ||
        itemBranch == CPU_STRATEGY_HOLD_RED_SHELL ||
        itemBranch == CPU_STRATEGY_ORBIT_TRIPLE_GREEN_SHELL ||
        itemBranch == CPU_STRATEGY_ORBIT_TRIPLE_RED_SHELL;
    if (!sArenaReady) BuildArena();
    if (!sArena.count || sArena.overflow) {bot.driveReason=3;return;}
    const auto pos=Position(p.pos);
    const float maxDrop=IsBigDonut() ? 18.f : 60.f;
    // Measure forward route progress in simulation time, not planning calls.
    // Sideways sliding along a wall must not reset the stuck watchdog.
    if(bot.motionAt<0 || gCourseTimer<bot.motionAt) {
        bot.motionAt=gCourseTimer;bot.motionPosition=pos;bot.motionTarget=bot.target;
    } else if(gCourseTimer-bot.motionAt>=.2f) {
        const float dx=bot.motionTarget.x-bot.motionPosition.x,dz=bot.motionTarget.z-bot.motionPosition.z;
        const float length=std::sqrt(dx*dx+dz*dz);
        const float progress=length>1 ? ((pos.x-bot.motionPosition.x)*dx+(pos.z-bot.motionPosition.z)*dz)/length : 0;
        if(progress>=2)bot.stuckSince=0;
        else bot.stuckSince+=std::min(gCourseTimer-bot.motionAt,.5f);
        bot.motionAt=gCourseTimer;bot.motionPosition=pos;bot.motionTarget=bot.target;
    }
    if(bot.recoveringEdge && (ArenaNavigation3DS::Distance2(pos,bot.recoveryTarget)<16*16 ||
       gCourseTimer>=bot.recoveryUntil)) {
        bot.recoveringEdge=false;bot.nextPlan=bot.nextSteer=0;
    }
    if(!bot.recoveringEdge && bot.stuckSince>=1.2f && gCourseTimer>=bot.nextRecovery) {
        if(sArena.EscapeTarget(pos,bot.target,bot.recoveryTarget,maxDrop,bot.recoveryAttempt++)) {
            bot.recoveringEdge=true;bot.recoveryUntil=gCourseTimer+2.5f;
            bot.target=bot.recoveryTarget;bot.motionTarget=bot.target;bot.stuckSince=0;
            bot.roamGoal=-1;bot.opponent=-1;
            const float angle=-p.rotation[1]*6.28318530718f/65536;
            const auto rear=ArenaNavigation3DS::Point{pos.x-std::sin(angle)*36,pos.y,pos.z-std::cos(angle)*36};
            if(p.speed<.8f && sArena.SafeSegment(pos,rear,14,maxDrop))bot.reverseUntil=gCourseTimer+.65f;
        }
        bot.nextRecovery=gCourseTimer+1;
    }
    if (!bot.recoveringEdge && gCourseTimer>=bot.nextPlan) {
        const int start=sArena.Nearest(pos);
        if(start<0)return;
        const int component=sArena.nodes[start].component;
        int goal=-1; float best=1e30f;
        float nearestOpponent=1e30f;
        for(int other=0;other<4;++other)if(other!=id && (gPlayers[other].type&PLAYER_EXISTS) &&
            gPlayerBalloonCount[other]>=0 && !(gPlayers[other].type&(PLAYER_CINEMATIC_MODE|PLAYER_INVISIBLE_OR_BOMB)) &&
            std::fabs(gPlayers[other].pos[1]-pos.y)<=25 && sArena.ClearWalls(pos,Position(gPlayers[other].pos)))
            nearestOpponent=std::min(nearestOpponent,ArenaNavigation3DS::Distance2(pos,Position(gPlayers[other].pos)));
        // A one-use bomb wins by contact. Ordinary armed karts reposition
        // during close combat, but applying that to bombs makes them flee
        // just before impact. Keep their floor/wall and stuck-recovery guards.
        if(!contactAttack && !shells && nearestOpponent<320*320) {
            if(bot.fightSince<0)bot.fightSince=gCourseTimer;
            if(nearestOpponent<150*150 || gCourseTimer-bot.fightSince>2.5f) {
                if(gCourseTimer>=bot.repositionUntil)bot.repositionUntil=gCourseTimer+3+id*.25f;
                bot.fightSince=-1;
            }
        } else bot.fightSince=-1;
        const int previousBox=bot.box;
        bot.box=-1;
        // Inventory is the native strategy, not a fabricated timed item.
        bool attackOpportunity=false;
        int opponent=-1;
        if (!bomb && cpu_ItemStrategy[id].branch==CPU_STRATEGY_WAIT_NEXT_ITEM && !Mk64CpuItem3DSRolling(id)) {
            for (size_t index=0;index<CM_GetActorSize();++index) {
                auto* actor=CM_GetActor(index);
                if (!actor || actor->type!=ACTOR_ITEM_BOX || !(actor->flags&0x8000) || actor->state!=2) continue;
                if(int(index)==bot.avoidBox && gCourseTimer<bot.avoidBoxUntil)continue;
                const auto box=Position(actor->pos);
                int node=sArena.Nearest(box,component);
                if (node<0 || ArenaNavigation3DS::SurfaceDistance2(box,sArena.nodes[node].vertices)>55*55 || !sArena.Reachable(start,node)) continue;
                const float route=sArena.RouteDistance(start,node,pos,box);
                float score=route*route;
                if(int(index)==previousBox)score*=.8f; // Finish a sensible pickup route instead of oscillating.
                for(int other=0;other<4;++other)if(other!=id && (gPlayers[other].type&PLAYER_CPU) && sBots[other].box==int(index))score+=240*240;
                if(score<best) {best=score;goal=node;bot.objective=box;bot.box=int(index);}
            }
        } else {
            for (int victim=0;victim<4;++victim) {
                auto& other=gPlayers[victim];
                if(victim==id || !(other.type&PLAYER_EXISTS) || gPlayerBalloonCount[victim]<0 ||
                    other.type&(PLAYER_CINEMATIC_MODE|PLAYER_INVISIBLE_OR_BOMB) ||
                    other.effects&(STAR_EFFECT|BOO_EFFECT)) continue;
                auto aim=Position(other.pos);
                int node=sArena.Nearest(aim,component);
                if(node<0 || ArenaNavigation3DS::SurfaceDistance2(aim,sArena.nodes[node].vertices)>55*55 || !sArena.Reachable(start,node)) continue;
                const float route=sArena.RouteDistance(start,node,pos,aim);
                float score=route*route;
                const bool clearShot=shells && std::fabs(aim.y-pos.y)<=25 && sArena.ClearWalls(pos,aim);
                // Prefer a usable shot and a weakened survivor, not a fixed
                // human/rival. Retain a reasonable target across plans rather
                // than switching back and forth between equidistant karts.
                if(clearShot)score*=.7f;
                if(gPlayerBalloonCount[victim]==0)score*=.85f;
                if(victim==bot.opponent)score*=.8f;
                if(score<best) {
                    best=score;goal=node;bot.objective=aim;opponent=victim;
                    attackOpportunity=contactAttack || shells;
                }
            }
        }
        // Position-only stuck checks miss circling a ramp: the kart moves,
        // but makes no progress toward its box. Allow real detours, then
        // temporarily reject just that objective and seek another real box.
        if(bot.box>=0) {
            const float remaining=sArena.RouteDistance(start,goal,pos,bot.objective);
            if(bot.box!=previousBox || remaining<bot.bestRemaining-20) {
                bot.bestRemaining=remaining;bot.progressUntil=gCourseTimer+6;
            } else if(gCourseTimer>=bot.progressUntil) {
                bot.avoidBox=bot.box;bot.avoidBoxUntil=gCourseTimer+8;
                bot.escapeUntil=gCourseTimer+1;bot.roamGoal=-1;bot.box=-1;
                bot.bestRemaining=1e30f;
            }
        } else bot.bestRemaining=1e30f;
        if(goal<0 || gCourseTimer<bot.escapeUntil ||
           (!attackOpportunity && !contactAttack &&
            ((bot.box<0 && gCourseTimer<bot.repositionUntil) || Mk64CpuItem3DSRolling(id)))) {
            // A roam/escape route is not an opponent pursuit. Keeping that
            // ID here lets the per-steer moving-target refresh overwrite the
            // roam endpoint with the rival's position, while retaining the
            // unrelated roam triangle. The mismatched route can brake forever
            // or orbit the same portal, even on perfectly flat arenas.
            opponent=-1;
            int roam=bot.roamGoal;
            if(gCourseTimer<bot.escapeUntil || roam<0 || !sArena.Reachable(start,roam) || ArenaNavigation3DS::Distance2(pos,sArena.nodes[roam].center)<80*80)
                roam=Reposition(start,pos,id);
            bot.roamGoal=roam;
            if(roam>=0){goal=roam;bot.objective=sArena.nodes[goal].center;bot.box=-1;}
            // Change reachable waypoint after an obstruction; never teleport,
            // drop collision or alter the opponent's balloons to escape.
            for(int tries=0;goal<0 && tries<sArena.count;++tries) {
                bot.wander=(bot.wander+17+id)%sArena.count;
                if(sArena.Reachable(start,bot.wander) && ArenaNavigation3DS::Distance2(pos,sArena.nodes[bot.wander].center)>80*80) {
                    goal=bot.wander;bot.objective=sArena.nodes[goal].center;break;
                }
            }
        } else bot.roamGoal=-1;
        bot.opponent=opponent;
        bot.goal=goal;
        bot.nextSteer=0;
        bot.nextPlan=gCourseTimer+(shells || contactAttack ? .25f : .5f);
    }
    // Refresh before driving past a short portal, not merely once per plan.
    // This also follows moving opponents rather than orbiting a stale point.
    if(!bot.recoveringEdge && bot.opponent>=0 && bot.box<0 && gCourseTimer>=bot.nextSteer &&
       (gPlayers[bot.opponent].type&PLAYER_EXISTS)) {
        bot.objective=Position(gPlayers[bot.opponent].pos);
        // Follow both parts of the route when a victim crosses a triangle or
        // changes floor; an updated endpoint with a stale goal is not a path.
        const int start=sArena.Nearest(pos);
        const int goal=start<0 ? -1 : sArena.Nearest(bot.objective,sArena.nodes[start].component);
        if(goal>=0 && sArena.Reachable(start,goal))bot.goal=goal;
        else bot.nextPlan=0;
    }
    if(!bot.recoveringEdge && (gCourseTimer>=bot.nextSteer || (gCourseTimer-bot.lastSteer>=.05f &&
        ArenaNavigation3DS::Distance2(pos,bot.target)<35*35))) {
        int start=sArena.Nearest(pos);
        if(!sArena.Steer(start,bot.goal,pos,bot.objective,bot.target,!IsBigDonut())) {
            bot.routeBlocked=true;
        } else bot.routeBlocked=false;
        bot.lastSteer=gCourseTimer;
        bot.nextSteer=gCourseTimer+.1f;
    }
    // Momentum can point into a hole while the kart faces sideways or turns
    // away. Probe it on every physics update, including mushroom/Star boosts.
    constexpr float tau=6.28318530718f;
    const float yaw=-p.rotation[1]*tau/65536;
    if(gCourseTimer<bot.reverseUntil) {
        const auto rear=ArenaNavigation3DS::Point{pos.x-std::sin(yaw)*28,pos.y,pos.z-std::cos(yaw)*28};
        if(sArena.SafeSegment(pos,rear,14,IsBigDonut()?18.f:60.f)) {
            p.kartProps|=BACK_UP;p.currentSpeed=100.f;
            p.kartPropulsionStrength=p.currentSpeed*p.currentSpeed/25.f;
            bot.driveReason=4;
            return;
        }
        bot.reverseUntil=0;
    }
    if(p.kartProps&BACK_UP) {
        p.kartProps&=~BACK_UP;p.currentSpeed=0;
        bot.nextPlan=bot.nextSteer=0;
    }
    const auto ahead=ArenaNavigation3DS::Point{pos.x+std::sin(yaw)*(28+p.speed*10),pos.y,
                                             pos.z+std::cos(yaw)*(28+p.speed*10)};
    const auto momentum=ArenaNavigation3DS::Point{pos.x+p.velocity[0]*12,pos.y,pos.z+p.velocity[2]*12};
    // A lower floor must actually exist under every probe. Double Deck's
    // raised spawn pad needs that short drop even before the portal itself
    // points downwards. Big Donut never allows a drop beside its lethal hole.
    const bool facingSafe=sArena.SafeSegment(pos,ahead,20,maxDrop,nullptr,&bot.facingFailure);
    if(bot.recoveringEdge)bot.target=bot.recoveryTarget;
    ArenaNavigation3DS::Point groundedAhead{};
    bool recovering=!facingSafe && sArena.SafeSegment(pos,ahead,0,maxDrop,&groundedAhead) &&
        (sArena.SafeSegment(groundedAhead,groundedAhead,20,maxDrop) || (bot.recoveringEdge &&
         sArena.SafeSegment(pos,bot.recoveryTarget,0,maxDrop) &&
         (ahead.x-pos.x)*(bot.target.x-pos.x)+(ahead.z-pos.z)*(bot.target.z-pos.z)>0));
    // A body already overlapping a ledge cannot pass the full-width probe at
    // its starting point. Find a supported way back onto the floor instead of
    // forever braking toward the same too-narrow portal.
    if(!facingSafe && !recovering && !bot.recoveringEdge && p.speed<.8f && bot.lastSteer==gCourseTimer) {
        if(gCourseTimer>=bot.nextRecovery && sArena.EscapeTarget(pos,bot.target,bot.recoveryTarget,maxDrop,bot.recoveryAttempt++)) {
            bot.target=bot.recoveryTarget;bot.recoveringEdge=true;bot.recoveryUntil=gCourseTimer+2.5f;
            bot.nextRecovery=gCourseTimer+1;bot.motionTarget=bot.target;
            // Re-evaluate against the newly owned target, not last frame's
            // pursuit. Planning cannot overwrite it midway through a turn.
            recovering=sArena.SafeSegment(pos,ahead,0,maxDrop) &&
                (ahead.x-pos.x)*(bot.target.x-pos.x)+(ahead.z-pos.z)*(bot.target.z-pos.z)>0;
        }
    }
    const bool momentumSafe=sArena.SafeSegment(pos,momentum,0,maxDrop,nullptr,&bot.momentumFailure);
    // Wall-bound sideways momentum is handled by native collision while the
    // recovery target drives away. A missing floor still ALWAYS brakes.
    const bool slidingWall=bot.recoveringEdge && bot.momentumFailure==ArenaNavigation3DS::Graph::Wall &&
        sArena.SafeSegment(pos,bot.recoveryTarget,0,maxDrop);
    bot.edgeRisk=(bot.routeBlocked && !bot.recoveringEdge) || (!facingSafe && !recovering) || (!momentumSafe && !slidingWall);
    Vec3f target={bot.target.x,bot.target.y,bot.target.z};
    const int16_t desired=-get_angle_between_two_vectors(p.pos,target);
    const int16_t error=desired-p.rotation[1];
    const int turn=std::clamp(int(error)*53/1456,-53,53);
    apply_cpu_turn(&p,turn);
    if((gCourseTimer<bot.escapeUntil || bot.edgeRisk || std::abs(int(error))>9000) && p.speed<.8f && !(p.effects&8))
        adjust_angle(&p.rotation[1],desired,182); // Native steering has zero authority at rest.
    const float speed=bot.edgeRisk ? .6f : recovering ? 1.2f : std::abs(int(error))>9000 ? 1.2f :
        std::abs(int(error))>4000 ? 2.4f : 4.5f;
    p.effects&=~0x200000; // Arena movement has no rubber-band force boost.
    if(bot.edgeRisk) {
        bot.driveReason=5;
        // Emergency braking, not a position clamp or collision bypass. Keep
        // the active item and native hit body; reduce only controllable motion.
        BrakeAtEdge(p);
    } else if(p.speed<speed || (p.effects&(BOOST_EFFECT|STAR_EFFECT))) {
        bot.driveReason=bot.recoveringEdge?6:7;player_accelerate_alternative(&p);
    } else {bot.driveReason=8;player_decelerate_alternative(&p,1.0f);}
}

extern "C" bool Mk64SoloBattle3DSWriteDiagnostics(const char* directory) {
    if(!directory)return false;
    char path[512];const int size=std::snprintf(path,sizeof(path),"%s/battle-ai.txt",directory);
    if(size<0 || size>=int(sizeof(path)))return false;
    FILE* out=std::fopen(path,"w");if(!out)return false;
    std::fprintf(out,"Battle AI snapshot: active=%d timer=%.3f arenaFloors=%d walls=%d overflow=%d\n"
        "reason: 0=idle 1=native-gate/rescue 2=native-hit 3=no-graph 4=reverse 5=edge-brake 6=recovery-drive 7=drive 8=speed-control\n"
        "probe: 0=safe 1=no-floor 2=wall 3=too-long; ring sampled every 0.1 simulation seconds\n",
        Mk64SoloBattle3DS(),double(gCourseTimer),sArena.count,sArena.wallCount,sArena.overflow);
    for(int id=0;id<4;++id) {
        const auto& p=gPlayers[id];const auto& b=sBots[id];const auto& t=sTrace[id];
        std::fprintf(out,"kart=%d character=%d balloons=%d type=%08X effects=%08X lakitu=%08X surface=%d rescueStage=%d slope=%.3f\n"
            "collision meshes=%u,%u,%u distances=%.3f,%.3f,%.3f floorNormal=%.3f,%.3f,%.3f\n"
            "goal=%d opponent=%d box=%d suspended=%d recovery=%d until=%.3f attempts=%d\n",
            id,int(p.characterId),int(gPlayerBalloonCount[id]),unsigned(p.type),unsigned(p.effects),unsigned(p.lakituProps),
            int(p.surfaceType),int(p.unk_222),double(p.slopeAccel),
            unsigned(p.collision.meshIndexYX),unsigned(p.collision.meshIndexZY),unsigned(p.collision.meshIndexZX),
            double(p.collision.surfaceDistance[0]),double(p.collision.surfaceDistance[1]),double(p.collision.surfaceDistance[2]),
            double(p.collision.orientationVector[0]),double(p.collision.orientationVector[1]),double(p.collision.orientationVector[2]),
            b.goal,b.opponent,b.box,b.controlSuspended,b.recoveringEdge,double(b.recoveryUntil),b.recoveryAttempt);
        for(unsigned n=0;n<t.count;++n) {
            const auto& s=t.samples[(t.next+64-t.count+n)%64];
            std::fprintf(out,"t=%.3f pos=%.2f,%.2f,%.2f vel=%.2f,%.2f,%.2f speed=%.3f yaw=%d target=%.2f,%.2f,%.2f stalled=%.3f type=%08X effects=%08X lakitu=%08X reason=%u front=%u momentum=%u recovery=%u\n",
                double(s.time),double(s.position.x),double(s.position.y),double(s.position.z),double(s.velocity.x),double(s.velocity.y),double(s.velocity.z),
                double(s.speed),int(s.yaw),double(s.target.x),double(s.target.y),double(s.target.z),double(s.stalled),
                unsigned(s.type),unsigned(s.effects),unsigned(s.lakitu),unsigned(s.reason),unsigned(s.facing),unsigned(s.momentum),unsigned(s.recovering));
        }
    }
    const bool okay=!std::ferror(out);return std::fclose(out)==0 && okay;
}

extern "C" bool Mk64SoloBattle3DSSpectating() {
    return Mk64SoloBattle3DS() && !Mk64Auto3DSActive() && gGamestate==RACING &&
        gRaceState<RACE_FINISHED && !(gPlayers[0].type&PLAYER_EXISTS);
}
extern "C" bool Mk64SoloBattle3DSFarView() { return sBattleFarView; }
extern "C" int Mk64SoloBattle3DSObservedPlayer() {
    if (!Mk64SoloBattle3DSSpectating()) return 0;
    const auto* camera = gScreenContexts[0].raceCamera;
    const int id = camera ? camera->playerId : -1;
    if (id >= 0 && id < 4 && (gPlayers[id].type & PLAYER_EXISTS) && gPlayerBalloonCount[id] >= 0) return id;
    for (int survivor = 1; survivor < 4; ++survivor)
        if ((gPlayers[survivor].type & PLAYER_EXISTS) && gPlayerBalloonCount[survivor] >= 0) return survivor;
    return 0;
}
extern "C" void Mk64SoloBattle3DSObserveInput(int direction,bool toggleDistance) {
    if(!Mk64SoloBattle3DS() || gGamestate!=RACING || gIsGamePaused ||
       gRaceState>=RACE_FINISHED || (!Mk64Auto3DSActive() && !Mk64SoloBattle3DSSpectating()))return;
    if(toggleDistance)sBattleFarView=!sBattleFarView;
    if(direction && Mk64SoloBattle3DSSpectating()) {
        if(sBattleObserved<0)sBattleObserved=gScreenContexts[0].raceCamera?gScreenContexts[0].raceCamera->playerId:0;
        for(int attempt=0;attempt<4;++attempt) {
            sBattleObserved=(sBattleObserved+(direction<0?3:1))%4;
            if((gPlayers[sBattleObserved].type&PLAYER_EXISTS) && gPlayerBalloonCount[sBattleObserved]>=0)break;
        }
    }
}
extern "C" void Mk64SoloBattle3DSCamera(void* rawCamera) {
    if (!Mk64SoloBattle3DS() || Mk64Auto3DSActive() || !rawCamera) return;
    auto* camera=static_cast<Camera*>(rawCamera);
    auto& screen=gScreenContexts[0];
    if (camera!=screen.raceCamera) return;
    // The native one-use bomb consumes PLAYER_EXISTS on impact. Follow a
    // survivor afterwards without lending it the human controller or reviving
    // the eliminated kart. Viewport-local camera arrays remain indexed by zero.
    int observed=0;
    if (!(gPlayers[0].type&PLAYER_EXISTS)) {
        if (gRaceState==RACE_FINISHED) observed=gPlayerWinningIndex;
        else if(sBattleObserved>=0 && (gPlayers[sBattleObserved].type&PLAYER_EXISTS) &&
                gPlayerBalloonCount[sBattleObserved]>=0)observed=sBattleObserved;
        else for (int id=1;id<4;++id)
            if ((gPlayers[id].type&PLAYER_EXISTS) && gPlayerBalloonCount[id]>=0) { observed=id; break; }
    }
    if (observed<0 || observed>3) observed=0;
    auto& player=gPlayers[observed];
    if (camera->playerId!=observed) camera_init(player.pos,player.rotation[1],9,camera->cameraId);
    camera->playerId=observed;
    screen.player=&player;
    if(Mk64SoloBattle3DSSpectating()) {
        camera->unk_30[0]=0;
        camera->unk_30[1]=sBattleFarView?24.0f:11.6f;
        camera->unk_30[2]=sBattleFarView?-65.0f:-38.5f;
        camera->unk_3C[2]=19.2f;
    }
}

extern "C" bool Mk64SoloBattle3DSClearShot(int owner,int target) {
    if(!Mk64SoloBattle3DS() || owner<0 || owner>3 || target<0 || target>3 || owner==target || !sArenaReady)return false;
    return sArena.ClearWalls(Position(gPlayers[owner].pos),Position(gPlayers[target].pos));
}
