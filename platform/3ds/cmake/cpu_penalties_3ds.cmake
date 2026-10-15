# No submodule edits: offscreen CPUs only enter full physics during penalties.
set(CPU_PHYSICS_SOURCE "${SPAGHETTIKART_ROOT}/src/player_controller.c")
set(SPAGHETTIKART_3DS_PLAYER_CONTROLLER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/player_controller_3ds.c")
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${CPU_PHYSICS_SOURCE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CPU_PHYSICS_SOURCE}")
file(READ "${CPU_PHYSICS_SOURCE}" CPU_PHYSICS_TEXT)
macro(mk64_penalty_replace old new)
    string(FIND "${CPU_PHYSICS_TEXT}" "${old}" CPU_PHYSICS_ANCHOR)
    if(CPU_PHYSICS_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream CPU physics anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" CPU_PHYSICS_TEXT "${CPU_PHYSICS_TEXT}")
endmacro()
mk64_penalty_replace("void func_80028864(Player* player, Camera* camera, s8 playerId, s8 screenId) {"
    "#include \"cpu_penalties_3ds.inc.h\"\n\nvoid func_80028864(Player* player, Camera* camera, s8 playerId, s8 screenId) {\n    Mk64CpuItemPenaltyTick3DS(playerId, Mk64CpuItemPenalty3DS(player));\n    if (Mk64CpuKeepHitHeight3DS(player) && (player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER))) {\n        player->effects &= ~0x1000;\n        func_8002B830(player, playerId, screenId); // Consume hit before choosing the movement body.\n    }")
mk64_penalty_replace("        if ((sp1E == 1) || ((player->type & PLAYER_INVISIBLE_OR_BOMB)"
    "        if (Mk64CpuFullPenalty3DS(player, sp1E == 1) || (sp1E == 1) || ((player->type & PLAYER_INVISIBLE_OR_BOMB)")
mk64_penalty_replace("void func_8002B830(Player* player, s8 playerId, s8 screenId) {"
    "void func_8002B830(Player* player, s8 playerId, s8 screenId) {\n    Mk64ItemHit3DSTick(player); // Shared human/CPU normal-driving recovery clock; native low/high triggers stay distinct.\n    if (Mk64NewItems3DSEnabled() && (player->type & PLAYER_CPU) && !(player->type & PLAYER_HUMAN) &&\n        (player->triggers & (HIT_TRIGGERS | HIT_BANANA_TRIGGER | SPINOUT_TRIGGER)))\n        player->triggers &= ~(SHROOM_TRIGGER | STAR_TRIGGER | BOO_TRIGGER); // A simultaneous CPU item must not clean_effect the incoming hit.")
# Match human per-wheel surface sampling (CPU variant assigns all four the
# centre triangle's material). Keep the human flag, steering and speed code intact.
mk64_penalty_replace("        func_8002A194(player, nextX, nextY, nextZ);"
    "        if (Mk64NewItems3DSEnabled() && (player->type & PLAYER_CPU) && !(player->type & PLAYER_HUMAN))\n            func_80029B4C(player, nextX, nextY, nextZ);\n        else func_8002A194(player, nextX, nextY, nextZ);")
# Native reverse gear/steering/collision remain intact. Modern Logic lets a
# held brake enter reverse only once stopped; OFF is native B + stick down.
mk64_penalty_replace("void func_80037CFC(Player* player, struct Controller* controller, s8 arg2) {"
    "#include \"reverse_3ds.inc.h\"\n\nvoid func_80037CFC(Player* player, struct Controller* controller, s8 arg2) {")
mk64_penalty_replace("((func_800388B0(controller) < (-0x31)) && (((player->speed / 18.0f) * 216.0f) <= 5.0f))"
    "(Mk64Reverse3DSCanStart(player, controller) && (((player->speed / 18.0f) * 216.0f) <= 5.0f))")
mk64_penalty_replace("(func_800388B0(controller) >= -0x1D) || (!(controller->button & B_BUTTON))"
    "Mk64Reverse3DSShouldStop(controller)")
# Retain the native force body byte-for-byte under a private name. Both cheap
# and full movement call the same wrapper AFTER apply_effect sets boost force.
mk64_penalty_replace("f32 func_80030150(Player* player, s8 arg1) {"
    "static f32 Mk64CpuNativeForce3DS(Player* player, s8 arg1) {")
string(APPEND CPU_PHYSICS_TEXT [=[

f32 func_80030150(Player* player, s8 arg1) {
    f32 nativeForce=Mk64CpuNativeForce3DS(player, arg1);
    f32 force=Mk64CpuDrive3DSLimitForce(player, nativeForce);
    if (force < nativeForce) {
        func_8002FE84(player, force);
        if (player->unk_098 > force) player->unk_098=force; // Steering force must not restore excess propulsion.
    }
    return force;
}
]=])
file(WRITE "${SPAGHETTIKART_3DS_PLAYER_CONTROLLER_SOURCE}" "${CPU_PHYSICS_TEXT}")
