    # Results balloons own a large optional object pool. Keep the owner alive
    # until every balloon has released its slot, and degrade cleanly if the
    # shared object list is already full instead of overwriting a live object.
    file(READ "${SPAGHETTIKART_GRAND_PRIX_BALLOONS_SOURCE}"
         SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT)
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_ORIGINAL
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")
    string(REPLACE
        "#include \"update_objects.h\""
        "#include \"update_objects.h\"\ns32 try_find_unused_obj_index(s32*);"
        SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_DECL_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")
    if(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_DECL_TEXT STREQUAL
       SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_ORIGINAL)
        message(FATAL_ERROR "The upstream GP balloon allocator include changed")
    endif()
    string(REPLACE
        [=[    for (size_t i = 0; i < _numBalloons; i++) {
        find_unused_obj_index(&gObjectParticle3[i]);
        init_object(gObjectParticle3[i], 0);
    }
}]=]
        [=[    for (size_t i = 0; i < _numBalloons; i++) {
        gObjectParticle3[i] = NULL_OBJECT_ID;
        if (try_find_unused_obj_index(&gObjectParticle3[i]) == NULL_OBJECT_ID) {
            _numBalloons = i;
            break;
        }
        init_object(gObjectParticle3[i], 0);
    }
    _count += 1;
}]=]
        SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_ALLOC_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")
    if(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_ALLOC_TEXT STREQUAL
       SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_DECL_TEXT)
        message(FATAL_ERROR "The upstream GP balloon object allocation changed")
    endif()
    string(REPLACE
        [=[                if (object->state == 0) {
                    delete_object_wrapper(&gObjectParticle3[someIndex]);
                    this->Destroy();
                }]=]
        [=[                if (object->state == 0) {
                    delete_object_wrapper(&gObjectParticle3[someIndex]);
                }]=]
        SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_RELEASE_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")
    if(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_RELEASE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_ALLOC_TEXT)
        message(FATAL_ERROR "The upstream GP balloon per-slot release changed")
    endif()
    string(REPLACE
        [=[    if (someCount == 0) {
        _active = 0;
    }]=]
        [=[    if (someCount == 0) {
        _active = 0;
        Destroy();
    }]=]
        SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_OWNER_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")
    if(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_OWNER_TEXT STREQUAL
       SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_RELEASE_TEXT)
        message(FATAL_ERROR "The upstream GP balloon owner lifetime changed")
    endif()
    string(REPLACE
        "        if ((_numBalloons <= gObjectList[objectIndex].offset[1]) &&"
        "        if ((_numBalloons2 <= gObjectList[objectIndex].offset[1]) &&"
        SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT STREQUAL
       SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_OWNER_TEXT)
        message(FATAL_ERROR "The upstream GP balloon fade threshold changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_SOURCE}"
         "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_TEXT}")

    # Moo Moo Farm needs 62 mandatory mole slots and a 128-entry optional dirt
    # pool. Reserve the mole objects first, then fill as much of the visual pool
    # as remains so object pressure cannot overwrite unrelated live sprites.
    file(READ "${SPAGHETTIKART_MOO_MOO_FARM_SOURCE}"
         SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT)
    set(SPAGHETTIKART_3DS_MOO_MOO_FARM_ORIGINAL
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}")
    string(REPLACE
        "    extern s8 gPlayerCount;"
        "    extern s8 gPlayerCount;\n    s32 try_find_unused_obj_index(s32*);"
        SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MOO_MOO_FARM_DECL_TEXT
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}")
    if(SPAGHETTIKART_3DS_MOO_MOO_FARM_DECL_TEXT STREQUAL
       SPAGHETTIKART_3DS_MOO_MOO_FARM_ORIGINAL)
        message(FATAL_ERROR "The upstream Moo Moo Farm object allocator declaration changed")
    endif()
    string(REPLACE
        [=[    for (size_t i = 0; i < gObjectParticle2_SIZE; i++) {
        find_unused_obj_index(&gObjectParticle2[i]);
    }]=]
        [=[    for (size_t i = 0; i < gObjectParticle2_SIZE; i++) {
        gObjectParticle2[i] = NULL_OBJECT_ID;
    }]=]
        SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MOO_MOO_FARM_RESET_TEXT
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}")
    if(SPAGHETTIKART_3DS_MOO_MOO_FARM_RESET_TEXT STREQUAL
       SPAGHETTIKART_3DS_MOO_MOO_FARM_DECL_TEXT)
        message(FATAL_ERROR "The upstream Moo Moo Farm dirt-pool reset changed")
    endif()
    string(REPLACE
        [=[        GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns1, tick1));
        GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns2, tick2));
        GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns3, tick3));]=]
        [=[        // Each of the 31 moles owns two mandatory object slots. Check
        // the complete transaction before publishing any OMole into World;
        // otherwise a late allocation failure leaves partial groups and
        // dangling group pointers behind.
        if (func_80071FBC() <= OBJECT_LIST_SIZE - 62) {
            GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns1, tick1));
            GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns2, tick2));
            GetWorld()->AddObject(std::make_unique<OMoleGroup>(moleSpawns3, tick3));

            for (size_t i = 0; i < gObjectParticle2_SIZE; i++) {
                if (try_find_unused_obj_index(&gObjectParticle2[i]) == NULL_OBJECT_ID) {
                    break;
                }
            }
        }]=]
        SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT STREQUAL
       SPAGHETTIKART_3DS_MOO_MOO_FARM_RESET_TEXT)
        message(FATAL_ERROR "The upstream Moo Moo Farm mole-group spawn point changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_MOO_MOO_FARM_SOURCE}"
         "${SPAGHETTIKART_3DS_MOO_MOO_FARM_TEXT}")

    file(READ "${SPAGHETTIKART_MOLE_GROUP_SOURCE}"
         SPAGHETTIKART_3DS_MOLE_GROUP_TEXT)
    set(SPAGHETTIKART_3DS_MOLE_GROUP_ORIGINAL
        "${SPAGHETTIKART_3DS_MOLE_GROUP_TEXT}")
    string(REPLACE
        [=[            s32 objectIndex = gObjectParticle2[i];
            if (gObjectList[objectIndex].state != 0) {]=]
        [=[            s32 objectIndex = gObjectParticle2[i];
            if (objectIndex != NULL_OBJECT_ID && gObjectList[objectIndex].state != 0) {]=]
        SPAGHETTIKART_3DS_MOLE_GROUP_TEXT
        "${SPAGHETTIKART_3DS_MOLE_GROUP_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MOLE_GROUP_TEXT STREQUAL
       SPAGHETTIKART_3DS_MOLE_GROUP_ORIGINAL)
        message(FATAL_ERROR "The upstream mole dirt tick loop changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_MOLE_GROUP_SOURCE}"
         "${SPAGHETTIKART_3DS_MOLE_GROUP_TEXT}")

    # Dirt is optional when the shared object pool is pressured. Skip missing
    # entries and scan the pool monotonically for each eruption, preserving the
    # real pool ordinal used for particle orientation while avoiding repeated
    # resource lookups and quadratic slot scans.
    file(READ "${SPAGHETTIKART_MOLE_SOURCE}" SPAGHETTIKART_3DS_MOLE_TEXT)
    set(SPAGHETTIKART_3DS_MOLE_ORIGINAL "${SPAGHETTIKART_3DS_MOLE_TEXT}")
    string(REPLACE
        [=[    if (gPlayerCountSelection1 == 1) {
        sp70 = 8;
    } else {
        sp70 = 4;
    }

    for (var_s7 = 0; var_s7 < sp70; var_s7++) {
        for (var_s1 = 0; var_s1 < gObjectParticle2_SIZE; var_s1++) {
            loopObjectIndex = gObjectParticle2[var_s1];

            if (gObjectList[loopObjectIndex].state != 0) {
                continue;
            }

            u8* mole = (u8*) LOAD_ASSET_RAW(d_course_moo_moo_farm_mole_dirt);]=]
        [=[    if (gPlayerCountSelection1 == 1) {
        sp70 = 8;
    } else {
        sp70 = 4;
    }

    u8* mole = (u8*) LOAD_ASSET_RAW(d_course_moo_moo_farm_mole_dirt);
    if (mole == nullptr) {
        return;
    }
    var_s1 = 0;
    for (var_s7 = 0; var_s7 < sp70; var_s7++) {
        for (; var_s1 < gObjectParticle2_SIZE; var_s1++) {
            loopObjectIndex = gObjectParticle2[var_s1];

            if (loopObjectIndex == NULL_OBJECT_ID ||
                gObjectList[loopObjectIndex].state != 0) {
                continue;
            }]=]
        SPAGHETTIKART_3DS_MOLE_TEXT
        "${SPAGHETTIKART_3DS_MOLE_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MOLE_SCAN_TEXT "${SPAGHETTIKART_3DS_MOLE_TEXT}")
    if(SPAGHETTIKART_3DS_MOLE_SCAN_TEXT STREQUAL SPAGHETTIKART_3DS_MOLE_ORIGINAL)
        message(FATAL_ERROR "The upstream mole dirt allocation loop changed")
    endif()
    string(REPLACE
        [=[            gObjectList[loopObjectIndex].origin_pos[2] = gObjectList[objectIndex].origin_pos[2];
            break;]=]
        [=[            gObjectList[loopObjectIndex].origin_pos[2] = gObjectList[objectIndex].origin_pos[2];
            ++var_s1;
            break;]=]
        SPAGHETTIKART_3DS_MOLE_TEXT
        "${SPAGHETTIKART_3DS_MOLE_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MOLE_CURSOR_TEXT "${SPAGHETTIKART_3DS_MOLE_TEXT}")
    if(SPAGHETTIKART_3DS_MOLE_CURSOR_TEXT STREQUAL SPAGHETTIKART_3DS_MOLE_SCAN_TEXT)
        message(FATAL_ERROR "The upstream mole dirt cursor point changed")
    endif()
    string(REPLACE
        [=[            s32 objectIndex = gObjectParticle2[i];
            Object* object = &gObjectList[objectIndex];
            if (object->state > 0) {]=]
        [=[            s32 objectIndex = gObjectParticle2[i];
            if (objectIndex == NULL_OBJECT_ID) {
                continue;
            }
            Object* object = &gObjectList[objectIndex];
            if (object->state > 0) {]=]
        SPAGHETTIKART_3DS_MOLE_TEXT
        "${SPAGHETTIKART_3DS_MOLE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MOLE_TEXT STREQUAL SPAGHETTIKART_3DS_MOLE_CURSOR_TEXT)
        message(FATAL_ERROR "The upstream mole dirt draw loop changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_MOLE_SOURCE}"
         "${SPAGHETTIKART_3DS_MOLE_TEXT}")
