# Keep animation selection, player updates, wheel palettes, palette ownership
# and DMA completion on suppressed ticks. Only per-image matrix/command work
# is omitted. Do not skip render_players(): it mutates gameplay-visible state.
string(PREPEND SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "#include \"game_runtime_3ds.h\"\n")
# Match load_kart_texture_and_render_kart_particles(): screens 3/4 reuse
# screen banks 1/2 and player slots 5..8. The old draw-side expression used
# screenId-1/playerId-4, indexing outside both dimensions in split-screen.
string(REPLACE "[screenId - 1][playerId - 4].unk_00"
    "[screenId - 2][playerId + 4].unk_00"
    SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
string(REPLACE "load_kart_texture(player, playerId - 4, screenId, screenId - 1,"
    "load_kart_texture(player, playerId + 4, screenId, screenId - 2,"
    SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
function(mk64_kart_visual_scope start_marker end_marker state_start state_end open_marker)
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${start_marker}" start)
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${end_marker}" end)
    if(start LESS 0 OR end LESS start)
        message(FATAL_ERROR "Kart visual function boundary changed")
    endif()
    math(EXPR length "${end}-${start}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" ${start} ${length} before)
    string(FIND "${before}" "${state_start}" state_begin)
    string(FIND "${before}" "${state_end}" state_finish)
    string(FIND "${before}" "${open_marker}" opening)
    if(state_begin LESS 0 OR state_finish LESS state_begin OR opening LESS 0)
        message(FATAL_ERROR "Kart visual state preparation boundary changed")
    endif()
    math(EXPR state_length "${state_finish}-${state_begin}")
    string(SUBSTRING "${before}" ${state_begin} ${state_length} state)
    string(REPLACE "${state}" "" after "${before}")
    string(REPLACE "${open_marker}"
        "${state}    if (Mk64Graphics3DSSkipVisualPreparation()) return;\n${open_marker}"
        after "${after}")
    string(REPLACE "${before}" "${after}" changed "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    set(SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${changed}" PARENT_SCOPE)
endfunction()
mk64_kart_visual_scope("void render_kart(" "void render_ghost("
    "    load_kart_palette(player, playerId, screenId, D_801651D0[screenId][playerId]);"
    "    mtxf_translate_rotate(mtx, sp154, sp14C);"
    "    FrameInterpolation_RecordOpenChild(\"player_kart\", (playerId << 4) | screenId);")
mk64_kart_visual_scope("void render_ghost(" "void func_80025DE8("
    "#ifdef AVOID_UB\n    gPlayerPalette ="
    "    mtxf_translate_rotate(mtx, spDC, spD4);"
    "    FrameInterpolation_RecordOpenChild(\"player_ghost\", (playerId << 4) | screenId);")
foreach(signature
        "void render_player_shadow(Player* player, s8 playerId, s8 screenId) {"
        "void render_player_shadow_credits(Player* player, s8 playerId, s8 screenId) {")
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${signature}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Kart shadow declaration changed")
    endif()
    string(REPLACE "${signature}" "${signature}\n    if (Mk64Graphics3DSSkipVisualPreparation()) return;"
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
endforeach()
foreach(signature
        "void func_80025DE8(Player* player, s8 playerId, s8 screenId, s8 flipOffset) {"
        "void render_player_ice_reflection(Player* player, s8 playerId, s8 screenId, s8 flipOffset) {")
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${signature}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Kart effect declaration changed")
    endif()
    string(REPLACE "${signature}"
        "${signature}\n    if (Mk64Graphics3DSSkipVisualPreparation()) { ++gMatrixEffectCount; return; }"
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
endforeach()
