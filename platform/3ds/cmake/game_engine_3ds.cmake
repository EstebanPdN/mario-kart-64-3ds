# Build-time adaptations of the pinned game and their explicit source list.
# Module order is part of the generation contract; never edit generated output.
if(MK64_3DS_BUILD_GAME_ENGINE)
    include(FetchContent)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)

    set(SPAGHETTIKART_ROOT "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart")
    file(GLOB SPAGHETTIKART_ENGINE_ROOT CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/*.cpp"
    )
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_ROOT "${SOLO_MANAGER_SOURCE}")
    list(APPEND SPAGHETTIKART_ENGINE_ROOT "${SOLO_MANAGER_GENERATED}")
    list(FILTER SPAGHETTIKART_ENGINE_ROOT EXCLUDE REGEX ".*/HM_Intro\\.cpp$")
    list(FILTER SPAGHETTIKART_ENGINE_ROOT EXCLUDE REGEX ".*/TrackBrowser\\.cpp$")
    list(FILTER SPAGHETTIKART_ENGINE_ROOT EXCLUDE REGEX ".*/wasm\\.cpp$")
    set(SPAGHETTIKART_WORLD_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/World.cpp")
    set(SPAGHETTIKART_3DS_WORLD_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/World_3ds.cpp")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_ROOT "${SPAGHETTIKART_WORLD_SOURCE}")
    file(GLOB SPAGHETTIKART_ENGINE_ACTORS CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/actors/*.cpp"
    )
    list(FILTER SPAGHETTIKART_ENGINE_ACTORS EXCLUDE REGEX ".*/(BowserStatue|Cloud|Ship|SpaghettiShip|Starship|Text)\\.cpp$")
    file(GLOB SPAGHETTIKART_ENGINE_CAMERAS CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/cameras/*.cpp"
    )
    file(GLOB SPAGHETTIKART_ENGINE_OBJECTS CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/objects/*.cpp"
    )
    set(MK64_LAKITU_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/objects/Lakitu.cpp")
    set(MK64_LAKITU_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/Lakitu_3ds.cpp")
    file(READ "${MK64_LAKITU_SOURCE}" MK64_LAKITU_TEXT)
    # The native CI8 silhouette is clean. Keep starter minification's native
    # bilinear/texel-centre sampling; retain the accepted finish flag's point
    # sampling. Replace unsupported random alpha dither with alpha rejection.
    # Restrict this state to the referee; restore normal filtering before its
    # shadow / the next object, rather than changing all course textures.
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007D78);"
        "            gSPDisplayList(gDisplayListHead++, (Gfx*) D_0D007D78);\n            gDPSetTextureFilter(gDisplayListHead++, object->unk_0D8 == 1 ? G_TF_BILERP : G_TF_POINT);")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gDPSetAlphaCompare(gDisplayListHead++, G_AC_DITHER);"
        "            gDPSetTextureFilter(gDisplayListHead++, object->unk_0D8 == 1 ? G_TF_BILERP : G_TF_POINT);\n            gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "        if (gScreenModeSelection == SCREEN_MODE_1P) {"
        "        gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);\n        gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);\n        gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);\n        if (gScreenModeSelection == SCREEN_MODE_1P) {")
    # The sampler fix solved finish-flag specks on hardware, but countdown
    # specks persist. Bypass CI8/TLUT import ONLY for the 32 starter frames,
    # using the exact native pixels in lifetime-stable RGBA16 storage.
    mk64_auto_replace(MK64_LAKITU_TEXT
        "        object = &gObjectList[objectIndex];"
        "        object = &gObjectList[objectIndex];\n        const u8* countdown = object->unk_0D8 == 1 ? Mk64LakituCountdown3DSFrame(object->unk_0D3, object->activeTLUT) : nullptr;")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gDPLoadTLUT_pal256(gDisplayListHead++, object->activeTLUT);\n            gDPLoadTextureTile(gDisplayListHead++, object->activeTexture, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0, 0,\n                               width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,\n                               G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);"
        "            if (countdown) {\n                gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);\n                gDPLoadTextureTile(gDisplayListHead++, countdown, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0, 0,\n                                   width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,\n                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);\n            } else {\n                gDPLoadTLUT_pal256(gDisplayListHead++, object->activeTLUT);\n                gDPLoadTextureTile(gDisplayListHead++, object->activeTexture, G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0, 0,\n                                   width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,\n                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);\n            }")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gDPLoadTLUT_pal256(gDisplayListHead++, object->activeTLUT);\n            rsp_load_texture((u8*) object->activeTexture, width, height);"
        "            if (countdown) {\n                gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);\n                gDPLoadTextureTile(gDisplayListHead++, countdown, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0, 0,\n                                   width - 1, height - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,\n                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);\n            } else {\n                gDPLoadTLUT_pal256(gDisplayListHead++, object->activeTLUT);\n                rsp_load_texture((u8*) object->activeTexture, width, height);\n            }")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "    gObjectList[objectIndex].vertex = fixed_common_vtx_lakitu;"
        "    Mk64LakituCountdown3DSPrepare(sLakituTextures, ARRAY_COUNT(sLakituTextures), common_tlut_lakitu_countdown, ARRAY_COUNT(common_tlut_lakitu_countdown));\n    gObjectList[objectIndex].vertex = fixed_common_vtx_lakitu;")
    # The shared referee setup lists do NOT set the cycle type. Their repeated
    # TEXEL0 combiner can consume the neighbouring stale tile in cycle two.
    # Make starter output independent of that tile without changing the caller's
    # cycle mode, native pixels, matrices, animation or the working finish flag.
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gDPSetTextureFilter(gDisplayListHead++, object->unk_0D8 == 1 ? G_TF_BILERP : G_TF_POINT);\n            s32 heightIndex;"
        "            gDPSetTextureFilter(gDisplayListHead++, object->unk_0D8 == 1 ? G_TF_BILERP : G_TF_POINT);\n            if (object->unk_0D8 == 1) {\n                gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0,\n                                 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);\n            }\n            s32 heightIndex;")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            set_transparency(object->primAlpha);"
        "            set_transparency(object->primAlpha);\n            if (object->unk_0D8 == 1) {\n                gDPSetCombineLERP(gDisplayListHead++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0,\n                                 0, 0, 0, COMBINED, 0, 0, 0, COMBINED);\n            }")
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gSPDisplayList(gDisplayListHead++, (Gfx*) common_rectangle_display);\n            FrameInterpolation_RecordCloseChild();"
        "            gSPDisplayList(gDisplayListHead++, (Gfx*) common_rectangle_display);\n            if (object->unk_0D8 == 1) {\n                if (func_80072354(objectIndex, 2) != 0) {\n                    gDPSetCombineMode(gDisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);\n                } else {\n                    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);\n                }\n            }\n            FrameInterpolation_RecordCloseChild();")
    # The opposite-diagonal submission removed the starter-corner specks in the
    # hardware capture; the maintainer saw them return after A restored the old
    # path. Keep 1--3 for every starter draw, independently of capture/Modern
    # Logic. Four vertices, winding, UVs and native shadows stay intact. Do not
    # alter finish/rescue geometry or claim a specific PICA root cause.
    mk64_auto_replace(MK64_LAKITU_TEXT
        "            gSPDisplayList(gDisplayListHead++, (Gfx*) common_rectangle_display);"
        "            if (object->unk_0D8 == 1) {\n                gSP2Triangles(gDisplayListHead++, 0, 3, 1, 0, 1, 3, 2, 0);\n            } else {\n                gSPDisplayList(gDisplayListHead++, (Gfx*) common_rectangle_display);\n            }")
    file(WRITE "${MK64_LAKITU_GENERATED}" "#include \"lakitu_sprite_3ds.h\"\n${MK64_LAKITU_TEXT}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MK64_LAKITU_SOURCE}")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_OBJECTS "${MK64_LAKITU_SOURCE}")
    list(APPEND SPAGHETTIKART_ENGINE_OBJECTS "${MK64_LAKITU_GENERATED}")
    set(SOLO_BOMB_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/objects/BombKart.cpp")
    set(SOLO_BOMB_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/BombKart_solo_3ds.cpp")
    file(READ "${SOLO_BOMB_SOURCE}" SOLO_BOMB_TEXT)
    mk64_auto_replace(SOLO_BOMB_TEXT "for (size_t i = 0; i < gPlayerCount; i++) {"
        "for (size_t i = 0; i < (Mk64SoloVs3DS() ? NUM_PLAYERS : gPlayerCount); i++) {")
    mk64_auto_replace(SOLO_BOMB_TEXT "                    if (!(player->effects & 0x80000000)) {"
        "                    if ((player->type & PLAYER_EXISTS) && !(player->type & PLAYER_INVISIBLE_OR_BOMB) && !(player->effects & 0x80000000)) {")
    file(WRITE "${SOLO_BOMB_GENERATED}" "#include \"solo_modes_3ds.h\"\n${SOLO_BOMB_TEXT}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOLO_BOMB_SOURCE}")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_OBJECTS "${SOLO_BOMB_SOURCE}")
    list(APPEND SPAGHETTIKART_ENGINE_OBJECTS "${SOLO_BOMB_GENERATED}")
    set(SPAGHETTIKART_PODIUM_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/objects/Podium.cpp")
    set(SPAGHETTIKART_3DS_PODIUM_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/Podium_3ds.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SPAGHETTIKART_PODIUM_SOURCE}")
    file(READ "${SPAGHETTIKART_PODIUM_SOURCE}" PODIUM_ORIGINAL)
    string(REPLACE "    find_unused_obj_index(&_podium3Index);"
        "    find_unused_obj_index(&_podium3Index);\n    // The legacy ceremony shadow reads these exact object slots.\n    indexObjectList1[0] = _podium1Index;\n    indexObjectList1[1] = _podium2Index;\n    indexObjectList1[2] = _podium3Index;"
        PODIUM_FIXED "${PODIUM_ORIGINAL}")
    if(PODIUM_FIXED STREQUAL PODIUM_ORIGINAL)
        message(FATAL_ERROR "The upstream podium object allocation changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_PODIUM_SOURCE}" "${PODIUM_FIXED}")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_OBJECTS "${SPAGHETTIKART_PODIUM_SOURCE}")
    set(SPAGHETTIKART_GRAND_PRIX_BALLOONS_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/objects/GrandPrixBalloons.cpp")
    set(SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/GrandPrixBalloons_3ds.cpp")
    set(SPAGHETTIKART_MOLE_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/objects/Mole.cpp")
    set(SPAGHETTIKART_3DS_MOLE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Mole_3ds.cpp")
    set(SPAGHETTIKART_MOLE_GROUP_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/objects/MoleGroup.cpp")
    set(SPAGHETTIKART_3DS_MOLE_GROUP_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/MoleGroup_3ds.cpp")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_OBJECTS
        "${SPAGHETTIKART_GRAND_PRIX_BALLOONS_SOURCE}"
        "${SPAGHETTIKART_MOLE_SOURCE}"
        "${SPAGHETTIKART_MOLE_GROUP_SOURCE}"
    )
    file(GLOB SPAGHETTIKART_ENGINE_PARTICLES CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/particles/*.cpp"
    )
    file(GLOB SPAGHETTIKART_ENGINE_SKY CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/sky/*.cpp"
    )
    file(GLOB SPAGHETTIKART_ENGINE_TRACKS CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/tracks/*.cpp"
    )
    list(FILTER SPAGHETTIKART_ENGINE_TRACKS EXCLUDE REGEX ".*/(CustomTrack|Harbour|TestTrack)\\.cpp$")
    set(SPAGHETTIKART_TOADS_TURNPIKE_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/tracks/ToadsTurnpike.cpp")
    set(SPAGHETTIKART_3DS_TOADS_TURNPIKE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/ToadsTurnpike_3ds.cpp")
    set(SPAGHETTIKART_LUIGI_RACEWAY_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/tracks/LuigiRaceway.cpp")
    set(SPAGHETTIKART_3DS_LUIGI_RACEWAY_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/LuigiRaceway_3ds.cpp")
    set(SPAGHETTIKART_WARIO_STADIUM_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/tracks/WarioStadium.cpp")
    set(SPAGHETTIKART_3DS_WARIO_STADIUM_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/WarioStadium_3ds.cpp")
    set(SPAGHETTIKART_MOO_MOO_FARM_SOURCE
        "${SPAGHETTIKART_ROOT}/src/engine/tracks/MooMooFarm.cpp")
    set(SPAGHETTIKART_3DS_MOO_MOO_FARM_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/MooMooFarm_3ds.cpp")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_TRACKS
        "${SPAGHETTIKART_TOADS_TURNPIKE_SOURCE}"
        "${SPAGHETTIKART_LUIGI_RACEWAY_SOURCE}"
        "${SPAGHETTIKART_WARIO_STADIUM_SOURCE}"
        "${SPAGHETTIKART_MOO_MOO_FARM_SOURCE}"
    )

    include(platform/3ds/cmake/world_lifetime_3ds.cmake)

    include(platform/3ds/cmake/world_effects_3ds.cmake)

    include(platform/3ds/cmake/world_tracks_bridge_3ds.cmake)

    add_library(mk64-3ds-game-engine STATIC
        platform/3ds/source/audio_runtime_3ds.cpp
        platform/3ds/source/cvar_3ds.cpp
        platform/3ds/source/frame_interpolation_3ds.cpp
        platform/3ds/source/editor_stubs_3ds.cpp
        platform/3ds/source/game_data_3ds.cpp
        platform/3ds/source/game_state_3ds.cpp
        platform/3ds/source/lakitu_sprite_3ds.cpp
        platform/3ds/source/threat_warning_3ds.cpp
        platform/3ds/source/auto_mode_3ds.cpp
        platform/3ds/source/solo_modes_3ds.cpp
        platform/3ds/source/gbi_middleware_3ds.cpp
        platform/3ds/source/install_log_3ds.cpp
        platform/3ds/source/jumbotron_3ds.cpp
        platform/3ds/source/art/loading_image_3ds.cpp
        platform/3ds/source/settings_3ds.cpp
        platform/3ds/source/torch_o2r_3ds.cpp
        platform/3ds/source/vanilla_stubs_3ds.cpp
        "${SPAGHETTIKART_ROOT}/src/engine/editor/GameObject.cpp"
        ${SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE}
        ${SPAGHETTIKART_ENGINE_ROOT}
        ${SPAGHETTIKART_ENGINE_ACTORS}
        ${SPAGHETTIKART_ENGINE_CAMERAS}
        ${SPAGHETTIKART_ENGINE_OBJECTS}
        ${SPAGHETTIKART_ENGINE_PARTICLES}
        ${SPAGHETTIKART_ENGINE_SKY}
        ${SPAGHETTIKART_ENGINE_TRACKS}
        "${SPAGHETTIKART_3DS_WORLD_SOURCE}"
        "${SPAGHETTIKART_3DS_TRACK_SOURCE}"
        "${SPAGHETTIKART_3DS_GRAND_PRIX_BALLOONS_SOURCE}"
        "${SPAGHETTIKART_3DS_MOLE_SOURCE}"
        "${SPAGHETTIKART_3DS_MOLE_GROUP_SOURCE}"
        "${SPAGHETTIKART_3DS_MOO_MOO_FARM_SOURCE}"
        "${SPAGHETTIKART_3DS_PODIUM_SOURCE}"
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_SOURCE}"
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_SOURCE}"
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_SOURCE}"
        ${SPAGHETTIKART_ENGINE_VEHICLES}
        ${SPAGHETTIKART_ENGINE_REGISTRY}
    )
    target_include_directories(mk64-3ds-game-engine PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/compat"
        "${SPAGHETTIKART_ROOT}"
        "${SPAGHETTIKART_ROOT}/include"
        "${SPAGHETTIKART_ROOT}/include/assets"
        "${SPAGHETTIKART_ROOT}/src"
        "${SPAGHETTIKART_ROOT}/src/port"
        "${SPAGHETTIKART_ROOT}/src/engine"
        "${SPAGHETTIKART_ROOT}/src/engine/objects"
        "${SPAGHETTIKART_ROOT}/src/engine/tracks"
        "${SPAGHETTIKART_ROOT}/libultraship/include"
        "${SPAGHETTIKART_ROOT}/libultraship/include/libultraship"
    )
    target_compile_features(mk64-3ds-game-engine PRIVATE cxx_std_20)
    target_compile_definitions(mk64-3ds-game-engine PRIVATE
        NDEBUG
        VERSION_US=1
        F3DEX_GBI=1
        PLATFORM_3DS=1
        NON_MATCHING=1
        MK64_3DS_ON_DEVICE_EXTRACTOR=1
        MK64_3DS_ENABLE_NDSP_AUDIO=1
    )
    target_compile_options(mk64-3ds-game-engine PRIVATE
        -O3
        -ffast-math
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -fpermissive
        -include
        engine_3ds_prelude.h
        -Wno-unused-parameter
    )
    target_link_libraries(mk64-3ds-game-engine PRIVATE nlohmann_json::nlohmann_json)
    if(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR)
        target_link_libraries(mk64-3ds-game-engine PRIVATE torch)
    endif()
endif()
