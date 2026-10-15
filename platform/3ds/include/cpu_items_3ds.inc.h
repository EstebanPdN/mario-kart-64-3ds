// Included by the generated legacy CPU translation unit, after its globals.
#include "settings_3ds.h"
#include "item_rules_3ds.h"
#include "auto_mode_3ds.h"
#include "solo_modes_3ds.h"
#include "threat_warning_3ds.inc.h"
#include "cpu_item_target_3ds.inc.h"
#include "cpu_hit_state_3ds.inc.h"
#include "item_hit_protection_3ds.inc.h"
#include "cpu_leader_3ds.inc.h"
static void Mk64OriginalCpuBranch3DS(s32 playerId, s16* branch, s32 itemId);
void cpu_decisions_branch_item(s32 playerId, s16* branch, s32 itemId);
static bool sMk64NewItems3DS = true;
static f32 sMk64LightningNext3DS = 30.0f;
static f32 sMk64CpuItemReady3DS[NUM_PLAYERS];
static s16 sMk64CpuItemOverride3DS[NUM_PLAYERS];
static f32 sMk64CpuGoldUntil3DS[NUM_PLAYERS], sMk64CpuGoldPulse3DS[NUM_PLAYERS];
static f32 sMk64CpuShroomNext3DS[NUM_PLAYERS];
static bool sMk64StarWarning3DS[NUM_PLAYERS];
static void Mk64CpuStarWarning3DS(s32 id) {
    Player* racer = &gPlayers[id];
    bool nearby = false;
    if (sMk64NewItems3DS && !gDemoMode &&
        (racer->type & PLAYER_EXISTS) && !(racer->type & (PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) &&
        (racer->effects & STAR_EFFECT) && gLapCountByPlayerId[id] < 3) {
        for (s32 listener = 0; listener < NUM_PLAYERS; ++listener)
            if (listener != id && Mk64ThreatBehind3DS(listener, racer->pos, kMk64ThreatStarRange3DS, kMk64ThreatStarHeight3DS)) {
                nearby = true;
                break;
            }
    }
    if (nearby && !sMk64StarWarning3DS[id])
        func_800C9D80(racer->pos, racer->velocity, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
    else if (!nearby && sMk64StarWarning3DS[id])
        func_800C9EF4(racer->pos, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
    sMk64StarWarning3DS[id] = nearby;
}
#include "cpu_box_seek_3ds.inc.h"
bool Mk64NewItems3DSEnabled(void) { return sMk64NewItems3DS; }
void Mk64NewItems3DSBeginRace(void) {
    sMk64NewItems3DS = Mk64Settings3DSGetNewItems();
    Mk64CpuLeaderReset3DS();
    Mk64CpuBoxSeekReset3DS();
    Mk64ItemHitReset3DS();
    sMk64LightningNext3DS = 30.0f;
    for (s32 i = 0; i < NUM_PLAYERS; ++i) {
        if (sMk64StarWarning3DS[i])
            func_800C9EF4(gPlayers[i].pos, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));
        sMk64StarWarning3DS[i] = false;
        sMk64CpuItemReady3DS[i] = -1.0f;
        sMk64CpuItemOverride3DS[i] = ITEM_NONE;
        sMk64CpuGoldUntil3DS[i] = sMk64CpuGoldPulse3DS[i] = -1.0f;
        sMk64CpuShroomNext3DS[i] = -1.0f;
    }
}
void Mk64NewItems3DSLightningUsed(void) {
    if (sMk64NewItems3DS) sMk64LightningNext3DS = gCourseTimer + 30.0f;
}
static s32 Mk64CpuRollItem3DS(s32 rank) {
    // MK8DX measured conditional range, adapted to MK64's eight-car field:
    // sixth/seventh/eighth: 1.5/2.5/3.5%. Not an exact MK8DX distance table.
    s32 rate = rank >= 5 && rank < NUM_PLAYERS ? 150 + (rank - 5) * 100 : 0;
    if (rate && gCourseTimer >= sMk64LightningNext3DS && random_int(10000) < rate) {
        // Reserve as soon as drawn, so CPUs cannot acquire a simultaneous batch.
        sMk64LightningNext3DS = gCourseTimer + 30.0f;
        return ITEM_THUNDERBOLT;
    }
    // Preserve relative non-lightning weights in the human rank table, without
    // changing that table or the human roll function. Bound custom-table retries.
    for (s32 attempt = 0; attempt < 32; ++attempt) {
        s32 item = RaceManager_GetRandomHumanItem(rank);
        if (item != ITEM_THUNDERBOLT) return item;
    }
    return ITEM_MUSHROOM;
}
bool Mk64CpuItemBox3DSPickup(int playerId, int itemId) {
    if ((!sMk64NewItems3DS && !Mk64SoloBattle3DS()) || playerId < 0 || playerId >= NUM_PLAYERS ||
        gModeSelection == TIME_TRIALS) return false;
    Player* player = &gPlayers[playerId];
    if (!(player->type & PLAYER_EXISTS) || !(player->type & PLAYER_CPU) ||
        (player->type & (PLAYER_HUMAN | PLAYER_CINEMATIC_MODE | PLAYER_INVISIBLE_OR_BOMB)) ||
        (player->effects & BOO_EFFECT) || gLapCountByPlayerId[playerId] >= 3 ||
        cpu_ItemStrategy[playerId].branch != CPU_STRATEGY_WAIT_NEXT_ITEM ||
        sMk64CpuItemReady3DS[playerId] >= 0.0f) return false;
    // Clock-based roulette wait is independent of display FPS and CPU ID parity.
    sMk64CpuItemReady3DS[playerId] = gCourseTimer + 3.0f;
    sMk64CpuItemOverride3DS[playerId] = itemId;
    cpu_ItemStrategy[playerId].timer = 0;
    // Only an accepted physical box, never the old timed/free-item strategy.
    func_800C98B8(player->pos, player->velocity, 0x19008406U);
    return true;
}
static void Mk64CpuAdvanceItemBox3DS(s32 playerId, CpuItemStrategyData* strategy) {
    if (sMk64CpuItemReady3DS[playerId] < 0.0f) return;
    if (gLapCountByPlayerId[playerId] >= 3) {
        sMk64CpuItemReady3DS[playerId] = -1.0f;
        return;
    }
    if (gCourseTimer < sMk64CpuItemReady3DS[playerId]) return;
    s32 item = sMk64CpuItemOverride3DS[playerId];
    sMk64CpuItemReady3DS[playerId] = -1.0f;
    sMk64CpuItemOverride3DS[playerId] = ITEM_NONE;
    if (item == ITEM_NONE) item = gModeSelection == BATTLE ? RaceManager_GetRandomHumanItem(0) :
        Mk64CpuRollItem3DS(gGPCurrentRaceRankByPlayerId[playerId]);
    cpu_decisions_branch_item(playerId, &strategy->branch, item);
    strategy->timer = 0;
}
static s32 Mk64CpuItemActorType3DS(s16 branch) {
    switch (branch) {
        case CPU_STRATEGY_HOLD_BANANA: case CPU_STRATEGY_DROP_BANANA:
        case CPU_STRATEGY_HOLD_THROW_BANANA: case CPU_STRATEGY_END_THROW_BANANA: return ACTOR_BANANA;
        case CPU_STRATEGY_HOLD_GREEN_SHELL: case CPU_STRATEGY_THROW_GREEN_SHELL: return ACTOR_GREEN_SHELL;
        case CPU_STRATEGY_HOLD_RED_SHELL: case CPU_STRATEGY_THROW_RED_SHELL: return ACTOR_RED_SHELL;
        case CPU_STRATEGY_HOLD_BLUE_SPINY_SHELL: case CPU_STRATEGY_THROW_BLUE_SPINY_SHELL: return ACTOR_BLUE_SPINY_SHELL;
        case CPU_STRATEGY_WAIT_INIT_BANANA_BUNCH: case CPU_STRATEGY_DROP_BANANA_BUNCH: return ACTOR_BANANA_BUNCH;
        case CPU_STRATEGY_HOLD_FAKE_ITEM_BOX: case CPU_STRATEGY_THROW_FAKE_ITEM_BOX: return ACTOR_FAKE_ITEM_BOX;
        case CPU_STRATEGY_ORBIT_TRIPLE_GREEN_SHELL: case CPU_STRATEGY_THROW_TRIPLE_GREEN_SHELL: return ACTOR_TRIPLE_GREEN_SHELL;
        case CPU_STRATEGY_ORBIT_TRIPLE_RED_SHELL: case CPU_STRATEGY_THROW_TRIPLE_RED_SHELL: return ACTOR_TRIPLE_RED_SHELL;
        default: return 0; // Newly allocated actors are checked in their creation branch.
    }
}

// Read-only HUD view; never advances roulette or consumes a strategy.
bool Mk64CpuItem3DSRolling(int playerId) {
    return playerId >= 0 && playerId < NUM_PLAYERS && (sMk64NewItems3DS || Mk64SoloBattle3DS()) &&
        sMk64CpuItemReady3DS[playerId] >= 0.0f;
}
int Mk64CpuItem3DSHud(int playerId) {
    if (playerId < 0 || playerId >= NUM_PLAYERS) return ITEM_NONE;
    if (sMk64NewItems3DS && sMk64CpuItemReady3DS[playerId] >= 0.0f)
        return 1 + ((int)(gCourseTimer * 12.0f) % 15);
    s16 branch = cpu_ItemStrategy[playerId].branch;
    switch (branch) {
        case CPU_STRATEGY_ITEM_BANANA: case CPU_STRATEGY_HOLD_BANANA:
        case CPU_STRATEGY_DROP_BANANA: case CPU_STRATEGY_THROW_BANANA:
        case CPU_STRATEGY_HOLD_THROW_BANANA: case CPU_STRATEGY_END_THROW_BANANA: return ITEM_BANANA;
        case CPU_STRATEGY_ITEM_BANANA_BUNCH: case CPU_STRATEGY_WAIT_INIT_BANANA_BUNCH:
        case CPU_STRATEGY_DROP_BANANA_BUNCH: return ITEM_BANANA_BUNCH;
        case CPU_STRATEGY_ITEM_GREEN_SHELL: case CPU_STRATEGY_HOLD_GREEN_SHELL:
        case CPU_STRATEGY_THROW_GREEN_SHELL: return ITEM_GREEN_SHELL;
        case CPU_STRATEGY_ITEM_TRIPLE_GREEN_SHELL: case CPU_STRATEGY_ORBIT_TRIPLE_GREEN_SHELL:
        case CPU_STRATEGY_THROW_TRIPLE_GREEN_SHELL: return ITEM_TRIPLE_GREEN_SHELL;
        case CPU_STRATEGY_ITEM_RED_SHELL: case CPU_STRATEGY_HOLD_RED_SHELL:
        case CPU_STRATEGY_THROW_RED_SHELL: return ITEM_RED_SHELL;
        case CPU_STRATEGY_ITEM_TRIPLE_RED_SHELL: case CPU_STRATEGY_ORBIT_TRIPLE_RED_SHELL:
        case CPU_STRATEGY_THROW_TRIPLE_RED_SHELL: return ITEM_TRIPLE_RED_SHELL;
        case CPU_STRATEGY_ITEM_BLUE_SPINY_SHELL: case CPU_STRATEGY_HOLD_BLUE_SPINY_SHELL:
        case CPU_STRATEGY_THROW_BLUE_SPINY_SHELL: return ITEM_BLUE_SPINY_SHELL;
        case CPU_STRATEGY_ITEM_FAKE_ITEM_BOX: case CPU_STRATEGY_HOLD_FAKE_ITEM_BOX:
        case CPU_STRATEGY_THROW_FAKE_ITEM_BOX: return ITEM_FAKE_ITEM_BOX;
        case CPU_STRATEGY_ITEM_THUNDERBOLT: return ITEM_THUNDERBOLT;
        case CPU_STRATEGY_ITEM_STAR: return ITEM_STAR;
        case CPU_STRATEGY_ITEM_BOO: return ITEM_BOO;
        case CPU_STRATEGY_ITEM_MUSHROOM: return ITEM_MUSHROOM;
        case CPU_STRATEGY_ITEM_DOUBLE_MUSHROOM: return ITEM_DOUBLE_MUSHROOM;
        case CPU_STRATEGY_ITEM_TRIPLE_MUSHROOM: return ITEM_TRIPLE_MUSHROOM;
        case CPU_STRATEGY_ITEM_SUPER_MUSHROOM: case CPU_STRATEGY_USE_SUPER_MUSHROOM: return ITEM_SUPER_MUSHROOM;
        default: return ITEM_NONE;
    }
}

// The native strategy timer is reset by BOOST_EFFECT at every call, so a gold
// mushroom can never reach 600. A separate race-clock deadline survives boosts
// and damage, freezes with pause, and is independent of display FPS.
static bool Mk64CpuGoldTick3DS(s32 id) {
    CpuItemStrategyData* strategy = &cpu_ItemStrategy[id];
    if (strategy->branch != CPU_STRATEGY_ITEM_SUPER_MUSHROOM &&
        strategy->branch != CPU_STRATEGY_USE_SUPER_MUSHROOM) {
        sMk64CpuGoldUntil3DS[id] = -1.0f;
        return false;
    }
    Player* player = &gPlayers[id];
    bool hit = Mk64RacerHitMotion3DS(player) ||
        (player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER));
    if (gLapCountByPlayerId[id] >= 3 || (sMk64CpuGoldUntil3DS[id] >= 0.0f &&
        gCourseTimer >= sMk64CpuGoldUntil3DS[id])) {
        strategy->branch = CPU_STRATEGY_WAIT_NEXT_ITEM;
        strategy->timer = 0;
        sMk64CpuGoldUntil3DS[id] = -1.0f;
        return true; // No final extra pulse at expiration, even while tumbling.
    }
    if (hit) return true;
    if (strategy->branch == CPU_STRATEGY_ITEM_SUPER_MUSHROOM || sMk64CpuGoldUntil3DS[id] < 0.0f) {
        strategy->branch = CPU_STRATEGY_USE_SUPER_MUSHROOM;
        strategy->timer = 0;
        strategy->timeBeforeThrow = 600;
        strategy->numItemUse++;
        sMk64CpuGoldUntil3DS[id] = gCourseTimer + 10.0f;
        sMk64CpuGoldPulse3DS[id] = gCourseTimer;
    }
    if (gCourseTimer >= sMk64CpuGoldPulse3DS[id]) {
        player->triggers |= SHROOM_TRIGGER;
        sMk64CpuGoldPulse3DS[id] = gCourseTimer + 1.0f;
    }
    return true;
}

// Native double -> single consumes the final shroom on the very next update.
// One shared gate keeps every pulse distinct, independent of strategy-timer
// resets during BOOST_EFFECT. Native/manual OFF keeps its original strategy.
static bool Mk64CpuShroomTick3DS(s32 id) {
    CpuItemStrategyData* strategy = &cpu_ItemStrategy[id];
    s16 branch = strategy->branch;
    if (branch != CPU_STRATEGY_ITEM_MUSHROOM && branch != CPU_STRATEGY_ITEM_DOUBLE_MUSHROOM &&
        branch != CPU_STRATEGY_ITEM_TRIPLE_MUSHROOM) {
        sMk64CpuShroomNext3DS[id] = -1.0f;
        return false;
    }
    Player* player = &gPlayers[id];
    if ((!Mk64NewItems3DSEnabled() && !Mk64Auto3DSActive()) ||
        !(player->type & PLAYER_CPU) || (player->type & PLAYER_HUMAN)) return false;
    if (gLapCountByPlayerId[id] >= 3) {
        strategy->branch = CPU_STRATEGY_WAIT_NEXT_ITEM;
        strategy->timer = 0;
        sMk64CpuShroomNext3DS[id] = -1.0f;
        return true;
    }
    if (gModeSelection == TIME_TRIALS || D_801646CC == 1 || (player->type & PLAYER_CINEMATIC_MODE) ||
        player->lakituProps || Mk64RacerHitMotion3DS(player) ||
        (player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER | SHROOM_TRIGGER)) ||
        (player->effects & BOOST_EFFECT) || gCourseTimer < sMk64CpuShroomNext3DS[id]) return true;
    player->triggers |= SHROOM_TRIGGER;
    strategy->branch = branch == CPU_STRATEGY_ITEM_TRIPLE_MUSHROOM ? CPU_STRATEGY_ITEM_DOUBLE_MUSHROOM :
        branch == CPU_STRATEGY_ITEM_DOUBLE_MUSHROOM ? CPU_STRATEGY_ITEM_MUSHROOM : CPU_STRATEGY_WAIT_NEXT_ITEM;
    strategy->timer = 0;
    if (strategy->branch == CPU_STRATEGY_WAIT_NEXT_ITEM) strategy->numItemUse++;
    sMk64CpuShroomNext3DS[id] = gCourseTimer + 0.75f;
    return true;
}

static void Mk64CpuAbandonItem3DS(Player* player, CpuItemStrategyData* strategy) {
    strategy->branch = CPU_STRATEGY_WAIT_NEXT_ITEM;
    strategy->timer = 0;
    strategy->actorIndex = -1;
    player->triggers &= ~DRAG_ITEM_EFFECT;
}

static bool Mk64CpuOwnsItemActor3DS(struct Actor* actor, s32 playerId) {
    switch (actor->type) {
        case ACTOR_BANANA: return ((struct BananaActor*)actor)->playerId == playerId;
        case ACTOR_BANANA_BUNCH: return ((struct BananaBunchParent*)actor)->playerId == playerId;
        case ACTOR_FAKE_ITEM_BOX: return ((struct FakeItemBox*)actor)->playerId == playerId;
        case ACTOR_GREEN_SHELL: case ACTOR_RED_SHELL: case ACTOR_BLUE_SPINY_SHELL:
            return ((struct ShellActor*)actor)->playerId == playerId;
        case ACTOR_TRIPLE_GREEN_SHELL: case ACTOR_TRIPLE_RED_SHELL:
            return ((TripleShellParent*)actor)->playerId == playerId;
        default: return false;
    }
}
