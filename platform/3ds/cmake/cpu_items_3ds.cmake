# Narrow, fail-closed edits to a generated copy; never modify the submodule.
set(SPAGHETTIKART_CPU_ITEMS_SOURCE "${SPAGHETTIKART_ROOT}/src/code_80005FD0.c")
set(SPAGHETTIKART_3DS_CPU_ITEMS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/code_80005FD0_3ds.c")
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_CPU_ITEMS_SOURCE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SPAGHETTIKART_CPU_ITEMS_SOURCE}")
file(READ "${SPAGHETTIKART_CPU_ITEMS_SOURCE}" CPU_ITEMS_TEXT)
# Keep upstream gameplay verbatim for OFF, apart from the shared stale-actor
# guards below. Slice on named function boundaries; fail closed on upstream drift.
string(FIND "${CPU_ITEMS_TEXT}" "void cpu_decisions_branch_item(" CPU_ORIGINAL_BRANCH_START)
string(FIND "${CPU_ITEMS_TEXT}" "void func_8001ABE0(" CPU_ORIGINAL_BRANCH_END)
string(FIND "${CPU_ITEMS_TEXT}" "void cpu_use_item_strategy(" CPU_ORIGINAL_USE_START)
string(FIND "${CPU_ITEMS_TEXT}" "void func_8001BE78(" CPU_ORIGINAL_USE_END)
if(CPU_ORIGINAL_BRANCH_START LESS 0 OR CPU_ORIGINAL_BRANCH_END LESS CPU_ORIGINAL_BRANCH_START OR
   CPU_ORIGINAL_USE_START LESS 0 OR CPU_ORIGINAL_USE_END LESS CPU_ORIGINAL_USE_START)
    message(FATAL_ERROR "Upstream original CPU function boundaries changed")
