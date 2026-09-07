// Preserve the original point counting, tie ordering and row animations.
void func_800A34A8(MenuItem* arg0) {
    s8 sp80[8];
    UNUSED s32 stackPadding0;
    UNUSED s32 stackPadding1;
    s32 var_a0;
    s32 var_v0;
    UNUSED s32 stackPadding2;
    s32 rank;
    s32 test;
    const s32 topCenter = Mk64ResultsCenter3DS(arg0->column);
    const s32 bottomCenter = Mk64ResultsCenter3DS(160 - arg0->column);

    if (arg0->state != 0) {
        if (arg0->state < 9) {
            for (rank = 0; rank < NUM_PLAYERS; rank++) {
                sp80[rank] = gPlayers[gGPCurrentRacePlayerIdByRank[rank]].characterId;
            }
        } else {
            func_800A3A10(sp80);
            func_800A3A10(gCharacterIdByGPOverallRank);
        }
        set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_1);
        print_text1_center_mode_1(topCenter, 0x19 - arg0->row, "driver's points", 0, 0.8f, 0.8f);
        set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_2);
        Mk64ResultsRound3DS(topCenter, 0x28 - arg0->row);
        for (rank = 0; rank < 4; rank++) {
            test = arg0->state;
            if ((test != 8) && (test != 9)) {
                var_v0 = 0;
            } else {
                if ((rank * 5) < arg0->param1) {
                    var_v0 = 1;
                } else {
                    var_v0 = 0;
                }
            }
            if (var_v0 == 0) {
                if (arg0->state < 9) {
                    var_v0 = gGPCurrentRacePlayerIdByRank[rank];
                } else {
                    var_v0 = gGetPlayerByCharacterId[sp80[rank]];
                }
                if (var_v0 < gPlayerCount) {
                    var_a0 = (s32) gGlobalTimer % 3;
                } else {
                    var_a0 = 3;
                }
                set_text_color(var_a0);
                func_800A3ADC(arg0, topCenter - (arg0->state < 9 ? 50 : 40), ((rank * 0x10) - arg0->row) + 0x38, sp80[rank], rank,
                              sp80);
            }
        }
        for (rank = 4; rank < NUM_PLAYERS; rank++) {
            test = arg0->state;
            if ((test != 8) && (test != 9)) {
                var_v0 = 0;
            } else {
                if ((rank * 5) < arg0->param1) {
                    var_v0 = 1;
                } else {
                    var_v0 = 0;
                }
            }
            if (var_v0 == 0) {
                if (arg0->state < 9) {
                    var_v0 = gGPCurrentRacePlayerIdByRank[rank];
                } else {
                    var_v0 = gGetPlayerByCharacterId[sp80[rank]];
                }
                if (var_v0 < gPlayerCount) {
                    var_a0 = (s32) gGlobalTimer % 3;
                } else {
                    var_a0 = 3;
                }
                set_text_color(var_a0);
                func_800A3ADC(arg0, bottomCenter - 40, arg0->row + (rank * 0x10) + 0x5A, sp80[rank], rank, sp80);
            }
        }
        set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_2);
        Mk64ResultsCup3DS(bottomCenter, arg0->row + 0xE1);
    }
}
