# Keep upstream untouched. Guard every integration point, including the four
# race native save boundary (a random cup must not award Mushroom Cup medals).
macro(mk64_detail_replace text old new)
    string(FIND "${${text}}" "${old}" DETAIL_ANCHOR)
    if(DETAIL_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream menu-detail anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" ${text} "${${text}}")
endmacro()
set(MENU_DETAIL_SOURCE "${SPAGHETTIKART_ROOT}/src/menus.c")
set(SAVE_DETAIL_SOURCE "${SPAGHETTIKART_ROOT}/src/save.c")
list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${MENU_DETAIL_SOURCE}" "${SAVE_DETAIL_SOURCE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MENU_DETAIL_SOURCE}" "${SAVE_DETAIL_SOURCE}")
set(SPAGHETTIKART_3DS_MENUS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/menus_3ds.c")
set(SPAGHETTIKART_3DS_SAVE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/save_3ds.c")
file(READ "${MENU_DETAIL_SOURCE}" MENU_DETAIL_TEXT)
file(READ "${SAVE_DETAIL_SOURCE}" SAVE_DETAIL_TEXT)
set(MENU_DETAIL_TEXT "#include \"game_state_3ds.h\"\n${MENU_DETAIL_TEXT}")
set(SAVE_DETAIL_TEXT "#include \"auto_mode_3ds.h\"\n#include \"game_state_3ds.h\"\n${SAVE_DETAIL_TEXT}")
mk64_detail_replace(MENU_DETAIL_TEXT "void load_menu_states(s32 menuSelection) {"
    "void load_menu_states(s32 menuSelection) {\n    Mk64RandomCup3DSReset();")
mk64_detail_replace(MENU_DETAIL_TEXT "                    func_8009E0F0(0x14);"
    "                    Mk64MenuBackTitle3DSBegin();\n                    func_8009DFE0(30);")
mk64_detail_replace(SAVE_DETAIL_TEXT "void func_800B536C(s32 arg0) {"
    "void func_800B536C(s32 arg0) {\n    if (Mk64RandomCup3DSActive() || Mk64Auto3DSActive()) return;")
mk64_detail_replace(SAVE_DETAIL_TEXT "void func_800B5404(s32 arg0, s32 arg1) {"
    "void func_800B5404(s32 arg0, s32 arg1) {\n    if (Mk64Auto3DSActive()) return;")
file(WRITE "${SPAGHETTIKART_3DS_MENUS_SOURCE}" "${MENU_DETAIL_TEXT}")
file(WRITE "${SPAGHETTIKART_3DS_SAVE_SOURCE}" "${SAVE_DETAIL_TEXT}")
mk64_detail_replace(SPAGHETTIKART_3DS_RACE_SETUP_TEXT "void setup_race(void) {"
    "void setup_race(void) {\n    extern void Mk64RandomCup3DSPrepareRace(void);\n    Mk64RandomCup3DSPrepareRace();")
file(WRITE "${SPAGHETTIKART_3DS_RACE_SETUP_SOURCE}" "${SPAGHETTIKART_3DS_RACE_SETUP_TEXT}")
set(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "#include \"game_state_3ds.h\"\n${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "            func_8009DF4C(0x00000014);"
    "            func_8009DF4C(Mk64MenuBackTitle3DSFadeIn(20));")
# Native cup names in results/pause and return-to-menu animation.
# Native no-record labels use EUC-JP long dashes. The port's text renderer
# accepts ASCII here, not that multibyte encoding; never treat it as a name.
string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    [=["\xA1\xBC\xA1\xBC\xA1\xBC\xA1\xBC"]=] NO_RECORD_ANCHOR)
if(NO_RECORD_ANCHOR LESS 0)
    message(FATAL_ERROR "Upstream no-record label changed")
endif()
string(REPLACE [=["\xA1\xBC\xA1\xBC\xA1\xBC\xA1\xBC"]=] "\"----\""
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "            case MENU_ITEM_TYPE_066:\n                if (arg0->type == MENU_ITEM_TYPE_065) {"
    "            case MENU_ITEM_TYPE_066:\n                if (gModeSelection != TIME_TRIALS) break;\n                if (arg0->type == MENU_ITEM_TYPE_065) {")
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "    var_s2 = (u32) temp_s1 < 0x927C0U ? var_s2 >> 0x14 : 8;"
    "    var_s2 = (u32) temp_s1 < 0x927C0U && (var_s2 >> 20) < 8 ? var_s2 >> 20 : 8;")
# ARM plain char is unsigned. This native decoder switches on negative EUC-JP
# lead bytes; without an explicit signed cast every digit becomes glyph C,
# followed by glyph B for the unconsumed second byte (VS/Battle score "CB").
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "    switch (character[0]) {"
    "    switch ((s8) character[0]) {")
string(REPLACE "gCupNames[GetCupIndex()]" "GetCupName()"
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
string(REPLACE "gCupNames[D_800DC540]" "GetCupName()"
    SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
# Fast3D rectangle endpoints are exclusive here. Native N64 end-course/retry
# shading still uses the last pixel (319,239), leaving one unshaded edge.
# Match only full-screen shades, not text boxes, scissors or moving shutters.
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "draw_box_wide(gDisplayListHead, 0, 0, 0x0000013F, 0x000000EF,"
    "draw_box_wide(gDisplayListHead, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,")
mk64_detail_replace(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
    "draw_box_wide(gDisplayListHead, 0, 0, 0x13F, 0xEF,"
    "draw_box_wide(gDisplayListHead, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,")
file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
