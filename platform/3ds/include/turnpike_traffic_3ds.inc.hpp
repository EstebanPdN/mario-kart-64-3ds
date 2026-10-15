// Included by the generated Game bridge: read the live World vehicles, not the
// obsolete native vehicle arrays or the draw camera's shared path cursor.
#include "item_rules_3ds.h"
#include "auto_mode_3ds.h"
#include "engine/vehicles/Car.h"
#include "engine/vehicles/Bus.h"
#include "engine/vehicles/Truck.h"
#include "engine/vehicles/TankerTruck.h"
extern "C" {
#include "code_80005FD0.h"
}
#include "cpu_obstacle_3ds.inc.hpp"
static bool sMk64TrafficAvoiding3DS[NUM_PLAYERS];
extern "C" bool Mk64CpuTraffic3DSAvoiding(int id) {
    return id >= 0 && id < NUM_PLAYERS && sMk64TrafficAvoiding3DS[id];
}
template<class Vehicle>
static void Mk64TrafficCandidate3DS(int id, const Vehicle* vehicle, int& best, float& target) {
    if (!vehicle || vehicle->PathIndex != 0) return;
    const int count = gPathCountByPathIndex[0];
    const int point = gNearestPathPointByPlayerId[id];
    if (vehicle->WaypointIndex >= count || point >= count) return;
    const int ahead = (vehicle->WaypointIndex - point + count) % count;
    if (ahead > 24 || ahead >= best ||
        fabsf(vehicle->Position[1]-gPlayers[id].pos[1]) > 45.0f) return;
    float dx=vehicle->Position[0]-gPlayers[id].pos[0], dz=vehicle->Position[2]-gPlayers[id].pos[2];
    if (dx*dx+dz*dz > 350.0f*350.0f) return;
    float factor = gTrackPositionFactor[id];
    // Box-seeking can be outside the narrow AI band. Native avoidance must
    // select a road lane, not preserve that temporary shoulder position.
    if (factor < -0.8f) factor=-0.8f;
    if (factor > 0.8f) factor=0.8f;
    target=player_track_position_factor_vehicle(vehicle->SomeType,factor,vehicle->WaypointIndex);
    best=ahead;
}
extern "C" void Mk64CpuTraffic3DSAvoid(int id) {
    if (id < 0 || id >= NUM_PLAYERS) return;
    const bool wasAvoiding=sMk64TrafficAvoiding3DS[id];
    sMk64TrafficAvoiding3DS[id]=false;
    const bool obstacle=Mk64CpuObstacle3DSAvoid(id,wasAvoiding);
    Mk64CpuObstacleStep3DS(id,obstacle);
    if (obstacle) {
        sMk64TrafficAvoiding3DS[id]=true; // Box seeking must yield to hazards too.
        return;
    }
    const Player* player=&gPlayers[id];
    if (!Mk64CpuObstacleEligible3DS(id) || !IsToadsTurnpike() ||
        gPathIndexByPlayerId[id] != 0 || gPathCountByPathIndex[0] < 3 ||
        (player->type & (PLAYER_EXISTS | PLAYER_CPU)) != (PLAYER_EXISTS | PLAYER_CPU) ||
        (player->type & (PLAYER_HUMAN | PLAYER_START_SEQUENCE | PLAYER_CINEMATIC_MODE)) ||
        player->lakituProps || gLapCountByPlayerId[id] >= 3) {
        if (wasAvoiding && (player->type & PLAYER_CPU) && !(player->type & PLAYER_HUMAN))
            gPlayerTrackPositionFactorInstruction[id].target=gPlayerTrackPositionFactorInstruction[id].unkC;
        return;
    }
    int best=25;
    float target=gPlayerTrackPositionFactorInstruction[id].target;
    for (const auto& actor : GetWorld()->Actors) {
        if (!actor) continue;
        if (const auto* car=dynamic_cast<const ACar*>(actor.get())) Mk64TrafficCandidate3DS(id,car,best,target);
        else if (const auto* bus=dynamic_cast<const ABus*>(actor.get())) Mk64TrafficCandidate3DS(id,bus,best,target);
        else if (const auto* truck=dynamic_cast<const ATruck*>(actor.get())) Mk64TrafficCandidate3DS(id,truck,best,target);
        else if (const auto* tanker=dynamic_cast<const ATankerTruck*>(actor.get())) Mk64TrafficCandidate3DS(id,tanker,best,target);
    }
    if (best<25) {
        sMk64TrafficAvoiding3DS[id]=true;
        gPlayerTrackPositionFactorInstruction[id].target=target;
    } else if (wasAvoiding) {
        gPlayerTrackPositionFactorInstruction[id].target=gPlayerTrackPositionFactorInstruction[id].unkC;
    }
}
