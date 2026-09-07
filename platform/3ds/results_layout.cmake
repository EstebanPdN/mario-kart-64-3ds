# Keep the upstream checkout intact; adapt the GP and Time Trial result renderers.
foreach(pair IN ITEMS "func_800A2EB8|func_800A32B4|results_times_3ds.inc.c"
                      "func_800A34A8|func_800A3A10|results_points_3ds.inc.c"
                      "time_trials_finish_text_render|render_lap_time|results_time_trial_3ds.inc.c")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 first)
    list(GET parts 1 next)
    list(GET parts 2 source)
    string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "void ${first}(" begin)
    string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "void ${next}(" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "The upstream GP results insertion points changed")
    endif()
    set(path "${CMAKE_CURRENT_LIST_DIR}/source/${source}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" replacement)
    string(SUBSTRING "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" 0 ${begin} prefix)
    string(SUBSTRING "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" ${end} -1 suffix)
    set(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${prefix}${replacement}\n${suffix}")
endforeach()
string(REPLACE "s8 gTextColor;"
    "s8 gTextColor;\n#include \"settings_3ds.h\"\nstatic int sMk64ResultsText3DS = 0;"
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
foreach(renderer IN ITEMS func_800A2EB8 func_800A34A8 time_trials_finish_text_render func_800A3E60)
    string(REPLACE "                ${renderer}(arg0);"
        "                sMk64ResultsText3DS = 1;\n                ${renderer}(arg0);\n                sMk64ResultsText3DS = 0;"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
endforeach()
set(signature "Gfx* print_letter(Gfx* arg0, MenuTexture* glyphTexture, f32 arg2, f32 arg3, s32 mode, f32 scaleX, f32 scaleY) {")
string(REPLACE "${signature}" "${signature}
    if (sMk64ResultsText3DS && mode != 3) {
        arg0 = print_letter(arg0, glyphTexture, arg2 + 1.0f, arg3 + 1.0f, 3, scaleX, scaleY);
    }"
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
# The shadow uses the same intensity-mask glyph, with constant black RGB.
# Its following normal glyph reinstates the stock combiner and gradient.
string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "${signature}" begin)
string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "Gfx* print_letter_wide_right(" end)
math(EXPR length "${end} - ${begin}")
string(SUBSTRING "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" ${begin} ${length} original)
string(REPLACE "                switch (mode) {" [=[                switch (mode) {
                    case 3:
                        gDPPipeSync(arg0++);
                        gDPSetRenderMode(arg0++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                        gDPSetCombineLERP(arg0++, 0, 0, 0, 0, TEXEL0, 0, SHADE, 0,
                                                0, 0, 0, 0, TEXEL0, 0, SHADE, 0);
                        arg0 = func_80095BD0(arg0, var_s0->textureData, var_s0->dX + arg2, var_s0->dY + arg3,
                                             var_s0->width, var_s0->height, scaleX, scaleY);
                        break;]=] replacement "${original}")
string(REPLACE "${original}" "${replacement}"
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
