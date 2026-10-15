# Generated native copies only. Deliberately separate AUTO from attract demo.
macro(mk64_auto_replace text old new)
    string(FIND "${${text}}" "${old}" MK64_AUTO_ANCHOR)
    if(MK64_AUTO_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream AUTO anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" ${text} "${${text}}")
endmacro()
# Scope presentation hooks to their native functions, not every A-button gate.
macro(mk64_auto_menu_function name next old new)
    string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "void ${name}(" AUTO_MENU_BEGIN)
    string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "void ${next}(" AUTO_MENU_END)
    if(AUTO_MENU_BEGIN LESS 0 OR AUTO_MENU_END LESS AUTO_MENU_BEGIN)
        message(FATAL_ERROR "Upstream AUTO result function boundaries changed")
    endif()
    math(EXPR AUTO_MENU_LENGTH "${AUTO_MENU_END}-${AUTO_MENU_BEGIN}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" ${AUTO_MENU_BEGIN} ${AUTO_MENU_LENGTH} AUTO_MENU_OLD)
    set(AUTO_MENU_NEW "${AUTO_MENU_OLD}")
    mk64_auto_replace(AUTO_MENU_NEW "${old}" "${new}")
    string(REPLACE "${AUTO_MENU_OLD}" "${AUTO_MENU_NEW}" SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
endmacro()
mk64_auto_menu_function(func_800AC458 func_800AC978
    "    switch (arg0->state) {"
    "    const bool autoConfirm = Mk64Auto3DSMenuConfirm(MK64_AUTO_CONFIRM_GP, arg0->state <= 11 && D_800DDB24);\n    switch (arg0->state) {")
# Pulse every native confirm/fast-forward gate in this result function only.
# Do not inject held/pressed state into the shared controllers or live race.
mk64_auto_menu_function(func_800AC458 func_800AC978
    "D_8018D9D8" "(D_8018D9D8 || autoConfirm)")
mk64_auto_menu_function(func_800ACA14 func_800ACC50
    "    switch (arg0->state) {"
    "    const bool autoConfirm = Mk64Auto3DSMenuConfirm(MK64_AUTO_CONFIRM_RETRY, arg0->state == 11 || arg0->state == 12);\n    switch (arg0->state) {")
mk64_auto_menu_function(func_800ACA14 func_800ACC50
    "            if (gControllerFive->buttonPressed & 0x9000) {"
    "            if ((gControllerFive->buttonPressed & 0x9000) || autoConfirm) {\n                if (autoConfirm) arg0->state = 11; // Poor finish: Retry, never Quit.")
mk64_auto_menu_function(func_800ACC50 func_800ACF40
    "    switch (arg0->state) {"
    "    const bool autoConfirm = Mk64Auto3DSMenuConfirm(MK64_AUTO_CONFIRM_SOLO, arg0->state >= 10 && arg0->state <= 13);\n    switch (arg0->state) {")
mk64_auto_menu_function(func_800ACC50 func_800ACF40
    "                if (gControllerFive->buttonPressed & 0x9000) {"
    "                if ((gControllerFive->buttonPressed & 0x9000) || autoConfirm) {")
mk64_auto_menu_function(func_800AF004 func_800AF1AC
    "    switch (arg0->state) {"
    "    const bool autoConfirm = Mk64Auto3DSMenuConfirm(MK64_AUTO_CONFIRM_CEREMONY, arg0->state <= 5);\n    switch (arg0->state) {")
mk64_auto_menu_function(func_800AF004 func_800AF1AC
    "((gControllerFive->buttonPressed != 0) || (gControllerFive->stickPressed != 0)))"
    "((gControllerFive->buttonPressed != 0) || (gControllerFive->stickPressed != 0) || autoConfirm))")
set(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "#include \"auto_mode_3ds.h\"\n${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
set(AUTO_CEREMONY_SOURCE "${SPAGHETTIKART_ROOT}/src/ending/code_80281780.c")
set(AUTO_CEREMONY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/ceremony_auto_3ds.c")
file(READ "${AUTO_CEREMONY_SOURCE}" AUTO_CEREMONY_TEXT)
mk64_auto_replace(AUTO_CEREMONY_TEXT
    "    debug_switch_character_ceremony_cutscene();\n    func_802818BC();"
    "    debug_switch_character_ceremony_cutscene();\n    Mk64Auto3DSPrepareCeremony();\n    func_802818BC();")
list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${AUTO_CEREMONY_SOURCE}")
list(APPEND SPAGHETTIKART_GAME_RECURSIVE "${AUTO_CEREMONY_GENERATED}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AUTO_CEREMONY_SOURCE}")
file(WRITE "${AUTO_CEREMONY_GENERATED}" "#include \"auto_mode_3ds.h\"\n${AUTO_CEREMONY_TEXT}")
mk64_auto_replace(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
    "    if (gModeSelection != BATTLE) {\n        init_players();"
    "    Mk64Auto3DSPreparePlayers();\n    if (gModeSelection != BATTLE) {\n        init_players();")
file(WRITE "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
    "#include \"auto_mode_3ds.h\"\n${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
mk64_auto_replace(CPU_ITEMS_TEXT "    if (gDemoMode == DEMO_MODE_INACTIVE) {"
    "    if (gDemoMode == DEMO_MODE_INACTIVE && !Mk64Auto3DSActive()) {")
mk64_auto_replace(CPU_ITEMS_TEXT "void func_8000F124(void) {"
    "void func_8000F124(void) {\n    if (Mk64Auto3DSActive()) return; // No principal-rival selection for spectators.")
mk64_auto_replace(CPU_ITEMS_TEXT "        for (var_s0 = 0; var_s0 < gPlayerCount; var_s0++) {"
    "        for (var_s0 = 0; var_s0 < (Mk64Auto3DSActive() ? NUM_PLAYERS : gPlayerCount); var_s0++) {")
mk64_auto_replace(CPU_ITEMS_TEXT "    if ((gDemoUseController == 1) && (!IsPodiumCeremony())) {"
    "    if (Mk64Auto3DSActive()) {\n        for (i = 0; i < NUM_PLAYERS; ++i) { D_80163330[i] = 0; D_8016334C[i] = 0; }\n        D_80163330[0] = 1; // Chosen driver alone uses the native principal-rival target.\n        D_80163344[0] = D_80163344[1] = 0;\n    }\n    if ((gDemoUseController == 1) && (!IsPodiumCeremony()) && !Mk64Auto3DSActive()) {")
file(WRITE "${SPAGHETTIKART_3DS_CPU_ITEMS_SOURCE}"
    "#include \"auto_mode_3ds.h\"\n${CPU_ITEMS_TEXT}")
# Only the chosen original slot completes the race; observing another car never
# changes the cup's result or marks that car human.
mk64_auto_replace(ITEM_HIT_TEXT "            if ((gPlayers[i].type & PLAYER_HUMAN) != 0) {"
    "            if ((gPlayers[i].type & PLAYER_HUMAN) != 0 || (Mk64Auto3DSActive() && i == 0)) {")
mk64_auto_replace(ITEM_HIT_TEXT
    "        if (!(player->type & PLAYER_HUMAN)) {\n            continue;\n        }\n        if (player->type & PLAYER_CPU) {\n            continue;\n        }"
    "        if (!(Mk64Auto3DSActive() && i == 0)) {\n            if (!(player->type & PLAYER_HUMAN) || (player->type & PLAYER_CPU)) continue;\n        }")
file(WRITE "${ITEM_HIT_RACE_LOGIC_3DS}"
    "#include \"auto_mode_3ds.h\"\n#include \"item_rules_3ds.h\"\nextern void Mk64RaceClock3DSReset(void);\n${ITEM_HIT_TEXT}")
# Catch all native camera tick callers, not just GameCamera::Tick.
set(MK64_AUTO_CAMERA_SOURCE "${SPAGHETTIKART_ROOT}/src/camera.c")
set(MK64_AUTO_CAMERA_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/camera_auto_3ds.c")
file(READ "${MK64_AUTO_CAMERA_SOURCE}" MK64_AUTO_CAMERA_TEXT)
mk64_auto_replace(MK64_AUTO_CAMERA_TEXT
    "void func_8001EE98(Player* player, Camera* camera, s8 index) {"
    "void func_8001EE98(Player* player, Camera* camera, s8 index) {\n    if (Mk64Auto3DSCameraTick(camera)) return;")
# Native FOV is dispatched for player zero; when watching driver 5--8, use
# that driver's effects but viewport zero's FOUR-slot camera effect arrays.
mk64_auto_replace(MK64_AUTO_CAMERA_TEXT
    "    Camera* camera = CM_GetPlayerCamera(playerIndex);"
    "    const bool autoView = Mk64Auto3DSActive() && gGamestate == RACING;\n    Camera* camera = autoView ? gScreenContexts[0].raceCamera : CM_GetPlayerCamera(playerIndex);\n    if (autoView) player = &gPlayers[Mk64Auto3DSObservedPlayer()];")
mk64_auto_replace(MK64_AUTO_CAMERA_TEXT
    "    if (D_80164A08[playerIndex] == 0) {"
    "    if (autoView) playerIndex = 0; // Viewport slot, not watched driver ID.\n    if (D_80164A08[playerIndex] == 0) {")
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${MK64_AUTO_CAMERA_SOURCE}")
list(APPEND SPAGHETTIKART_GAME_ROOT "${MK64_AUTO_CAMERA_GENERATED}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MK64_AUTO_CAMERA_SOURCE}")
file(WRITE "${MK64_AUTO_CAMERA_GENERATED}" "#include \"auto_mode_3ds.h\"\n${MK64_AUTO_CAMERA_TEXT}")

# Only the sound gates of three native boost-entry functions change. Never
# impersonate a human CPU (that would alter controls, physics and item pickup).
set(MK64_AUTO_EFFECTS_SOURCE "${SPAGHETTIKART_ROOT}/src/effects.c")
file(READ "${MK64_AUTO_EFFECTS_SOURCE}" MK64_AUTO_EFFECTS_TEXT)
foreach(MK64_BOOST_PAIR "trigger_shroom|apply_boost_effect" "trigger_asphalt_ramp_boost|apply_boost_ramp_asphalt_effect" "trigger_wood_ramp_boost|apply_boost_ramp_wood_effect")
    string(REPLACE "|" ";" MK64_BOOST_NAMES "${MK64_BOOST_PAIR}")
    list(GET MK64_BOOST_NAMES 0 MK64_BOOST_START_NAME)
    list(GET MK64_BOOST_NAMES 1 MK64_BOOST_END_NAME)
    string(FIND "${MK64_AUTO_EFFECTS_TEXT}" "void ${MK64_BOOST_START_NAME}(" MK64_BOOST_START)
    string(FIND "${MK64_AUTO_EFFECTS_TEXT}" "void ${MK64_BOOST_END_NAME}(" MK64_BOOST_END)
    if(MK64_BOOST_START LESS 0 OR MK64_BOOST_END LESS MK64_BOOST_START)
        message(FATAL_ERROR "Upstream AUTO boost sound boundaries changed")
    endif()
    math(EXPR MK64_BOOST_LENGTH "${MK64_BOOST_END}-${MK64_BOOST_START}")
    string(SUBSTRING "${MK64_AUTO_EFFECTS_TEXT}" ${MK64_BOOST_START} ${MK64_BOOST_LENGTH} MK64_BOOST_ORIGINAL)
    set(MK64_BOOST_NEW "${MK64_BOOST_ORIGINAL}")
    mk64_auto_replace(MK64_BOOST_NEW "((player->type & PLAYER_HUMAN) == PLAYER_HUMAN)"
        "(((player->type & PLAYER_HUMAN) == PLAYER_HUMAN) || Mk64Auto3DSWatchedAudio(player - gPlayerOne))")
    string(REPLACE "${MK64_BOOST_ORIGINAL}" "${MK64_BOOST_NEW}" MK64_AUTO_EFFECTS_TEXT "${MK64_AUTO_EFFECTS_TEXT}")
endforeach()
set(MK64_AUTO_EFFECTS_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_auto_3ds.c")
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${MK64_AUTO_EFFECTS_SOURCE}")
list(APPEND SPAGHETTIKART_GAME_ROOT "${MK64_AUTO_EFFECTS_GENERATED}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MK64_AUTO_EFFECTS_SOURCE}")
file(WRITE "${MK64_AUTO_EFFECTS_GENERATED}" "#include \"auto_mode_3ds.h\"\n${MK64_AUTO_EFFECTS_TEXT}")
