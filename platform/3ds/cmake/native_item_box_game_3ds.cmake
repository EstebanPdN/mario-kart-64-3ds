if(MK64_3DS_NATIVE_ITEM_BOX_LAB)
    file(READ "${SPAGHETTIKART_ROOT}/src/actors/item_box/render.inc.c" nativeItemBox)
    set(nativeItemRoots D_0D002EE8 itemBoxQuestionMarkModel D_0D003090 D_0D003158
        D_0D0031B8 D_0D003128 D_0D0031E8 D_0D003188 D_0D0030F8)
    set(rootId 12)
    foreach(root IN LISTS nativeItemRoots)
        set(model someMatrix1)
        if(rootId GREATER 14)
            set(model someMatrix2)
        endif()
        set(before "gSPDisplayList(gDisplayListHead++, ${root});")
        string(FIND "${nativeItemBox}" "${before}" found)
        if(found LESS 0)
            message(FATAL_ERROR "Native item-box root changed: ${root}")
        endif()
        string(REPLACE "${before}" "Mk64NativeVegetationEnqueue(${rootId}, gDisplayListHead, &${model}[0][0]);" nativeItemBox "${nativeItemBox}")
        math(EXPR rootId "${rootId}+1")
    endforeach()
    foreach(model someMatrix1 someMatrix2)
        string(REPLACE "!render_set_position(${model}, 0)" "gMatrixObjectCount >= MTX_OBJECT_POOL_SIZE" nativeItemBox "${nativeItemBox}")
    endforeach()
    foreach(flag G_LIGHTING G_CULL_BACK)
        string(REPLACE "gSPClearGeometryMode(gDisplayListHead++, ${flag});"
            "Mk64NativeItemBoxState(gDisplayListHead, ${flag}, 0, ~0u, 0);" nativeItemBox "${nativeItemBox}")
    endforeach()
    foreach(flag G_SHADING_SMOOTH G_CULL_BACK)
        string(REPLACE "gSPSetGeometryMode(gDisplayListHead++, ${flag});"
            "Mk64NativeItemBoxState(gDisplayListHead, 0, ${flag}, ~0u, 0);" nativeItemBox "${nativeItemBox}")
    endforeach()
    string(REGEX REPLACE "gDPSetRenderMode\\(gDisplayListHead\\+\\+, ([A-Za-z0-9_]+), ([A-Za-z0-9_]+)\\);"
        "Mk64NativeItemBoxState(gDisplayListHead, 0, 0, (\\1 | \\2), 0);" nativeItemBox "${nativeItemBox}")
    # The shade resource overwrites this combine before the next draw; the
    # original matrix-failure branches precede it. BlendMask is a GBI no-op.
    string(REPLACE "gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA, G_CC_MODULATEIA);" "/* Native shade material owns the combine. */" nativeItemBox "${nativeItemBox}")
    string(REPLACE "gDPSetBlendMask(gDisplayListHead++, 0xFF);" "/* Source blend-mask command is a no-op. */" nativeItemBox "${nativeItemBox}")
    string(REPLACE "gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);"
        "Mk64NativeItemBoxState(gDisplayListHead, 0, 0, ~0u, 1);" nativeItemBox "${nativeItemBox}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/native_item_box_3ds.inc.c" "${nativeItemBox}")
    string(REPLACE "actors/item_box/render.inc.c" "native_item_box_3ds.inc.c"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
endif()
