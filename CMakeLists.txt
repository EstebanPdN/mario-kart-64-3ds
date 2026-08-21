cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/version.txt" MK64_3DS_VERSION LIMIT_COUNT 1)
project(mk64_3ds LANGUAGES C CXX)

# Keep local checkout paths out of diagnostics embedded in distributable 3DSX
# files and make builds reproducible across maintainer machines.
add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}/=")

option(MK64_3DS_ENABLE_O2R_READER "Build the owner-supplied O2R archive reader" OFF)
option(MK64_3DS_ENABLE_FAST3D_RENDERER "Build the native Citro3D Fast3D renderer" ON)
option(MK64_3DS_BUILD_RENDERER_PROBE "Build a standalone renderer validation program" ON)
option(MK64_3DS_BUILD_INTERPRETER_PROBE "Build a Fast3D display-list validation program" ON)
option(MK64_3DS_BUILD_GAME_CORE "Compile the portable Mario Kart 64 C game core" OFF)
option(MK64_3DS_BUILD_GAME_ENGINE "Compile the portable SpaghettiKart C++ engine layer" OFF)
option(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR "Build Torch so the 3DS can generate mk64.o2r from a legal ROM" ON)

set(MK64_3DS_ROMFS_DIR "${CMAKE_CURRENT_BINARY_DIR}/romfs")
set(MK64_3DS_TORCH_ROMFS_DIR "${MK64_3DS_ROMFS_DIR}/torch")
file(MAKE_DIRECTORY "${MK64_3DS_TORCH_ROMFS_DIR}")
file(COPY
    "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/config.yml"
    DESTINATION "${MK64_3DS_TORCH_ROMFS_DIR}"
)
file(COPY
    "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/yamls"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/meta"
    DESTINATION "${MK64_3DS_TORCH_ROMFS_DIR}"
)

add_library(mk64-3ds-platform STATIC
    platform/3ds/source/audio_dma_registry_3ds.cpp
    platform/3ds/source/audio_ndsp_3ds.cpp
    platform/3ds/source/input_3ds.cpp
    platform/3ds/source/libultra_3ds.cpp
    platform/3ds/source/system_3ds.cpp
)
target_include_directories(mk64-3ds-platform PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
    "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include/libultraship"
)
target_compile_features(mk64-3ds-platform PUBLIC cxx_std_17)
target_compile_definitions(mk64-3ds-platform PRIVATE MK64_3DS_ENABLE_NDSP_AUDIO=1)
target_compile_options(mk64-3ds-platform PRIVATE
    -O3
    -fomit-frame-pointer
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wpedantic
)
target_link_libraries(mk64-3ds-platform PUBLIC ctru m)

