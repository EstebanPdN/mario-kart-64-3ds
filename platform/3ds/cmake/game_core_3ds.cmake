# Pinned game/core target inputs. Keep platform adapters in the platform target;
# responsibility-specific generated edits are assembled by game_engine_3ds.cmake.
if(MK64_3DS_BUILD_GAME_CORE)
    set(SPAGHETTIKART_ROOT "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart")

    file(GLOB SPAGHETTIKART_GAME_ROOT CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/*.c"
    )
    list(FILTER SPAGHETTIKART_GAME_ROOT EXCLUDE REGEX ".*/stubs\\.c$")
    list(FILTER SPAGHETTIKART_GAME_ROOT EXCLUDE REGEX ".*/crash_screen\\.c$")
    file(GLOB_RECURSE SPAGHETTIKART_GAME_RECURSIVE CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/audio/*.c"
        "${SPAGHETTIKART_ROOT}/src/buffers/*.c"
        "${SPAGHETTIKART_ROOT}/src/data/*.c"
        "${SPAGHETTIKART_ROOT}/src/debug/*.c"
        "${SPAGHETTIKART_ROOT}/src/ending/*.c"
        "${SPAGHETTIKART_ROOT}/src/engine/*.c"
        "${SPAGHETTIKART_ROOT}/src/racing/*.c"
        "${SPAGHETTIKART_ROOT}/courses/*.c"
    )
    list(FILTER SPAGHETTIKART_GAME_RECURSIVE EXCLUDE REGEX ".*\\.inc\\.c$")
    list(FILTER SPAGHETTIKART_GAME_RECURSIVE EXCLUDE REGEX ".*/crash_screen_enhancement\\.c$")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_ROOT}/src/audio/effects.c")

    include(platform/3ds/cmake/racing_resources_3ds.cmake)

    include(platform/3ds/cmake/racing_objects_items_3ds.cmake)

    include(platform/3ds/cmake/racing_audio_menu_3ds.cmake)

    include(platform/3ds/cmake/racing_ending_3ds.cmake)

    add_library(mk64-3ds-game-core STATIC
        ${SPAGHETTIKART_GAME_ROOT}
        ${SPAGHETTIKART_GAME_RECURSIVE}
        ${SPAGHETTIKART_GAME_OS_MATH}
        platform/3ds/source/resource_prefetch_3ds.c
        platform/3ds/source/audio_effects_3ds.c
        "${SPAGHETTIKART_3DS_MEMORY_SOURCE}"
        "${SPAGHETTIKART_3DS_RACE_SETUP_SOURCE}"
        "${SPAGHETTIKART_3DS_MENUS_SOURCE}"
        "${SPAGHETTIKART_3DS_SAVE_SOURCE}"
        "${SPAGHETTIKART_3DS_COLLISION_SOURCE}"
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
        "${SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE}"
        "${SPAGHETTIKART_3DS_ACTORS_SOURCE}"
        "${SPAGHETTIKART_3DS_AUDIO_EXTERNAL_SOURCE}"
        "${SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE}"
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_SOURCE}"
        "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}"
        "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}"
        "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}"
        "${SPAGHETTIKART_3DS_HUD_SOURCE}"
        "${SPAGHETTIKART_3DS_MAIN_SOURCE}"
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_SOURCE}"
        "${SPAGHETTIKART_3DS_CPU_ITEMS_SOURCE}"
        "${SPAGHETTIKART_3DS_PLAYER_CONTROLLER_SOURCE}"
        "${SPAGHETTIKART_3DS_ENDING_SOURCE}"
    )
    target_include_directories(mk64-3ds-game-core PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/compat"
        "${SPAGHETTIKART_ROOT}"
        "${SPAGHETTIKART_ROOT}/include"
        "${SPAGHETTIKART_ROOT}/include/assets"
        "${SPAGHETTIKART_ROOT}/src"
        "${SPAGHETTIKART_ROOT}/src/racing"
        "${SPAGHETTIKART_ROOT}/src/ending"
        "${SPAGHETTIKART_ROOT}/libultraship/include"
        "${SPAGHETTIKART_ROOT}/libultraship/include/libultraship"
        "${SPAGHETTIKART_ROOT}/libultraship/include/fast"
    )
    target_compile_features(mk64-3ds-game-core PRIVATE c_std_11)
    target_compile_definitions(mk64-3ds-game-core PRIVATE
        NDEBUG
        VERSION_US=1
        ENABLE_RUMBLE=1
        F3DEX_GBI=1
        _LANGUAGE_C
        _USE_MATH_DEFINES
        NON_MATCHING=1
        NON_EQUIVALENT=1
        AVOID_UB=1
        PLATFORM_3DS=1
        SPAGHETTI_VERSION="3DS-v${MK64_3DS_VERSION}"
    )
    target_compile_options(mk64-3ds-game-core PRIVATE
        -O3
        -ffast-math
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -Wno-incompatible-pointer-types
        -Wno-error=int-conversion
        -Wno-unused-parameter
    )
endif()