endif()
math(EXPR CPU_ORIGINAL_BRANCH_LENGTH "${CPU_ORIGINAL_BRANCH_END} - ${CPU_ORIGINAL_BRANCH_START}")
math(EXPR CPU_ORIGINAL_USE_LENGTH "${CPU_ORIGINAL_USE_END} - ${CPU_ORIGINAL_USE_START}")
string(SUBSTRING "${CPU_ITEMS_TEXT}" ${CPU_ORIGINAL_BRANCH_START} ${CPU_ORIGINAL_BRANCH_LENGTH} CPU_ORIGINAL_BRANCH)
string(SUBSTRING "${CPU_ITEMS_TEXT}" ${CPU_ORIGINAL_USE_START} ${CPU_ORIGINAL_USE_LENGTH} CPU_ORIGINAL_USE)
string(REPLACE "cpu_decisions_branch_item(" "Mk64OriginalCpuBranch3DS(" CPU_ORIGINAL_BRANCH "${CPU_ORIGINAL_BRANCH}")
string(REPLACE "cpu_decisions_branch_item(" "Mk64OriginalCpuBranch3DS(" CPU_ORIGINAL_USE "${CPU_ORIGINAL_USE}")
string(REPLACE "void cpu_use_item_strategy(" "static void Mk64OriginalCpuUse3DS(" CPU_ORIGINAL_USE "${CPU_ORIGINAL_USE}")
string(REPLACE "void Mk64OriginalCpuBranch3DS(" "static void Mk64OriginalCpuBranch3DS(" CPU_ORIGINAL_BRANCH "${CPU_ORIGINAL_BRANCH}")
macro(mk64_cpu_replace old new)
    string(FIND "${CPU_ITEMS_TEXT}" "${old}" CPU_ITEMS_ANCHOR)
    if(CPU_ITEMS_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream CPU item anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" CPU_ITEMS_TEXT "${CPU_ITEMS_TEXT}")
endmacro()
mk64_cpu_replace("void cpu_decisions_branch_item(" "#include \"cpu_items_3ds.inc.h\"\n\nvoid cpu_decisions_branch_item(")
mk64_cpu_replace("void cpu_decisions_branch_item(UNUSED s32 playerId, s16* branch, s32 itemId) {"
    "void cpu_decisions_branch_item(UNUSED s32 playerId, s16* branch, s32 itemId) {\n    if (!Mk64NewItems3DSEnabled()) { Mk64OriginalCpuBranch3DS(playerId, branch, itemId); return; }")
set(CPU_ITEMS_TEXT "#include \"item_rules_3ds.h\"\n${CPU_ITEMS_TEXT}")
mk64_cpu_replace("void func_8001AB00(void) {" "void func_8001AB00(void) {\n    Mk64NewItems3DSBeginRace();")
mk64_cpu_replace("    sp2C = cpu_track_position_factor(arg0);"
    "    Mk64CpuTraffic3DSAvoid(arg0);\n    sp2C = cpu_track_position_factor(arg0);\n    sp2C = Mk64CpuSeekBox3DS(arg0, arg1, sp2C);")
mk64_cpu_replace("s16 get_angle_between_path(Vec3f arg0, Vec3f arg1) {"
    "#include \"cpu_hit_state_3ds.inc.h\"\n\ns16 get_angle_between_path(Vec3f arg0, Vec3f arg1) {")
mk64_cpu_replace("    f32 var_f2;\n    f32 var_f0;\n    UNUSED s32 thing;"
    "    if (Mk64CpuLeader3DSRegulate(playerId, targetSpeed)) return;\n    f32 var_f2;\n    f32 var_f0;\n    UNUSED s32 thing;")
# Keep the actual road/curve/off-track demand selected immediately above.
# A failed scripted drift must not replace it with the walking-speed target:
# nearby/full movement used that override while remote/cheap AI clears it.
# OFF/manual, damage, ceremony, finish and crossing safety retain native code.
mk64_cpu_replace([=[                if ((D_801630E8[playerId] == 2) || (D_801630E8[playerId] == -2) || (D_801630E8[playerId] == 3)) {
                    cpu_TargetSpeed[playerId] = 3.3333333f;
                }]=]
    [=[                if (((D_801630E8[playerId] == 2) || (D_801630E8[playerId] == -2) || (D_801630E8[playerId] == 3)) &&
                    !Mk64CpuKeepCornerThrottle3DS(playerId)) {
                    cpu_TargetSpeed[playerId] = 3.3333333f;
                }]=])
mk64_cpu_replace("                if ((D_801631F8[playerId] == 1) && (D_801631E0[playerId] == false)) {"
    "                if ((D_801631F8[playerId] == 1) && (D_801631E0[playerId] == false) &&\n                    !Mk64CpuKeepHitHeight3DS(player)) {")
mk64_cpu_replace("                if (D_801631E0[playerId] == true) {\n                    player->pos[1] = gPlayerPathY[playerId];"
    "                if (D_801631E0[playerId] == true && !Mk64CpuKeepHitHeight3DS(player)) {\n                    player->pos[1] = gPlayerPathY[playerId];")
mk64_cpu_replace("void regulate_cpu_speed(s32 playerId, f32 targetSpeed, Player* player) {"
    "void regulate_cpu_speed(s32 playerId, f32 targetSpeed, Player* player) {\n    if (Mk64NewItems3DSEnabled() && (player->type & PLAYER_CPU) &&\n        (Mk64RacerHitMotion3DS(player) || (player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER)))) {\n        player->effects &= ~0x200000;\n        player->unk_0E8 = 0.0f; // Do not inject the CPU-only recovery boost during a hit.\n        return;\n    }")
mk64_cpu_replace("if (CVarGetInteger(\"gHarderCPU\", false) == true) {" "{ // 3DS: all player-table items have CPU strategies.")
set(CPU_ITEMS_WAIT_OLD [=[            // Harder CPU Items
            if (((gNumPathPointsTraversed[playerId] + (playerId * 20) + 100) % 8 == 0) && (cpuStrategy->timer >= 512) &&
                (CVarGetInteger("gHarderCPU", 0) == true)) {

                cpu_decisions_branch_item(playerId, &cpuStrategy->branch,
                                          RaceManager_GetRandomCPUItem(gGPCurrentRaceRankByPlayerId[playerId]));
                // Stock CPU Items
            } else if ((((playerId * 20) + 100) < gNumPathPointsTraversed[playerId]) && (cpuStrategy->timer >= 0x259) &&
                       (cpuStrategy->numItemUse < 3) && (gLapCountByPlayerId[playerId] < 3)) {
                cpu_decisions_branch_item(
                    playerId, &cpuStrategy->branch,
                    RaceManager_GetRandomCPUItem(gGPCurrentRaceRankByPlayerId[playerId]));
            } else {
                func_8001ABE0(playerId, cpuStrategy);
            }]=])
mk64_cpu_replace("${CPU_ITEMS_WAIT_OLD}" [=[            // No timed free items: only a real box collision starts this wait.
            Mk64CpuAdvanceItemBox3DS(playerId, cpuStrategy);]=])
# A consumed Star is an effect, not an occupied inventory slot. Preserve the
# native OFF function sliced above; modern CPUs may roll/hold/use a next item.
mk64_cpu_replace("cpuStrategy->branch = CPU_STRATEGY_END_ITEM_STAR;"
    "cpuStrategy->branch = CPU_STRATEGY_WAIT_NEXT_ITEM; // Star effect outlives inventory.")
mk64_cpu_replace("if (player->effects & (BOO_EFFECT | BOOST_EFFECT | STAR_EFFECT)) {"
    "if (player->effects & (BOO_EFFECT | BOOST_EFFECT)) {")
mk64_cpu_replace("    if ((arg0->actorIndex < 0) || (arg0->actorIndex >= 0x64)) {"
    "    if ((arg0->actorIndex < 0) || ((size_t)arg0->actorIndex >= CM_GetActorSize())) {")
mk64_cpu_replace("    bool isValidBanana2;" "    bool isValidBanana2;\n    s32 attackTarget;")
# Remove the human-only banana condition (including character/rank restrictions).
string(FIND "${CPU_ITEMS_TEXT}" "            // never true\n" CPU_BANANA_START)
string(FIND "${CPU_ITEMS_TEXT}" "                cpuStrategy->actorIndex = use_banana_item(player);" CPU_BANANA_END)
if(CPU_BANANA_START LESS 0 OR CPU_BANANA_END LESS CPU_BANANA_START)
    message(FATAL_ERROR "Upstream banana target block changed")
endif()
math(EXPR CPU_BANANA_LENGTH "${CPU_BANANA_END} - ${CPU_BANANA_START}")
string(SUBSTRING "${CPU_ITEMS_TEXT}" ${CPU_BANANA_START} ${CPU_BANANA_LENGTH} CPU_BANANA_OLD)
mk64_cpu_replace("${CPU_BANANA_OLD}" [=[            attackTarget = Mk64CpuItemTarget3DS(playerId, 1);
            if (attackTarget >= 0) {
                cpuStrategy->branch = CPU_STRATEGY_THROW_BANANA;
            } else {
]=])
mk64_cpu_replace("        case CPU_STRATEGY_THROW_BANANA:\n            cpuStrategy->actorIndex"
    "        case CPU_STRATEGY_THROW_BANANA:\n            attackTarget = Mk64CpuItemTarget3DS(playerId, 1);\n            if (attackTarget < 0) { cpuStrategy->branch = CPU_STRATEGY_ITEM_BANANA; break; }\n            cpuStrategy->actorIndex")
mk64_cpu_replace([=[                pathPoint = &gTrackPaths[gPathIndexByPlayerId[0]]
                                        [(gNearestPathPointByPlayerId[gBestRankedHumanPlayer] + 30) %
                                         gPathCountByPathIndex[gPathIndexByPlayerId[gBestRankedHumanPlayer]]];
                BANANA_ACTOR(actor)->velocity[0] = (pathPoint->x - player->pos[0]) / 20.0;
                BANANA_ACTOR(actor)->velocity[1] = ((pathPoint->y - player->pos[1]) / 20.0) + 4.0;
                BANANA_ACTOR(actor)->velocity[2] = (pathPoint->z - player->pos[2]) / 20.0;]=]
    [=[                // Predict this nearby car's position; no player-0/path-0 bias.
                Player* victim = &gPlayerOne[attackTarget];
                BANANA_ACTOR(actor)->velocity[0] = (victim->pos[0] - player->pos[0]) / 20.0f + victim->velocity[0];
                BANANA_ACTOR(actor)->velocity[1] = (victim->pos[1] - player->pos[1]) / 20.0f + 4.0f;
                BANANA_ACTOR(actor)->velocity[2] = (victim->pos[2] - player->pos[2]) / 20.0f + victim->velocity[2];]=])
mk64_cpu_replace("    TrackPathPoint* pathPoint;\n    bool isValidBanana1;" "    bool isValidBanana1;")
mk64_cpu_replace([=[            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer) {
                cpuStrategy->branch = CPU_STRATEGY_DROP_BANANA;]=]
    [=[            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer ||
                       (cpuStrategy->timer >= 4 && Mk64CpuItemTarget3DS(playerId, -1) >= 0)) {
                cpuStrategy->branch = CPU_STRATEGY_DROP_BANANA;]=])
foreach(CPU_SHELL_KIND GREEN RED BLUE_SPINY)
    mk64_cpu_replace("            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer) {\n                cpuStrategy->branch = CPU_STRATEGY_THROW_${CPU_SHELL_KIND}_SHELL;"
        "            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer ||\n                       (cpuStrategy->timer >= 4 && Mk64CpuItemTarget3DS(playerId, 0) >= 0)) {\n                cpuStrategy->branch = CPU_STRATEGY_THROW_${CPU_SHELL_KIND}_SHELL;")
endforeach()
mk64_cpu_replace("                } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer) {\n                    cpuStrategy->branch = CPU_STRATEGY_DROP_BANANA_BUNCH;"
    "                } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer ||\n                           (cpuStrategy->timer >= 4 && Mk64CpuItemTarget3DS(playerId, -1) >= 0)) {\n                    cpuStrategy->branch = CPU_STRATEGY_DROP_BANANA_BUNCH;")
foreach(CPU_REAR_ITEM FAKE_ITEM_BOX)
    mk64_cpu_replace("            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer) {\n                cpuStrategy->branch = CPU_STRATEGY_THROW_${CPU_REAR_ITEM};"
        "            } else if (cpuStrategy->timeBeforeThrow < cpuStrategy->timer ||\n                       (cpuStrategy->timer >= 4 && Mk64CpuItemTarget3DS(playerId, -1) >= 0)) {\n                cpuStrategy->branch = CPU_STRATEGY_THROW_${CPU_REAR_ITEM};")
endforeach()
set(CPU_GREEN_HUMAN_OLD [=[                // Check for players behind the CPU and throw shell backwards
                s16 highestHumanRank = gPlayerOne->currentRank;
                // Handle two players
                //! @warning Likely needs an update to support more than two human players
                if ((gPlayerTwo->type & PLAYER_HUMAN) && gPlayerTwo->currentRank > highestHumanRank) {
                    highestHumanRank = gPlayerTwo->currentRank;
                }

                if (player->currentRank < highestHumanRank) {]=])
mk64_cpu_replace("${CPU_GREEN_HUMAN_OLD}" [=[                attackTarget = Mk64CpuItemTarget3DS(playerId, 0);
                Vec3f firingAxis = {0.0f, 0.0f, 1.0f};
                func_802B64C4(firingAxis, player->rotation[1] + player->unk_0C0);
                if (attackTarget >= 0 &&
                    (gPlayerOne[attackTarget].pos[0] - player->pos[0]) * firingAxis[0] +
                    (gPlayerOne[attackTarget].pos[2] - player->pos[2]) * firingAxis[2] < 0.0f) {]=])
# More item use exercises recycled actor slots: validate bounds, activity and
# type BEFORE every cast. The old fixed-100 limit is invalid for World actors.
mk64_cpu_replace("void cpu_use_item_strategy(s32 playerId) {"
    "${CPU_ORIGINAL_BRANCH}\n${CPU_ORIGINAL_USE}\nvoid cpu_use_item_strategy(s32 playerId) {\n    if (playerId < 0 || playerId >= NUM_PLAYERS) return;\n    Mk64CpuStarWarning3DS(playerId);\n    if (Mk64CpuGoldTick3DS(playerId)) return;\n    if (Mk64CpuShroomTick3DS(playerId)) return;\n    if (!Mk64NewItems3DSEnabled()) { Mk64OriginalCpuUse3DS(playerId); return; }\n    if (Mk64RacerHitMotion3DS(&gPlayers[playerId]) ||\n        (gPlayers[playerId].triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER))) return; // Keep item state, never clean away a hit with an automatic boost.")
mk64_cpu_replace("actor = GET_ACTOR(cpuStrategy->actorIndex);" [=[if (cpuStrategy->actorIndex < 0 || (size_t)cpuStrategy->actorIndex >= CM_GetActorSize()) {
                Mk64CpuAbandonItem3DS(player, cpuStrategy); return;
            }
            actor = GET_ACTOR(cpuStrategy->actorIndex);
            s32 expectedType = Mk64CpuItemActorType3DS(cpuStrategy->branch);
            if (!(actor->flags & 0x8000) || (expectedType && actor->type != expectedType)) {
                Mk64CpuAbandonItem3DS(player, cpuStrategy); return;
            }]=])
# A declaration after each assignment must have its own scope/name: use an
# expression instead, so multiple case labels remain legal C.
string(REPLACE "            s32 expectedType = Mk64CpuItemActorType3DS(cpuStrategy->branch);\n            if (!(actor->flags & 0x8000) || (expectedType && actor->type != expectedType)) {"
    "            if (!(actor->flags & 0x8000) || !Mk64CpuOwnsItemActor3DS(actor, playerId) ||\n                (Mk64CpuItemActorType3DS(cpuStrategy->branch) &&\n                 actor->type != Mk64CpuItemActorType3DS(cpuStrategy->branch))) {"
    CPU_ITEMS_TEXT "${CPU_ITEMS_TEXT}")
file(WRITE "${SPAGHETTIKART_3DS_CPU_ITEMS_SOURCE}" "${CPU_ITEMS_TEXT}")
