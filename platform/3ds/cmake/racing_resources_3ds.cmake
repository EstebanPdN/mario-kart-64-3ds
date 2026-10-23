    # The desktop port reserves 256 MiB in racing/memory.c. Generate a 3DS-only
    # translation unit with a runtime-allocated 8 MiB arena while leaving
    # upstream clean. Keeping this arena out of .bss leaves enough heap for the
    # first-run on-device O2R installer on Old 3DS.
    set(SPAGHETTIKART_MEMORY_SOURCE "${SPAGHETTIKART_ROOT}/src/racing/memory.c")
    set(SPAGHETTIKART_3DS_MEMORY_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/memory_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_MEMORY_SOURCE}")
    set(SPAGHETTIKART_COLLISION_SOURCE "${SPAGHETTIKART_ROOT}/src/racing/collision.c")
    set(SPAGHETTIKART_3DS_COLLISION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/collision_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_COLLISION_SOURCE}")
    set(SPAGHETTIKART_SPAWN_PLAYERS_SOURCE "${SPAGHETTIKART_ROOT}/src/spawn_players.c")
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/spawn_players_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_SPAWN_PLAYERS_SOURCE}")
    set(SPAGHETTIKART_RENDER_PLAYER_SOURCE "${SPAGHETTIKART_ROOT}/src/render_player.c")
    set(SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/render_player_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_RENDER_PLAYER_SOURCE}")
    set(SPAGHETTIKART_AUDIO_PORT_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/port_eu.c")
    set(SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/port_eu_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_AUDIO_PORT_SOURCE}")
    set(SPAGHETTIKART_AUDIO_EXTERNAL_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/external.c")
    set(SPAGHETTIKART_3DS_AUDIO_EXTERNAL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/external_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_AUDIO_EXTERNAL_SOURCE}")
    set(SPAGHETTIKART_SEQPLAYER_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/seqplayer.c")
    set(SPAGHETTIKART_3DS_SEQPLAYER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/seqplayer_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_SEQPLAYER_SOURCE}")
    set(SPAGHETTIKART_MENU_ITEMS_SOURCE "${SPAGHETTIKART_ROOT}/src/menu_items.c")
    set(SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/menu_items_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_MENU_ITEMS_SOURCE}")
    set(SPAGHETTIKART_HUD_SOURCE "${SPAGHETTIKART_ROOT}/src/code_80057C60.c")
    set(SPAGHETTIKART_3DS_HUD_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/code_80057C60_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_HUD_SOURCE}")
    set(SPAGHETTIKART_MAIN_SOURCE "${SPAGHETTIKART_ROOT}/src/main.c")
    set(SPAGHETTIKART_3DS_MAIN_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/main_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_MAIN_SOURCE}")
    set(SPAGHETTIKART_UPDATE_OBJECTS_SOURCE "${SPAGHETTIKART_ROOT}/src/update_objects.c")
    set(SPAGHETTIKART_3DS_UPDATE_OBJECTS_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/update_objects_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_UPDATE_OBJECTS_SOURCE}")
    set(SPAGHETTIKART_ENDING_SOURCE "${SPAGHETTIKART_ROOT}/src/ending/code_80280000.c")
    set(SPAGHETTIKART_3DS_ENDING_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/code_80280000_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_ENDING_SOURCE}")
    set(SPAGHETTIKART_ACTORS_SOURCE "${SPAGHETTIKART_ROOT}/src/racing/actors.c")
    set(SPAGHETTIKART_3DS_ACTORS_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actors_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_ACTORS_SOURCE}")
    set(SPAGHETTIKART_RENDER_COURSES_SOURCE
        "${SPAGHETTIKART_ROOT}/src/racing/render_courses.c")
    set(SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/render_courses_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE
        "${SPAGHETTIKART_RENDER_COURSES_SOURCE}")
    set(SPAGHETTIKART_ACTORS_EXTENDED_SOURCE "${SPAGHETTIKART_ROOT}/src/racing/actors_extended.c")
    set(SPAGHETTIKART_RED_BLUE_SHELL_SOURCE
        "${SPAGHETTIKART_ROOT}/src/actors/blue_and_red_shells/update.inc.c")
    set(SPAGHETTIKART_3DS_RED_BLUE_SHELL_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/update_red_blue_shell_3ds.inc.c")
    set(SPAGHETTIKART_3DS_ACTORS_EXTENDED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actors_extended_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_ACTORS_EXTENDED_SOURCE}")
    file(READ "${SPAGHETTIKART_MEMORY_SOURCE}" SPAGHETTIKART_MEMORY_TEXT)
    string(REPLACE
        "static u8 sMemoryPool[0x10000000] ALIGNED4096;"
        "static u8* sMemoryPool = NULL;\nstatic size_t sMemoryPoolSize = 0;\nint Mk64MemoryArena3DSIsReady(void) { return sMemoryPool != NULL; }\nsize_t Mk64MemoryArena3DSCapacity(void) { return sMemoryPoolSize; }"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_MEMORY_TEXT}"
    )
    string(REPLACE
        "uintptr_t sPoolEnd = sMemoryPool + sizeof(sMemoryPool);"
        "uintptr_t sPoolEnd = 0;"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    string(REPLACE
        "#include <stdio.h>"
        "#include <stdio.h>\n#include <stdlib.h>\nextern void Mk64Diagnostics3DSSetGameArena(const void*, size_t) __attribute__((weak));\nextern void Mk64GameState3DSPrepareRaceResources(void);"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MEMORY_TRACK_RELEASE_ORIGINAL
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}")
    string(REPLACE
        "    CM_CleanWorld();\n    LoadTrack();"
        "    CM_CleanWorld();\n    Mk64GameState3DSPrepareRaceResources();\n    LoadTrack();"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MEMORY_TEXT STREQUAL
       SPAGHETTIKART_3DS_MEMORY_TRACK_RELEASE_ORIGINAL)
        message(FATAL_ERROR "The upstream track-load cleanup boundary changed")
    endif()
    string(REPLACE
        "    // Clear pool\n    memset(sMemoryPool, 0, sizeof(sMemoryPool));\n\n    // Force the pointer to be exactly at the start of the aligned array\n    uintptr_t poolStart = (uintptr_t)sMemoryPool;\n    \n    // Ensure sPoolEnd is exactly at the end of the 256MB block\n    sPoolEnd = poolStart + sizeof(sMemoryPool);"
        "    if (sMemoryPool == NULL) {\n        sMemoryPoolSize = 0x00800000;\n        sMemoryPool = (u8*)aligned_alloc(4096, sMemoryPoolSize);\n        if (sMemoryPool == NULL) {\n            sMemoryPoolSize = 0;\n            printf(\"[memory.c] initialize_memory_pool(): 3DS arena allocation failed!\\n\");\n            return;\n        }\n    }\n    memset(sMemoryPool, 0, sMemoryPoolSize);\n\n    uintptr_t poolStart = (uintptr_t)sMemoryPool;\n    sPoolEnd = poolStart + sMemoryPoolSize;\n    if (Mk64Diagnostics3DSSetGameArena != NULL) {\n        Mk64Diagnostics3DSSetGameArena(sMemoryPool, sMemoryPoolSize);\n    }"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    string(REPLACE
        "(((sMemoryPool + sizeof(sMemoryPool)) - sMemoryPool))"
        "((uintptr_t)sMemoryPoolSize)"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    string(REPLACE
        "(void*) (sMemoryPool + sizeof(sMemoryPool))"
        "(void*) (sMemoryPool + sMemoryPoolSize)"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    string(REPLACE
        "#include \"memory.h\""
        "#include \"racing/memory.h\""
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MEMORY_TEXT STREQUAL SPAGHETTIKART_MEMORY_TEXT)
        message(FATAL_ERROR "The upstream racing memory arena declaration changed")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    include(platform/3ds/cmake/cpu_items_3ds.cmake)
    include(platform/3ds/cmake/spatial_audio_3ds.cmake)
    include(platform/3ds/cmake/cpu_penalties_3ds.cmake)
    include(platform/3ds/cmake/item_hit_protection_3ds.cmake)
    if(EXISTS "${SPAGHETTIKART_3DS_MEMORY_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MEMORY_SOURCE}" SPAGHETTIKART_3DS_MEMORY_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MEMORY_EXISTING STREQUAL SPAGHETTIKART_3DS_MEMORY_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MEMORY_SOURCE}" "${SPAGHETTIKART_3DS_MEMORY_TEXT}")
    endif()

    # Reusing a Track address skipped all cleanup on restarts. A new scene
    # must reload even when the selected course is unchanged. Record the
    # reconstructed Track address only after load_track has returned.
    set(SPAGHETTIKART_RACE_SETUP_SOURCE "${SPAGHETTIKART_ROOT}/src/code_800029B0.c")
    set(SPAGHETTIKART_3DS_RACE_SETUP_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/race_setup_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_ROOT "${SPAGHETTIKART_RACE_SETUP_SOURCE}")
    file(READ "${SPAGHETTIKART_RACE_SETUP_SOURCE}" SPAGHETTIKART_RACE_SETUP_TEXT)
    string(REPLACE
        [=[    if (CM_GetTrack() != gCurrentlyLoadedTrackAddr) {
        D_80150120 = 0;
        gCurrentlyLoadedTrackAddr = CM_GetTrack();
        gNextFreeMemoryAddress = gFreeMemoryResetAnchor;
        load_track(gCurrentCourseId);
        gFreeMemoryCourseAnchor = gNextFreeMemoryAddress;
    } else {
        gNextFreeMemoryAddress = gFreeMemoryCourseAnchor;
    }]=]
        [=[    D_80150120 = 0;
    gNextFreeMemoryAddress = gFreeMemoryResetAnchor;
    load_track(gCurrentCourseId);
    gCurrentlyLoadedTrackAddr = CM_GetTrack();
    gFreeMemoryCourseAnchor = gNextFreeMemoryAddress;]=]
        SPAGHETTIKART_3DS_RACE_SETUP_TEXT "${SPAGHETTIKART_RACE_SETUP_TEXT}")
    if(SPAGHETTIKART_3DS_RACE_SETUP_TEXT STREQUAL SPAGHETTIKART_RACE_SETUP_TEXT)
        message(FATAL_ERROR "The upstream race restart resource boundary changed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SPAGHETTIKART_RACE_SETUP_SOURCE}")
    file(WRITE "${SPAGHETTIKART_3DS_RACE_SETUP_SOURCE}" "${SPAGHETTIKART_3DS_RACE_SETUP_TEXT}")

    # The desktop collision-mesh builder recursively walks course display lists
    # while loading O2R resources. On 3DS that path needs hard safety bounds:
    # repeated/corrupt display lists can exhaust heap or overrun the vanilla
    # collision arrays before a race is visible. Generate a 3DS copy with those
    # guards while keeping the public upstream submodule clean.
    file(READ "${SPAGHETTIKART_COLLISION_SOURCE}" SPAGHETTIKART_COLLISION_TEXT)
    string(REPLACE
        "#include <stdio.h>"
        [=[#include <stdio.h>
#include <stddef.h>
#include <string.h>
#if defined(PLATFORM_3DS)
#include "resource_prefetch_3ds.h"
#include "resource_runtime_3ds.h"
#define MK64_3DS_MAX_COLLISION_DLS 2048
#define MK64_3DS_COLLISION_VISIT_SLOTS 4096
#define MK64_3DS_MAX_COLLISION_COMMANDS 8192
#define MK64_3DS_MAX_COLLISION_TRIANGLES 8192
#define MK64_3DS_MAX_COLLISION_INDICES 32768
extern void Mk64Diagnostics3DSCoursePrefetch(size_t, size_t, size_t, size_t, size_t)
    __attribute__((weak));
#endif]=]
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_COLLISION_TEXT}"
    )
    string(REPLACE
        "u32 numTimes = 0;"
        [=[u32 numTimes = 0;

#if defined(PLATFORM_3DS)
typedef struct {
    uintptr_t addr;
    s8 surfaceType;
    u16 sectionId;
} Mk64CollisionVisit3DS;

static Mk64CollisionVisit3DS sCollisionVisits3DS[MK64_3DS_MAX_COLLISION_DLS];
static u16 sCollisionVisitSlots3DS[MK64_3DS_COLLISION_VISIT_SLOTS];
static u32 sCollisionVisitCount3DS = 0;
static Mk64PrefetchBudget3DS sCourseTexturePrefetch3DS;
static size_t sCourseTexturePrefetchLoaded3DS = 0;
static size_t sCourseTexturePrefetchLoadFailures3DS = 0;

static void mk64_3ds_report_course_prefetch(void) {
    if (Mk64Diagnostics3DSCoursePrefetch != NULL) {
        Mk64Diagnostics3DSCoursePrefetch(
            sCourseTexturePrefetch3DS.seenEntries,
            sCourseTexturePrefetchLoaded3DS,
            sCourseTexturePrefetch3DS.chargedBytes,
            sCourseTexturePrefetch3DS.byteLimitSkips +
                sCourseTexturePrefetch3DS.entryLimitSkips,
            sCourseTexturePrefetch3DS.unavailableEntries +
                sCourseTexturePrefetchLoadFailures3DS);
    }
}

void mk64_3ds_collision_reset_visits(void) {
    sCollisionVisitCount3DS = 0;
    memset(sCollisionVisitSlots3DS, 0, sizeof(sCollisionVisitSlots3DS));
    Mk64PrefetchBudget3DSReset(&sCourseTexturePrefetch3DS,
                               MK64_3DS_TRACK_PREFETCH_MAX_BYTES,
                               MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);
    sCourseTexturePrefetchLoaded3DS = 0;
    sCourseTexturePrefetchLoadFailures3DS = 0;
    mk64_3ds_report_course_prefetch();
}

static void mk64_3ds_prefetch_course_texture_crc(uint64_t crc) {
    (void)crc;
}

static void mk64_3ds_prefetch_course_texture_name(const char* name) {
    (void)name;
}

static bool mk64_3ds_collision_mark_visited(uintptr_t addr, s8 surfaceType, u16 sectionId) {
    u32 hash = (u32)addr;
    hash ^= (u32)(u8)surfaceType * 0x9E3779B9u;
    hash ^= (u32)sectionId * 0x85EBCA6Bu;
    hash ^= hash >> 16;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15;
    u32 slot = hash & (MK64_3DS_COLLISION_VISIT_SLOTS - 1u);
    for (u32 probe = 0; probe < MK64_3DS_COLLISION_VISIT_SLOTS; ++probe) {
        const u16 stored = sCollisionVisitSlots3DS[slot];
        if (stored == 0) {
            if (sCollisionVisitCount3DS >= MK64_3DS_MAX_COLLISION_DLS) {
                return false;
            }
            const u32 index = sCollisionVisitCount3DS++;
            sCollisionVisits3DS[index].addr = addr;
            sCollisionVisits3DS[index].surfaceType = surfaceType;
            sCollisionVisits3DS[index].sectionId = sectionId;
            sCollisionVisitSlots3DS[slot] = (u16)(index + 1u);
            return true;
        }
        const u32 index = (u32)stored - 1u;
        if ((index < sCollisionVisitCount3DS) &&
            (sCollisionVisits3DS[index].addr == addr) &&
            (sCollisionVisits3DS[index].surfaceType == surfaceType) &&
            (sCollisionVisits3DS[index].sectionId == sectionId)) {
            return false;
        }
        slot = (slot + 1u) & (MK64_3DS_COLLISION_VISIT_SLOTS - 1u);
    }
    return false;
}
#endif]=]
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "void add_collision_triangle(Vtx* vtx1, Vtx* vtx2, Vtx* vtx3, s8 surfaceType, u16 sectionId) {\n    CollisionTriangle* triangle = &gCollisionMesh[gCollisionMeshCount];"
        "void add_collision_triangle(Vtx* vtx1, Vtx* vtx2, Vtx* vtx3, s8 surfaceType, u16 sectionId) {\n#if defined(PLATFORM_3DS)\n    if ((vtx1 == NULL) || (vtx2 == NULL) || (vtx3 == NULL) ||\n        (gCollisionMeshCount >= MK64_3DS_MAX_COLLISION_TRIANGLES)) {\n        return;\n    }\n#endif\n    CollisionTriangle* triangle = &gCollisionMesh[gCollisionMeshCount];"
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "void set_vtx_buffer(uintptr_t addr, u32 numVertices, u32 bufferIndex) {\n    u32 i;\n    Vtx* vtx = (Vtx*) addr;"
        "void set_vtx_buffer(uintptr_t addr, u32 numVertices, u32 bufferIndex) {\n    u32 i;\n#if defined(PLATFORM_3DS)\n    if ((addr == 0) || (bufferIndex >= 32)) {\n        return;\n    }\n    if (numVertices > (32 - bufferIndex)) {\n        numVertices = 32 - bufferIndex;\n    }\n#endif\n    Vtx* vtx = (Vtx*) addr;"
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "                    if (gCollisionGrid[index].numTriangles == 0) {\n                        gCollisionGrid[index].triangle = gNumCollisionTriangles;\n                    }\n                    gCollisionGrid[index].numTriangles++;\n                    gCollisionIndices[gNumCollisionTriangles] = (u16) i;"
        "#if defined(PLATFORM_3DS)\n                    if (gNumCollisionTriangles >= MK64_3DS_MAX_COLLISION_INDICES) {\n                        continue;\n                    }\n#endif\n                    if (gCollisionGrid[index].numTriangles == 0) {\n                        gCollisionGrid[index].triangle = gNumCollisionTriangles;\n                    }\n                    gCollisionGrid[index].numTriangles++;\n                    gCollisionIndices[gNumCollisionTriangles] = (u16) i;"
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "void generate_collision_mesh(Gfx* addr, s8 surfaceType, u16 sectionId) {\n    if (GameEngine_OTRSigCheck((char*)addr)) {\n        addr = LOAD_ASSET(addr);\n    }"
        "void generate_collision_mesh(Gfx* addr, s8 surfaceType, u16 sectionId) {\n#if defined(PLATFORM_3DS)\n    if (addr == NULL) {\n        return;\n    }\n#endif\n    if (GameEngine_OTRSigCheck((char*)addr)) {\n        addr = LOAD_ASSET(addr);\n    }\n#if defined(PLATFORM_3DS)\n    if ((addr == NULL) || !mk64_3ds_collision_mark_visited((uintptr_t)addr, surfaceType, sectionId)) {\n        return;\n    }\n#endif"
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "    while (run) {\n        lo = gfx->words.w0;"
        "    size_t commandBudget = 0;\n    while (run) {\n#if defined(PLATFORM_3DS)\n        if (++commandBudget > MK64_3DS_MAX_COLLISION_COMMANDS) {\n            return;\n        }\n#endif\n        lo = gfx->words.w0;"
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    set(SPAGHETTIKART_3DS_COLLISION_PREFETCH_ORIGINAL
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
    string(REPLACE
        [=[            case G_MARKER:
            case G_MTX_OTR:
            case G_SETTIMG_OTR_HASH:
                gfx++;
                break;]=]
        [=[            case G_MARKER:
            case G_MTX_OTR:
                gfx++;
                break;
            case G_SETTIMG_OTR_HASH: {
                gfx++;
                const uint64_t textureHash = ((uint64_t)gfx->words.w0 << 32) | gfx->words.w1;
#if defined(PLATFORM_3DS)
                mk64_3ds_prefetch_course_texture_crc(textureHash);
#endif
                break;
            }
            case G_SETTIMG_OTR_FILEPATH:
#if defined(PLATFORM_3DS)
                mk64_3ds_prefetch_course_texture_name((const char*)hi);
#endif
                break;]=]
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    if(SPAGHETTIKART_3DS_COLLISION_TEXT STREQUAL
       SPAGHETTIKART_3DS_COLLISION_PREFETCH_ORIGINAL)
        message(FATAL_ERROR "The upstream course texture prefetch insertion point changed")
    endif()
    string(REPLACE
        "#include \"collision.h\""
        "#include \"racing/collision.h\""
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    string(REPLACE
        "#include \"math_util.h\""
        "#include \"racing/math_util.h\""
        SPAGHETTIKART_3DS_COLLISION_TEXT
        "${SPAGHETTIKART_3DS_COLLISION_TEXT}"
    )
    if(SPAGHETTIKART_3DS_COLLISION_TEXT STREQUAL SPAGHETTIKART_COLLISION_TEXT)
        message(FATAL_ERROR "The upstream racing collision mesh insertion point changed")
    endif()
    if(MK64_3DS_NATIVE_FULL_GAME)
        string(PREPEND SPAGHETTIKART_3DS_COLLISION_TEXT "#include \"native_program_3ds.h\"\n")
        string(REPLACE "void nullify_displaylist(uintptr_t addr) {"
            "void nullify_displaylist(uintptr_t addr) {\n    Mk64NativeProgramDisable(addr);"
            SPAGHETTIKART_3DS_COLLISION_TEXT "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
        string(REPLACE "void find_and_set_tile_size(uintptr_t addr, s32 uls, s32 ult) {"
            "void find_and_set_tile_size(uintptr_t addr, s32 uls, s32 ult) {\n    Mk64NativeProgramSetTileBounds(addr,uls,ult);"
            SPAGHETTIKART_3DS_COLLISION_TEXT "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
    endif()
    if(MK64_3DS_NATIVE_DYNAMIC)
        string(FIND "${SPAGHETTIKART_3DS_COLLISION_TEXT}" "void find_and_set_tile_size(" nativeScrollBegin)
        string(FIND "${SPAGHETTIKART_3DS_COLLISION_TEXT}" "void set_vertex_colours(" nativeScrollEnd)
        if(nativeScrollBegin LESS 0 OR nativeScrollEnd LESS nativeScrollBegin)
            message(FATAL_ERROR "Native texture scroll function bounds changed")
        endif()
        math(EXPR nativeScrollLength "${nativeScrollEnd}-${nativeScrollBegin}")
        string(SUBSTRING "${SPAGHETTIKART_3DS_COLLISION_TEXT}" ${nativeScrollBegin} ${nativeScrollLength} nativeScrollLegacy)
        string(REPLACE "${nativeScrollLegacy}"
            "void find_and_set_tile_size(uintptr_t addr, s32 uls, s32 ult) {\n    Mk64NativeProgramSetTileBounds(addr,uls,ult);\n}\n\n"
            SPAGHETTIKART_3DS_COLLISION_TEXT "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
    endif()
    set(SPAGHETTIKART_3DS_COLLISION_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_COLLISION_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_COLLISION_SOURCE}" SPAGHETTIKART_3DS_COLLISION_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_COLLISION_EXISTING STREQUAL SPAGHETTIKART_3DS_COLLISION_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_COLLISION_SOURCE}" "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
    endif()

    # Preload bounded compressed ZIP ranges for the initial kart angle group.
    # Decoded resources and GPU textures retain their existing bounded lifetime.
    file(READ "${SPAGHETTIKART_SPAWN_PLAYERS_SOURCE}" SPAGHETTIKART_SPAWN_PLAYERS_TEXT)
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_ORIGINAL
        "${SPAGHETTIKART_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "#include \"port/Game.h\""
        [=[#include "port/Game.h"
#if defined(PLATFORM_3DS)
#include "libultraship/bridge/resourcebridge.h"
#include "resource_runtime_3ds.h"
extern void Mk64BottomUI3DSShowLoadingProgress(const char*, const char*, unsigned) __attribute__((weak));
extern void Mk64Diagnostics3DSKartPrefetch(size_t, size_t, size_t, size_t, size_t)
    __attribute__((weak));
#endif]=]
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_SPAWN_PLAYERS_ORIGINAL)
        message(FATAL_ERROR "The upstream initial kart prefetch include insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_INCLUDE_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "Player* D_801653C0[8];"
        [=[Player* D_801653C0[8];

#if defined(PLATFORM_3DS)
#define MK64_3DS_INITIAL_KART_PREFETCH_LIMIT 192
#define MK64_3DS_INITIAL_KART_PREFETCH_FRAMES 35
#define MK64_3DS_INITIAL_KART_GROUP_COUNT 9
#define MK64_3DS_INITIAL_KART_WHEEL_COUNT 4
extern u8**** gKartTextureTable0[];
static uint64_t sInitialKartPrefetchKeys3DS[MK64_3DS_INITIAL_KART_PREFETCH_LIMIT];
static size_t sInitialKartPrefetchAttempted3DS = 0;
static size_t sInitialKartPrefetchLoaded3DS = 0;
static size_t sInitialKartPrefetchBytes3DS = 0;
static size_t sInitialKartPrefetchDuplicates3DS = 0;
static size_t sInitialKartPrefetchUnavailable3DS = 0;

static void mk64_3ds_report_initial_kart_prefetch(void) {
    if (Mk64Diagnostics3DSKartPrefetch != NULL) {
        Mk64Diagnostics3DSKartPrefetch(
            sInitialKartPrefetchAttempted3DS,
            sInitialKartPrefetchLoaded3DS,
            sInitialKartPrefetchBytes3DS,
            sInitialKartPrefetchDuplicates3DS,
            sInitialKartPrefetchUnavailable3DS);
    }
}

static void mk64_3ds_reset_initial_kart_prefetch(void) {
    sInitialKartPrefetchAttempted3DS = 0;
    sInitialKartPrefetchLoaded3DS = 0;
    sInitialKartPrefetchBytes3DS = 0;
    sInitialKartPrefetchDuplicates3DS = 0;
    sInitialKartPrefetchUnavailable3DS = 0;
    mk64_3ds_report_initial_kart_prefetch();
}

static void mk64_3ds_prefetch_initial_kart(const char* path) {
    ++sInitialKartPrefetchAttempted3DS;
    size_t bytes = sInitialKartPrefetchBytes3DS;
    if (Mk64Resource3DSPrefetchKart(path, &bytes)) {
        if (bytes == sInitialKartPrefetchBytes3DS) ++sInitialKartPrefetchDuplicates3DS;
        else ++sInitialKartPrefetchLoaded3DS;
    } else {
        ++sInitialKartPrefetchUnavailable3DS;
    }
    sInitialKartPrefetchBytes3DS = bytes;
    mk64_3ds_report_initial_kart_prefetch();
}
#endif]=]
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_SPAWN_PLAYERS_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream initial kart prefetch helper insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_HELPER_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        [=[        }

        gLastAnimFrameSelector[screenId][playerId] = player->animFrameSelector[screenId];]=]
        [=[        }

#if defined(PLATFORM_3DS) && !defined(MK64_3DS_NATIVE_FULL_GAME)
        if (!Mk64Resource3DSIsResident()) {
        const char* initialKartTexture = ((screenId == 0) || (screenId == 1))
            ? (const char*)gEncodedKartTexture[0][screenId][playerId].unk_00
            : (const char*)gEncodedKartTexture[0][screenId - 2][playerId + 4].unk_00;
        mk64_3ds_prefetch_initial_kart(initialKartTexture);
        const s32 initialKartGroup = player->animGroupSelector[screenId];
        const s32 initialKartFrame = player->animFrameSelector[screenId];
        if (player->characterId < NUM_PLAYERS && initialKartGroup >= 0 &&
            initialKartGroup < MK64_3DS_INITIAL_KART_GROUP_COUNT &&
            initialKartFrame >= 0 && initialKartFrame < 35) {
            for (s32 frameOffset = 0;
                 frameOffset < MK64_3DS_INITIAL_KART_PREFETCH_FRAMES; ++frameOffset) {
                const s32 frame = frameOffset;
                if (frame >= 35) break;
                for (s32 wheel = 0; wheel < MK64_3DS_INITIAL_KART_WHEEL_COUNT; ++wheel) {
                    mk64_3ds_prefetch_initial_kart((const char*)
                        gKartTextureTable0[player->characterId][initialKartGroup][frame][wheel]);
                }
            }
        }
        }
#endif
        gLastAnimFrameSelector[screenId][playerId] = player->animFrameSelector[screenId];]=]
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_SPAWN_PLAYERS_HELPER_TEXT)
        message(FATAL_ERROR "The upstream initial kart prefetch call insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_CALL_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "void load_kart_textures(void) {\n    size_t screens = 0;"
        "void load_kart_textures(void) {\n#if defined(PLATFORM_3DS)\n    mk64_3ds_reset_initial_kart_prefetch();\n#endif\n    size_t screens = 0;"
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_SPAWN_PLAYERS_CALL_TEXT)
        message(FATAL_ERROR "The upstream initial kart prefetch reset insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_RESULTS_CAMERAS_ORIGINAL
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        [=[    camera = CM_AddCamera(spawn, player->rotation[1], 3);
    if (!camera) {
        CM_ThrowRuntimeError("[spawn_players] [func_8003DB5C] NULL camera while attempting to create camera for player one");
    }
    CM_AttachCamera(camera, PLAYER_ONE);
    camera = CM_AddCamera(spawn, player->rotation[1], 3);
    if (!camera) {
        CM_ThrowRuntimeError("[spawn_players] [func_8003DB5C] NULL camera while attempting to create camera for player two");
    }
    CM_AttachCamera(camera, PLAYER_TWO);]=]
        [=[    Camera* cameraOne = gScreenContexts[PLAYER_ONE].raceCamera;
    Camera* cameraTwo = gScreenContexts[PLAYER_TWO].raceCamera;
    if (cameraOne == NULL || cameraTwo == NULL) {
        CM_ThrowRuntimeError("[spawn_players] [func_8003DB5C] missing GP results cameras");
    }
    camera_init(spawn, player->rotation[1], 3, cameraOne->cameraId);
    camera_init(spawn, player->rotation[1], 3, cameraTwo->cameraId);]=]
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_RESULTS_CAMERAS_ORIGINAL)
        message(FATAL_ERROR "The upstream GP results-camera allocation path changed")
    endif()
    set(SPAGHETTIKART_3DS_RESULTS_CAMERA_DECL_ORIGINAL
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "void func_8003DB5C(void) {\n    Player* player = gPlayerOne;\n    Camera* camera;\n    s32 playerId;"
        "void func_8003DB5C(void) {\n    Player* player = gPlayerOne;\n    s32 playerId;"
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT STREQUAL
       SPAGHETTIKART_3DS_RESULTS_CAMERA_DECL_ORIGINAL)
        message(FATAL_ERROR "The upstream GP results-camera local declaration changed")
    endif()
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
             SPAGHETTIKART_3DS_SPAWN_PLAYERS_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_SPAWN_PLAYERS_EXISTING STREQUAL
       SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT)
        string(REPLACE
        "    static const size_t playerCounts[4] = { 8, 8, 4, 4 };"
        "    if (Mk64BottomUI3DSShowLoadingProgress != NULL) Mk64BottomUI3DSShowLoadingProgress(\"LOADING RACE\", \"KART TEXTURES\", 0);\n    static const size_t playerCounts[4] = { 8, 8, 4, 4 };"
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "            func_8003CD98(&gPlayers[ply], gScreenContexts[i].camera, ply, i);"
        "            func_8003CD98(&gPlayers[ply], gScreenContexts[i].camera, ply, i);\n            if (Mk64BottomUI3DSShowLoadingProgress != NULL) Mk64BottomUI3DSShowLoadingProgress(\"LOADING RACE\", \"KART TEXTURES\", (unsigned)(100 * (i * playerCounts[gPlayerCountSelection1 - 1] + ply + 1) / (screens * playerCounts[gPlayerCountSelection1 - 1])));"
        SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    file(WRITE "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
             "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    endif()

    # The upstream desktop workaround reloads the current animated kart frame
    # from render_kart() on every draw. That bypasses init_render_player()'s
    # original near/medium/far update scheduler, so all eight racers continually
    # churn frame textures even when a distant racer should retain one of its
    # coarse direction frames. Generate a 3DS-only copy which lets the scheduler
    # own frame changes and only performs a synchronous load for a genuinely
    # uninitialised double-buffer slot. A missing slot is repaired, never hidden:
    # resource pressure must not make an on-camera racer disappear.
    file(READ "${SPAGHETTIKART_RENDER_PLAYER_SOURCE}"
         SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT)
    set(SPAGHETTIKART_3DS_RENDER_PLAYER_LOD_ORIGINAL
        "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    string(REPLACE
        [=[    // When screenId >= 2 (in 3/4 player mode), adjust indices to fit gEncodedKartTexture[2][2][8] dimensions
    if ((screenId == 0) || (screenId == 1)) {
        load_kart_texture(player, playerId, screenId, screenId, D_801651D0[screenId][playerId]);
    } else {
        load_kart_texture(player, playerId - 4, screenId, screenId - 1, D_801651D0[screenId][playerId]);
    }
    if ((screenId == 0) || (screenId == 1)) {
        sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId][playerId].unk_00;
    } else {
        sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId - 1][playerId - 4].unk_00;
    }]=]
        [=[    // init_render_player() schedules near racers at full animation rate and
    // progressively coarser updates for distant racers. Preserve the selected
    // double-buffered frame; only repair an empty slot synchronously.
    if ((screenId == 0) || (screenId == 1)) {
        sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId][playerId].unk_00;
        if (sKartTexture == NULL) {
            load_kart_texture(player, playerId, screenId, screenId,
                              D_801651D0[screenId][playerId]);
            sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId][playerId].unk_00;
        }
    } else {
        sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId - 1][playerId - 4].unk_00;
        if (sKartTexture == NULL) {
            load_kart_texture(player, playerId - 4, screenId, screenId - 1,
                              D_801651D0[screenId][playerId]);
            sKartTexture = gEncodedKartTexture[D_801651D0[screenId][playerId]][screenId - 1][playerId - 4].unk_00;
        }
    }]=]
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT
        "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}"
    )
    if(SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT STREQUAL
       SPAGHETTIKART_3DS_RENDER_PLAYER_LOD_ORIGINAL)
        message(FATAL_ERROR "The upstream kart LOD restoration point changed")
    endif()
    # Podium karts have a separate, elevated ceremony shadow. Do not also
    # draw the ordinary track-ground shadow after the podium jump begins.
    set(RENDER_PLAYER_SHADOW_ORIGINAL "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    string(REPLACE
        "        (player->surfaceType != ICE)) {"
        "        (player->surfaceType != ICE) &&\n        !(gGamestate == ENDING && (player->kartProps & UNUSED_0x2000))) {"
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    if(SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT STREQUAL RENDER_PLAYER_SHADOW_ORIGINAL)
        message(FATAL_ERROR "The upstream ordinary kart shadow condition changed")
    endif()
    string(REPLACE "    Vec3f sp94 = { 9.0f, 7.0f, 5.0f };"
        "    Vec3f sp94 = { 9.0f, 7.0f, 5.0f };\n    if (playerId < 0 || playerId >= 3) return;"
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    include(platform/3ds/cmake/native_kart_game_3ds.cmake)
    include(platform/3ds/cmake/kart_visual_preparation_3ds.cmake)
    set(SPAGHETTIKART_3DS_RENDER_PLAYER_EXISTING "")
    set(SMOKE_VISIBILITY_ANCHOR "check_player_camera_collision(player, camera, (f32) D_80165580, (f32) D_80165582) == 1")
    string(FIND "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}" "${SMOKE_VISIBILITY_ANCHOR}" SMOKE_VISIBILITY_FOUND)
    if(SMOKE_VISIBILITY_FOUND LESS 0)
        message(FATAL_ERROR "Upstream exhaust particle visibility condition changed")
    endif()
    string(REPLACE
        "check_player_camera_collision(player, camera, (f32) D_80165580, (f32) D_80165582) == 1"
        "(player->unk_002 & (UNK_002_UNKNOWN_0x2 << (screenId * 4))) != 0"
        SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    if(EXISTS "${SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE}"
             SPAGHETTIKART_3DS_RENDER_PLAYER_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_RENDER_PLAYER_EXISTING STREQUAL
       SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_RENDER_PLAYER_SOURCE}"
             "${SPAGHETTIKART_3DS_RENDER_PLAYER_TEXT}")
    endif()
