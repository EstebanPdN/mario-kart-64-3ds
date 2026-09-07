// Native results typography, positioned within the visible quarter-screen panels.
// The original horizontal slide still follows the menu item's column.
static s32 Mk64ResultsCenter3DS(s32 column) {
    return Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE
        ? 60 + (column * 5) / 4 : 80 + column;
}

static void Mk64ResultsRound3DS(s32 center, s32 row) {
    char text[] = "round 1";
    text[6] += GetCupCursorPosition();
    print_text1_center_mode_1(center, row, text, 0, 0.7f, 0.7f);
}

static void Mk64ResultsCup3DS(s32 center, s32 row) {
    char text[96];
    snprintf(text, sizeof(text), "%s %s", GetCupName(), D_800E76CC[gCCSelection]);
    print_text1_center_mode_1(center, row, text, 0, 0.6f, 0.6f);
}

void func_800A2EB8(MenuItem* arg0) {
    s8 sp70[8];
    UNUSED s32 stackPadding0;
    s32 var_a0;
    s32 var_s2;
    const s32 topCenter = Mk64ResultsCenter3DS(arg0->column);
    const s32 bottomCenter = Mk64ResultsCenter3DS(160 - arg0->column);

    for (var_s2 = 0; var_s2 < NUM_PLAYERS; var_s2++) {
        sp70[var_s2] = gPlayers[gGPCurrentRacePlayerIdByRank[var_s2]].characterId;
    }
    set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_1);
    print_text1_center_mode_1(topCenter, arg0->row + 0x19, "results", 0, 1.0f, 1.0f);
    set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_2);
    Mk64ResultsRound3DS(topCenter, arg0->row + 0x28);
    for (var_s2 = 0; var_s2 < 4; var_s2++) {
        if (gGPCurrentRacePlayerIdByRank[var_s2] < gPlayerCount) {
            var_a0 = (s32) gGlobalTimer % 3;
        } else {
            var_a0 = TEXT_YELLOW;
        }
        set_text_color(var_a0);
        func_800A32B4(topCenter - 59, arg0->row + (0x10 * var_s2) + 0x38, (s32) sp70[var_s2], var_s2);
    }
    for (var_s2 = 4; var_s2 < 8; var_s2++) {
        if (gGPCurrentRacePlayerIdByRank[var_s2] < gPlayerCount) {
            var_a0 = (s32) gGlobalTimer % 3;
        } else {
            var_a0 = TEXT_YELLOW;
        }
        set_text_color(var_a0);
        func_800A32B4(bottomCenter - 59, arg0->row + (0x10 * var_s2) + 0x5A, sp70[var_s2], var_s2);
    }
    set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_2);
    Mk64ResultsCup3DS(bottomCenter, arg0->row + 0xE1);
}
