if(MK64_3DS_NATIVE_VEGETATION_LAB)
    file(READ "${SPAGHETTIKART_ROOT}/src/actors/trees/render.inc.c" nativeTrees)
    set(nativeFunctions mario_raceway yoshi_valley royal_raceway moo_moo_farm luigi_raceway
        peach_castle bowser_castle frappe_snowland cactus1_kalimari_desert
        cactus2_kalimari_desert cactus3_kalimari_desert)
    set(nativeRoots d_course_mario_raceway_dl_tree d_course_yoshi_valley_dl_tree
        d_course_royal_raceway_dl_tree d_course_moo_moo_farm_dl_tree d_course_luigi_raceway_dl_FC70
        d_course_royal_raceway_dl_castle_tree d_course_bowsers_castle_dl_bush
        d_course_frappe_snowland_dl_tree d_course_kalimari_desert_dl_cactus1
        d_course_kalimari_desert_dl_cactus2 d_course_kalimari_desert_dl_cactus3)
    set(rootId 0)
    foreach(suffix IN LISTS nativeFunctions)
        if(suffix STREQUAL "bowser_castle")
            set(function "render_actor_bush_${suffix}")
        elseif(suffix MATCHES "^cactus")
            set(function "render_actor_tree_${suffix}")
        else()
            set(function "render_actor_tree_${suffix}")
        endif()
        list(GET nativeRoots ${rootId} root)
        string(FIND "${nativeTrees}" "void ${function}(" begin)
        string(FIND "${nativeTrees}" "    if (render_set_position(arg1, 0) != 0) {" insertion)
        # Prior functions have already lost this tail, so the next occurrence
        # belongs to the current function; check its source root before editing.
        if(begin LESS 0 OR insertion LESS begin)
            message(FATAL_ERROR "Native vegetation function changed: ${function}")
        endif()
        string(SUBSTRING "${nativeTrees}" ${insertion} -1 remainder)
        string(FIND "${remainder}" "\n}" end)
        if(end LESS 0)
            message(FATAL_ERROR "Native vegetation tail is unterminated")
        endif()
        math(EXPR length "${end}+2")
        string(SUBSTRING "${remainder}" 0 ${length} before)
        string(FIND "${before}" "gSPDisplayList(gDisplayListHead++, ${root});" hasRoot)
        if(hasRoot LESS 0)
            message(FATAL_ERROR "Native vegetation root mapping changed: ${function}")
        endif()
        set(after "    if (gMatrixObjectCount < MTX_OBJECT_POOL_SIZE)\n        Mk64NativeVegetationEnqueue(${rootId},gDisplayListHead,&arg1[0][0]);\n}")
        string(REPLACE "${before}" "${after}" nativeTrees "${nativeTrees}")
        math(EXPR rootId "${rootId}+1")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/native_trees_3ds.inc.c" "${nativeTrees}")
    string(REPLACE "actors/trees/render.inc.c" "native_trees_3ds.inc.c"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    set(before [=[    if (render_set_position(sp20, 0) != 0) {
        gSPDisplayList(gDisplayListHead++, D_0D007B20);
        pos[1] -= 2.0f;
    }]=])
    set(after [=[    if (gMatrixObjectCount < MTX_OBJECT_POOL_SIZE) {
        Mk64NativeVegetationEnqueue(11,gDisplayListHead,&sp20[0][0]);
        pos[1] -= 2.0f;
    }]=])
    string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "${before}" hasShadow)
    if(hasShadow LESS 0)
        message(FATAL_ERROR "Native vegetation shared shadow changed")
    endif()
    string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(PREPEND SPAGHETTIKART_3DS_ACTORS_TEXT "#include \"native_vegetation_lab_3ds.h\"\n")
endif()
