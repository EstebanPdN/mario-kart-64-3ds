# Four solo menu rows without changing the native [4][3] table ABI used by
# other translation units. Guard generated-source integration against drift.
foreach(text MENU_DETAIL_TEXT SPAGHETTIKART_3DS_MENU_ITEMS_TEXT)
    string(REGEX REPLACE "gGameModePlayerSelection\\[([^\n;]+)\\]\\[gGameModeMenuColumn\\[([^\n;]+)\\]\\]"
        "Mk64SoloMode3DS(\\1, gGameModeMenuColumn[\\2])" ${text} "${${text}}")
    string(REGEX REPLACE "gGameModeSubMenuColumn\\[([^\n;]+)\\]\\[gGameModeMenuColumn\\[([^\n;]+)\\]\\]"
        "(*Mk64SoloSubMode3DS(\\1, gGameModeMenuColumn[\\2]))" ${text} "${${text}}")
    string(REGEX REPLACE "gPlayerModeSelection\\[([^]\n]+)\\]"
        "Mk64SoloModeLimit3DS(\\1)" ${text} "${${text}}")
    set(${text} "#include \"solo_modes_3ds.h\"\n${${text}}")
endforeach()
foreach(pair "Default|false" "Extra|true")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 name)
    list(GET parts 1 extra)
    mk64_auto_replace(MENU_DETAIL_TEXT
        "sGameModePlayerColumn${name}[gPlayerCount - 1][gGameModeMenuColumn[gPlayerCount - 1]]"
        "Mk64SoloLevelLimit3DS(gPlayerCount - 1, gGameModeMenuColumn[gPlayerCount - 1], ${extra})")
endforeach()
file(WRITE "${SPAGHETTIKART_3DS_MENUS_SOURCE}" "${MENU_DETAIL_TEXT}")
# Reuse the four original mode textures and the existing column animation.
mk64_auto_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "MenuTexture* D_800E8234[] = {"
    "static MenuTexture sSoloColumn3DS[] = {\n    {1, texture_menu_1p_game, 64, 54, 0, 0, 0},\n    {1, texture_mode_mario_gp, 64, 18, 0, 65, 0},\n    {1, texture_mode_time_trials, 64, 18, 0, 83, 0},\n    {1, texture_mode_vs, 64, 18, 0, 101, 0},\n    {1, texture_mode_battle, 64, 18, 0, 119, 0},\n    {0, NULL, 0, 0, 0, 0, 0}\n};\nMenuTexture* D_800E8234[] = {")
string(REPLACE "seg2_menu_1p_column" "sSoloColumn3DS" SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
mk64_auto_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "&D_800E7300[((gPlayerCount - 2) * 4) + temp_a2]"
    "&D_800E7300[((gPlayerCount < 2 ? 0 : gPlayerCount - 2) * 4) + temp_a2]")
mk64_auto_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "    switch (gPlayerCount) {\n        case 2:\n            func_800A69C8(arg0);"
    "    switch (gPlayerCount) {\n        case 1:\n        case 2:\n            func_800A69C8(arg0);")
# Keep the native cumulative win counter, but a single human has no second
# human score to separate. Leave the original two-player presentation intact.
mk64_auto_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "    // EUC-JP character which sort of looks like a hyphen -"
    "    if (!Mk64SoloVs3DS() && !Mk64SoloBattle3DS())\n    // EUC-JP character which sort of looks like a hyphen -")
# The native VS/Battle result menu starts its screen fade without the audio
# fade used by GP/pause menus. Wire the actual A/Start branch, not just the
# fade helper: all four choices leave the current result music behind.
mk64_auto_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "                    func_8009DFE0(0x0000001E);\n                    play_sound2(SOUND_MENU_OK_CLICKED);\n                    if (gModeSelection == VERSUS) {"
    "                    if (Mk64SoloVs3DS() || Mk64SoloBattle3DS()) func_800CA330(0x19U);\n                    func_8009DFE0(0x0000001E);\n                    play_sound2(SOUND_MENU_OK_CLICKED);\n                    if (gModeSelection == VERSUS) {")
file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
mk64_auto_replace(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
    "            case SCREEN_MODE_1P:\n                switch (gModeSelection) {\n                    case GRAND_PRIX:"
    "            case SCREEN_MODE_1P:\n                switch (gModeSelection) {\n                    case VERSUS:\n                    case GRAND_PRIX:")
