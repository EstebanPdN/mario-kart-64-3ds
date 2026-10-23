    # The upstream circular object allocator overwrites its starting slot when
    # all 550 entries are occupied. Expose a non-fatal form for optional visual
    # pools and make mandatory allocations stop with a precise diagnostic
    # instead of corrupting a live object and producing broken sprites later.
    file(READ "${SPAGHETTIKART_UPDATE_OBJECTS_SOURCE}"
         SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT)
    set(SPAGHETTIKART_3DS_UPDATE_OBJECTS_ORIGINAL
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT}")
    string(REPLACE
        [=[s32 find_unused_obj_index(s32* arg0) {
    s32 temp_v0;
    s32 temp_v1;

    temp_v1 = objectListSize;
    // clang-format off
    temp_v0 = 0; do { // this two SHOULD be on the same line
        // clang-format on
        ++temp_v1;
        ++temp_v0;

        if (temp_v1 == OBJECT_LIST_SIZE) {
            temp_v1 = 0;
        }
    } while ((gObjectList[temp_v1].unk_0CA != 0) && (temp_v0 != OBJECT_LIST_SIZE));

    gObjectList[temp_v1].unk_0CA = 1;

    *arg0 = temp_v1;
    objectListSize = temp_v1;
    return temp_v1;
}]=]
        [=[static s32 mk64_3ds_find_unused_obj_index(s32* arg0, s32 limit) {
    s32 objectIndex = objectListSize;
    if (objectIndex < 0 || objectIndex >= limit) {
        objectIndex = limit - 1;
    }
    for (s32 count = 0; count < limit; ++count) {
        if (++objectIndex == limit) {
            objectIndex = 0;
        }
        if (gObjectList[objectIndex].unk_0CA == 0) {
            gObjectList[objectIndex].unk_0CA = 1;
            *arg0 = objectIndex;
            objectListSize = objectIndex;
            return objectIndex;
        }
    }
    *arg0 = NULL_OBJECT_ID;
    return NULL_OBJECT_ID;
}

s32 try_find_unused_obj_index(s32* arg0) {
    // Particle, smoke and other optional pools use this non-fatal allocator.
    // Keep a small tail of the vanilla object array available for mandatory
    // race/results objects so visual pressure degrades effects, not gameplay.
    return mk64_3ds_find_unused_obj_index(arg0, OBJECT_LIST_SIZE - 32);
}

s32 find_unused_obj_index(s32* arg0) {
    s32 objectIndex = mk64_3ds_find_unused_obj_index(arg0, OBJECT_LIST_SIZE);
    if (objectIndex == NULL_OBJECT_ID) {
        CM_ThrowRuntimeError("[update_objects] 3DS object pool exhausted");
    }
    return objectIndex;
}]=]
        SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT STREQUAL
       SPAGHETTIKART_3DS_UPDATE_OBJECTS_ORIGINAL)
        message(FATAL_ERROR "The upstream object-slot allocator changed")
    endif()
    set(SPAGHETTIKART_3DS_OPTIONAL_OBJECT_ALLOC_ORIGINAL
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT}")
    string(REPLACE
        "            objectIndex = find_unused_obj_index(id);"
        "            objectIndex = try_find_unused_obj_index(id);"
        SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT STREQUAL
       SPAGHETTIKART_3DS_OPTIONAL_OBJECT_ALLOC_ORIGINAL)
        message(FATAL_ERROR "The upstream optional object-pool allocation path changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_SOURCE}"
         "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_TEXT}")

    # The original game reuses a fixed 100-slot actor array. SpaghettiKart's
    # vector port disabled that reuse and appended a C++ allocation for every
    # shell, banana, and fake item box. Use the 3DS sidecar pool, preserve the
    # real World index after reuse, and keep the original destructible fallback
    # away from permanent slots and non-base C++ wrappers.
    file(READ "${SPAGHETTIKART_ACTORS_SOURCE}" SPAGHETTIKART_3DS_ACTORS_TEXT)
    set(SPAGHETTIKART_3DS_ACTORS_ORIGINAL "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(REPLACE
        "#include <stdio.h>"
        "#include <stdio.h>\nextern bool Mk64BaseActor3DSIsDynamic(const struct Actor*, size_t);"
        SPAGHETTIKART_3DS_ACTORS_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_ACTORS_DECL_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    if(SPAGHETTIKART_3DS_ACTORS_DECL_TEXT STREQUAL SPAGHETTIKART_3DS_ACTORS_ORIGINAL)
        message(FATAL_ERROR "The upstream actor-pool declaration point changed")
    endif()
    string(REPLACE
        [=[    var_t1 = (struct ActorSpawnData*) LOAD_ASSET(d_course_moo_moo_farm_cow_spawn);
    D_8015F704 = 6.4e7f;]=]
        [=[    var_t1 = (struct ActorSpawnData*) LOAD_ASSET_RAW(d_course_moo_moo_farm_cow_spawn);
    if (var_t1 == NULL) {
        return;
    }
    D_8015F704 = 6.4e7f;]=]
        SPAGHETTIKART_3DS_ACTORS_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_ACTORS_COW_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    if(SPAGHETTIKART_3DS_ACTORS_COW_TEXT STREQUAL SPAGHETTIKART_3DS_ACTORS_DECL_TEXT)
        message(FATAL_ERROR "The upstream Moo Moo Farm cow resource lookup changed")
    endif()
    string(REPLACE
        [=[    // try removing a red shell, green shell, banana, or a fake item box if the actor is expired
    for (actorIndex = gNumPermanentActors; actorIndex < ACTOR_LIST_SIZE; actorIndex++) {
        compare = (struct ShellActor*) CM_GetActor(actorIndex);]=]
        [=[    // try removing a red shell, green shell, banana, or a fake item box if the actor is expired
    for (actorIndex = 0; actorIndex < ACTOR_LIST_SIZE; actorIndex++) {
        compare = (struct ShellActor*) CM_GetActor(actorIndex);
        if (!Mk64BaseActor3DSIsDynamic((struct Actor*) compare, gNumPermanentActors)) {
            continue;
        }]=]
        SPAGHETTIKART_3DS_ACTORS_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_ACTORS_FIRST_FALLBACK_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    if(SPAGHETTIKART_3DS_ACTORS_FIRST_FALLBACK_TEXT STREQUAL
       SPAGHETTIKART_3DS_ACTORS_COW_TEXT)
        message(FATAL_ERROR "The upstream expired-actor fallback loop changed")
    endif()
    string(REPLACE
        [=[    // will remove the oldest destructable actor in the list
    for (actorIndex = gNumPermanentActors; actorIndex < ACTOR_LIST_SIZE; actorIndex++) {
        compare = (struct ShellActor*) CM_GetActor(actorIndex);]=]
        [=[    // will remove the oldest destructable actor in the list
    for (actorIndex = 0; actorIndex < ACTOR_LIST_SIZE; actorIndex++) {
        compare = (struct ShellActor*) CM_GetActor(actorIndex);
        if (!Mk64BaseActor3DSIsDynamic((struct Actor*) compare, gNumPermanentActors)) {
            continue;
        }]=]
        SPAGHETTIKART_3DS_ACTORS_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}"
    )
    set(SPAGHETTIKART_3DS_ACTORS_SECOND_FALLBACK_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    if(SPAGHETTIKART_3DS_ACTORS_SECOND_FALLBACK_TEXT STREQUAL
       SPAGHETTIKART_3DS_ACTORS_FIRST_FALLBACK_TEXT)
        message(FATAL_ERROR "The upstream oldest-actor fallback loop changed")
    endif()
    string(REPLACE
        [=[// returns actor index if any available actor type is -1
s16 add_actor_to_empty_slot(Vec3f pos, Vec3s rot, Vec3f velocity, s16 actorType) {
    size_t index;

    // if (gNumActors >= CM_GetActorSize()) {
    //     return try_remove_destructable_item(pos, rot, velocity, actorType);
    // }

    // Cleanup unused actors
    for (index = 0; index < CM_GetActorSize(); index++) {
        // if (CM_GetActor(index)->flags == 0) {
        //! @todo Commented out because deletes too soon.
        // CM_DeleteActor(index);
        // gNumActors--;
        //}
    }
    gNumActors++;
    struct Actor* actor = CM_AddBaseActor();
    actor_init(actor, pos, rot, velocity, actorType);
    CM_ActorBeginPlay(actor);
    return (s16) CM_GetActorSize() - 1; // Return current index;
}]=]
        [=[// returns actor index if any available actor type is -1
s16 add_actor_to_empty_slot(Vec3f pos, Vec3s rot, Vec3f velocity, s16 actorType) {
    struct Actor* actor = CM_AddBaseActor();
    if (actor == NULL) {
        return try_remove_destructable_item(pos, rot, velocity, actorType);
    }

    size_t actorIndex = CM_FindActorIndex(actor);
    if (actorIndex > INT16_MAX) {
        return -1;
    }
    actor_init(actor, pos, rot, velocity, actorType);
    CM_ActorBeginPlay(actor);
    gNumActors++;
    return (s16) actorIndex;
}]=]
        SPAGHETTIKART_3DS_ACTORS_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_ACTORS_TEXT STREQUAL
       SPAGHETTIKART_3DS_ACTORS_SECOND_FALLBACK_TEXT)
        message(FATAL_ERROR "The upstream actor allocation path changed")
    endif()
    # Culling a fake item box used to leave its interpolation child open.
    # Open only once the visibility exits have passed, preserving strict recorder checks.
    file(READ "${SPAGHETTIKART_ROOT}/src/actors/fake_item_box/render.inc.c" FAKE_BOX_ORIGINAL)
    string(REPLACE
        "    FrameInterpolation_RecordOpenChild(\"fake_item_box\", uniqueIdentifier);\n"
        "" FAKE_BOX_FIXED "${FAKE_BOX_ORIGINAL}")
    string(REPLACE
        "    actor_rendered(camera, (struct Actor*) fakeItemBox);"
        "    FrameInterpolation_RecordOpenChild(\"fake_item_box\", uniqueIdentifier);\n    actor_rendered(camera, (struct Actor*) fakeItemBox);"
        FAKE_BOX_FIXED "${FAKE_BOX_FIXED}")
    if(FAKE_BOX_FIXED STREQUAL FAKE_BOX_ORIGINAL)
        message(FATAL_ERROR "The upstream fake item box visibility boundary changed")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/fake_item_box_render_3ds.inc.c" "${FAKE_BOX_FIXED}")
    string(REPLACE "actors/fake_item_box/render.inc.c" "fake_item_box_render_3ds.inc.c"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    # Restore only the original train LOD. The desktop fallback forces every
    # visible train (including far-away wheels) into its highest-detail path.
    file(READ "${SPAGHETTIKART_ROOT}/src/actors/train/render.inc.c" TRAIN_RENDER_ORIGINAL)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/actors/train/render.inc.c")
    string(REGEX MATCHALL "gDisableLod\", 1" TRAIN_LOD_MATCHES "${TRAIN_RENDER_ORIGINAL}")
    list(LENGTH TRAIN_LOD_MATCHES TRAIN_LOD_COUNT)
    if(NOT TRAIN_LOD_COUNT EQUAL 3)
        message(FATAL_ERROR "The upstream three train LOD controls changed")
    endif()
    string(REPLACE "gDisableLod\", 1" "gDisableLod\", 0" TRAIN_RENDER_FIXED "${TRAIN_RENDER_ORIGINAL}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/train_render_3ds.inc.c" "${TRAIN_RENDER_FIXED}")
    string(REPLACE "actors/train/render.inc.c" "train_render_3ds.inc.c"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(REPLACE
        "    FrameInterpolation_RecordOpenChild(\"shell\", TAG_ITEM_ADDR(((actorIdx << 4) | (camera - cameras))));\n"
        "" SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(REPLACE
        "    actor_rendered(camera, (struct Actor*) shell);"
        "    FrameInterpolation_RecordOpenChild(\"shell\", TAG_ITEM_ADDR(((actorIdx << 4) | (camera - cameras))));\n    actor_rendered(camera, (struct Actor*) shell);"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(REPLACE "    if (maxObjectsReached) {\n        return;"
        "    if (maxObjectsReached) {\n        FrameInterpolation_RecordCloseChild();\n        return;"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    include(platform/3ds/cmake/jungle_trees_3ds.cmake)
    # Use the exact same consumed box and collision as humans, not a parallel
    # proximity scan. OFF retains the original CPU box destruction/no reward.
    set(SPAGHETTIKART_3DS_ACTORS_TEXT "#include \"item_rules_3ds.h\"\n#include \"auto_mode_3ds.h\"\n${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "void destroy_destructable_actor(struct Actor* actor) {" CPU_WARNING_STOP_ANCHOR)
    if(CPU_WARNING_STOP_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream shell destruction hook changed")
    endif()
    string(REPLACE "void destroy_destructable_actor(struct Actor* actor) {"
        "void destroy_destructable_actor(struct Actor* actor) {\n    if (actor->type == ACTOR_RED_SHELL || actor->type == ACTOR_BLUE_SPINY_SHELL) Mk64ShellWarning3DSStop(actor);"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    foreach(CPU_BOX_ITEM ITEM_NONE ITEM_BLUE_SPINY_SHELL)
        set(CPU_BOX_PICKUP_OLD "                if (player->type & PLAYER_HUMAN) {\n                    func_8007ABFC(player - gPlayerOne, ${CPU_BOX_ITEM});\n                }")
        string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "${CPU_BOX_PICKUP_OLD}" CPU_BOX_PICKUP_ANCHOR)
        if(CPU_BOX_PICKUP_ANCHOR LESS 0)
            message(FATAL_ERROR "Upstream CPU box collision changed: ${CPU_BOX_ITEM}")
        endif()
        string(REPLACE "${CPU_BOX_PICKUP_OLD}"
            "                if (player->type & PLAYER_HUMAN) {\n                    func_8007ABFC(player - gPlayerOne, ${CPU_BOX_ITEM});\n                } else {\n                    Mk64CpuItemBox3DSPickup(player - gPlayerOne, ${CPU_BOX_ITEM});\n                }"
            SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    endforeach()
    # Acceptance after the real sphere collision, before damage/audio/destruction.
    # Red and blue share the same native assignment; replace both together.
    foreach(ITEM_HIT_TRIGGER HIT_BANANA_TRIGGER LOW_TUMBLE_TRIGGER HIGH_TUMBLE_TRIGGER VERTICAL_TUMBLE_TRIGGER)
        set(ITEM_HIT_OLD "player->triggers |= ${ITEM_HIT_TRIGGER};")
        string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "${ITEM_HIT_OLD}" ITEM_HIT_ANCHOR)
        if(ITEM_HIT_ANCHOR LESS 0)
            message(FATAL_ERROR "Upstream actor item hit changed: ${ITEM_HIT_TRIGGER}")
        endif()
        # AUTO owns real CPU items, so native human-owner-only celebration
        # never runs. Credit only an accepted hit, using the actual owner and
        # native voice; rejected shields/ghosts/self hits must stay silent.
        string(REPLACE "${ITEM_HIT_OLD}" "if (!Mk64ItemHit3DSTry(player)) break;\n            ${ITEM_HIT_OLD}\n            if (temp_lo != temp_v1 && Mk64Auto3DSWatchedAudio(temp_v1) && !(gPlayers[temp_v1].type & PLAYER_HUMAN))\n                func_800C90F4(temp_v1, (gPlayers[temp_v1].characterId * 0x10) + SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x06));"
            SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    endforeach()
    # A rejected guided-shell hit is consumed, not deferred until protection
    # expires. Run before native tumble guards, which otherwise keep it alive.
    set(CPU_PROTECTED_SHELL_OLD "    temp_lo = player - gPlayerOne;\n    switch (actor->type) {")
    string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "${CPU_PROTECTED_SHELL_OLD}" CPU_PROTECTED_SHELL_ANCHOR)
    if(CPU_PROTECTED_SHELL_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream player/actor collision entry changed")
    endif()
    set(CPU_PROTECTED_SHELL_HOOK [=[    temp_lo = player - gPlayerOne;
    if (Mk64NewItems3DSEnabled() &&
        (actor->type == ACTOR_RED_SHELL || actor->type == ACTOR_BLUE_SPINY_SHELL) &&
        actor->state != DESTROYED_SHELL &&
        !(player->effects & BOO_EFFECT) && !(player->type & PLAYER_INVISIBLE_OR_BOMB) &&
        !((temp_lo == actor->rot[2]) && (actor->flags & 0x1000)) &&
        Mk64ItemHit3DSProtected(player) && query_collision_player_vs_actor_item(player, actor) == COLLISION) {
        // Red ends on contact. Blue passes protected road victims without a
        // delayed hit; only its final target can consume it.
        if (actor->type == ACTOR_RED_SHELL || temp_lo == actor->unk_04)
            destroy_destructable_actor(actor);
        return; // No damage, owner-hit credit, shield renewal or delayed hit.
    }
    switch (actor->type) {]=])
    string(REPLACE "${CPU_PROTECTED_SHELL_OLD}" "${CPU_PROTECTED_SHELL_HOOK}"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    # Travelling modern blue shells must not cancel one another on the road.
    # Ordinary native collision already lets them clear other destructibles.
    set(CPU_BLUE_PAIR_OLD "        if ((actor1->type == ACTOR_BLUE_SPINY_SHELL) && (actor2->type == ACTOR_BLUE_SPINY_SHELL)) {")
    string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "${CPU_BLUE_PAIR_OLD}" CPU_BLUE_PAIR_ANCHOR)
    if(CPU_BLUE_PAIR_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream blue/blue actor collision changed")
    endif()
    set(CPU_BLUE_PAIR_HOOK [=[        if (Mk64NewItems3DSEnabled() &&
            actor1->type == ACTOR_BLUE_SPINY_SHELL && actor2->type == ACTOR_BLUE_SPINY_SHELL) {
            bool travelling1 = actor1->state != DESTROYED_SHELL && ((struct ShellActor*)actor1)->unk_16 > 0;
            bool travelling2 = actor2->state != DESTROYED_SHELL && ((struct ShellActor*)actor2)->unk_16 > 0;
            if (travelling1 && travelling2) return; // Both continue to their own leaders.
            if (travelling1 || travelling2) {
                destroy_destructable_actor(travelling1 ? actor2 : actor1);
                play_sound_on_destructible_actor_collision(actor1, actor2);
                return;
            }
        }
]=])
    string(REPLACE "${CPU_BLUE_PAIR_OLD}" "${CPU_BLUE_PAIR_HOOK}${CPU_BLUE_PAIR_OLD}"
        SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
    include(platform/3ds/cmake/native_vegetation_game_3ds.cmake)
    include(platform/3ds/cmake/native_item_box_game_3ds.cmake)
    include(platform/3ds/cmake/native_projectile_game_3ds.cmake)
    file(WRITE "${SPAGHETTIKART_3DS_ACTORS_SOURCE}"
         "${SPAGHETTIKART_3DS_ACTORS_TEXT}")

    # Red shells normally ignore stick direction. Generate a 3DS-only actor
    # copy that gives a human player the requested backwards launch while
    # preserving the existing guided forward path and all CPU behavior.
    file(READ "${SPAGHETTIKART_RED_BLUE_SHELL_SOURCE}" SPAGHETTIKART_RED_BLUE_SHELL_TEXT)
    set(SPAGHETTIKART_3DS_RED_BLUE_SHELL_ORIGINAL "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    string(REPLACE
        [=[                    controller->buttonDepressed &= ~Z_TRIG;
                    shell->state = RELEASED_SHELL;]=]
        [=[                    controller->buttonDepressed &= ~Z_TRIG;
                    if (Mk64NewItems3DSEnabled() && pad13 == ACTOR_RED_SHELL &&
                        controller->rawStickY < THROW_SHELL_BACKWARDS) {
                        height = player->speed > 8.0f ? player->speed * 1.2f : 8.0f;
                        somePosVel[0] = 0.0f;
                        somePosVel[1] = 0.0f;
                        somePosVel[2] = -height;
                        func_802B64C4(somePosVel, player->rotation[1] + player->unk_0C0);
                        shell->velocity[0] = somePosVel[0];
                        shell->velocity[1] = somePosVel[1];
                        shell->velocity[2] = somePosVel[2];
                        shell->state = MOVING_SHELL;
                        shell->someTimer = 0x001E;
                        func_800C9060(shell->playerId, SOUND_ARG_LOAD(0x19, 0x00, 0x80, 0x04));
                        func_800C90F4(shell->playerId,
                                      (player->characterId * 0x10) +
                                          SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x00));
                        add_red_shell_in_unexpired_actor_list(CM_FindActorIndex(shell));
                        return;
                    }
                    shell->state = RELEASED_SHELL;]=]
        SPAGHETTIKART_RED_BLUE_SHELL_TEXT
        "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}"
    )
    if(SPAGHETTIKART_RED_BLUE_SHELL_TEXT STREQUAL SPAGHETTIKART_3DS_RED_BLUE_SHELL_ORIGINAL)
        message(FATAL_ERROR "The upstream red-shell release point changed")
    endif()
    string(REGEX MATCHALL "void update_actor_red_blue_shell\\(struct ShellActor\\* shell\\)" ROAD_SHELL_FUNCTIONS "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    list(LENGTH ROAD_SHELL_FUNCTIONS ROAD_SHELL_FUNCTION_COUNT)
    string(REGEX MATCHALL "pad13 = shell->type" ROAD_SHELL_HOOKS "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    list(LENGTH ROAD_SHELL_HOOKS ROAD_SHELL_HOOK_COUNT)
    if(NOT ROAD_SHELL_FUNCTION_COUNT EQUAL 1 OR NOT ROAD_SHELL_HOOK_COUNT EQUAL 1)
        message(FATAL_ERROR "The upstream road-shell guidance hook changed")
    endif()
    string(REPLACE "void update_actor_red_blue_shell(struct ShellActor* shell) {"
        "#include \"road_shells_3ds.inc.h\"\n\nvoid update_actor_red_blue_shell(struct ShellActor* shell) {"
        SPAGHETTIKART_RED_BLUE_SHELL_TEXT "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    string(REPLACE "    pad13 = shell->type;"
        "    pad13 = shell->type;\n    Mk64RoadShellWarning3DS(shell);\n    if (Mk64UpdateRoadShell3DS(shell)) return;"
        SPAGHETTIKART_RED_BLUE_SHELL_TEXT "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    string(REPLACE "                        func_800C9D80(shell->pos, shell->velocity, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));"
        "                        if (!Mk64NewItems3DSEnabled()) func_800C9D80(shell->pos, shell->velocity, SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08));"
        SPAGHETTIKART_RED_BLUE_SHELL_TEXT "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    set(MK64_CPU_SHELL_SOUND_ANCHOR "if ((gPlayers[shell->playerId].type & PLAYER_HUMAN) != 0) {")
    string(FIND "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}" "${MK64_CPU_SHELL_SOUND_ANCHOR}" MK64_CPU_SHELL_SOUND_FOUND)
    if(MK64_CPU_SHELL_SOUND_FOUND LESS 0)
        message(FATAL_ERROR "Upstream CPU shell release sound gate changed")
    endif()
    string(REPLACE "${MK64_CPU_SHELL_SOUND_ANCHOR}"
        "if ((gPlayers[shell->playerId].type & PLAYER_HUMAN) != 0 || Mk64NewItems3DSEnabled() || Mk64Auto3DSActive()) {"
        SPAGHETTIKART_RED_BLUE_SHELL_TEXT "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    set(SPAGHETTIKART_RED_BLUE_SHELL_TEXT "#include \"auto_mode_3ds.h\"\n${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    file(WRITE "${SPAGHETTIKART_3DS_RED_BLUE_SHELL_SOURCE}"
         "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    file(READ "${SPAGHETTIKART_ACTORS_EXTENDED_SOURCE}" SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT)
    set(SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT "#include \"item_rules_3ds.h\"\n${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    string(FIND "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}" "void update_actor_triple_shell(TripleShellParent* parent, s16 shellType) {" CPU_TRIPLE_TARGET_ANCHOR)
    string(REGEX MATCHALL "someVelocity\\[2\\] = 8" CPU_TRIPLE_RELEASES "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    list(LENGTH CPU_TRIPLE_RELEASES CPU_TRIPLE_RELEASE_COUNT)
    if(CPU_TRIPLE_TARGET_ANCHOR LESS 0 OR NOT CPU_TRIPLE_RELEASE_COUNT EQUAL 3)
        message(FATAL_ERROR "Upstream triple-shell release changed")
    endif()
    string(REPLACE "void update_actor_triple_shell(TripleShellParent* parent, s16 shellType) {"
        "#include \"cpu_item_target_3ds.inc.h\"\n\nvoid update_actor_triple_shell(TripleShellParent* parent, s16 shellType) {"
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    string(REPLACE "someVelocity[2] = 8;" "someVelocity[2] = Mk64CpuTripleShellSpeed3DS(parent->playerId, parent->type);"
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    string(REPLACE "void use_thunder_item(Player* player) {"
        "void use_thunder_item(Player* player) {\n    Mk64NewItems3DSLightningUsed();"
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    string(REPLACE
        "#include \"actors/blue_and_red_shells/update.inc.c\""
        "#include \"${SPAGHETTIKART_3DS_RED_BLUE_SHELL_SOURCE}\""
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}"
    )
    string(REGEX MATCHALL "shell->rotVelocity = 0" ROAD_SHELL_RESETS "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    list(LENGTH ROAD_SHELL_RESETS ROAD_SHELL_RESET_COUNT)
    if(NOT ROAD_SHELL_RESET_COUNT EQUAL 3)
        message(FATAL_ERROR "The upstream three shell constructors changed")
    endif()
    string(REPLACE "    shell->rotVelocity = 0;"
        "    shell->rotVelocity = 0;\n    shell->unk_16 = 0; // Reset road guidance when an actor slot is reused."
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    set(SPAGHETTIKART_3DS_BLUE_SHELL_GUARD_ORIGINAL
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
    string(REPLACE
        [=[    s16 actorIndex = use_red_shell_item(player);
    GET_ACTOR(actorIndex)->type = ACTOR_BLUE_SPINY_SHELL;]=]
        [=[    s16 actorIndex = use_red_shell_item(player);
    if (actorIndex < 0) {
        return -1;
    }
    GET_ACTOR(actorIndex)->type = ACTOR_BLUE_SPINY_SHELL;]=]
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}"
    )
    if(SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT STREQUAL
       SPAGHETTIKART_3DS_BLUE_SHELL_GUARD_ORIGINAL)
        message(FATAL_ERROR "The upstream blue-shell allocation path changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_SOURCE}"
         "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}")
