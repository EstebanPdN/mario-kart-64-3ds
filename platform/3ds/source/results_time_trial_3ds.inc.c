// Time Trial uses the same physical quarter-screen panels as GP results.
static void Mk64TrialText3DS(s32 center, s32 row, const char* text, f32 scale) {
    const s32 width = get_string_width((char*)text);
    const f32 available = Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE ? 174.0f : 146.0f;
    if (width > 0 && width * scale > available) scale = available / width;
    print_text1_center_mode_1(center, row, (char*)text, 0, scale, scale);
}

static void Mk64TrialTime3DS(u32 time, char* text, size_t size) {
    if (time >= 600000) snprintf(text, size, "--'--\"--");
    else snprintf(text, size, "%02lu'%02lu\"%02lu", (unsigned long)(time/6000),
                  (unsigned long)(time/100%60), (unsigned long)(time%100));
}

static void Mk64TrialLaps3DS(MenuItem* item, s32 center, s32 y) {
    char time[20], line[64];
    MenuItem* records = find_menu_items_dupe(MENU_ITEM_TYPE_0BB);
    set_text_color(TEXT_BLUE_GREEN_RED_CYCLE_1);
    Mk64TrialText3DS(center, y+17, CM_GetProps()->Name, 0.55f);
    set_text_color(TEXT_YELLOW);
    Mk64TrialText3DS(center, y+32, gLapTimeText, 0.68f);
    for (s32 lap=0;lap<4;++lap) {
        const u32 value = lap<3 ? playerHUD[PLAYER_ONE].lapDurations[lap] : playerHUD[PLAYER_ONE].someTimer;
        Mk64TrialTime3DS(value,time,sizeof(time));
        if(lap<3) snprintf(line,sizeof(line),"LAP %ld  %s",(long)(lap+1),time);
        else snprintf(line,sizeof(line),"TOTAL  %s",time);
        const s32 best = records && (lap<3 ? (records->param2 & (1<<lap)) : records->param1>=0);
        set_text_color(best ? gGlobalTimer%3 : lap<3 ? TEXT_YELLOW : TEXT_GREEN);
        Mk64TrialText3DS(center,y+49+lap*16,line,0.61f);
    }
}

static void Mk64TrialRecord3DS(s32 record, s32 center, s32 y) {
    const u32 packed=record<5 ? func_800B4E24(record) : func_800B4F2C();
    const u32 time=packed&0xfffff, character=packed>>20;
    char value[20],line[72];
    Mk64TrialTime3DS(time,value,sizeof(value));
    const char* name=time<600000 && character<8 ? D_800E76A8[character] : "";
    if(record<5) snprintf(line,sizeof(line),"%ld  %s  %s",(long)(record+1),value,name);
    else snprintf(line,sizeof(line),"%s  %s",value,name);
    MenuItem* item=find_menu_items_dupe(MENU_ITEM_TYPE_0BB);
    const s32 best=item && (record<5 ? item->param1==record : item->param2!=0);
    set_text_color(best ? gGlobalTimer%3 : TEXT_YELLOW);
    Mk64TrialText3DS(center,y,line,0.55f);
}

void time_trials_finish_text_render(MenuItem* arg0) {
    const s32 top=Mk64ResultsCenter3DS(arg0->column);
    const s32 bottom=Mk64ResultsCenter3DS(160-arg0->column);
    Mk64TrialLaps3DS(arg0,top,arg0->row);
    set_text_color(TEXT_YELLOW);
    Mk64TrialText3DS(bottom,arg0->row+131,gBestTimeText[0],0.65f);
    for(s32 i=0;i<5;++i) Mk64TrialRecord3DS(i,bottom,arg0->row+146+i*14);
    set_text_color(TEXT_YELLOW);
    Mk64TrialText3DS(bottom,arg0->row+216,gBestTimeText[1],0.6f);
    Mk64TrialRecord3DS(5,bottom,arg0->row+231);
}

void func_800A3E60(MenuItem* arg0) {
    if(arg0->state==0 || arg0->state==31) return;
    const s32 center=Mk64ResultsCenter3DS(160-arg0->column);
    const s32 y=arg0->row;
    s32 cursor=-1;
    const s32 cursorX=center-(Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE ? 82 : 70);
    Unk_D_800E70A0 position;
    Mk64TrialLaps3DS(arg0,Mk64ResultsCenter3DS(arg0->column),-y);
    switch(arg0->state) {
    case 1: case 5: case 6: case 7: case 8: case 9: case 10: case 30:
        for(s32 i=0;i<6;++i) {
            text_rainbow_effect(arg0->state-5,i,1);
            const s32 unavailable=(i==4 && gPostTimeTrialReplayCannotSave==1) || (i==5 && bPlayerGhostDisabled!=0);
            if(unavailable) set_text_color(TEXT_BLUE);
            Mk64TrialText3DS(center,y+140+16*i,gTextPauseButton[i+1],0.7f);
        }
        if(arg0->state>=5 && arg0->state<=10) cursor=y+140+16*(arg0->state-5);
        if(arg0->state==30 && arg0->param1>=5 && arg0->param1<=10) cursor=y+140+16*(arg0->param1-5);
        break;
    case 11: case 12: case 13: case 14: case 15: case 16:
        set_text_color(TEXT_YELLOW);
        for(s32 i=0;i<7;++i) Mk64TrialText3DS(center,y+140+14*i,D_800E798C[(arg0->state-11)*7+i],0.6f);
        break;
    case 17: case 18:
        set_text_color(TEXT_GREEN);
        for(s32 i=0;i<2;++i) Mk64TrialText3DS(center,y+138+i*15,D_800E7A3C[i],0.65f);
        for(s32 i=0;i<2;++i) {
            char line[96];
            const u32 track=D_8018EE10[i].trackIndex;
            const char* name=D_800E7A44;
            if(D_8018EE10[i].ghostDataSaved && track<16)
                name=TrackBrowser_GetTrackNameByIdx(gCupCourseOrder[track/4][track%4]);
            snprintf(line,sizeof(line),"%ld  %s",(long)(i+1),name);
            text_rainbow_effect(arg0->state-17,i,1);
            Mk64TrialText3DS(center,y+181+i*30,line,0.58f);
        }
        cursor=y+181+(arg0->state-17)*30;
        break;
    case 19:
        set_text_color(TEXT_YELLOW);
        for(s32 i=0;i<3;++i) Mk64TrialText3DS(center,y+155+i*17,D_800E7A48[i],0.7f);
        break;
    case 20: case 21:
        set_text_color(TEXT_YELLOW);
        for(s32 i=0;i<3;++i) Mk64TrialText3DS(center,y+138+i*14,D_800E7A60[i],0.65f);
        for(s32 i=0;i<2;++i) {
            text_rainbow_effect(arg0->state-20,i,1);
            Mk64TrialText3DS(center,y+194+i*22,D_800E7A6C[i],0.7f);
        }
        cursor=y+194+(arg0->state-20)*22;
        break;
    case 25:
        set_text_color(TEXT_YELLOW);
        for(s32 i=0;i<3;++i) Mk64TrialText3DS(center,y+155+i*17,D_800E7A74[i],0.65f);
        break;
    case 26:
        set_text_color(TEXT_YELLOW);
        for(s32 i=0;i<2;++i) Mk64TrialText3DS(center,y+162+i*18,D_800E7A80[i],0.7f);
        break;
    }
    if(cursor>=0) { position.column=cursorX;position.row=cursor;pause_menu_item_box_cursor(arg0,&position); }
}