mk64_auto_replace(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
    "                        spawn_players_gp_one_player(D_80165210, D_80165230, sp5A);"
    "                        if (Mk64SoloVs3DS()) Mk64SoloVs3DSSpawn(D_80165210, D_80165230, sp5A);\n                        else spawn_players_gp_one_player(D_80165210, D_80165230, sp5A);")
mk64_auto_replace(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
    "    } else if (IsBlockFort()) {"
    "    } else if (Mk64SoloBattle3DS()) {\n        Mk64SoloBattle3DSSpawn();\n    } else if (IsBlockFort()) {")
file(WRITE "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}" "#include \"solo_modes_3ds.h\"\n#include \"auto_mode_3ds.h\"\n${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
mk64_auto_replace(CPU_ITEMS_TEXT "            var_t4 = gPlayerCount;" "            var_t4 = Mk64SoloVs3DS() ? NUM_PLAYERS : gPlayerCount;")
mk64_auto_replace(CPU_ITEMS_TEXT "            var_a2 = gPlayerCount;" "            var_a2 = Mk64SoloVs3DS() ? NUM_PLAYERS : gPlayerCount;")
mk64_auto_replace(CPU_ITEMS_TEXT "void update_player(s32 playerId) {"
    "void update_player(s32 playerId) {\n    if (Mk64SoloBattle3DS()) { Mk64SoloBattle3DSTick(playerId); return; }")
# The original racing CPU policy excludes shells and aims thrown bananas at
# road waypoints. Newly introduced arena bots need box-driven inventory and
# spatial targeting even when optional modern race rules are disabled.
mk64_auto_replace(CPU_ITEMS_TEXT
    "if (!Mk64NewItems3DSEnabled()) { Mk64OriginalCpuBranch3DS(playerId, branch, itemId); return; }"
    "if (!Mk64NewItems3DSEnabled() && !Mk64SoloBattle3DS()) { Mk64OriginalCpuBranch3DS(playerId, branch, itemId); return; }")
mk64_auto_replace(CPU_ITEMS_TEXT
    "if (!Mk64NewItems3DSEnabled()) { Mk64OriginalCpuUse3DS(playerId); return; }"
    "if (!Mk64NewItems3DSEnabled() && !Mk64SoloBattle3DS()) { Mk64OriginalCpuUse3DS(playerId); return; }")
string(APPEND CPU_ITEMS_TEXT [=[

void Mk64SoloBattle3DSItems(int id) {
    Mk64CpuAdvanceItemBox3DS(id, &cpu_ItemStrategy[id]);
    CpuItemStrategyData* strategy = &cpu_ItemStrategy[id];
    switch (strategy->branch) {
        case CPU_STRATEGY_HOLD_GREEN_SHELL:
        case CPU_STRATEGY_HOLD_RED_SHELL:
        case CPU_STRATEGY_ORBIT_TRIPLE_GREEN_SHELL:
        case CPU_STRATEGY_ORBIT_TRIPLE_RED_SHELL:
            // Seek a visible target instead of spending shells at a wall or
            // into empty space while navigating to combat. Triple children
            // still get the native assembly delay, never instant firing.
            if (Mk64CpuItemTarget3DS(id, 0) < 0) strategy->timer = 0;
            strategy->timeBeforeThrow =
                strategy->branch == CPU_STRATEGY_ORBIT_TRIPLE_GREEN_SHELL ||
                strategy->branch == CPU_STRATEGY_ORBIT_TRIPLE_RED_SHELL ? 50 : 4;
            break;
        default: break;
    }
    cpu_use_item_strategy(id);
}
]=])
file(WRITE "${SPAGHETTIKART_3DS_CPU_ITEMS_SOURCE}" "#include \"solo_modes_3ds.h\"\n#include \"auto_mode_3ds.h\"\n${CPU_ITEMS_TEXT}")
# Full-screen solo controls must also dispatch in VS/Battle.
mk64_auto_replace(CPU_PHYSICS_TEXT
    "    if ((player->type & PLAYER_CPU) && (player->surfaceType == OUT_OF_BOUNDS) && !(player->effects & 8)) {"
    "    if ((player->type & PLAYER_CPU) && (player->surfaceType == OUT_OF_BOUNDS) && !(player->effects & 8) &&\n        (!Mk64SoloBattle3DS() || !(player->lakituProps & (HELD_BY_LAKITU | LAKITU_SCENE)))) {")
# A persistent OOB contact may last through multiple rescue steps. Restarting
# func_80090778 each step resets unk_222 to zero, stranding the body forever.
# Leave the native racing policy untouched; arena bots resume native rescue.
mk64_auto_replace(CPU_PHYSICS_TEXT
    "                case GRAND_PRIX:\n                    handle_a_press_for_player_during_race"
    "                case VERSUS:\n                case BATTLE:\n                case GRAND_PRIX:\n                    handle_a_press_for_player_during_race")
file(WRITE "${SPAGHETTIKART_3DS_PLAYER_CONTROLLER_SOURCE}" "${CPU_PHYSICS_TEXT}")
mk64_auto_replace(CPU_PHYSICS_TEXT
    "                    case TIME_TRIALS:\n                    case VERSUS:\n                        func_80028C44(player, camera, playerId, screenId);"
    "                    case TIME_TRIALS:\n                    case VERSUS:\n                        if (Mk64SoloVs3DS() && (player->type & PLAYER_CPU))\n                            func_80028864(player, camera, playerId, screenId);\n                        else func_80028C44(player, camera, playerId, screenId);")
file(WRITE "${SPAGHETTIKART_3DS_PLAYER_CONTROLLER_SOURCE}" "#include \"solo_modes_3ds.h\"\n${CPU_PHYSICS_TEXT}")
mk64_auto_replace(SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "void func_8002276C(void) {"
    "void func_8002276C(void) {\n    if (Mk64SoloVs3DS() || Mk64SoloBattle3DS()) {\n        for (int id = 0; id < (Mk64SoloVs3DS() ? 8 : 4); ++id) func_80022A98(&gPlayers[id], id);\n        return;\n    }")
file(WRITE "${SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE}" "#include \"solo_modes_3ds.h\"\n${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
mk64_auto_replace(SPAGHETTIKART_3DS_HUD_TEXT "void func_8006E058(void) {"
    "void func_8006E058(void) {\n    if (Mk64SoloVs3DS() || Mk64SoloBattle3DS()) {\n        for (int id = 0; id < (Mk64SoloVs3DS() ? 8 : 4); ++id) func_8006E420(&gPlayers[id], id, 0);\n        return;\n    }")
file(WRITE "${SPAGHETTIKART_3DS_HUD_SOURCE}" "#include \"solo_modes_3ds.h\"\n${SPAGHETTIKART_3DS_HUD_TEXT}")
mk64_auto_replace(ITEM_HIT_TEXT "                        switch (gPlayerCountSelection1) {\n                            case 2:"
    "                        switch (gPlayerCountSelection1) {\n                            case 1:\n                                gPlayerWinningIndex = gPlayerPositionLUT[0];\n                                if (currentPosition == 0 && gNmiUnknown1[0] < 99) ++gNmiUnknown1[0];\n                                D_802BA038 = 690;\n                                func_8028E298();\n                                gRaceState = RACE_FINISHED;\n                                break;\n                            case 2:")
mk64_auto_replace(ITEM_HIT_TEXT
    "    switch (gPlayerCountSelection1) {\n        case 2:\n            *(gNmiUnknown4 + gPlayerWinningIndex) += 1;"
    "    switch (gPlayerCountSelection1) {\n        case 1:\n            if (gPlayerWinningIndex == 0 && gNmiUnknown4[0] < 99) ++gNmiUnknown4[0];\n            break;\n        case 2:\n            *(gNmiUnknown4 + gPlayerWinningIndex) += 1;")
mk64_auto_replace(ITEM_HIT_TEXT "    ScreenContext* temp_v0 = &gScreenContexts[gPlayerWinningIndex];"
    "    ScreenContext* temp_v0 = &gScreenContexts[(Mk64SoloVs3DS() || Mk64SoloBattle3DS()) ? 0 : gPlayerWinningIndex];")
mk64_auto_replace(ITEM_HIT_TEXT
    "            if (!(player->type & PLAYER_HUMAN) || (player->type & PLAYER_CPU)) continue;"
    "            if (!(Mk64SoloBattle3DS() && i == 0) && (!(player->type & PLAYER_HUMAN) || (player->type & PLAYER_CPU))) continue;")
# Notify audio after the battle actually ends, not during an elimination.
mk64_auto_replace(ITEM_HIT_TEXT
    "    func_800CA118((u8) gPlayerWinningIndex);\n    gRaceState = RACE_FINISHED;"
    "    gRaceState = RACE_FINISHED;\n    func_800CA118((u8) gPlayerWinningIndex);")
mk64_auto_replace(ITEM_HIT_TEXT "    D_802BA038 = 10;"
    "    D_802BA038 = Mk64SoloBattle3DS() ? 360 : 10; // Let the finish jingle/voice precede the results menu.")
file(WRITE "${ITEM_HIT_RACE_LOGIC_3DS}" "#include \"solo_modes_3ds.h\"\n#include \"auto_mode_3ds.h\"\n#include \"item_rules_3ds.h\"\nextern void Mk64RaceClock3DSReset(void);\n${ITEM_HIT_TEXT}")

# Once its one-use bomb is consumed, slot zero keeps controller ownership for
# pause/quit while a surviving CPU becomes the camera subject, never human.
mk64_auto_replace(MK64_AUTO_CAMERA_TEXT
    "void func_8001EE98(Player* player, Camera* camera, s8 index) {"
    "void func_8001EE98(Player* player, Camera* camera, s8 index) {\n    Mk64SoloBattle3DSCamera(camera);\n    if (Mk64SoloBattle3DS() && camera == gScreenContexts[0].raceCamera) player = &gPlayers[camera->playerId];")
mk64_auto_replace(MK64_AUTO_CAMERA_TEXT
    "if ((gDemoMode == 1) || ((D_8015F894 == 2) && (D_80164A89 == 1))) {"
    "if (!Mk64SoloBattle3DS() && ((gDemoMode == 1) || ((D_8015F894 == 2) && (D_80164A89 == 1)))) {")
file(WRITE "${MK64_AUTO_CAMERA_GENERATED}" "#include \"solo_modes_3ds.h\"\n#include \"auto_mode_3ds.h\"\n${MK64_AUTO_CAMERA_TEXT}")
# Fade-to-bomb is native. Keep CPU bombs autonomous; the native transition
# otherwise leaves every eliminated racer marked human, waiting for a pad.
mk64_auto_replace(MK64_AUTO_EFFECTS_TEXT
    "        player->type |= PLAYER_UNKNOWN_0x40;"
    "        player->type |= PLAYER_UNKNOWN_0x40;\n        if (Mk64SoloBattle3DS() && (playerIndex > 0 || Mk64Auto3DSActive())) {\n            player->type &= ~PLAYER_HUMAN;\n            player->type |= PLAYER_CPU;\n        }")
file(WRITE "${MK64_AUTO_EFFECTS_GENERATED}" "#include \"solo_modes_3ds.h\"\n#include \"auto_mode_3ds.h\"\n${MK64_AUTO_EFFECTS_TEXT}")

# Reuse the entire native GP finish audio scheduler for solo VS, including
# rank-sensitive jingles, tick-30 voice, long result music and engine fade.
# Do not spoof gModeSelection or change playback channel/script tables.
string(FIND "${SPATIAL_AUDIO_TEXT}" "void func_800C76C0(" SOLO_AUDIO_START)
string(FIND "${SPATIAL_AUDIO_TEXT}" "void func_800C847C(" SOLO_AUDIO_END)
math(EXPR SOLO_AUDIO_LENGTH "${SOLO_AUDIO_END}-${SOLO_AUDIO_START}")
if(SOLO_AUDIO_START LESS 0 OR SOLO_AUDIO_LENGTH LESS 1)
    message(FATAL_ERROR "Solo finish audio scheduler boundaries changed")
endif()
string(SUBSTRING "${SPATIAL_AUDIO_TEXT}" ${SOLO_AUDIO_START} ${SOLO_AUDIO_LENGTH} SOLO_AUDIO_ORIGINAL)
set(SOLO_AUDIO_FIXED "${SOLO_AUDIO_ORIGINAL}")
mk64_auto_replace(SOLO_AUDIO_FIXED "void func_800C76C0(u8 playerId) {"
    "void func_800C76C0(u8 playerId) {\n    const s32 finishMode = Mk64SoloVs3DS() ? GRAND_PRIX : gModeSelection;")
string(REPLACE "switch (gModeSelection)" "switch (finishMode)" SOLO_AUDIO_FIXED "${SOLO_AUDIO_FIXED}")
mk64_auto_replace(SOLO_AUDIO_FIXED
    "case BATTLE:\n                    switch (D_800EA1C0) { /* switch 2; irregular */\n                        case 1:"
    "case BATTLE:\n                    switch (D_800EA1C0) { /* switch 2; irregular */\n                        case 0: // Solo Battle result is relative to the human, not the CPU winner.\n                            func_800C3448(0x100100FF);\n                            func_800C3448(0x110100FF);\n                            func_800C5278(5U);\n                            func_800C9060(playerId, 0x1900F103U);\n                            play_sequences(gPlayerWinningIndex == 0 ? MUSIC_SEQ_FINISH_1ST_PLACE : MUSIC_SEQ_FINISH_5TH_8TH_PLACE,\n                                           gPlayerWinningIndex == 0 ? MUSIC_SEQ_VS_BATTLE_RESULTS : MUSIC_SEQ_LOSING_RESULTS);\n                            D_800EA0EC[playerId] = 2;\n                            break;\n                        case 1:")
mk64_auto_replace(SOLO_AUDIO_FIXED
    "case BATTLE:          /* switch 3 */\n                    break;"
    "case BATTLE:          /* switch 3 */\n                    if (Mk64SoloBattle3DS()) {\n                        D_800EA0EC[playerId] = 2;\n                        func_800C90F4(playerId, (gPlayers[0].characterId * 0x10) +\n                            (gPlayerWinningIndex == 0 ? SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x0D) : SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x03)));\n                    }\n                    break;")
string(REPLACE "${SOLO_AUDIO_ORIGINAL}" "${SOLO_AUDIO_FIXED}" SPATIAL_AUDIO_TEXT "${SPATIAL_AUDIO_TEXT}")
mk64_auto_replace(SPATIAL_AUDIO_TEXT "void func_800CA118(u8 arg0) {"
    "void func_800CA118(u8 arg0) {\n    if (Mk64SoloBattle3DS()) {\n        if (gRaceState != RACE_FINISHED) {\n            func_800C90F4(arg0, (gPlayers[arg0].characterId * 0x10) + SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x03));\n            return; // Losing one kart must not stop arena music.\n        }\n        arg0 = 0; // Result music belongs to the sole audio listener, not a bot ID.\n    }")
mk64_auto_replace(SPATIAL_AUDIO_TEXT "void func_800CA330(u8 arg0) {"
    "void func_800CA330(u8 arg0) {\n    if (gGamestate == RACING && (Mk64SoloVs3DS() || Mk64SoloBattle3DS())) {\n        D_800E9EA4[0] = 0; // Do not restart the result during a pause/menu exit.\n        D_800EA0EC[0] = 0;\n        D_800EA104 = 0;\n        func_800C3448(0xC0F00000U); // Cancel both pending short->long setup queues.\n        func_800C3448(0xC1F00000U);\n        if (arg0 > 10) arg0 = 10; // Complete before the native 30-tick black fade.\n    }")
file(WRITE "${SPAGHETTIKART_3DS_AUDIO_EXTERNAL_SOURCE}" "#include \"solo_modes_3ds.h\"\n${SPATIAL_AUDIO_TEXT}")
# Original item tables have no one-player VS case. Its eight-car race uses
# the GP probability tables; Battle uses the existing arena table verbatim.
set(SOLO_MANAGER_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/RaceManager.cpp")
set(SOLO_MANAGER_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/RaceManager_solo_3ds.cpp")
file(READ "${SOLO_MANAGER_SOURCE}" SOLO_MANAGER_TEXT)
mk64_auto_replace(SOLO_MANAGER_TEXT "                case TWO_PLAYERS_SELECTED:"
    "                case ONE_PLAYERS_SELECTED:\n                    humanTableName = \"mk:human_grand_prix\";\n                    cpuTableName = \"mk:cpu_grand_prix\";\n                    break;\n                case TWO_PLAYERS_SELECTED:")
file(WRITE "${SOLO_MANAGER_GENERATED}" "${SOLO_MANAGER_TEXT}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOLO_MANAGER_SOURCE}")
