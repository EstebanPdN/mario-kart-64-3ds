if(MK64_3DS_NATIVE_KART_LAB)
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "void render_kart(" begin)
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "void render_ghost(" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "Native kart game adapter function bounds changed")
    endif()
    math(EXPR length "${end}-${begin}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" ${begin} ${length} before)
    set(anchor "    convert_to_fixed_point_matrix(GetKartMatrix(playerId + (screenId * 8)), mtx);")
    string(FIND "${before}" "${anchor}" location)
    if(location LESS 0)
        message(FATAL_ERROR "Native kart packet insertion point changed")
    endif()
    set(native [=[
    {
        const int boo=(player->effects & BOO_EFFECT)==BOO_EFFECT;
        const int fizzle=(player->lakituProps & LAKITU_FIZZLE) ||
            (player->triggers & (BECOME_BOMB_EFFECT | LOSE_BATTLE_EFFECT));
        const int blend=boo || fizzle;
        const uint8_t alpha=(boo && screenId!=playerId) ? gPlayerOtherScreensAlpha[playerId] : player->alpha;
        const uint8_t colors[8]={gPlayerRedEffect[playerId],gPlayerGreenEffect[playerId],gPlayerBlueEffect[playerId],alpha,
            gPlayerCyanEffect[playerId],gPlayerMagentaEffect[playerId],gPlayerYellowEffect[playerId],255};
        const uint32_t mode=boo ? (AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
            GBL_c1(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA) | GBL_c2(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA)) :
            (fizzle ? (G_RM_ZB_XLU_SURF | G_RM_ZB_XLU_SURF2) : (G_RM_AA_ZB_TEX_EDGE | G_RM_AA_ZB_TEX_EDGE2));
        Mk64NativeKartEnqueue(gDisplayListHead,GetKartMatrix(playerId+(screenId*8)),&mtx[0][0],
            &gPlayerVtx[playerId][flipOffset],sKartTexture,gPlayerPalette,colors,mode,blend,MK64_NATIVE_BODY);
        FrameInterpolation_RecordCloseChild();
        return;
    }
]=])
    string(LENGTH "${anchor}" anchorLength)
    math(EXPR prefixLength "${location}+${anchorLength}")
    string(SUBSTRING "${before}" 0 ${prefixLength} prefix)
    set(after "${prefix}\n${native}\n}\n\n")
    string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    set(anchor "    convert_to_fixed_point_matrix(GetShadowMatrix(playerId + (screenId * 8)), mtx);")
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "void render_player_shadow(" begin)
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "void render_player_shadow_credits(" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "Native ground-shadow function bounds changed")
    endif()
    math(EXPR length "${end}-${begin}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" ${begin} ${length} before)
    string(FIND "${before}" "${anchor}" location)
    if(location LESS 0)
        message(FATAL_ERROR "Native ground-shadow insertion point changed")
    endif()
    set(native [=[
    Mk64NativeShadowEnqueue(gDisplayListHead,GetShadowMatrix(playerId+(screenId*8)),&mtx[0][0],
        gKartShadowVertices,gTextureKartShadow,G_RM_ZB_CLD_SURF | G_RM_ZB_CLD_SURF2,255,MK64_NATIVE_GROUND_SHADOW);
    FrameInterpolation_RecordCloseChild();
    return;
]=])
    string(LENGTH "${anchor}" anchorLength)
    math(EXPR prefixLength "${location}+${anchorLength}")
    string(SUBSTRING "${before}" 0 ${prefixLength} prefix)
    set(after "${prefix}\n${native}\n}\n\n")
    string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    # Remove the old command-writing tail, rather than retaining dead GBI code.
    # Keep transform construction, palette/frame selection and interpolation.
    macro(native_character_insert startMarker endMarker insertion nativeBody)
        string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${startMarker}" begin)
        string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${endMarker}" end)
        if(begin LESS 0 OR end LESS begin)
            message(FATAL_ERROR "Native character function bounds changed: ${startMarker}")
        endif()
        math(EXPR length "${end}-${begin}")
        string(SUBSTRING "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" ${begin} ${length} before)
        string(FIND "${before}" "${insertion}" location)
        if(location LESS 0)
            message(FATAL_ERROR "Native character insertion changed: ${startMarker}")
        endif()
        string(SUBSTRING "${before}" 0 ${location} prefix)
        set(after "${prefix}\n${nativeBody}\n}\n\n")
        string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    endmacro()
    set(native [=[
    {
        const uint8_t colors[8]={gPlayerRedEffect[playerId],gPlayerGreenEffect[playerId],gPlayerBlueEffect[playerId],spC2,
            gPlayerCyanEffect[playerId],gPlayerMagentaEffect[playerId],gPlayerYellowEffect[playerId],255};
        const uint32_t mode=AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
            GBL_c1(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA) | GBL_c2(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA);
        Mk64NativeKartEnqueue(gDisplayListHead,GetKartMatrix(playerId+(screenId*8)),&mtx[0][0],
            &gPlayerVtx[playerId][flipOffset],sKartTexture,gPlayerPalette,colors,mode,true,MK64_NATIVE_GHOST);
        FrameInterpolation_RecordCloseChild();
        return;
    }
]=])
    native_character_insert("void render_ghost(" "void func_80025DE8("
        "    gSPMatrix(gDisplayListHead++, GetKartMatrix(playerId + (screenId * 8))," "${native}")
    set(native [=[
    {
        Mtx* key=GetEffectMatrix();
        convert_to_fixed_point_matrix(key,mtx);
        const uint8_t colors[8]={gPlayerRedEffect[playerId],gPlayerGreenEffect[playerId],gPlayerBlueEffect[playerId],64,
            gPlayerCyanEffect[playerId],gPlayerMagentaEffect[playerId],gPlayerYellowEffect[playerId],255};
        const uint32_t mode=AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_WRAP | ZMODE_XLU | CVG_X_ALPHA | FORCE_BL |
            GBL_c1(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA) | GBL_c2(G_BL_CLR_IN,G_BL_A_IN,G_BL_CLR_MEM,G_BL_1MA);
        Mk64NativeKartEnqueue(gDisplayListHead,key,&mtx[0][0],&gPlayerVtx[playerId][flipOffset],
            sKartTexture,gPlayerPalette,colors,mode,true,MK64_NATIVE_TRAIL);
        gMatrixEffectCount+=1;
        FrameInterpolation_RecordCloseChild();
        return;
    }
]=])
    native_character_insert("void func_80025DE8(" "void render_player_ice_reflection("
        "    AddEffectMatrix(mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);" "${native}")
    set(native [=[
    {
        Mtx* key=GetEffectMatrix();
        convert_to_fixed_point_matrix(key,mtx);
        const uint8_t colors[8]={gPlayerRedEffect[playerId],gPlayerGreenEffect[playerId],gPlayerBlueEffect[playerId],(s16)player->alpha/2,
            gPlayerCyanEffect[playerId],gPlayerMagentaEffect[playerId],gPlayerYellowEffect[playerId],255};
        Mk64NativeKartEnqueue(gDisplayListHead,key,&mtx[0][0],&gPlayerVtx[playerId][flipOffset],
            sKartTexture,gPlayerPalette,colors,G_RM_ZB_XLU_SURF | G_RM_ZB_XLU_SURF2,true,MK64_NATIVE_REFLECTION);
        gMatrixEffectCount+=1;
        FrameInterpolation_RecordCloseChild();
        return;
    }
]=])
    native_character_insert("void render_player_ice_reflection(" "void render_player("
        "    AddEffectMatrix(mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);" "${native}")
    set(native [=[
    Mk64NativeShadowEnqueue(gDisplayListHead,GetShadowMatrix(playerId+(screenId*8)),&mtx[0][0],
        gKartShadowVertices,gTextureKartShadow,G_RM_ZB_CLD_SURF | G_RM_ZB_CLD_SURF2,208,MK64_NATIVE_CREDITS_SHADOW);
    return;
]=])
    native_character_insert("void render_player_shadow_credits(" "typedef u8** kart_texture_t;"
        "    gSPMatrix(gDisplayListHead++, GetShadowMatrix(playerId + (screenId * 8))," "${native}")

    if(MK64_3DS_NATIVE_CHARACTER_SCENARIOS)
        set(fixture [=[
// Development-only render fixture. No simulated player state survives this call.
static void native_character_fixture(Player* player,s8 playerId,s8 screenId,s8 flipOffset) {
    const u32 effects=player->effects;
    const s16 lakitu=player->lakituProps;
    const s16 alpha=player->alpha;
    const u16 ghostAlpha=D_8015F890;
    const s32 otherAlpha=gPlayerOtherScreensAlpha[playerId];
    if(playerId==0)render_player_shadow_credits(player,playerId,screenId);
    else render_player_shadow(player,playerId,screenId);
    if(playerId==1 || playerId==2) {
        D_8015F890=playerId==2;
        render_ghost(player,playerId,screenId,flipOffset);
    } else {
        if(playerId==0 || playerId==5) {
            player->effects|=BOO_EFFECT;player->alpha=160;gPlayerOtherScreensAlpha[playerId]=96;
        }
        if(playerId==6){player->lakituProps|=LAKITU_FIZZLE;player->alpha=80;}
        render_kart(player,playerId,screenId,flipOffset);
        if(playerId==3)func_80025DE8(player,playerId,screenId,flipOffset);
        if(playerId==4)render_player_ice_reflection(player,playerId,screenId,flipOffset);
    }
    player->effects=effects;player->lakituProps=lakitu;player->alpha=alpha;
    D_8015F890=ghostAlpha;gPlayerOtherScreensAlpha[playerId]=otherAlpha;
}

]=])
        string(REPLACE "void render_player(Player* player, s8 playerId, s8 screenId) {"
            "${fixture}void render_player(Player* player, s8 playerId, s8 screenId) {"
            SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
        set(anchor "    temp_t1 = SIDE_OF_KART << (screenId * 4);")
        set(fixture [=[
    native_character_fixture(player,playerId,screenId,var_v1);
    osRecvMesg(&gDmaMesgQueue,(OSMesg*)&sp34,OS_MESG_BLOCK);
    return;
]=])
        string(REPLACE "${anchor}" "${anchor}\n${fixture}" SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    endif()
    string(PREPEND SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "#include \"native_kart_lab_3ds.h\"\n")
endif()
