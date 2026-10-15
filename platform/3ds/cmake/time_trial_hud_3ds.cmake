# Keep native distance-meter portraits in Time Trials, but not their rank digits.
# Work on a generated copy: never modify the locally preserved upstream tree.
set(MK64_RENDER_OBJECTS_SOURCE "${SPAGHETTIKART_ROOT}/src/render_objects.c")
set(MK64_RENDER_OBJECTS_PORT "${CMAKE_CURRENT_BINARY_DIR}/generated/render_objects_3ds.c")
file(READ "${MK64_RENDER_OBJECTS_SOURCE}" MK64_RENDER_OBJECTS_TEXT)
set(MK64_TT_RANK_BLOCK [=[        gDPLoadTLUT_pal256(gDisplayListHead++, common_tlut_hud_type_C_rank_tiny_font);
        rsp_load_texture(common_texture_hud_type_C_rank_tiny_font[arg1 + 1], 8, 8);
        if (spB8 != 0) {
            func_80042330_unchanged(spD0 + 0x26, (spC4 + spCC) + 4, 0U, 1.0f);
        } else {
            func_80042330_unchanged(spD0 + 0x1B, (spC4 + spCC) + 4, 0U, 1.0f);
        }
        gSPDisplayList(gDisplayListHead++, D_0D006950);]=])
string(FIND "${MK64_RENDER_OBJECTS_TEXT}" "${MK64_TT_RANK_BLOCK}" MK64_TT_RANK_ANCHOR)
if(MK64_TT_RANK_ANCHOR LESS 0)
    message(FATAL_ERROR "Upstream distance-meter rank draw block changed")
endif()
string(REPLACE "${MK64_TT_RANK_BLOCK}"
    "        if (gModeSelection != TIME_TRIALS) {\n${MK64_TT_RANK_BLOCK}\n        }"
    MK64_RENDER_OBJECTS_TEXT "${MK64_RENDER_OBJECTS_TEXT}")
set(MK64_RENDER_OBJECTS_EXISTING "")
if(EXISTS "${MK64_RENDER_OBJECTS_PORT}")
    file(READ "${MK64_RENDER_OBJECTS_PORT}" MK64_RENDER_OBJECTS_EXISTING)
endif()
if(NOT MK64_RENDER_OBJECTS_EXISTING STREQUAL MK64_RENDER_OBJECTS_TEXT)
    file(WRITE "${MK64_RENDER_OBJECTS_PORT}" "${MK64_RENDER_OBJECTS_TEXT}")
endif()
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${MK64_RENDER_OBJECTS_SOURCE}")
list(APPEND SPAGHETTIKART_GAME_ROOT "${MK64_RENDER_OBJECTS_PORT}")
