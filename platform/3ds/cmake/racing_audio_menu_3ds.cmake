    # The desktop direct-audio path normally advances sample-DMA state through
    # an N64 audio task. The 3DS runtime synthesizes directly into NDSP buffers,
    # so generate a narrow 3DS copy that keeps those per-frame queues moving
    # without modifying the upstream SpaghettiKart submodule.
    file(READ "${SPAGHETTIKART_AUDIO_PORT_SOURCE}" SPAGHETTIKART_AUDIO_PORT_TEXT)
    string(REPLACE
        "    gAudioFrameCount++;\n    gCurrAiBufferIndex %= 3;\n\n    gCurrAudioFrameDmaCount = 0;"
        "    gAudioFrameCount++;\n    gCurrAiBufferIndex %= 3;\n\n#if defined(PLATFORM_3DS)\n    decrease_sample_dma_ttls();\n#endif\n\n    gCurrAudioFrameDmaCount = 0;"
        SPAGHETTIKART_3DS_AUDIO_PORT_TEXT
        "${SPAGHETTIKART_AUDIO_PORT_TEXT}"
    )
    if(SPAGHETTIKART_3DS_AUDIO_PORT_TEXT STREQUAL SPAGHETTIKART_AUDIO_PORT_TEXT)
        message(FATAL_ERROR "The upstream audio port insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}" SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING STREQUAL SPAGHETTIKART_3DS_AUDIO_PORT_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}" "${SPAGHETTIKART_3DS_AUDIO_PORT_TEXT}")
    endif()

    # Reject malformed layer opcodes before they can index beyond the four
    # SequenceChannel layer slots. Valid MK64 sequences use only layers 0..3;
    # this guard turns corrupt archive/script data into a skipped command instead
    # of aliasing soundScriptIO and dereferencing values such as 0xffffffff.
    file(READ "${SPAGHETTIKART_SEQPLAYER_SOURCE}" SPAGHETTIKART_SEQPLAYER_TEXT)
    string(REPLACE
        "s32 seq_channel_set_layer(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer;"
        "s32 seq_channel_set_layer(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer;\n\n#if defined(PLATFORM_3DS)\n    if (layerIndex < 0 || layerIndex >= LAYERS_MAX) {\n        return -1;\n    }\n#endif"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_SEQPLAYER_TEXT}"
    )
    set(SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    if(SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT STREQUAL SPAGHETTIKART_SEQPLAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer allocation guard insertion point changed")
    endif()
    string(REPLACE
        "void seq_channel_layer_free(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer = seqChannel->layers[layerIndex];"
        "void seq_channel_layer_free(struct SequenceChannel* seqChannel, s32 layerIndex) {\n#if defined(PLATFORM_3DS)\n    if (layerIndex < 0 || layerIndex >= LAYERS_MAX) {\n        return;\n    }\n#endif\n    struct SequenceChannelLayer* layer = seqChannel->layers[layerIndex];"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}"
    )
    set(SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    if(SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer free guard insertion point changed")
    endif()
    string(REPLACE
        "                        if (seqChannel->layers[loBits] != NULL) {"
        "                        if (loBits < LAYERS_MAX && seqChannel->layers[loBits] != NULL) {"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SEQPLAYER_TEXT STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer read guard insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_SEQPLAYER_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}" SPAGHETTIKART_3DS_SEQPLAYER_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_SEQPLAYER_EXISTING STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}" "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    endif()

    # Generate a 3DS-only menu copy without modifying the pinned upstream
    # gitlink. The stock unselected-portrait shade belongs here, but its wide
    # desktop helper expands a 64 px portrait across neighboring cells at 5:3.
    # Keep the shade and use the original fixed-width box on 3DS.
    file(READ "${SPAGHETTIKART_MENU_ITEMS_SOURCE}" SPAGHETTIKART_MENU_ITEMS_TEXT)
    string(REPLACE
        "                 func_800A12BC(arg0, D_800E7D54[arg0->type - CHARACTER_SELECT_MENU_MARIO]);\n                /* fallthrough */"
        "                func_800A12BC(arg0, D_800E7D54[arg0->type - CHARACTER_SELECT_MENU_MARIO]);\n                func_8009A76C(arg0->D_8018DEE0_index, arg0->column, arg0->row, arg0->param1);\n                func_800A1350(arg0);\n                break;"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL SPAGHETTIKART_MENU_ITEMS_TEXT)
        message(FATAL_ERROR "The upstream character-portrait shade insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_CALL_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "                gDisplayListHead = draw_box_wide(gDisplayListHead, arg0->column, arg0->row, arg0->column + 0x40,\n                                                 arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        "                gDisplayListHead = draw_box(gDisplayListHead, arg0->column, arg0->row, arg0->column + 0x40,\n                                            arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_CALL_TEXT)
        message(FATAL_ERROR "The upstream static portrait shade box insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_STATIC_BOX_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "                gDisplayListHead = draw_box_wide(gDisplayListHead, arg0->column + thing, arg0->row,\n                                                 (arg0->column - thing) + 0x40, arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        "                gDisplayListHead = draw_box(gDisplayListHead, arg0->column + thing, arg0->row,\n                                            (arg0->column - thing) + 0x40, arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_STATIC_BOX_TEXT)
        message(FATAL_ERROR "The upstream animated portrait shade box insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_BOX_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        [=[                if (CVarGetInteger("gShowSpaghettiVersion", true)) {
                    draw_version();
                    draw_debug();
                }]=]
        [=[#if !defined(PLATFORM_3DS)
                if (CVarGetInteger("gShowSpaghettiVersion", true)) {
                    draw_version();
                    draw_debug();
                }
#endif]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_BOX_TEXT)
        message(FATAL_ERROR "The upstream Game Select version render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_VERSION_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        [=[            case MAIN_MENU_OPTION_GFX:
            case MAIN_MENU_DATA_GFX:
                var_a1 = arg0->type - MENU_ITEM_UI_OK;]=]
        [=[            case MAIN_MENU_OPTION_GFX:
            case MAIN_MENU_DATA_GFX:
#if defined(PLATFORM_3DS)
                break;
#endif
                var_a1 = arg0->type - MENU_ITEM_UI_OK;]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_VERSION_TEXT)
        message(FATAL_ERROR "The upstream Game Select Option/Data render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_GAME_SELECT_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "void render_pause_menu(MenuItem* arg0) {\n    if (gIsGamePaused != 0) {"
        [=[#if defined(PLATFORM_3DS)
static void mk64_3ds_render_pause_title(void) {
    const char* courseName = CM_GetProps() != NULL ? CM_GetProps()->Name : "";
    gDisplayListHead = draw_box_wide(gDisplayListHead, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                                     0, 0, 0, 0x8C);
    set_text_color(TEXT_YELLOW);
    if (gModeSelection == GRAND_PRIX) {
        const f32 cupHalf = (get_string_width(gCupNames[GetCupIndex()]) + 10.0f) * 0.5f;
        const f32 ccHalf = (get_string_width(D_800E76CC[gCCSelection]) + 10.0f) * 0.5f;
        print_text1_center_mode_1((s32)(160.0f - ccHalf), 90,
                                  gCupNames[GetCupIndex()], 0, 1.0f, 1.0f);
        print_text1_center_mode_1((s32)(160.0f + cupHalf), 90,
                                  D_800E76CC[gCCSelection], 0, 1.0f, 1.0f);
    }
    print_text1_center_mode_1(160, 112, courseName, 0, 1.0f, 1.0f);
}
#endif

void render_pause_menu(MenuItem* arg0) {
    if (gIsGamePaused != 0) {
#if defined(PLATFORM_3DS)
        mk64_3ds_render_pause_title();
        return;
#endif]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_GAME_SELECT_TEXT)
        message(FATAL_ERROR "The upstream pause-menu dispatch point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PAUSE_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "void func_800A54EC(void) {\n    Unk_D_800E70A0 sp50;"
        "void func_800A54EC(void) {\n#if defined(PLATFORM_3DS)\n    return;\n#endif\n    Unk_D_800E70A0 sp50;"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PAUSE_TEXT)
        message(FATAL_ERROR "The upstream pause cursor render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING "")
    string(REPLACE "void render_menus(MenuItem* arg0) {"
        [=[extern bool Mk64BottomUI3DSHasTitleCopyright(void) __attribute__((weak));
extern bool Mk64BottomUI3DSIsUpdateOpen(void) __attribute__((weak));
void render_menus(MenuItem* arg0) {
    // Keep the original background/filter, but omit every foreground menu item.
    // In a race this removes the pause title without replacing the 3D scene.
    if (Mk64BottomUI3DSIsUpdateOpen != NULL && Mk64BottomUI3DSIsUpdateOpen() &&
        arg0->type != MAIN_MENU_BACKGROUND &&
        arg0->type != CHARACTER_SELECT_BACKGROUND &&
        arg0->type != COURSE_SELECT_BACKGROUND &&
        arg0->type != MENU_ITEM_UI_START_BACKGROUND) return;
    if ((gMenuSelection == DATA_MENU || gMenuSelection == COURSE_DATA_MENU) && arg0->type != MAIN_MENU_BACKGROUND) return;]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    set(MK64_TITLE_COPYRIGHT_BEFORE "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "                gDisplayListHead =\n                    render_menu_textures(gDisplayListHead, seg2_copyright_1996_texture, arg0->column, arg0->row);"
        "                if (Mk64BottomUI3DSHasTitleCopyright == NULL || !Mk64BottomUI3DSHasTitleCopyright()) {\n                    gDisplayListHead = render_menu_textures(gDisplayListHead, seg2_copyright_1996_texture, arg0->column, arg0->row);\n                }"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL MK64_TITLE_COPYRIGHT_BEFORE)
        message(FATAL_ERROR "The upstream title copyright draw changed")
    endif()
    string(REPLACE
        "            case COURSE_DATA_MENU:\n                add_menu_item(MENU_ITEM_DATA_COURSE_IMAGE"
        "            case COURSE_DATA_MENU:\n                add_menu_item(MAIN_MENU_BACKGROUND, 0, 0, MENU_ITEM_PRIORITY_2);\n                add_menu_item(MENU_ITEM_DATA_COURSE_IMAGE"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    if(NOT SPAGHETTIKART_3DS_MENU_ITEMS_TEXT MATCHES "gMenuSelection == DATA_MENU \\|\\| gMenuSelection == COURSE_DATA_MENU")
        message(FATAL_ERROR "The upstream data background render point changed")
    endif()
    # Coalesce first-run EEPROM repairs without deferring normal game saves.
    string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "            load_save_data();" MK64_SAVE_INIT_ANCHOR)
    if(MK64_SAVE_INIT_ANCHOR EQUAL -1)
        message(FATAL_ERROR "The upstream initial save load changed")
    endif()
    string(REPLACE "            load_save_data();"
        "            extern void Mk64LoadSaveData3DS(void);\n            Mk64LoadSaveData3DS();"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    include(platform/3ds/results_layout.cmake)
    # Observe white/black transitions on every menu page AND the ceremony
    # before the stock timer advances. Skip uses the same native ending fade.
    string(REPLACE "void func_8009CA6C(s32 arg0) {"
        "extern void Mk64BottomUI3DSSetMenuTransition(int, int, int, unsigned, unsigned) __attribute__((weak));\nvoid func_8009CA6C(s32 arg0) {\n    if (arg0 == 4 && Mk64BottomUI3DSSetMenuTransition != NULL)\n        Mk64BottomUI3DSSetMenuTransition(gGamestate <= COURSE_SELECT_MENU_FROM_QUIT && gMenuSelection == LOGO_INTRO_MENU, (gGamestate >= START_MENU_FROM_QUIT && gGamestate <= COURSE_SELECT_MENU_FROM_QUIT) || gGamestate == ENDING, gTransitionType[4], gCurrentTransitionTime[4], gTransitionDuration[4]);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    # Synchronize lower opening shutters to the exact native 0.008 + 0.028
    # viewport-opening fraction after game logic, not a second UI timer.
    string(REPLACE "void func_8009CA6C(s32 arg0) {"
        "extern f32 D_802BA034;\nextern void Mk64BottomUI3DSSetRaceOpening(float) __attribute__((weak));\nextern void Mk64BottomUI3DSSetRaceExitSlide(int) __attribute__((weak));\nvoid func_8009CA6C(s32 arg0) {\n    if (arg0 == 4 && gGamestate == RACING && gRaceState <= RACE_STAGING && Mk64BottomUI3DSSetRaceOpening != NULL)\n        Mk64BottomUI3DSSetRaceOpening(gRaceState == RACE_INIT ? 0.0f : D_802BA034);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    # GP and Time Trial/retry share this signed vertical viewport updater.
    string(REPLACE "void func_8009CA6C(s32 arg0) {"
        "extern void Mk64BottomUI3DSSetRaceExitFade(int, unsigned, unsigned) __attribute__((weak));\nvoid func_8009CA6C(s32 arg0) {\n    if (arg0 == 4 && gGamestate == RACING && gRaceState > RACE_STAGING && Mk64BottomUI3DSSetRaceExitFade != NULL)\n        Mk64BottomUI3DSSetRaceExitFade(gTransitionType[4], gCurrentTransitionTime[4], gTransitionDuration[4]);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    # Each callback follows the actual pixel step, including the final hold.
    foreach(MK64_RACE_EXIT_PAIR IN ITEMS
        "play_sound2(SOUND_ACTION_NEXT_COURSE);|0"
        "gScreenTwoCtx->screenStartY -= 0x10;|arg0->param2"
        "gScreenOneCtx->screenStartY = 0x012C;|240")
        # C statements contain semicolons; do not split them as CMake lists.
        string(FIND "${MK64_RACE_EXIT_PAIR}" "|" MK64_RACE_EXIT_SEPARATOR)
        string(SUBSTRING "${MK64_RACE_EXIT_PAIR}" 0 ${MK64_RACE_EXIT_SEPARATOR} MK64_RACE_EXIT_ANCHOR)
        math(EXPR MK64_RACE_EXIT_VALUE_START "${MK64_RACE_EXIT_SEPARATOR} + 1")
        string(SUBSTRING "${MK64_RACE_EXIT_PAIR}" ${MK64_RACE_EXIT_VALUE_START} -1 MK64_RACE_EXIT_PROGRESS)
        string(FIND "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}" "${MK64_RACE_EXIT_ANCHOR}" MK64_RACE_EXIT_OFFSET)
        if(MK64_RACE_EXIT_OFFSET LESS 0)
            message(FATAL_ERROR "Native race-exit transition anchor changed: ${MK64_RACE_EXIT_ANCHOR}")
        endif()
        string(REPLACE "${MK64_RACE_EXIT_ANCHOR}"
            "${MK64_RACE_EXIT_ANCHOR}\n                if (Mk64BottomUI3DSSetRaceExitSlide != NULL) Mk64BottomUI3DSSetRaceExitSlide(${MK64_RACE_EXIT_PROGRESS});"
            SPAGHETTIKART_3DS_MENU_ITEMS_TEXT "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    endforeach()
    if(EXISTS "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING STREQUAL SPAGHETTIKART_3DS_MENU_ITEMS_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    endif()

    # The stock global HUD pass is outside render_hud(), so gDrawHUD alone
    # cannot hide its ranking, place, minimap or lap-progress elements. Build
    # a 3DS-only copy that keeps viewport restoration/debug work while routing
    # visible top HUD elements through the explicit FULL/NONE/LAP/SPEED mode.
    file(READ "${SPAGHETTIKART_HUD_SOURCE}" SPAGHETTIKART_HUD_TEXT)
    set(SPAGHETTIKART_3DS_HUD_ORIGINAL "${SPAGHETTIKART_HUD_TEXT}")
    string(REPLACE
        "#include \"code_80057C60.h\""
        [=[#include "code_80057C60.h"
#if defined(PLATFORM_3DS)
extern int Mk64GameState3DSGetTopHudRenderMode(void);
extern bool Mk64BottomUI3DSIsUpdateOpen(void) __attribute__((weak));
#define MK64_TOP_HUD_RENDER_FULL (-1)
#define MK64_TOP_HUD_RENDER_NONE 0
#define MK64_TOP_HUD_RENDER_LAP_PROGRESS 1
#define MK64_TOP_HUD_RENDER_SPEEDOMETER 2
#define MK64_TOP_HUD_RENDER_POSITION_LAP 3
#endif]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_ORIGINAL)
        message(FATAL_ERROR "The upstream global HUD include point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_INCLUDE_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "void func_800591B4(void) {\n\n    if ((gHUDDisable == 0) && (D_800DC5B8 != 0)) {"
        "void func_800591B4(void) {\n#if defined(PLATFORM_3DS)\n    const bool mk64UpdateOpen = Mk64BottomUI3DSIsUpdateOpen != NULL && Mk64BottomUI3DSIsUpdateOpen();\n    const int mk64TopHudMode = mk64UpdateOpen ? MK64_TOP_HUD_RENDER_NONE : Mk64GameState3DSGetTopHudRenderMode();\n#endif\n\n    if ((gHUDDisable == 0) && (D_800DC5B8 != 0)) {"
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream global HUD entry point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_ENTRY_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "        if (gIsHUDVisible != 0) {\n            if (D_801657D8 == 0) {"
        [=[        if ((gIsHUDVisible != 0)
#if defined(PLATFORM_3DS)
            && (mk64TopHudMode == MK64_TOP_HUD_RENDER_FULL)
#endif
        ) {
            if (D_801657D8 == 0) {]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_ENTRY_TEXT)
        message(FATAL_ERROR "The upstream global HUD visibility point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_FULL_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "            func_800590D4();\n        }\n        func_8005902C();"
        [=[            func_800590D4();
        }
#if defined(PLATFORM_3DS)
        if ((gIsHUDVisible != 0) && (D_801657D8 == 0)) {
            if (mk64TopHudMode == MK64_TOP_HUD_RENDER_LAP_PROGRESS) {
                func_800514BC();
            } else if (mk64TopHudMode == MK64_TOP_HUD_RENDER_SPEEDOMETER) {
                set_matrix_hud_screen();
                render_speedometer(PLAYER_ONE);
            }
        }
#endif
        func_8005902C();]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_FULL_TEXT)
        message(FATAL_ERROR "The upstream selective HUD insertion point changed")
    endif()
    include(platform/3ds/cmake/cpu_comic_3ds.cmake)
    set(SPAGHETTIKART_3DS_HUD_UPDATE_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "    D_8018D21C = arg0;"
        "    D_8018D21C = arg0;\n    if (Mk64BottomUI3DSIsUpdateOpen != NULL && Mk64BottomUI3DSIsUpdateOpen()) return;"
        SPAGHETTIKART_3DS_HUD_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "        func_8005902C();\n        func_80057DD0();"
        "        if (!mk64UpdateOpen) {\n            func_8005902C();\n            func_80057DD0();\n        }"
        SPAGHETTIKART_3DS_HUD_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_UPDATE_TEXT)
        message(FATAL_ERROR "The upstream updater HUD render points changed")
    endif()
    if(MK64_3DS_NATIVE_PARTICLE_LAB)
        include(platform/3ds/cmake/native_particle_game_3ds.cmake)
        mk64_convert_native_particle_producers()
    endif()
    set(SPAGHETTIKART_3DS_HUD_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_HUD_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_HUD_SOURCE}" SPAGHETTIKART_3DS_HUD_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_HUD_EXISTING STREQUAL SPAGHETTIKART_3DS_HUD_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_HUD_SOURCE}" "${SPAGHETTIKART_3DS_HUD_TEXT}")
    endif()

    # The Citro3D backend cannot read a rendered framebuffer back into the
    # N64 CPU buffer: its current implementation returns zero-filled pixels.
    # Avoid building two off-screen targets and a 320x240 readback command
    # stream every race frame when the only consumers would receive black.
    # Luigi Raceway and Wario Stadium keep their static billboard textures in
    # the 3DS engine copies below.
    file(READ "${SPAGHETTIKART_MAIN_SOURCE}" SPAGHETTIKART_3DS_MAIN_TEXT)
    set(SPAGHETTIKART_3DS_MAIN_ORIGINAL "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    FB_WriteFramebufferSliceToCPU(&gDisplayListHead, gPortFramebuffers[sRenderingFramebuffer], true);"
        "    /* PLATFORM_3DS: dynamic jumbotron readback is unsupported and intentionally omitted. */"
        SPAGHETTIKART_3DS_MAIN_TEXT
        "${SPAGHETTIKART_3DS_MAIN_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MAIN_TEXT STREQUAL SPAGHETTIKART_3DS_MAIN_ORIGINAL)
        message(FATAL_ERROR "The upstream race framebuffer-readback call changed")
    endif()
    set(SPAGHETTIKART_3DS_MAIN_READBACK_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    FB_CreateFramebuffers();"
        "    /* PLATFORM_3DS: no CPU framebuffer consumer requires off-screen targets. */"
        SPAGHETTIKART_3DS_MAIN_TEXT
        "${SPAGHETTIKART_3DS_MAIN_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MAIN_TEXT STREQUAL SPAGHETTIKART_3DS_MAIN_READBACK_TEXT)
        message(FATAL_ERROR "The upstream per-frame framebuffer creation call changed")
    endif()
    string(REPLACE "void update_gamestate(void) {"
        "extern void Mk64BottomUI3DSShowLoadingProgress(const char*, const char*, unsigned) __attribute__((weak));\nvoid update_gamestate(void) {\n    if (Mk64BottomUI3DSShowLoadingProgress != NULL) Mk64BottomUI3DSShowLoadingProgress(\"LOADING GAME\", \"PLEASE WAIT\", 0);"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    # Keep startup diagnostics in the generated port copy, not the submodule.
    # After the startup worker stops these hooks perform no file I/O.
    string(PREPEND SPAGHETTIKART_3DS_MAIN_TEXT "#include \"startup_trace_3ds.h\"\n")
    set(MK64_STARTUP_MAIN_BEFORE "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    rendering_init();\n    read_controllers();\n    func_800C5CB8();"
        "    Mk64StartupStage(\"vanilla-initial-clear-enter\");\n    rendering_init();\n    Mk64StartupStage(\"vanilla-initial-controllers-enter\");\n    read_controllers();\n    Mk64StartupStage(\"vanilla-audio-state-init-enter\");\n    func_800C5CB8();\n    Mk64StartupStage(\"vanilla-audio-state-init-returned\");"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    if(MK64_STARTUP_MAIN_BEFORE STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        message(FATAL_ERROR "The upstream vanilla initialization order changed")
    endif()
    set(MK64_STARTUP_MAIN_BEFORE "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "void thread5_iteration(void) {\n    func_800CB2C4();\n    calculate_delta_time();"
        "void thread5_iteration(void) {\n    Mk64StartupStage(\"logic-audio-update-enter\");\n    func_800CB2C4();\n    Mk64StartupStage(\"logic-audio-update-returned\");\n    calculate_delta_time();\n    Mk64StartupStage(\"logic-state-update-enter\");"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    if(MK64_STARTUP_MAIN_BEFORE STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        message(FATAL_ERROR "The upstream first logic/audio boundary changed")
    endif()
    set(MK64_STARTUP_MAIN_BEFORE "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    profiler_log_thread5_time(THREAD5_START);\n    config_gfx_pool();\n    read_controllers();"
        "    Mk64StartupStage(\"logic-state-update-returned\");\n    profiler_log_thread5_time(THREAD5_START);\n    config_gfx_pool();\n    Mk64StartupStage(\"logic-controllers-enter\");\n    read_controllers();\n    Mk64StartupStage(\"logic-controllers-returned\");"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    if(MK64_STARTUP_MAIN_BEFORE STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        message(FATAL_ERROR "The upstream logic/controller boundary changed")
    endif()
    set(MK64_STARTUP_MAIN_BEFORE "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    game_state_handler();\n\n    // call_render_hook();"
        "    Mk64StartupStage(\"logic-game-state-handler-enter\");\n    game_state_handler();\n    Mk64StartupStage(\"logic-game-state-handler-returned\");\n\n    // call_render_hook();"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    if(MK64_STARTUP_MAIN_BEFORE STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        message(FATAL_ERROR "The upstream game-state-handler boundary changed")
    endif()
    set(SPAGHETTIKART_3DS_MAIN_EXISTING "")
    string(FIND "${SPAGHETTIKART_3DS_MAIN_TEXT}" "gCourseTimer += TRACK_TIMER_ITER;" MK64_CLOCK_FOUND)
    if(MK64_CLOCK_FOUND LESS 0)
        message(FATAL_ERROR "Upstream course clock increment changed")
    endif()
    string(PREPEND SPAGHETTIKART_3DS_MAIN_TEXT
        "extern float Mk64RaceClock3DSAdvance(float);\nextern void Mk64RaceClock3DSObserve(void);\n")
    string(REPLACE "gCourseTimer += TRACK_TIMER_ITER;"
        "gCourseTimer = Mk64RaceClock3DSAdvance(gCourseTimer);"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    set(MK64_CLOCK_PAUSE_ANCHOR "    if (gIsGamePaused == false) {\n        for (size_t i = 0; i < gTickLogic; i++) {")
    string(FIND "${SPAGHETTIKART_3DS_MAIN_TEXT}" "${MK64_CLOCK_PAUSE_ANCHOR}" MK64_CLOCK_PAUSE_FOUND)
    if(MK64_CLOCK_PAUSE_FOUND LESS 0)
        message(FATAL_ERROR "Upstream clock pause boundary changed")
    endif()
    string(REPLACE "${MK64_CLOCK_PAUSE_ANCHOR}"
        "    Mk64RaceClock3DSObserve();\n    if (gIsGamePaused == false) {\n        for (size_t i = 0; i < gTickLogic; i++) {"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    include(platform/3ds/cmake/native_kart_main_3ds.cmake)
    include(platform/3ds/cmake/game_preparation_3ds.cmake)
    if(EXISTS "${SPAGHETTIKART_3DS_MAIN_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MAIN_SOURCE}" SPAGHETTIKART_3DS_MAIN_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MAIN_EXISTING STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MAIN_SOURCE}" "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    endif()