if(MK64_3DS_BUILD_GAME_CORE)
    set(SPAGHETTIKART_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart")

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
    set(SPAGHETTIKART_AUDIO_PORT_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/port_eu.c")
    set(SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/port_eu_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_AUDIO_PORT_SOURCE}")
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
        "#include <stdio.h>\n#include <stdlib.h>\nextern void Mk64Diagnostics3DSSetGameArena(const void*, size_t) __attribute__((weak));\nextern void Mk64ReleaseLoadedTrackResources3DS(void) __attribute__((weak));"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
    set(SPAGHETTIKART_3DS_MEMORY_TRACK_RELEASE_ORIGINAL
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}")
    string(REPLACE
        "    CM_CleanWorld();\n    LoadTrack();"
        "    CM_CleanWorld();\n    if (Mk64ReleaseLoadedTrackResources3DS != NULL) {\n        Mk64ReleaseLoadedTrackResources3DS();\n    }\n    LoadTrack();"
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
    if(EXISTS "${SPAGHETTIKART_3DS_MEMORY_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MEMORY_SOURCE}" SPAGHETTIKART_3DS_MEMORY_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MEMORY_EXISTING STREQUAL SPAGHETTIKART_3DS_MEMORY_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MEMORY_SOURCE}" "${SPAGHETTIKART_3DS_MEMORY_TEXT}")
    endif()

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
    set(SPAGHETTIKART_3DS_COLLISION_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_COLLISION_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_COLLISION_SOURCE}" SPAGHETTIKART_3DS_COLLISION_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_COLLISION_EXISTING STREQUAL SPAGHETTIKART_3DS_COLLISION_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_COLLISION_SOURCE}" "${SPAGHETTIKART_3DS_COLLISION_TEXT}")
    endif()

    # Keep the generated seam used by E1 diagnostics, but do not preload kart
    # animation frames. Fast3D's bounded GPU cache now owns their exact lifetime;
    # eager raw-resource loads bypass that ownership and create the start hitch.
    file(READ "${SPAGHETTIKART_SPAWN_PLAYERS_SOURCE}" SPAGHETTIKART_SPAWN_PLAYERS_TEXT)
    set(SPAGHETTIKART_3DS_SPAWN_PLAYERS_ORIGINAL
        "${SPAGHETTIKART_SPAWN_PLAYERS_TEXT}")
    string(REPLACE
        "#include \"port/Game.h\""
        [=[#include "port/Game.h"
#if defined(PLATFORM_3DS)
#include "libultraship/bridge/resourcebridge.h"
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
#define MK64_3DS_INITIAL_KART_PREFETCH_FRAMES 0
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
    (void)path;
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

#if defined(PLATFORM_3DS)
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
                const s32 frame = initialKartFrame + frameOffset;
                if (frame >= 35) break;
                for (s32 wheel = 0; wheel < MK64_3DS_INITIAL_KART_WHEEL_COUNT; ++wheel) {
                    mk64_3ds_prefetch_initial_kart((const char*)
                        gKartTextureTable0[player->characterId][initialKartGroup][frame][wheel]);
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
        file(WRITE "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
             "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_TEXT}")
    endif()

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
                    if (pad13 == ACTOR_RED_SHELL &&
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
    file(WRITE "${SPAGHETTIKART_3DS_RED_BLUE_SHELL_SOURCE}"
         "${SPAGHETTIKART_RED_BLUE_SHELL_TEXT}")
    file(READ "${SPAGHETTIKART_ACTORS_EXTENDED_SOURCE}" SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT)
    string(REPLACE
        "#include \"actors/blue_and_red_shells/update.inc.c\""
        "#include \"${SPAGHETTIKART_3DS_RED_BLUE_SHELL_SOURCE}\""
        SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_TEXT}"
    )
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

    # The desktop direct-audio path normally advances sample-DMA state through
    # an N64 audio task. The 3DS runtime synthesizes directly into NDSP buffers,
    # so generate a narrow 3DS copy that keeps those per-frame queues moving
    # without modifying the upstream SpaghettiKart submodule.
    file(READ "${SPAGHETTIKART_AUDIO_PORT_SOURCE}" SPAGHETTIKART_AUDIO_PORT_TEXT)
    string(REPLACE
        "    gAudioFrameCount++;\n    gCurrAiBufferIndex %= 3;\n\n    gCurrAudioFrameDmaCount = 0;"
        "    gAudioFrameCount++;\n    gCurrAiBufferIndex %= 3;\n\n#if defined(PLATFORM_3DS)\n    decrease_sample_dma_ttls();\n#endif\n\n    gCurrAudioFrameDmaCount = 0;"
        SPAGHETTIKART_3DS_AUDIO_PORT_TEXT
        "${SPAGHETTIKART_AUDIO_PORT_TEXT}"
    )
    if(SPAGHETTIKART_3DS_AUDIO_PORT_TEXT STREQUAL SPAGHETTIKART_AUDIO_PORT_TEXT)
        message(FATAL_ERROR "The upstream audio port insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}" SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_AUDIO_PORT_EXISTING STREQUAL SPAGHETTIKART_3DS_AUDIO_PORT_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}" "${SPAGHETTIKART_3DS_AUDIO_PORT_TEXT}")
    endif()

    # Reject malformed layer opcodes before they can index beyond the four
    # SequenceChannel layer slots. Valid MK64 sequences use only layers 0..3;
    # this guard turns corrupt archive/script data into a skipped command instead
    # of aliasing soundScriptIO and dereferencing values such as 0xffffffff.
    file(READ "${SPAGHETTIKART_SEQPLAYER_SOURCE}" SPAGHETTIKART_SEQPLAYER_TEXT)
    string(REPLACE
        "s32 seq_channel_set_layer(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer;"
        "s32 seq_channel_set_layer(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer;\n\n#if defined(PLATFORM_3DS)\n    if (layerIndex < 0 || layerIndex >= LAYERS_MAX) {\n        return -1;\n    }\n#endif"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_SEQPLAYER_TEXT}"
    )
    set(SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    if(SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT STREQUAL SPAGHETTIKART_SEQPLAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer allocation guard insertion point changed")
    endif()
    string(REPLACE
        "void seq_channel_layer_free(struct SequenceChannel* seqChannel, s32 layerIndex) {\n    struct SequenceChannelLayer* layer = seqChannel->layers[layerIndex];"
        "void seq_channel_layer_free(struct SequenceChannel* seqChannel, s32 layerIndex) {\n#if defined(PLATFORM_3DS)\n    if (layerIndex < 0 || layerIndex >= LAYERS_MAX) {\n        return;\n    }\n#endif\n    struct SequenceChannelLayer* layer = seqChannel->layers[layerIndex];"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}"
    )
    set(SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    if(SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_SET_LAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer free guard insertion point changed")
    endif()
    string(REPLACE
        "                        if (seqChannel->layers[loBits] != NULL) {"
        "                        if (loBits < LAYERS_MAX && seqChannel->layers[loBits] != NULL) {"
        SPAGHETTIKART_3DS_SEQPLAYER_TEXT
        "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}"
    )
    if(SPAGHETTIKART_3DS_SEQPLAYER_TEXT STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_FREE_LAYER_TEXT)
        message(FATAL_ERROR "The upstream sequence-layer read guard insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_SEQPLAYER_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}" SPAGHETTIKART_3DS_SEQPLAYER_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_SEQPLAYER_EXISTING STREQUAL SPAGHETTIKART_3DS_SEQPLAYER_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}" "${SPAGHETTIKART_3DS_SEQPLAYER_TEXT}")
    endif()

    # Generate a 3DS-only menu copy without modifying the pinned upstream
    # gitlink. The stock unselected-portrait shade belongs here, but its wide
    # desktop helper expands a 64 px portrait across neighboring cells at 5:3.
    # Keep the shade and use the original fixed-width box on 3DS.
    file(READ "${SPAGHETTIKART_MENU_ITEMS_SOURCE}" SPAGHETTIKART_MENU_ITEMS_TEXT)
    string(REPLACE
        "                 func_800A12BC(arg0, D_800E7D54[arg0->type - CHARACTER_SELECT_MENU_MARIO]);\n                /* fallthrough */"
        "                func_800A12BC(arg0, D_800E7D54[arg0->type - CHARACTER_SELECT_MENU_MARIO]);\n                func_8009A76C(arg0->D_8018DEE0_index, arg0->column, arg0->row, arg0->param1);\n                func_800A1350(arg0);\n                break;"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL SPAGHETTIKART_MENU_ITEMS_TEXT)
        message(FATAL_ERROR "The upstream character-portrait shade insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_CALL_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "                gDisplayListHead = draw_box_wide(gDisplayListHead, arg0->column, arg0->row, arg0->column + 0x40,\n                                                 arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        "                gDisplayListHead = draw_box(gDisplayListHead, arg0->column, arg0->row, arg0->column + 0x40,\n                                            arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_CALL_TEXT)
        message(FATAL_ERROR "The upstream static portrait shade box insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_STATIC_BOX_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "                gDisplayListHead = draw_box_wide(gDisplayListHead, arg0->column + thing, arg0->row,\n                                                 (arg0->column - thing) + 0x40, arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        "                gDisplayListHead = draw_box(gDisplayListHead, arg0->column + thing, arg0->row,\n                                            (arg0->column - thing) + 0x40, arg0->row + 0x4C, 0, 0, 0, 0x00000064);"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_STATIC_BOX_TEXT)
        message(FATAL_ERROR "The upstream animated portrait shade box insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_BOX_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        [=[                if (CVarGetInteger("gShowSpaghettiVersion", true)) {
                    draw_version();
                    draw_debug();
                }]=]
        [=[#if !defined(PLATFORM_3DS)
                if (CVarGetInteger("gShowSpaghettiVersion", true)) {
                    draw_version();
                    draw_debug();
                }
#endif]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PORTRAIT_BOX_TEXT)
        message(FATAL_ERROR "The upstream Game Select version render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_VERSION_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        [=[            case MAIN_MENU_OPTION_GFX:
            case MAIN_MENU_DATA_GFX:
                var_a1 = arg0->type - MENU_ITEM_UI_OK;]=]
        [=[            case MAIN_MENU_OPTION_GFX:
            case MAIN_MENU_DATA_GFX:
#if defined(PLATFORM_3DS)
                break;
#endif
                var_a1 = arg0->type - MENU_ITEM_UI_OK;]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_VERSION_TEXT)
        message(FATAL_ERROR "The upstream Game Select Option/Data render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_GAME_SELECT_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "void render_pause_menu(MenuItem* arg0) {\n    if (gIsGamePaused != 0) {"
        [=[#if defined(PLATFORM_3DS)
static void mk64_3ds_render_pause_title(void) {
    const char* courseName = CM_GetProps() != NULL ? CM_GetProps()->Name : "";
    gDisplayListHead = draw_box_wide(gDisplayListHead, 0, 0, 0x13F, 0xEF,
                                     0, 0, 0, 0x8C);
    set_text_color(TEXT_YELLOW);
    if (gModeSelection == GRAND_PRIX) {
        const f32 cupHalf = (get_string_width(gCupNames[GetCupIndex()]) + 10.0f) * 0.5f;
        const f32 ccHalf = (get_string_width(D_800E76CC[gCCSelection]) + 10.0f) * 0.5f;
        print_text1_center_mode_1((s32)(160.0f - ccHalf), 90,
                                  gCupNames[GetCupIndex()], 0, 1.0f, 1.0f);
        print_text1_center_mode_1((s32)(160.0f + cupHalf), 90,
                                  D_800E76CC[gCCSelection], 0, 1.0f, 1.0f);
    }
    print_text1_center_mode_1(160, 112, courseName, 0, 1.0f, 1.0f);
}
#endif

void render_pause_menu(MenuItem* arg0) {
    if (gIsGamePaused != 0) {
#if defined(PLATFORM_3DS)
        mk64_3ds_render_pause_title();
        return;
#endif]=]
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_GAME_SELECT_TEXT)
        message(FATAL_ERROR "The upstream pause-menu dispatch point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_PAUSE_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    string(REPLACE
        "void func_800A54EC(void) {\n    Unk_D_800E70A0 sp50;"
        "void func_800A54EC(void) {\n#if defined(PLATFORM_3DS)\n    return;\n#endif\n    Unk_D_800E70A0 sp50;"
        SPAGHETTIKART_3DS_MENU_ITEMS_TEXT
        "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MENU_ITEMS_TEXT STREQUAL
       SPAGHETTIKART_3DS_MENU_ITEMS_PAUSE_TEXT)
        message(FATAL_ERROR "The upstream pause cursor render point changed")
    endif()
    set(SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MENU_ITEMS_EXISTING STREQUAL SPAGHETTIKART_3DS_MENU_ITEMS_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}" "${SPAGHETTIKART_3DS_MENU_ITEMS_TEXT}")
    endif()

    # The stock global HUD pass is outside render_hud(), so gDrawHUD alone
    # cannot hide its ranking, place, minimap or lap-progress elements. Build
    # a 3DS-only copy that keeps viewport restoration/debug work while routing
    # visible top HUD elements through the explicit FULL/NONE/LAP/SPEED mode.
    file(READ "${SPAGHETTIKART_HUD_SOURCE}" SPAGHETTIKART_HUD_TEXT)
    set(SPAGHETTIKART_3DS_HUD_ORIGINAL "${SPAGHETTIKART_HUD_TEXT}")
    string(REPLACE
        "#include \"code_80057C60.h\""
        [=[#include "code_80057C60.h"
#if defined(PLATFORM_3DS)
extern int Mk64GameState3DSGetTopHudRenderMode(void);
#define MK64_TOP_HUD_RENDER_FULL (-1)
#define MK64_TOP_HUD_RENDER_NONE 0
#define MK64_TOP_HUD_RENDER_LAP_PROGRESS 1
#define MK64_TOP_HUD_RENDER_SPEEDOMETER 2
#define MK64_TOP_HUD_RENDER_POSITION_LAP 3
#endif]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_ORIGINAL)
        message(FATAL_ERROR "The upstream global HUD include point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_INCLUDE_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "void func_800591B4(void) {\n\n    if ((gHUDDisable == 0) && (D_800DC5B8 != 0)) {"
        "void func_800591B4(void) {\n#if defined(PLATFORM_3DS)\n    const int mk64TopHudMode = Mk64GameState3DSGetTopHudRenderMode();\n#endif\n\n    if ((gHUDDisable == 0) && (D_800DC5B8 != 0)) {"
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream global HUD entry point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_ENTRY_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "        if (gIsHUDVisible != 0) {\n            if (D_801657D8 == 0) {"
        [=[        if ((gIsHUDVisible != 0)
#if defined(PLATFORM_3DS)
            && (mk64TopHudMode == MK64_TOP_HUD_RENDER_FULL)
#endif
        ) {
            if (D_801657D8 == 0) {]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_ENTRY_TEXT)
        message(FATAL_ERROR "The upstream global HUD visibility point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_FULL_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
    string(REPLACE
        "            func_800590D4();\n        }\n        func_8005902C();"
        [=[            func_800590D4();
        }
#if defined(PLATFORM_3DS)
        if ((gIsHUDVisible != 0) && (D_801657D8 == 0)) {
            if (mk64TopHudMode == MK64_TOP_HUD_RENDER_LAP_PROGRESS) {
                func_800514BC();
            } else if (mk64TopHudMode == MK64_TOP_HUD_RENDER_SPEEDOMETER) {
                set_matrix_hud_screen();
                render_speedometer(PLAYER_ONE);
            } else if (mk64TopHudMode == MK64_TOP_HUD_RENDER_POSITION_LAP) {
                set_matrix_hud_screen();
                draw_simplified_lap_count(PLAYER_ONE);
                func_8004E800(PLAYER_ONE);
            }
        }
#endif
        func_8005902C();]=]
        SPAGHETTIKART_3DS_HUD_TEXT
        "${SPAGHETTIKART_3DS_HUD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_HUD_TEXT STREQUAL SPAGHETTIKART_3DS_HUD_FULL_TEXT)
        message(FATAL_ERROR "The upstream selective HUD insertion point changed")
    endif()
    set(SPAGHETTIKART_3DS_HUD_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_HUD_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_HUD_SOURCE}" SPAGHETTIKART_3DS_HUD_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_HUD_EXISTING STREQUAL SPAGHETTIKART_3DS_HUD_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_HUD_SOURCE}" "${SPAGHETTIKART_3DS_HUD_TEXT}")
    endif()

    # The Citro3D backend cannot read a rendered framebuffer back into the
    # N64 CPU buffer: its current implementation returns zero-filled pixels.
    # Avoid building two off-screen targets and a 320x240 readback command
    # stream every race frame when the only consumers would receive black.
    # Luigi Raceway and Wario Stadium keep their static billboard textures in
    # the 3DS engine copies below.
    file(READ "${SPAGHETTIKART_MAIN_SOURCE}" SPAGHETTIKART_3DS_MAIN_TEXT)
    set(SPAGHETTIKART_3DS_MAIN_ORIGINAL "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    FB_WriteFramebufferSliceToCPU(&gDisplayListHead, gPortFramebuffers[sRenderingFramebuffer], true);"
        "    /* PLATFORM_3DS: dynamic jumbotron readback is unsupported and intentionally omitted. */"
        SPAGHETTIKART_3DS_MAIN_TEXT
        "${SPAGHETTIKART_3DS_MAIN_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MAIN_TEXT STREQUAL SPAGHETTIKART_3DS_MAIN_ORIGINAL)
        message(FATAL_ERROR "The upstream race framebuffer-readback call changed")
    endif()
    set(SPAGHETTIKART_3DS_MAIN_READBACK_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    string(REPLACE
        "    FB_CreateFramebuffers();"
        "    /* PLATFORM_3DS: no CPU framebuffer consumer requires off-screen targets. */"
        SPAGHETTIKART_3DS_MAIN_TEXT
        "${SPAGHETTIKART_3DS_MAIN_TEXT}"
    )
    if(SPAGHETTIKART_3DS_MAIN_TEXT STREQUAL SPAGHETTIKART_3DS_MAIN_READBACK_TEXT)
        message(FATAL_ERROR "The upstream per-frame framebuffer creation call changed")
    endif()
    set(SPAGHETTIKART_3DS_MAIN_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_MAIN_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_MAIN_SOURCE}" SPAGHETTIKART_3DS_MAIN_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_MAIN_EXISTING STREQUAL SPAGHETTIKART_3DS_MAIN_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_MAIN_SOURCE}" "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    endif()

    # The podium ceremony rebuilds collision data independently of Track::Init.
    # Reset the same fixed visit hash there so a completed race cannot leak
    # entries into the Royal Raceway ending mesh.
    file(READ "${SPAGHETTIKART_ENDING_SOURCE}" SPAGHETTIKART_ENDING_TEXT)
    set(SPAGHETTIKART_3DS_ENDING_ORIGINAL "${SPAGHETTIKART_ENDING_TEXT}")
    string(REPLACE
        "#include <racing/memory.h>"
        "#include <racing/memory.h>\n\nextern void mk64_3ds_collision_reset_visits(void);"
        SPAGHETTIKART_3DS_ENDING_TEXT
        "${SPAGHETTIKART_ENDING_TEXT}"
    )
    if(SPAGHETTIKART_3DS_ENDING_TEXT STREQUAL SPAGHETTIKART_3DS_ENDING_ORIGINAL)
        message(FATAL_ERROR "The upstream ending collision-reset declaration point changed")
    endif()
    set(SPAGHETTIKART_3DS_ENDING_DECL_TEXT "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    string(REPLACE
        "    gCollisionMeshCount = 0;\n    bFog = false;"
        "    gCollisionMeshCount = 0;\n    mk64_3ds_collision_reset_visits();\n    bFog = false;"
        SPAGHETTIKART_3DS_ENDING_TEXT
        "${SPAGHETTIKART_3DS_ENDING_TEXT}"
    )
    if(SPAGHETTIKART_3DS_ENDING_TEXT STREQUAL SPAGHETTIKART_3DS_ENDING_DECL_TEXT)
        message(FATAL_ERROR "The upstream ending collision-reset call point changed")
    endif()
    set(SPAGHETTIKART_3DS_ENDING_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_ENDING_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_ENDING_SOURCE}" SPAGHETTIKART_3DS_ENDING_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_ENDING_EXISTING STREQUAL SPAGHETTIKART_3DS_ENDING_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_ENDING_SOURCE}" "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    endif()

    file(READ "${SPAGHETTIKART_RENDER_COURSES_SOURCE}"
        SPAGHETTIKART_3DS_RENDER_COURSES_TEXT)
    set(SPAGHETTIKART_3DS_RENDER_COURSES_ORIGINAL
        "${SPAGHETTIKART_3DS_RENDER_COURSES_TEXT}")
    string(REPLACE
        [=[void parse_track_displaylists(TrackSections* asset) {
    TrackSections* section = (TrackSections*) asset;]=]
        [=[void parse_track_displaylists(TrackSections* asset) {
    if (asset == NULL) {
        // Resource pressure can reject a course-section load. Leave the
        // collision mesh empty instead of dereferencing a failed C lookup;
        // the renderer recovery path can then reclaim caches safely.
        return;
    }
    TrackSections* section = (TrackSections*) asset;]=]
        SPAGHETTIKART_3DS_RENDER_COURSES_TEXT
        "${SPAGHETTIKART_3DS_RENDER_COURSES_TEXT}"
    )
    if(SPAGHETTIKART_3DS_RENDER_COURSES_TEXT STREQUAL
       SPAGHETTIKART_3DS_RENDER_COURSES_ORIGINAL)
        message(FATAL_ERROR "The upstream track-section parser guard point changed")
    endif()
    set(SPAGHETTIKART_3DS_RENDER_COURSES_EXISTING "")
    if(EXISTS "${SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE}")
        file(READ "${SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE}"
            SPAGHETTIKART_3DS_RENDER_COURSES_EXISTING)
    endif()
    if(NOT SPAGHETTIKART_3DS_RENDER_COURSES_EXISTING STREQUAL
       SPAGHETTIKART_3DS_RENDER_COURSES_TEXT)
        file(WRITE "${SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE}"
            "${SPAGHETTIKART_3DS_RENDER_COURSES_TEXT}")
    endif()

    set(SPAGHETTIKART_GAME_OS_MATH
        "${SPAGHETTIKART_ROOT}/src/os/guLookAtF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guPerspectiveF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guMtxF2L.c"
        "${SPAGHETTIKART_ROOT}/src/os/guRotateF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guOrthoF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guScaleF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guTranslateF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guMtxCatF.c"
        "${SPAGHETTIKART_ROOT}/src/os/guMtxCatL.c"
        "${SPAGHETTIKART_ROOT}/src/os/guNormalize.c"
        "${SPAGHETTIKART_ROOT}/src/port/interpolation/matrix.c"
    )

    add_library(mk64-3ds-game-core STATIC
        ${SPAGHETTIKART_GAME_ROOT}
        ${SPAGHETTIKART_GAME_RECURSIVE}
        ${SPAGHETTIKART_GAME_OS_MATH}
        platform/3ds/source/resource_prefetch_3ds.c
        "${SPAGHETTIKART_3DS_MEMORY_SOURCE}"
        "${SPAGHETTIKART_3DS_COLLISION_SOURCE}"
        "${SPAGHETTIKART_3DS_SPAWN_PLAYERS_SOURCE}"
        "${SPAGHETTIKART_3DS_ACTORS_SOURCE}"
        "${SPAGHETTIKART_3DS_RENDER_COURSES_SOURCE}"
        "${SPAGHETTIKART_3DS_ACTORS_EXTENDED_SOURCE}"
        "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}"
        "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}"
        "${SPAGHETTIKART_3DS_MENU_ITEMS_SOURCE}"
        "${SPAGHETTIKART_3DS_HUD_SOURCE}"
        "${SPAGHETTIKART_3DS_MAIN_SOURCE}"
        "${SPAGHETTIKART_3DS_UPDATE_OBJECTS_SOURCE}"
        "${SPAGHETTIKART_3DS_ENDING_SOURCE}"
    )
    target_include_directories(mk64-3ds-game-core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
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

if(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR)
    set(MK64_3DS_ARM_ABI_FLAGS
        -march=armv6k
        -mtune=mpcore
        -mfloat-abi=hard
        -mtp=soft
        -mword-relocations
    )
    set(USE_STANDALONE OFF CACHE BOOL "" FORCE)
    set(BUILD_STORMLIB OFF CACHE BOOL "" FORCE)
    set(BUILD_SM64 OFF CACHE BOOL "" FORCE)
    set(BUILD_MK64 ON CACHE BOOL "" FORCE)
    set(BUILD_SF64 OFF CACHE BOOL "" FORCE)
    set(BUILD_FZERO OFF CACHE BOOL "" FORCE)
    set(BUILD_MARIO_ARTIST OFF CACHE BOOL "" FORCE)
    set(BUILD_NAUDIO ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/SpaghettiKart/torch "${CMAKE_CURRENT_BINARY_DIR}/torch")
    foreach(MK64_3DS_TORCH_TARGET
            torch
            yaml-cpp
            spdlog
            tinyxml2
            BinaryTools
            N64Graphics)
        if(TARGET ${MK64_3DS_TORCH_TARGET})
            target_compile_options(${MK64_3DS_TORCH_TARGET} PRIVATE ${MK64_3DS_ARM_ABI_FLAGS})
            target_compile_definitions(${MK64_3DS_TORCH_TARGET} PRIVATE PLATFORM_3DS=1)
        endif()
    endforeach()
    if(TARGET spdlog)
        target_compile_definitions(spdlog PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    target_compile_definitions(torch PRIVATE PLATFORM_3DS=1)
    target_compile_options(torch PRIVATE
        -O3
        -ffunction-sections
        -fdata-sections
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-missing-field-initializers
    )
endif()

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

    set(SPAGHETTIKART_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart")
    file(GLOB SPAGHETTIKART_ENGINE_ROOT CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/*.cpp"
    )
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

    # SpaghettiKart advances two 60 Hz logic ticks for every 30 Hz key frame,
    # but its replacement Toad's Turnpike traffic currently performs the full
    # path/trigonometry update on both ticks and compensates by halving speed.
    # Preserve the original every-other-tick vehicle cadence on 3DS so the 28
    # traffic actors do half the path-update work at the intended cadence.
    file(READ "${SPAGHETTIKART_WORLD_SOURCE}" SPAGHETTIKART_3DS_WORLD_TEXT)
    set(SPAGHETTIKART_3DS_WORLD_ORIGINAL "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    string(REPLACE
        "#include <memory>"
        "#include <memory>\n#include <cstring>\n#include \"base_actor_pool_3ds.hpp\""
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WORLD_TEXT STREQUAL SPAGHETTIKART_3DS_WORLD_ORIGINAL)
        message(FATAL_ERROR "The upstream World include point changed")
    endif()
    set(SPAGHETTIKART_3DS_WORLD_INCLUDE_TEXT "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    string(REPLACE
        "World* World::Instance;"
        [=[namespace {
constexpr size_t kMk64BaseActorCapacity3DS = 100;
mk64_3ds::BaseActorPool3DS<AActor, kMk64BaseActorCapacity3DS>
    sMk64BaseActors3DS;
}

extern "C" bool Mk64BaseActor3DSIsDynamic(const struct Actor* actor,
                                             size_t permanentCount) {
    if (actor == nullptr) {
        return false;
    }
    const auto* cppActor = reinterpret_cast<const AActor*>(
        reinterpret_cast<const char*>(actor) - sizeof(void*));
    return sMk64BaseActors3DS.IsDynamic(cppActor, permanentCount);
}

World* World::Instance;]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_INCLUDE_TEXT)
        message(FATAL_ERROR "The upstream World actor-pool state point changed")
    endif()
    string(REPLACE
        [=[struct Actor* World::AddBaseActor() {
    Actors.push_back(std::make_unique<AActor>());

    AActor* actor = Actors.back().get();

    // Skip C++ vtable to access variables in C
    return reinterpret_cast<struct Actor*>(reinterpret_cast<char*>(actor) + sizeof(void*));
}]=]
        [=[struct Actor* World::AddBaseActor() {
    AActor* actor = sMk64BaseActors3DS.FindReusable([](const AActor* candidate) {
        return candidate->Flags == 0 && candidate->Type == 0;
    });
    if (actor != nullptr) {
        actor->bPendingDestroy = false;
        actor->Name = "";
        actor->ResourceName = "";
        actor->Triangles.clear();
        return ConvertAActorToActor(actor);
    }
    if (sMk64BaseActors3DS.Full()) {
        return nullptr;
    }

    Actors.push_back(std::make_unique<AActor>());
    actor = Actors.back().get();
    if (!sMk64BaseActors3DS.Track(actor)) {
        Actors.pop_back();
        return nullptr;
    }
    return ConvertAActorToActor(actor);
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_STATE_TEXT)
        message(FATAL_ERROR "The upstream World base-actor allocation point changed")
    endif()
    string(REPLACE
        "    Actors.clear();"
        "    sMk64BaseActors3DS.Reset();\n    Actors.clear();"
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_ALLOC_TEXT)
        message(FATAL_ERROR "The upstream World actor cleanup points changed")
    endif()
    string(REPLACE
        [=[void World::TickCameras() {

    for (size_t i = 0; i < 4; i++) {
        ScreenContext* screen = &gScreenContexts[i];
        if (NULL == screen->pendingCamera) { continue; }
        if (screen->pendingCamera != screen->camera) {
            screen->camera = screen->pendingCamera;
            screen->pendingCamera = nullptr;
        }
    }

    for (auto& camera : Cameras) {
        if (camera->IsActive()) {
            camera->Tick();
        }
    }
}]=]
        [=[void World::TickCameras() {

    for (size_t i = 0; i < 4; i++) {
        ScreenContext* screen = &gScreenContexts[i];
        if (NULL == screen->pendingCamera) { continue; }
        if (screen->pendingCamera != screen->camera) {
            screen->camera = screen->pendingCamera;
            screen->pendingCamera = nullptr;
        }
    }

    size_t activeScreens = 1;
    if (gActiveScreenMode == SCREEN_MODE_2P_SPLITSCREEN_HORIZONTAL ||
        gActiveScreenMode == SCREEN_MODE_2P_SPLITSCREEN_VERTICAL) {
        activeScreens = 2;
    } else if (gActiveScreenMode == SCREEN_MODE_3P_4P_SPLITSCREEN) {
        activeScreens = 4;
    }
    for (auto& camera : Cameras) {
        if (!camera->IsActive()) {
            continue;
        }
        Camera* rawCamera = camera->Get();
        bool usedByActiveScreen = false;
        for (size_t screenIndex = 0; screenIndex < activeScreens; ++screenIndex) {
            const ScreenContext* screen = &gScreenContexts[screenIndex];
            if (rawCamera == screen->camera || rawCamera == screen->raceCamera ||
                rawCamera == screen->lookBehindCamera) {
                usedByActiveScreen = true;
                break;
            }
        }
        if (usedByActiveScreen) {
            camera->Tick();
        }
    }
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    set(SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}")
    if(SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT STREQUAL
       SPAGHETTIKART_3DS_WORLD_POOL_RESET_TEXT)
        message(FATAL_ERROR "The upstream World camera tick loop changed")
    endif()
    string(REPLACE
        [=[void World::TickActors() {
    // This only ticks modded actors
    for (auto& actor : Actors) {
        if (actor->IsMod()) {
            actor->Tick();
        }
    }
}]=]
        [=[void World::TickActors() {
    static bool trafficTickPhase3DS = false;
    trafficTickPhase3DS = !trafficTickPhase3DS;

    // This only ticks modded actors. The four built-in Turnpike traffic
    // classes follow the original game's one-in-two logic cadence.
    for (auto& actor : Actors) {
        if (!actor->IsMod()) {
            continue;
        }
        const char* resourceName = actor->ResourceName;
        if (!trafficTickPhase3DS && gCurrentCourseId == TRACK_TOADS_TURNPIKE &&
            resourceName != nullptr &&
            (std::strcmp(resourceName, "mk:car") == 0 ||
             std::strcmp(resourceName, "mk:bus") == 0 ||
             std::strcmp(resourceName, "mk:truck") == 0 ||
             std::strcmp(resourceName, "mk:tanker_truck") == 0)) {
            continue;
        }
        actor->Tick();
    }
}]=]
        SPAGHETTIKART_3DS_WORLD_TEXT
        "${SPAGHETTIKART_3DS_WORLD_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WORLD_TEXT STREQUAL SPAGHETTIKART_3DS_WORLD_CAMERA_TICK_TEXT)
        message(FATAL_ERROR "The upstream World actor tick loop changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_WORLD_SOURCE}" "${SPAGHETTIKART_3DS_WORLD_TEXT}")

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

    file(READ "${SPAGHETTIKART_TOADS_TURNPIKE_SOURCE}" SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT)
    set(SPAGHETTIKART_3DS_TOADS_TURNPIKE_ORIGINAL
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}")
    string(REPLACE
        "        a /= 2; // Normally vehicle logic is only ran every 2 frames. This slows the vehicles down to match.\n        b /= 2;"
        "        // PLATFORM_3DS restores the original every-other-tick cadence in World::TickActors."
        SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT STREQUAL
       SPAGHETTIKART_3DS_TOADS_TURNPIKE_ORIGINAL)
        message(FATAL_ERROR "The upstream Toad's Turnpike traffic speed compensation changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_SOURCE}"
         "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_TEXT}")

    set(SPAGHETTIKART_TRACK_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/tracks/Track.cpp")
    set(SPAGHETTIKART_3DS_TRACK_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/Track_3ds.cpp")
    list(REMOVE_ITEM SPAGHETTIKART_ENGINE_TRACKS "${SPAGHETTIKART_TRACK_SOURCE}")
    file(READ "${SPAGHETTIKART_TRACK_SOURCE}" SPAGHETTIKART_TRACK_TEXT)
    string(REPLACE
        "extern \"C\" {\n#include \"main.h\""
        "extern \"C\" {\nvoid mk64_3ds_collision_reset_visits(void);\n#include \"main.h\""
        SPAGHETTIKART_3DS_TRACK_TEXT
        "${SPAGHETTIKART_TRACK_TEXT}"
    )
    set(SPAGHETTIKART_3DS_TRACK_DECL_TEXT "${SPAGHETTIKART_3DS_TRACK_TEXT}")
    if(SPAGHETTIKART_3DS_TRACK_DECL_TEXT STREQUAL SPAGHETTIKART_TRACK_TEXT)
        message(FATAL_ERROR "The upstream Track collision-reset declaration insertion point changed")
    endif()
    string(REPLACE
        "    gCollisionMeshCount = 0;\n    gCollisionMesh = (CollisionTriangle*) gNextFreeMemoryAddress;"
        "    gCollisionMeshCount = 0;\n    mk64_3ds_collision_reset_visits();\n    gCollisionMesh = (CollisionTriangle*) gNextFreeMemoryAddress;"
        SPAGHETTIKART_3DS_TRACK_TEXT
        "${SPAGHETTIKART_3DS_TRACK_TEXT}"
    )
    if(SPAGHETTIKART_3DS_TRACK_TEXT STREQUAL SPAGHETTIKART_3DS_TRACK_DECL_TEXT)
        message(FATAL_ERROR "The upstream Track collision-reset call insertion point changed")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(WRITE "${SPAGHETTIKART_3DS_TRACK_SOURCE}" "${SPAGHETTIKART_3DS_TRACK_TEXT}")

    # Citro3D's framebuffer readback is intentionally absent, so copying its
    # zero-filled CPU buffer into six billboard textures and invalidating all
    # six GPU cache entries every frame only creates stalls. Keep the original
    # static course textures on 3DS; desktop source and behavior remain intact.
    set(MK64_3DS_JUMBOTRON_INVALIDATION_BLOCK [=[    // Invalidate Jumbotron textures so they update each frame
    // This could be more efficient if we exposed the non-opcode based invalidation to be called
    // inside copy_framebuffers_port
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture68272C);
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture682928);
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture682B24);
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture682D20);
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture682F1C);
    gSPInvalidateTexCache(gDisplayListHead++, (uintptr_t) gTexture683118);
]=])

    file(READ "${SPAGHETTIKART_LUIGI_RACEWAY_SOURCE}"
         SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT)
    set(SPAGHETTIKART_3DS_LUIGI_RACEWAY_ORIGINAL
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    string(REPLACE "${MK64_3DS_JUMBOTRON_INVALIDATION_BLOCK}" ""
        SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    if(SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT STREQUAL
       SPAGHETTIKART_3DS_LUIGI_RACEWAY_ORIGINAL)
        message(FATAL_ERROR "The upstream Luigi Raceway jumbotron invalidation block changed")
    endif()
    set(SPAGHETTIKART_3DS_LUIGI_RACEWAY_INVALIDATION_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")
    string(REPLACE
        [=[        CopyJumbotron(D_800DC5DC, D_800DC5E0, currentScreenSection,
                                            (u16*) gPortFramebuffers[prevFrame]);]=]
        "        /* PLATFORM_3DS: retain the static billboard texture. */"
        SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}"
    )
    if(SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT STREQUAL
       SPAGHETTIKART_3DS_LUIGI_RACEWAY_INVALIDATION_TEXT)
        message(FATAL_ERROR "The upstream Luigi Raceway jumbotron copy call changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_SOURCE}"
         "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_TEXT}")

    file(READ "${SPAGHETTIKART_WARIO_STADIUM_SOURCE}"
         SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT)
    set(SPAGHETTIKART_3DS_WARIO_STADIUM_ORIGINAL
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    string(REPLACE "${MK64_3DS_JUMBOTRON_INVALIDATION_BLOCK}" ""
        SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    if(SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT STREQUAL
       SPAGHETTIKART_3DS_WARIO_STADIUM_ORIGINAL)
        message(FATAL_ERROR "The upstream Wario Stadium jumbotron invalidation block changed")
    endif()
    set(SPAGHETTIKART_3DS_WARIO_STADIUM_INVALIDATION_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")
    string(REPLACE
        "        CopyJumbotron(D_800DC5DC, D_800DC5E0, currentScreenSection, (u16*) gPortFramebuffers[prevFrame]);"
        "        /* PLATFORM_3DS: retain the static billboard texture. */"
        SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}"
    )
    if(SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT STREQUAL
       SPAGHETTIKART_3DS_WARIO_STADIUM_INVALIDATION_TEXT)
        message(FATAL_ERROR "The upstream Wario Stadium jumbotron copy call changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_WARIO_STADIUM_SOURCE}"
         "${SPAGHETTIKART_3DS_WARIO_STADIUM_TEXT}")

    file(GLOB SPAGHETTIKART_ENGINE_VEHICLES CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/vehicles/*.cpp"
    )
    set(SPAGHETTIKART_ENGINE_REGISTRY
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItems.cpp"
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItemTables.cpp"
    )
    set(SPAGHETTIKART_GAME_BRIDGE_SOURCE "${SPAGHETTIKART_ROOT}/src/port/Game.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${SPAGHETTIKART_GAME_BRIDGE_SOURCE}")
    set(SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Game_3ds.cpp")
    file(READ "${SPAGHETTIKART_GAME_BRIDGE_SOURCE}" SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT)
    set(SPAGHETTIKART_3DS_GAME_BRIDGE_ORIGINAL "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[                } else {
                    GetWorld()->GetTrack()->DrawCredits();
                }
            case RENDER_TRACK_SECTIONS:]=]
        [=[                } else {
                    GetWorld()->GetTrack()->DrawCredits();
                }
                break;
            case RENDER_TRACK_SECTIONS:]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_BRIDGE_ORIGINAL)
        message(FATAL_ERROR "The upstream full-scene render switch changed")
    endif()
    set(SPAGHETTIKART_3DS_GAME_VEHICLE_COLLISION_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[void CM_VehicleCollision(s32 playerId, Player* player) {
    for (auto& actor : GetWorld()->Actors) {
        if (actor) {
            actor->VehicleCollision(playerId, player);
        }
    }
}]=]
        [=[void CM_VehicleCollision(s32 playerId, Player* player) {
    for (auto& actor : GetWorld()->Actors) {
        if (actor && actor->IsMod()) {
            actor->VehicleCollision(playerId, player);
        }
    }
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_VEHICLE_COLLISION_ORIGINAL)
        message(FATAL_ERROR "The upstream vehicle-collision dispatch changed")
    endif()
    set(SPAGHETTIKART_3DS_GAME_TRACK_RELEASE_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        "#include <atomic>"
        "#include <atomic>\n#include <array>\n#include <cstdio>\n#include <cstring>\n#include \"libultraship/bridge/resourcebridge.h\""
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    string(REPLACE
        [=[void LoadTrack() {
    GetWorld()->GetRaceManager().Load();
}]=]
        [=[namespace {
std::array<char, 64> sMk64LoadedTrackResources3DS = {};

void Mk64UnloadTrackDirectory3DS(const char* root, const char* track) {
    std::array<char, 96> directory = {};
    const int written = std::snprintf(directory.data(), directory.size(), "%s%s/", root, track);
    if (written > 0 && static_cast<size_t>(written) < directory.size()) {
        ResourceUnloadDirectory(directory.data());
    }
}
}

extern "C" void Mk64ReleaseLoadedTrackResources3DS(void) {
    if (std::strncmp(sMk64LoadedTrackResources3DS.data(), "mk:", 3) != 0) {
        sMk64LoadedTrackResources3DS[0] = '\0';
        return;
    }
    const char* archiveTrack = sMk64LoadedTrackResources3DS.data() + 3;
    if (std::strcmp(archiveTrack, "podium_ceremony") == 0) {
        // The ceremony combines its own top-level resources with Royal
        // Raceway geometry. Release both groups after CM_CleanWorld has
        // destroyed every actor that could still reference them.
        Mk64UnloadTrackDirectory3DS("models/", "ceremony_data");
        Mk64UnloadTrackDirectory3DS("other/", "ceremony_data");
        Mk64UnloadTrackDirectory3DS("textures/", "ceremony_data");
        archiveTrack = "royal_raceway";
    }
    if (std::strcmp(archiveTrack, "dk_jungle") == 0) {
        archiveTrack = "dks_jungle_parkway";
    }
    Mk64UnloadTrackDirectory3DS("models/tracks/", archiveTrack);
    Mk64UnloadTrackDirectory3DS("other/tracks/", archiveTrack);
    Mk64UnloadTrackDirectory3DS("textures/tracks/", archiveTrack);
    sMk64LoadedTrackResources3DS[0] = '\0';
}

void LoadTrack() {
    GetWorld()->GetRaceManager().Load();
    Track* track = GetWorld()->GetTrack();
    sMk64LoadedTrackResources3DS[0] = '\0';
    if (track != nullptr) {
        std::snprintf(sMk64LoadedTrackResources3DS.data(),
                      sMk64LoadedTrackResources3DS.size(), "%s",
                      track->ResourceName.c_str());
    }
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_GAME_TRACK_RELEASE_ORIGINAL)
        message(FATAL_ERROR "The upstream track-load resource boundary changed")
    endif()
    set(SPAGHETTIKART_3DS_CAMERA_CLEAN_ORIGINAL
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    string(REPLACE
        [=[void CM_CleanCameras(void) {
    GetWorld()->Cameras.clear();
}]=]
        [=[void CM_CleanCameras(void) {
    // Screen contexts do not own these cameras. Clear every alias before the
    // owning vector so a pending switch cannot reactivate a freed camera on
    // the first tick of the next race or results transition.
    for (ScreenContext& screen : gScreenContexts) {
        screen.camera = nullptr;
        screen.raceCamera = nullptr;
        screen.lookBehindCamera = nullptr;
        screen.freeCamera = nullptr;
        screen.pendingCamera = nullptr;
    }
    GetWorld()->Cameras.clear();
}]=]
        SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT
        "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}"
    )
    if(SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT STREQUAL
       SPAGHETTIKART_3DS_CAMERA_CLEAN_ORIGINAL)
        message(FATAL_ERROR "The upstream camera cleanup boundary changed")
    endif()
    file(WRITE "${SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE}"
         "${SPAGHETTIKART_3DS_GAME_BRIDGE_TEXT}")
    set_source_files_properties("${SPAGHETTIKART_3DS_GAME_BRIDGE_SOURCE}" PROPERTIES
        GENERATED TRUE
        COMPILE_DEFINITIONS "main=spaghettikart_desktop_main;Graphics_PushFrame=Graphics_PushFrameDesktop;CustomEngineInit=DesktopCustomEngineInit;CustomEngineDestroy=DesktopCustomEngineDestroy"
    )

    add_library(mk64-3ds-game-engine STATIC
        platform/3ds/source/audio_runtime_3ds.cpp
        platform/3ds/source/cvar_3ds.cpp
        platform/3ds/source/frame_interpolation_3ds.cpp
        platform/3ds/source/editor_stubs_3ds.cpp
        platform/3ds/source/game_data_3ds.cpp
        platform/3ds/source/game_state_3ds.cpp
        platform/3ds/source/gbi_middleware_3ds.cpp
        platform/3ds/source/install_log_3ds.cpp
        platform/3ds/source/loading_image_3ds.cpp
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
        "${SPAGHETTIKART_3DS_TOADS_TURNPIKE_SOURCE}"
        "${SPAGHETTIKART_3DS_LUIGI_RACEWAY_SOURCE}"
        "${SPAGHETTIKART_3DS_WARIO_STADIUM_SOURCE}"
        ${SPAGHETTIKART_ENGINE_VEHICLES}
        ${SPAGHETTIKART_ENGINE_REGISTRY}
    )
    target_include_directories(mk64-3ds-game-engine PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
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

set(MK64_3DS_SMDH "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds.smdh")
ctr_generate_smdh(
    OUTPUT "${MK64_3DS_SMDH}"
    NAME "Mario Kart 64 3DS v${MK64_3DS_VERSION}"
    DESCRIPTION "Native SpaghettiKart port for Nintendo 3DS"
    AUTHOR "EstebanPdN"
    ICON "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/assets/icon-48.png"
)

add_executable(mk64-3ds
    platform/3ds/source/main.c
)

target_include_directories(mk64-3ds PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
)

target_compile_features(mk64-3ds PRIVATE c_std_11)
set_target_properties(mk64-3ds PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)
target_compile_options(mk64-3ds PRIVATE
    -O3
    -fomit-frame-pointer
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -Wall
    -Wextra
    -Wpedantic
)
target_link_options(mk64-3ds PRIVATE
    -Wl,--gc-sections
)
target_link_libraries(mk64-3ds PRIVATE citro2d citro3d ctru m)

if(MK64_3DS_ENABLE_O2R_READER)
    add_library(mk64-3ds-o2r-reader STATIC
        platform/3ds/source/o2r_archive_reader.cpp
        platform/3ds/source/resource_runtime_3ds.cpp
        third_party/SpaghettiKart/libultraship/src/ship/utils/StrHash64.cpp
    )
    target_include_directories(mk64-3ds-o2r-reader PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
    )
    target_include_directories(mk64-3ds-o2r-reader PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/include"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/include/assets"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/src"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include/libultraship"
    )
    target_include_directories(mk64-3ds-o2r-reader SYSTEM PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/torch/lib"
    )
    target_compile_features(mk64-3ds-o2r-reader PUBLIC cxx_std_17)
    target_compile_definitions(mk64-3ds-o2r-reader PRIVATE F3DEX_GBI=1)
    if(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR)
        target_compile_definitions(mk64-3ds-o2r-reader PRIVATE MINIZ_HEADER_FILE_ONLY=1)
    endif()
    target_compile_options(mk64-3ds-o2r-reader PRIVATE
        -O3
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(MK64_3DS_ENABLE_FAST3D_RENDERER)
    ctr_add_shader_library(mk64-3ds-fast3d-shader
        platform/3ds/shaders/fast3d_passthrough.v.pica
        OUTPUT fast3d_passthrough.shbin
    )
    dkp_add_embedded_binary_library(mk64-3ds-fast3d-shader-bin mk64-3ds-fast3d-shader)

    add_library(mk64-3ds-fast3d-renderer STATIC
        platform/3ds/source/gfx_citro3d.cpp
    )
    target_include_directories(mk64-3ds-fast3d-renderer PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
    )
    target_compile_features(mk64-3ds-fast3d-renderer PUBLIC cxx_std_17)
    target_compile_options(mk64-3ds-fast3d-renderer PRIVATE
        -O3
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -Wall
        -Wextra
        -Wpedantic
    )
    target_link_libraries(mk64-3ds-fast3d-renderer PUBLIC
        mk64-3ds-fast3d-shader-bin
        citro3d
        ctru
        m
    )

    if(MK64_3DS_BUILD_RENDERER_PROBE)
        add_executable(mk64-3ds-renderer-probe
            platform/3ds/source/renderer_probe.cpp
        )
        target_compile_features(mk64-3ds-renderer-probe PRIVATE cxx_std_17)
        target_compile_options(mk64-3ds-renderer-probe PRIVATE
            -O3
            -fomit-frame-pointer
            -ffunction-sections
            -fdata-sections
            -Wall
            -Wextra
            -Wpedantic
        )
        target_link_options(mk64-3ds-renderer-probe PRIVATE -Wl,--gc-sections)
        target_link_libraries(mk64-3ds-renderer-probe PRIVATE mk64-3ds-fast3d-renderer)

        ctr_create_3dsx(mk64-3ds-renderer-probe-3dsx
            TARGET mk64-3ds-renderer-probe
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds-renderer-probe.3dsx"
            SMDH "${MK64_3DS_SMDH}"
        )
    endif()

    if(MK64_3DS_BUILD_INTERPRETER_PROBE)
        set(SPAGHETTIKART_INTERPRETER_SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/src/fast/interpreter.cpp")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${SPAGHETTIKART_INTERPRETER_SOURCE}")
        set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/interpreter_3ds.cpp")
        file(READ "${SPAGHETTIKART_INTERPRETER_SOURCE}" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
        set(SPAGHETTIKART_3DS_INTERPRETER_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "GfxExecStack g_exec_stack = {};"
            "extern \"C\" void Mk64Diagnostics3DSGfxWatchdog(const void*, uint32_t, uint32_t, size_t) __attribute__((weak));\n\nGfxExecStack g_exec_stack = {};"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        set(SPAGHETTIKART_3DS_CULL_INCLUDE_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "#include \"fast/types.h\""
            "#include \"fast/types.h\"\n#include \"fast3d_culling_3ds.hpp\"\n#include \"fast3d_math_3ds.hpp\"\n\nextern \"C\" bool Mk64FrameInterpolation3DSLookup(const Mtx*, MtxF*) __attribute__((weak));"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_CULL_INCLUDE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D culling include point changed")
        endif()
        set(SPAGHETTIKART_3DS_CULL_HANDLER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    // TODO:
    return false;
}]=]
            [=[bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = mInstance.lock().get();
    ::mk64_3ds::Fast3DCullEncoding encoding3DS =
        ::mk64_3ds::Fast3DCullEncoding::F3DEX;
    if (ucode_handler_index == ucode_f3db || ucode_handler_index == ucode_f3d) {
        encoding3DS = ::mk64_3ds::Fast3DCullEncoding::F3D;
    } else if (ucode_handler_index == ucode_f3dex2) {
        encoding3DS = ::mk64_3ds::Fast3DCullEncoding::F3DEX2;
    }

    const auto range3DS = ::mk64_3ds::DecodeFast3DCullRange(
        (*cmd)->words.w0, (*cmd)->words.w1, encoding3DS, MAX_VERTICES);
    const bool shouldCull3DS = ::mk64_3ds::ShouldCullFast3DRange(
        range3DS, [gfx](std::size_t index3DS) {
            return gfx->mRsp->loaded_vertices[index3DS].clip_rej;
        });
    if (!shouldCull3DS) {
        return false;
    }

    gfx->mMarkerOn = false;
    g_exec_stack.ret();
    return true;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_CULL_HANDLER_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D G_CULLDL handler changed")
        endif()
        set(SPAGHETTIKART_3DS_EXEC_STACK_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void GfxExecStack::start(F3DGfx* dlist) {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
    cmd_stack.push(dlist);
    disp_stack.clear();
}

void GfxExecStack::stop() {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
}

F3DGfx*& GfxExecStack::currCmd() {
    return cmd_stack.top();
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    F3DGfx* old = cmd_stack.top();
    cmd_stack.pop();
    cmd_stack.push(nullptr);
    cmd_stack.push(old);

    gfx_path.push_back(caller);
}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    cmd_stack.push(callee);
    gfx_path.push_back(caller);
}

F3DGfx* GfxExecStack::ret() {
    F3DGfx* cmd = cmd_stack.top();

    cmd_stack.pop();
    if (!gfx_path.empty()) {
        gfx_path.pop_back();
    }

    while (cmd_stack.size() > 0 && cmd_stack.top() == nullptr) {
        cmd_stack.pop();
        if (!gfx_path.empty()) {
            gfx_path.pop_back();
        }
    }
    return cmd;
}]=]
            [=[namespace {
constexpr size_t kMk64ExecStackCapacity3DS = 512;
std::array<F3DGfx*, kMk64ExecStackCapacity3DS> sMk64ExecStack3DS = {};
size_t sMk64ExecStackDepth3DS = 0;
F3DGfx* sMk64NullCommand3DS = nullptr;

bool Mk64ExecStackEmpty3DS() {
    return sMk64ExecStackDepth3DS == 0;
}

size_t Mk64ExecStackDepth3DS() {
    return sMk64ExecStackDepth3DS;
}

F3DGfx* Mk64ExecStackCurrent3DS() {
    return Mk64ExecStackEmpty3DS() ? nullptr
                                   : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}
}

void GfxExecStack::start(F3DGfx* dlist) {
    sMk64ExecStackDepth3DS = 0;
    gfx_path.clear();
    disp_stack.clear();
    if (gfx_path.capacity() < kMk64ExecStackCapacity3DS) {
        gfx_path.reserve(kMk64ExecStackCapacity3DS);
    }
    if (disp_stack.capacity() < 16) {
        disp_stack.reserve(16);
    }
    if (dlist != nullptr) {
        sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = dlist;
    }
}

void GfxExecStack::stop() {
    sMk64ExecStackDepth3DS = 0;
    gfx_path.clear();
    disp_stack.clear();
}

F3DGfx*& GfxExecStack::currCmd() {
    return Mk64ExecStackEmpty3DS()
               ? sMk64NullCommand3DS
               : sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    if (!disp_stack.empty()) disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    if (Mk64ExecStackEmpty3DS() ||
        sMk64ExecStackDepth3DS >= kMk64ExecStackCapacity3DS) {
        stop();
        return;
    }
    F3DGfx* old = sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1];
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS - 1] = nullptr;
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = old;
    gfx_path.push_back(caller);
}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    if (callee == nullptr || sMk64ExecStackDepth3DS >= kMk64ExecStackCapacity3DS) {
        stop();
        return;
    }
    sMk64ExecStack3DS[sMk64ExecStackDepth3DS++] = callee;
    gfx_path.push_back(caller);
}

F3DGfx* GfxExecStack::ret() {
    if (Mk64ExecStackEmpty3DS()) return nullptr;
    F3DGfx* cmd = sMk64ExecStack3DS[--sMk64ExecStackDepth3DS];
    if (!gfx_path.empty()) gfx_path.pop_back();
    while (!Mk64ExecStackEmpty3DS() && Mk64ExecStackCurrent3DS() == nullptr) {
        --sMk64ExecStackDepth3DS;
        if (!gfx_path.empty()) gfx_path.pop_back();
    }
    return cmd;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_EXEC_STACK_ORIGINAL)
            message(FATAL_ERROR "The Fast3D execution-stack insertion point changed")
        endif()
        string(REPLACE
            "    while (!g_exec_stack.cmd_stack.empty()) {\n        auto cmd = g_exec_stack.cmd_stack.top();"
            "    size_t commandCount = 0;\n    constexpr size_t kMaxCommandsPerFrame = 250000;\n    constexpr size_t kMaxDisplayListDepth = 512;\n    while (!Mk64ExecStackEmpty3DS()) {\n        auto cmd = Mk64ExecStackCurrent3DS();\n        if (++commandCount > kMaxCommandsPerFrame || Mk64ExecStackDepth3DS() > kMaxDisplayListDepth) {\n            if (Mk64Diagnostics3DSGfxWatchdog != nullptr && cmd != nullptr) {\n                Mk64Diagnostics3DSGfxWatchdog(cmd, cmd->words.w0, cmd->words.w1, commandCount);\n            }\n            g_exec_stack.stop();\n            break;\n        }"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        set(SPAGHETTIKART_3DS_POINTER_GUARD_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[#else
    if (i <= 0x0FFFFFFF) {
        return false;
    }
#endif]=]
            [=[#else
#if defined(__3DS__)
    // 3DS application-heap pointers commonly live below 0x10000000, so the
    // desktop numeric-address guard rejects valid framebuffer and sprite data.
    // SegAddr rejects an unset or out-of-range segment with nullptr.
    if ((cmd->words.w1 & 1U) != 0 && i == 0) {
        gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                  imgData, 0, rawTexMetdata, nullptr);
        return false;
    }
#else
    if (i <= 0x0FFFFFFF) {
        return false;
    }
#endif
#endif]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_POINTER_GUARD_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D texture pointer guard insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        # Animated eight-racer scenes continuously rotate through more than the
        # old 256-entry working set. The runtime selects 256 entries on Old 3DS
        # and up to 384 on New 3DS; the compile-time cap bounds every fixed
        # owner table without restoring desktop's 1024-entry memory footprint.
        string(REPLACE
            "#define TEXTURE_CACHE_MAX_SIZE 1024"
            "#define TEXTURE_CACHE_MAX_SIZE 384"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D texture cache limit insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "void Interpreter::TextureCacheClear() {"
            [=[#if defined(__3DS__)
extern "C" size_t Mk64Graphics3DSTextureCacheCapacity(void) __attribute__((weak));

struct Mk64TextureOwners3DS {
    std::shared_ptr<Ship::IResource> image;
    std::array<std::shared_ptr<Ship::IResource>, 2> palettes;
    const uint8_t* imageBase = nullptr;
    std::array<const uint8_t*, 2> paletteBases = {};
};

static std::array<Mk64TextureOwners3DS, TEXTURE_CACHE_MAX_SIZE + 1>
    sMk64TextureOwners3DS;
static std::array<std::shared_ptr<Ship::IResource>, 2> sMk64PaletteOwners3DS;
static std::array<const uint8_t*, 2> sMk64PaletteOwnerBases3DS = {};
static std::array<uint32_t, TEXTURE_CACHE_MAX_SIZE + 1> sMk64TextureFrameUses3DS = {};
static uint32_t sMk64TextureFrameSerial3DS = 1;

struct Mk64BlendedTextureOwner3DS {
    const uint8_t* data = nullptr;
    std::shared_ptr<Fast::Texture> resource;
};
static std::array<Mk64BlendedTextureOwner3DS, TEXTURE_CACHE_MAX_SIZE>
    sMk64BlendedTextureOwners3DS;

static size_t Mk64TextureCacheCapacity3DS() {
    const size_t requested = Mk64Graphics3DSTextureCacheCapacity != nullptr
                                 ? Mk64Graphics3DSTextureCacheCapacity()
                                 : 256;
    return std::max<size_t>(1, std::min<size_t>(requested, TEXTURE_CACHE_MAX_SIZE));
}

static void Mk64BeginTextureFrame3DS() {
    if (++sMk64TextureFrameSerial3DS == 0) {
        sMk64TextureFrameUses3DS.fill(0);
        sMk64TextureFrameSerial3DS = 1;
    }
}

static void Mk64MarkTextureUsed3DS(uint32_t textureId) {
    if (textureId < sMk64TextureFrameUses3DS.size()) {
        sMk64TextureFrameUses3DS[textureId] = sMk64TextureFrameSerial3DS;
    }
}

static bool Mk64TextureUsedThisFrame3DS(uint32_t textureId) {
    return textureId < sMk64TextureFrameUses3DS.size() &&
           sMk64TextureFrameUses3DS[textureId] == sMk64TextureFrameSerial3DS;
}

static bool Mk64AddressInRange3DS(const uint8_t* address, const uint8_t* base,
                                  size_t size) {
    if (address == nullptr || base == nullptr) return false;
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    return value >= start && value - start < std::max<size_t>(size, 1);
}

static std::shared_ptr<Fast::Texture> Mk64FindBlendedTextureOwner3DS(
    const uint8_t* data) {
    for (const auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data) return entry.resource;
    }
    return {};
}

static bool Mk64RegisterBlendedTextureOwner3DS(
    const uint8_t* data, const std::shared_ptr<Fast::Texture>& resource) {
    if (data == nullptr || resource == nullptr) return true;
    for (auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data || entry.data == nullptr) {
            entry.data = data;
            entry.resource = resource;
            return true;
        }
    }
    return false;
}

static void Mk64UnregisterBlendedTextureOwner3DS(const uint8_t* data) {
    for (auto& entry : sMk64BlendedTextureOwners3DS) {
        if (entry.data == data) {
            entry = {};
            return;
        }
    }
}

static void Mk64RetainTextureOwners3DS(uint32_t textureId,
                                       const TextureCacheKey& key, const RDP* rdp) {
    if (textureId >= sMk64TextureOwners3DS.size() || rdp == nullptr) return;
    auto& owners = sMk64TextureOwners3DS[textureId];
    owners = {};

    if (auto blendedOwner = Mk64FindBlendedTextureOwner3DS(key.texture_addr)) {
        owners.image = blendedOwner;
        owners.imageBase = blendedOwner->ImageData;
    } else {
        for (const auto& loaded : rdp->loaded_texture) {
            const auto& imageOwner = loaded.raw_tex_metadata.resource;
            const uint8_t* resourceBase = imageOwner != nullptr
                                              ? imageOwner->ImageData
                                              : loaded.addr;
            const size_t resourceSize = imageOwner != nullptr
                                            ? imageOwner->ImageDataSize
                                            : std::max(loaded.orig_size_bytes,
                                                       loaded.size_bytes);
            if (Mk64AddressInRange3DS(key.texture_addr, loaded.addr,
                                      std::max(loaded.orig_size_bytes,
                                               loaded.size_bytes)) ||
                Mk64AddressInRange3DS(key.texture_addr, resourceBase,
                                      resourceSize)) {
                owners.image = imageOwner;
                owners.imageBase = resourceBase;
                break;
            }
        }
    }
    for (size_t palette = 0; palette < sMk64PaletteOwners3DS.size(); ++palette) {
        if (key.palette_addrs[palette] != nullptr &&
            key.palette_addrs[palette] == rdp->palette_dram_addr[palette]) {
            owners.palettes[palette] = sMk64PaletteOwners3DS[palette];
            owners.paletteBases[palette] = sMk64PaletteOwnerBases3DS[palette];
        }
    }
}

static void Mk64ReleaseTextureOwner3DS(uint32_t textureId) {
    if (textureId < sMk64TextureOwners3DS.size()) {
        sMk64TextureOwners3DS[textureId] = {};
        sMk64TextureFrameUses3DS[textureId] = 0;
    }
}

static bool Mk64TextureOwnerReferences3DS(uint32_t textureId,
                                          const uint8_t* address) {
    if (textureId >= sMk64TextureOwners3DS.size() || address == nullptr) return false;
    const auto& owners = sMk64TextureOwners3DS[textureId];
    if (owners.imageBase == address) return true;
    return owners.paletteBases[0] == address || owners.paletteBases[1] == address;
}

static void Mk64ResetTextureOwners3DS() {
    for (auto& owners : sMk64TextureOwners3DS) owners = {};
    for (auto& resource : sMk64PaletteOwners3DS) {
        resource.reset();
    }
    sMk64PaletteOwnerBases3DS.fill(nullptr);
    sMk64TextureFrameUses3DS.fill(0);
    for (auto& owner : sMk64BlendedTextureOwners3DS) owner = {};
}
#endif

void Interpreter::TextureCacheClear() {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner helper insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FREE_IDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);"
            "mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);\n    mTextureCache.free_texture_ids.reserve(TEXTURE_CACHE_MAX_SIZE);"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FREE_IDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D free-texture-ID reserve point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_CLEAR_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
    }]=]
            [=[    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
#if defined(__3DS__)
        Mk64ReleaseTextureOwner3DS(entry.second.texture_id);
#endif
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_CLEAR_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner clear point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_CACHE_LOOKUP_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (mTextureCache.map.size() >= TEXTURE_CACHE_MAX_SIZE) {
        // Remove the texture that was least recently used
        it = mTextureCache.lru.front().it;
        mTextureCache.free_texture_ids.push_back(it->second.texture_id);
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        mTextureCache.map.erase(it);
        mTextureCache.lru.pop_front();
    }

    uint32_t texture_id;]=]
            [=[    if (mTextureCache.map.size() >= Mk64TextureCacheCapacity3DS()) {
        // Never overwrite a backing texture that an earlier draw in this
        // submitted frame still references. Prefer a cold LRU node; if a frame
        // really needs more than the profile capacity, grow only up to the
        // compile-time bound and reclaim those spill nodes on later misses.
        auto lruLocation3DS = mTextureCache.lru.end();
        for (auto candidate3DS = mTextureCache.lru.begin();
             candidate3DS != mTextureCache.lru.end(); ++candidate3DS) {
            if (!Mk64TextureUsedThisFrame3DS(candidate3DS->it->second.texture_id)) {
                lruLocation3DS = candidate3DS;
                break;
            }
        }
        if (lruLocation3DS == mTextureCache.lru.end()) {
            if (mTextureCache.map.size() >= TEXTURE_CACHE_MAX_SIZE) {
                throw std::bad_alloc();
            }
        } else {
        it = lruLocation3DS->it;
        const uint32_t textureId3DS = it->second.texture_id;
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        auto nodeHandle3DS = mTextureCache.map.extract(it);
        nodeHandle3DS.key() = key;
        auto insertResult3DS = mTextureCache.map.insert(std::move(nodeHandle3DS));
        assert(insertResult3DS.inserted);
        it = insertResult3DS.position;
        lruLocation3DS->it = it;
        mTextureCache.lru.splice(mTextureCache.lru.end(), mTextureCache.lru,
                                 lruLocation3DS);
        // Match the value-initialized state of an ordinary cache miss. Keeping
        // the evicted sampler metadata can make the interpreter skip a needed
        // sampler update for the new texture.
        it->second = TextureCacheValue{};
        it->second.texture_id = textureId3DS;
        it->second.lru_location = lruLocation3DS;

        // The cache no longer exposes the old key or node state. Release its
        // resource before retaining the new owner, preserving normal eviction
        // ordering while keeping the re-key operation exception-safe.
        Mk64ReleaseTextureOwner3DS(textureId3DS);
        Mk64RetainTextureOwners3DS(textureId3DS, key, mRdp);
        Mk64MarkTextureUsed3DS(textureId3DS);
        mRapi->SelectTexture(i, textureId3DS);
        mRapi->SetSamplerParameters(i, false, 0, 0);
        *n = &*it;
        return false;
        }
    }

    uint32_t texture_id;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_CACHE_LOOKUP_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-cache reuse point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FRAME_HIT_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (it != mTextureCache.map.end()) {
        mRapi->SelectTexture(i, it->second.texture_id);]=]
            [=[    if (it != mTextureCache.map.end()) {
        Mk64MarkTextureUsed3DS(it->second.texture_id);
        mRapi->SelectTexture(i, it->second.texture_id);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FRAME_HIT_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture frame-use hit point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_RETAIN_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });

    mRapi->SelectTexture(i, texture_id);]=]
            [=[    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif

    mRapi->SelectTexture(i, texture_id);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_RETAIN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner retain point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FRAME_BEGIN_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::Run(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    SpReset();]=]
            [=[void Interpreter::Run(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    Mk64BeginTextureFrame3DS();
    SpReset();]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FRAME_BEGIN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture frame-use begin point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_MISS_TRANSACTION_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    uint32_t texture_id;
    if (!mTextureCache.free_texture_ids.empty()) {
        texture_id = mTextureCache.free_texture_ids.back();
        mTextureCache.free_texture_ids.pop_back();
    } else {
        texture_id = mRapi->NewTexture();
    }

    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    node->second.texture_id = texture_id;
    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif]=]
            [=[    // Allocate both CPU cache nodes before consuming a renderer ID. If
    // ordinary-heap pressure rejects either node, no unreachable GPU slot is
    // left behind to accumulate across recovery attempts.
    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    auto lruLocation3DS = mTextureCache.lru.end();
    uint32_t texture_id = 0;
    try {
        lruLocation3DS = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });
        if (!mTextureCache.free_texture_ids.empty()) {
            texture_id = mTextureCache.free_texture_ids.back();
            mTextureCache.free_texture_ids.pop_back();
        } else {
            texture_id = mRapi->NewTexture();
        }
    } catch (...) {
        if (lruLocation3DS != mTextureCache.lru.end()) {
            mTextureCache.lru.erase(lruLocation3DS);
        }
        mTextureCache.map.erase(it);
        throw;
    }
    node->second.texture_id = texture_id;
    node->second.lru_location = lruLocation3DS;
#if defined(__3DS__)
    Mk64RetainTextureOwners3DS(texture_id, key, mRdp);
    Mk64MarkTextureUsed3DS(texture_id);
#endif]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_MISS_TRANSACTION_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture miss transaction point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_ADDRESS_DELETE_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "            if (it->first.texture_addr == origAddr) {"
            "            if (it->first.texture_addr == origAddr ||\n                it->first.palette_addrs[0] == origAddr ||\n                it->first.palette_addrs[1] == origAddr ||\n                Mk64TextureOwnerReferences3DS(it->second.texture_id, origAddr)) {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_OWNER_ADDRESS_DELETE_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner address delete point changed")
        endif()
        set(SPAGHETTIKART_3DS_PALETTE_OWNER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    const uint8_t* src = mRdp->texture_to_load.addr;
    uint32_t entryCount = high_index + 1;]=]
            [=[    const uint8_t* src = mRdp->texture_to_load.addr;
    const std::shared_ptr<Fast::Texture> paletteOwner3DS =
        mRdp->texture_to_load.raw_tex_metadata.resource;
    const uint8_t* paletteOwnerBase3DS = paletteOwner3DS != nullptr
                                             ? paletteOwner3DS->ImageData
                                             : src;
    if (src == nullptr) {
        mRdp->palette_dram_addr[0] = nullptr;
        mRdp->palette_dram_addr[1] = nullptr;
        sMk64PaletteOwners3DS[0].reset();
        sMk64PaletteOwners3DS[1].reset();
        sMk64PaletteOwnerBases3DS[0] = nullptr;
        sMk64PaletteOwnerBases3DS[1] = nullptr;
        return;
    }
    uint32_t entryCount = high_index + 1;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;]=]
            [=[            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;
            sMk64PaletteOwners3DS[0] = paletteOwner3DS;
            sMk64PaletteOwners3DS[1] = paletteOwner3DS;
            sMk64PaletteOwnerBases3DS[0] = paletteOwnerBase3DS;
            sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "            mRdp->palette_dram_addr[0] = src;\n        } else {"
            "            mRdp->palette_dram_addr[0] = src;\n            sMk64PaletteOwners3DS[0] = paletteOwner3DS;\n            sMk64PaletteOwnerBases3DS[0] = paletteOwnerBase3DS;\n        } else {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "            mRdp->palette_dram_addr[1] = src;\n        }\n    } else {"
            "            mRdp->palette_dram_addr[1] = src;\n            sMk64PaletteOwners3DS[1] = paletteOwner3DS;\n            sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;\n        }\n    } else {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;]=]
            [=[        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;
        sMk64PaletteOwners3DS[1] = paletteOwner3DS;
        sMk64PaletteOwnerBases3DS[1] = paletteOwnerBase3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_PALETTE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D palette-owner insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_OWNER_DELETE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[                mTextureCache.map.erase(it->first);
                again = true;]=]
            [=[                const uint32_t textureId3DS = it->second.texture_id;
                mTextureCache.map.erase(it->first);
#if defined(__3DS__)
                Mk64ReleaseTextureOwner3DS(textureId3DS);
#endif
                again = true;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_OWNER_DELETE_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture-owner delete point changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_DELETE_SCAN_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (mTextureCache.map.bucket_count() > 0) {
        TextureCacheKey key = { origAddr, { 0 }, 0, 0, 0 }; // bucket index only depends on the address
        size_t bucket = mTextureCache.map.bucket(key);
        bool again = false;
        for (auto it = mTextureCache.map.begin(bucket); it != mTextureCache.map.end(bucket); ++it) {]=]
            [=[void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (!mTextureCache.map.empty()) {
        bool again = false;
        // Palette addresses are not part of the unordered-map hash. Explicit
        // source eviction is transition-time work, so scan the bounded cache.
        for (auto it = mTextureCache.map.begin(); it != mTextureCache.map.end(); ++it) {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_DELETE_SCAN_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture source-eviction scan changed")
        endif()
        set(SPAGHETTIKART_3DS_MATRIX_LOOKUP_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    if (auto it = mCurMtxReplacements->find((Mtx*)addr); it != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = it->second.mf[i][j];]=]
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    MtxF replacement3DS = {};
    const bool hasReplacement3DS = Mk64FrameInterpolation3DSLookup != nullptr &&
                                   Mk64FrameInterpolation3DSLookup((Mtx*)addr, &replacement3DS);
    auto replacementIt = mCurMtxReplacements->find((Mtx*)addr);
    if (hasReplacement3DS || replacementIt != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = hasReplacement3DS ? replacement3DS.mf[i][j]
                                            : replacementIt->second.mf[i][j];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_MATRIX_LOOKUP_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D matrix lookup insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_INTERPRETER_INSTANCE_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[static std::weak_ptr<Interpreter> mInstance;
// Set a cached pointer to the instance so we don't need to go through the window every time
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    mInstance = gfx;
}]=]
            [=[// 3DS command dispatch is synchronous and the runtime explicitly clears this
// pointer before releasing its owning shared_ptr. Avoid taking an atomic weak
// reference for every Fast3D command in large race display lists.
static void Mk64ResetTextureOwners3DS();
static Interpreter* mInstance = nullptr;
static float sMk64AspectCorrection3DS = 1.0f;
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    if (gfx == nullptr) {
        Mk64ResetTextureOwners3DS();
    }
    mInstance = gfx.get();
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "mInstance.lock().get()"
            "mInstance"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_INTERPRETER_INSTANCE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D interpreter instance insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_ASPECT_FACTOR_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;]=]
            [=[    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;
    sMk64AspectCorrection3DS = ::mk64_3ds::Fast3DAspectCorrection(
        static_cast<float>(mCurDimensions.width),
        static_cast<float>(mCurDimensions.height));]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        return x * (4.0f / 3.0f) / ((float)mCurDimensions.width / (float)mCurDimensions.height);]=]
            [=[        return x * sMk64AspectCorrection3DS;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_ASPECT_FACTOR_ORIGINAL)
            message(FATAL_ERROR "The Fast3D aspect-factor insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_CURRENT_DIR_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "std::stack<std::string> currentDir;"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "    currentDir = std::stack<std::string>();\n"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void gfx_push_current_dir(char* path) {
    if (gfx_check_image_signature(path) == 1)
        path = &path[7];

    currentDir.push(GetPathWithoutFileName(path));
}]=]
            [=[void gfx_push_current_dir(char* path) {
    // No renderer path consumes this directory on 3DS. Keep the ABI entry
    // point while avoiding per-command string and deque allocations.
    (void)path;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_CURRENT_DIR_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D current-directory insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_DEPTH_READ_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::GetPixelDepthPrepare(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);
    mGetPixelDepthPending.emplace(x, y);
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);

    if (auto it = mGetPixelDepthCached.find(std::make_pair(x, y)); it != mGetPixelDepthCached.end()) {
        return it->second;
    }

    mGetPixelDepthPending.emplace(x, y);

    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res =
        mRapi->GetPixelDepth(mRendersToFb ? mGameFb : 0, mGetPixelDepthPending);
    mGetPixelDepthCached.merge(res);
    mGetPixelDepthPending.clear();

    return mGetPixelDepthCached.find(std::make_pair(x, y))->second;
}]=]
            [=[void Interpreter::GetPixelDepthPrepare(float x, float y) {
    (void)x;
    (void)y;
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    (void)x;
    (void)y;
    return 0;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_DEPTH_READ_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D pixel depth insertion point changed")
        endif()
        set(SPAGHETTIKART_3DS_RESOURCE_NULL_GUARDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        texFlags = tex->Flags;]=]
            [=[            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        texFlags = tex->Flags;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    if (ucode_handler_index == ucode_f3dex2) {
        gfx->GfxSpMovememF3dex2(index, offset,
                                Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash));
    } else {
        auto light =
            (Fast::LightEntry*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        uintptr_t data = (uintptr_t)&light->Ambient;
        gfx->GfxSpMovememF3d(index, offset, (void*)(data + (hasOffset == 1 ? 0x8 : 0)));
    }]=]
            [=[    if (ucode_handler_index == ucode_f3dex2) {
        const void* data =
            Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        if (data != nullptr) {
            gfx->GfxSpMovememF3dex2(index, offset, data);
        }
    } else {
        auto* light =
            (Fast::LightEntry*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        if (light != nullptr) {
            const uintptr_t data = reinterpret_cast<uintptr_t>(&light->Ambient);
            gfx->GfxSpMovememF3d(index, offset,
                                 reinterpret_cast<void*>(data + (hasOffset == 1 ? 0x8 : 0)));
        }
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    F3DVtx* vtx =
        (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);
    vtx += vtxDataOff;

    gfx->GfxSpVertex(vtxCnt, vtxIdxOff, vtx);]=]
            [=[    F3DVtx* vtx =
        (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);
    if (vtx != nullptr) {
        gfx->GfxSpVertex(vtxCnt, vtxIdxOff, vtx + vtxDataOff);
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        } else {
            assert(0 && "???");
            // gfx_path.pop_back();
            // cmd = cmd_stack.top();
            // cmd_stack.pop();
        }]=]
            [=[        }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_RESOURCE_NULL_GUARDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D resource null-guard insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_FAILURE_GUARDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[            if (tex == nullptr) {
                (*cmd0)++;
                return false;
            }]=]
            [=[            if (tex == nullptr || tex->ImageData == nullptr) {
                gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                          imgData, 0, rawTexMetdata, nullptr);
                return false;
            }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    if (texture != nullptr) {
        Interpreter* gfx = mInstance;]=]
            [=[    if (texture != nullptr && texture->ImageData != nullptr) {
        Interpreter* gfx = mInstance;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_FILEPATH: Texture is null");
    }
    return false;
}]=]
            [=[    } else {
        Interpreter* gfx = mInstance;
        gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1,
                                  fileName, 0, RawTexMetadata{}, nullptr);
        SPDLOG_ERROR("G_SETTIMG_OTR_FILEPATH: Texture is null");
    }
    return false;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        Fast::Texture* tex = std::static_pointer_cast<Fast::Texture>(
                                 Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                                     reinterpret_cast<char*>(replacement)))
                                 .get();

        replacement = tex->ImageData;]=]
            [=[        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        replacement = tex->ImageData;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_FAILURE_GUARDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture failure guards changed")
        endif()
        set(SPAGHETTIKART_3DS_BLENDED_TEXTURE_OWNER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    if (gfx_check_image_signature(reinterpret_cast<char*>(replacement))) {
        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (tex == nullptr || tex->ImageData == nullptr) {
            return;
        }
        replacement = tex->ImageData;
    }

    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    mMaskedTextures.erase(name);
}]=]
            [=[void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (name == nullptr) return;
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    std::shared_ptr<Fast::Texture> replacementOwner3DS;
    if (gfx_check_image_signature(reinterpret_cast<char*>(replacement))) {
        replacementOwner3DS = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                reinterpret_cast<char*>(replacement)));
        if (replacementOwner3DS == nullptr || replacementOwner3DS->ImageData == nullptr) {
            return;
        }
        replacement = replacementOwner3DS->ImageData;
    }

    if (auto previous3DS = mMaskedTextures.find(name);
        previous3DS != mMaskedTextures.end()) {
        Mk64UnregisterBlendedTextureOwner3DS(previous3DS->second.replacementData);
    }
    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
    if (!Mk64RegisterBlendedTextureOwner3DS(replacement, replacementOwner3DS)) {
        mMaskedTextures.erase(name);
    }
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    if (name == nullptr) return;
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    if (auto entry3DS = mMaskedTextures.find(name);
        entry3DS != mMaskedTextures.end()) {
        Mk64UnregisterBlendedTextureOwner3DS(entry3DS->second.replacementData);
        mMaskedTextures.erase(entry3DS);
    }
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_BLENDED_TEXTURE_OWNER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D blended-texture owner point changed")
        endif()
        set(SPAGHETTIKART_3DS_HASH_TEXTURE_HANDLER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {
    uintptr_t addr = (*cmd0)->words.w1;
    (*cmd0)++;
    uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (uint64_t)(*cmd0)->words.w1;

    const char* fileName =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    if (fileName == nullptr) {
        (*cmd0)++;
        return false;
    }

    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
            Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash)));
    if (texture != nullptr) {
        texFlags = texture->Flags;
        rawTexMetadata.width = texture->Width;
        rawTexMetadata.height = texture->Height;
        rawTexMetadata.h_byte_scale = texture->HByteScale;
        rawTexMetadata.v_pixel_scale = texture->VPixelScale;
        rawTexMetadata.type = texture->Type;
        rawTexMetadata.resource = texture;

        // OTRTODO: We have disabled caching for now to fix a texture corruption issue with HD texture
        // support. In doing so, there is a potential performance hit since we are not caching lookups. We
        // need to do proper profiling to see whether or not it is worth it to keep the caching system.

        char* tex = reinterpret_cast<char*>(texture->ImageData);

        if (tex != nullptr) {
            (*cmd0)--;
            uintptr_t oldData = (*cmd0)->words.w1;
            // TODO: wtf??
            (*cmd0)->words.w1 = (uintptr_t)tex;

            // if (ourHash != (uint64_t)-1) {
            //     auto res = ResourceLoad(ourHash);
            // }

            (*cmd0)++;
        }

        (*cmd0)--;
        F3DGfx* cmd = (*cmd0);
        uint32_t fmt = C0(21, 3);
        uint32_t size = C0(19, 2);
        uint32_t width = C0(0, 12) + 1;

        if (tex != NULL) {
            Interpreter* gfx = mInstance;
            gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texFlags, rawTexMetadata, tex);
        }
    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_HASH: Texture is null");
    }

    (*cmd0)++;
    return false;
}]=]
            [=[bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance;
    F3DGfx* cmd = *cmd0;
    (*cmd0)++;
    const uint64_t hash =
        (static_cast<uint64_t>((*cmd0)->words.w0) << 32) |
        static_cast<uint64_t>((*cmd0)->words.w1);
    const char* fileName =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
    const uint32_t fmt = C0(21, 3);
    const uint32_t size = C0(19, 2);
    const uint32_t width = C0(0, 12) + 1;
    RawTexMetadata rawTexMetadata = {};

    if (fileName == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, nullptr, 0,
                                  rawTexMetadata, nullptr);
        return false;
    }

    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    if (texture == nullptr || texture->ImageData == nullptr) {
        gfx->GfxDpSetTextureImage(fmt, size, width, fileName, 0,
                                  rawTexMetadata, nullptr);
        SPDLOG_ERROR("G_SETTIMG_OTR_HASH: Texture is null");
        return false;
    }

    rawTexMetadata.width = texture->Width;
    rawTexMetadata.height = texture->Height;
    rawTexMetadata.h_byte_scale = texture->HByteScale;
    rawTexMetadata.v_pixel_scale = texture->VPixelScale;
    rawTexMetadata.type = texture->Type;
    rawTexMetadata.resource = texture;
    gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texture->Flags,
                              rawTexMetadata, texture->ImageData);
    return false;
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_HASH_TEXTURE_HANDLER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D hashed texture handler changed")
        endif()
        set(SPAGHETTIKART_3DS_TEXTURE_VALIDATION_ORDER_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[    if (TextureCacheLookup(i, key)) {
        return;
    }

    // Guard against zero-sized textures that would cause divide-by-zero
    // or GPU API errors in UploadTexture.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 || mRdp->loaded_texture[tmemIdex].size_bytes == 0 ||
        origAddr == nullptr) {
        return;
    }]=]
            [=[    // Never publish a cache key until its source has passed the
    // cheap validation gates. Otherwise an invalid upload becomes a permanent
    // false hit and keeps sampling the previous texture bound to this unit.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 ||
        mRdp->loaded_texture[tmemIdex].size_bytes == 0 || origAddr == nullptr) {
        return;
    }

    const bool directUpload3DS =
        (texFlags & (TEX_FLAG_LOAD_AS_IMG | TEX_FLAG_LOAD_AS_RAW)) != 0;
    const bool supportedFormat3DS = directUpload3DS ||
        (fmt == G_IM_FMT_RGBA && (siz == G_IM_SIZ_16b || siz == G_IM_SIZ_32b)) ||
        (fmt == G_IM_FMT_IA && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b ||
                                siz == G_IM_SIZ_16b)) ||
        (fmt == G_IM_FMT_CI && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b ||
                                siz == G_IM_SIZ_16b || siz == G_IM_SIZ_32b)) ||
        (fmt == G_IM_FMT_I && (siz == G_IM_SIZ_4b || siz == G_IM_SIZ_8b));
    if (!supportedFormat3DS) {
        return;
    }
    if (!directUpload3DS && fmt == G_IM_FMT_CI) {
        if ((siz == G_IM_SIZ_4b && mRdp->palettes[paletteIndex / 8] == nullptr) ||
            (siz == G_IM_SIZ_8b &&
             (mRdp->palettes[0] == nullptr || mRdp->palettes[1] == nullptr))) {
            return;
        }
    }

    if (TextureCacheLookup(i, key)) {
        return;
    }]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_TEXTURE_VALIDATION_ORDER_ORIGINAL)
            message(FATAL_ERROR "The Fast3D texture validation order changed")
        endif()
        set(SPAGHETTIKART_3DS_INTERPRETER_BOUNDS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            [=[void* Interpreter::SegAddr(uintptr_t w1) {
    // Segmented?
    if (w1 & 1) {
        uint32_t segNum = (uint32_t)(w1 >> 24);

        uint32_t offset = w1 & 0x00FFFFFE;

        if (mSegmentPointers[segNum] != 0) {
            return (void*)(mSegmentPointers[segNum] + offset);
        } else {
            return (void*)w1;
        }
    } else {
        return (void*)w1;
    }
}]=]
            [=[void* Interpreter::SegAddr(uintptr_t w1) {
    if ((w1 & 1U) == 0) {
        return reinterpret_cast<void*>(w1);
    }
    const uint32_t segNum = static_cast<uint32_t>(w1 >> 24U);
    if (segNum >= MAX_SEGMENT_POINTERS || mSegmentPointers[segNum] == 0) {
        return nullptr;
    }
    const uint32_t offset = static_cast<uint32_t>(w1) & 0x00FFFFFEU;
    return reinterpret_cast<void*>(mSegmentPointers[segNum] + offset);
}]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {]=]
            [=[void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    if (vertices == nullptr || dest_index >= MAX_VERTICES ||
        n_vertices > MAX_VERTICES - dest_index) {
        return;
    }
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    SUPPORT_CHECK(where == G_MWO_POINT_ST);]=]
            [=[void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    if (vtx_idx >= MAX_VERTICES) return;
    SUPPORT_CHECK(where == G_MWO_POINT_ST);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
            [=[void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    if (vtx1_idx >= MAX_VERTICES || vtx2_idx >= MAX_VERTICES ||
        vtx3_idx >= MAX_VERTICES) {
        return;
    }
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        float dx1 = v1->x / (v1->w) - v2->x / (v2->w);
        float dy1 = v1->y / (v1->w) - v2->y / (v2->w);
        float dx2 = v3->x / (v3->w) - v2->x / (v2->w);
        float dy2 = v3->y / (v3->w) - v2->y / (v2->w);
        float cross = dx1 * dy2 - dy1 * dx2;]=]
            [=[        float cross = ::mk64_3ds::Fast3DProjectedCross(
            v1->x, v1->y, v1->w, v2->x, v2->y, v2->w,
            v3->x, v3->y, v3->w);]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            "    if ((mRsp->geometry_mode & cull_both) != 0) {"
            "    if ((mRsp->geometry_mode & cull_both) != 0 &&\n        ::mk64_3ds::Fast3DCanProject(v1->w, v2->w, v3->w)) {"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];]=]
            [=[void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    if (addr == nullptr) return;
    float matrix[4][4];]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;]=]
            [=[    } else if (subGFX != nullptr) {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_INTERPRETER_BOUNDS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D bounds-guard insertion points changed")
        endif()
        set(SPAGHETTIKART_3DS_DEBUGGER_BYPASS_ORIGINAL
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
        string(REPLACE
            "    auto dbg = mGfxDebugger;\n"
            ""
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        string(REPLACE
            [=[        if (dbg->IsDebugging()) {
            g_exec_stack.gfx_path.push_back(cmd);
            if (dbg->HasBreakPoint(g_exec_stack.gfx_path)) {
                // On a breakpoint with the active framebuffer still set, we need to reset back to prevent
                // soft locking the renderer
                if (mFbActive) {
                    mFbActive = 0;
                    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, 1);
                }

                break;
            }
            g_exec_stack.gfx_path.pop_back();
        }
        gfx_step();]=]
            [=[        gfx_step();]=]
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL
           SPAGHETTIKART_3DS_DEBUGGER_BYPASS_ORIGINAL)
            message(FATAL_ERROR "The Fast3D debugger bypass insertion points changed")
        endif()
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_INTERPRETER_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D interpreter watchdog insertion points changed")
        endif()
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        file(WRITE "${SPAGHETTIKART_3DS_INTERPRETER_SOURCE}" "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")

        add_library(mk64-3ds-fast3d-interpreter STATIC
            platform/3ds/source/gfx_window_manager_3ds.cpp
            third_party/SpaghettiKart/libultraship/src/fast/debug/GfxDebugger.cpp
            "${SPAGHETTIKART_3DS_INTERPRETER_SOURCE}"
        )
        target_include_directories(mk64-3ds-fast3d-interpreter PUBLIC
            "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
            "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/probe_stubs"
            "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
        )
        target_compile_features(mk64-3ds-fast3d-interpreter PUBLIC cxx_std_20)
        # Mario Kart 64 and its O2R archives use Fast3DEX. Keep the
        # interpreter itself on the same GBI so conditional command layouts
        # cannot silently fall back to Fast3DEX2.
        target_compile_definitions(mk64-3ds-fast3d-interpreter PRIVATE F3DEX_GBI=1)
        if(MK64_3DS_ENABLE_O2R_READER)
            target_sources(mk64-3ds-fast3d-interpreter PRIVATE
                platform/3ds/source/resource_manager_3ds.cpp
                third_party/SpaghettiKart/libultraship/src/fast/resource/type/Texture.cpp
            )
            # ResourceManager is embedded in the header-only Context singleton.
            # Propagate this definition so every consumer sees the same layout.
            target_compile_definitions(mk64-3ds-fast3d-interpreter PUBLIC
                MK64_3DS_RESOURCE_MANAGER=1
            )
        endif()
        target_compile_options(mk64-3ds-fast3d-interpreter PRIVATE
            -O3
            -fomit-frame-pointer
            -ffunction-sections
            -fdata-sections
            -fno-strict-aliasing
            -include
            array
            -Wno-unused-parameter
        )
        target_link_libraries(mk64-3ds-fast3d-interpreter PUBLIC
            mk64-3ds-fast3d-renderer
            ctru
            m
        )
        if(MK64_3DS_ENABLE_O2R_READER)
            target_link_libraries(mk64-3ds-fast3d-interpreter PUBLIC
                mk64-3ds-o2r-reader
            )
        endif()

        add_executable(mk64-3ds-interpreter-probe
            platform/3ds/source/interpreter_probe.cpp
        )
        target_compile_features(mk64-3ds-interpreter-probe PRIVATE cxx_std_20)
        target_compile_definitions(mk64-3ds-interpreter-probe PRIVATE F3DEX_GBI=1)
        target_compile_options(mk64-3ds-interpreter-probe PRIVATE
            -O3
            -fomit-frame-pointer
            -ffunction-sections
            -fdata-sections
            -Wno-unused-parameter
        )
        target_link_options(mk64-3ds-interpreter-probe PRIVATE
            -Wl,--gc-sections
            -Wl,--allow-multiple-definition
        )
        target_link_libraries(mk64-3ds-interpreter-probe PRIVATE
            mk64-3ds-fast3d-interpreter
            mk64-3ds-platform
        )
        if(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR)
            # The compact O2R reader uses miniz from Torch's ZWrapper. The
            # complete game already receives this through game-engine; the
            # standalone interpreter probe must request it directly.
            target_link_libraries(mk64-3ds-interpreter-probe PRIVATE
                "$<TARGET_FILE:mk64-3ds-o2r-reader>"
                "$<TARGET_FILE:torch>"
            )
        endif()

        ctr_create_3dsx(mk64-3ds-interpreter-probe-3dsx
            TARGET mk64-3ds-interpreter-probe
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds-interpreter-probe.3dsx"
            SMDH "${MK64_3DS_SMDH}"
        )
    endif()
endif()

ctr_create_3dsx(mk64-3ds-3dsx
    TARGET mk64-3ds
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds-v${MK64_3DS_VERSION}.3dsx"
    SMDH "${MK64_3DS_SMDH}"
)

if(MK64_3DS_BUILD_GAME_CORE AND MK64_3DS_BUILD_GAME_ENGINE AND
   MK64_3DS_ENABLE_O2R_READER AND MK64_3DS_ENABLE_FAST3D_RENDERER AND
   MK64_3DS_BUILD_INTERPRETER_PROBE)
    add_library(mk64-3ds-game-runtime STATIC
        platform/3ds/source/game_runtime_3ds.cpp
    )
    target_include_directories(mk64-3ds-game-runtime PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/probe_stubs"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
    )
    target_compile_features(mk64-3ds-game-runtime PRIVATE cxx_std_20)
    target_compile_definitions(mk64-3ds-game-runtime PRIVATE F3DEX_GBI=1)
    target_compile_options(mk64-3ds-game-runtime PRIVATE
        -O3
        -ffast-math
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -include
        array
        -Wno-unused-parameter
    )
    target_link_libraries(mk64-3ds-game-runtime PUBLIC
        mk64-3ds-fast3d-interpreter
    )

    add_executable(mk64-3ds-game
        platform/3ds/source/bottom_ui_3ds.cpp
        platform/3ds/source/diagnostics_3ds.cpp
        platform/3ds/source/game_main_3ds.cpp
    )
    target_include_directories(mk64-3ds-game PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/include"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/include/assets"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/src"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
        "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include/libultraship"
    )
    target_compile_features(mk64-3ds-game PRIVATE cxx_std_20)
    target_compile_definitions(mk64-3ds-game PRIVATE
        NDEBUG
        VERSION_US=1
        F3DEX_GBI=1
        PLATFORM_3DS=1
        NON_MATCHING=1
        MK64_3DS_VERSION="${MK64_3DS_VERSION}"
    )
    target_compile_options(mk64-3ds-game PRIVATE
        -O3
        -ffast-math
        -fomit-frame-pointer
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -fpermissive
        -include
        engine_3ds_prelude.h
    )
    target_link_options(mk64-3ds-game PRIVATE -Wl,--gc-sections -Wl,--allow-multiple-definition)
    target_link_libraries(mk64-3ds-game PRIVATE
        mk64-3ds-game-runtime
        -Wl,--start-group
        mk64-3ds-game-core
        mk64-3ds-game-engine
        mk64-3ds-platform
        mk64-3ds-o2r-reader
        -Wl,--end-group
        nlohmann_json::nlohmann_json
        citro2d
        citro3d
        ctru
        m
    )

    ctr_create_3dsx(mk64-3ds-game-3dsx
        TARGET mk64-3ds-game
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds-game-v${MK64_3DS_VERSION}.3dsx"
        SMDH "${MK64_3DS_SMDH}"
        ROMFS "${MK64_3DS_ROMFS_DIR}"
    )
endif()
