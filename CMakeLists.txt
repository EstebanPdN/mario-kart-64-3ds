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
    set(SPAGHETTIKART_AUDIO_PORT_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/port_eu.c")
    set(SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/port_eu_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_AUDIO_PORT_SOURCE}")
    set(SPAGHETTIKART_SEQPLAYER_SOURCE "${SPAGHETTIKART_ROOT}/src/audio/seqplayer.c")
    set(SPAGHETTIKART_3DS_SEQPLAYER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/seqplayer_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_SEQPLAYER_SOURCE}")
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
        "#include <stdio.h>\n#include <stdlib.h>\nextern void Mk64Diagnostics3DSSetGameArena(const void*, size_t) __attribute__((weak));"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_3DS_MEMORY_TEXT}"
    )
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
        "#include <stdio.h>\n#include <stddef.h>\n#if defined(PLATFORM_3DS)\n#define MK64_3DS_MAX_COLLISION_DLS 2048\n#define MK64_3DS_MAX_COLLISION_COMMANDS 8192\n#define MK64_3DS_MAX_COLLISION_TRIANGLES 8192\n#define MK64_3DS_MAX_COLLISION_INDICES 32768\n#endif"
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
static u32 sCollisionVisitCount3DS = 0;

void mk64_3ds_collision_reset_visits(void) {
    sCollisionVisitCount3DS = 0;
}

static bool mk64_3ds_collision_mark_visited(uintptr_t addr, s8 surfaceType, u16 sectionId) {
    for (u32 i = 0; i < sCollisionVisitCount3DS; i++) {
        if ((sCollisionVisits3DS[i].addr == addr) &&
            (sCollisionVisits3DS[i].surfaceType == surfaceType) &&
            (sCollisionVisits3DS[i].sectionId == sectionId)) {
            return false;
        }
    }
    if (sCollisionVisitCount3DS >= MK64_3DS_MAX_COLLISION_DLS) {
        return false;
    }
    sCollisionVisits3DS[sCollisionVisitCount3DS].addr = addr;
    sCollisionVisits3DS[sCollisionVisitCount3DS].surfaceType = surfaceType;
    sCollisionVisits3DS[sCollisionVisitCount3DS].sectionId = sectionId;
    sCollisionVisitCount3DS++;
    return true;
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
        "${SPAGHETTIKART_3DS_MEMORY_SOURCE}"
        "${SPAGHETTIKART_3DS_COLLISION_SOURCE}"
        "${SPAGHETTIKART_3DS_AUDIO_PORT_SOURCE}"
        "${SPAGHETTIKART_3DS_SEQPLAYER_SOURCE}"
    )
    target_include_directories(mk64-3ds-game-core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/compat"
        "${SPAGHETTIKART_ROOT}"
        "${SPAGHETTIKART_ROOT}/include"
        "${SPAGHETTIKART_ROOT}/include/assets"
        "${SPAGHETTIKART_ROOT}/src"
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
    file(GLOB SPAGHETTIKART_ENGINE_VEHICLES CONFIGURE_DEPENDS
        "${SPAGHETTIKART_ROOT}/src/engine/vehicles/*.cpp"
    )
    set(SPAGHETTIKART_ENGINE_REGISTRY
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItems.cpp"
        "${SPAGHETTIKART_ROOT}/src/engine/registry/RegisterItemTables.cpp"
    )
    set(SPAGHETTIKART_GAME_BRIDGE_SOURCE "${SPAGHETTIKART_ROOT}/src/port/Game.cpp")
    set_source_files_properties("${SPAGHETTIKART_GAME_BRIDGE_SOURCE}" PROPERTIES
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
        platform/3ds/source/torch_o2r_3ds.cpp
        platform/3ds/source/vanilla_stubs_3ds.cpp
        "${SPAGHETTIKART_ROOT}/src/engine/editor/GameObject.cpp"
        ${SPAGHETTIKART_GAME_BRIDGE_SOURCE}
        ${SPAGHETTIKART_ENGINE_ROOT}
        ${SPAGHETTIKART_ENGINE_ACTORS}
        ${SPAGHETTIKART_ENGINE_CAMERAS}
        ${SPAGHETTIKART_ENGINE_OBJECTS}
        ${SPAGHETTIKART_ENGINE_PARTICLES}
        ${SPAGHETTIKART_ENGINE_SKY}
        ${SPAGHETTIKART_ENGINE_TRACKS}
        "${SPAGHETTIKART_3DS_TRACK_SOURCE}"
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
        string(REPLACE
            "    while (!g_exec_stack.cmd_stack.empty()) {\n        auto cmd = g_exec_stack.cmd_stack.top();"
            "    size_t commandCount = 0;\n    constexpr size_t kMaxCommandsPerFrame = 250000;\n    constexpr size_t kMaxDisplayListDepth = 512;\n    while (!g_exec_stack.cmd_stack.empty()) {\n        auto cmd = g_exec_stack.cmd_stack.top();\n        if (++commandCount > kMaxCommandsPerFrame || g_exec_stack.cmd_stack.size() > kMaxDisplayListDepth) {\n            if (Mk64Diagnostics3DSGfxWatchdog != nullptr && cmd != nullptr) {\n                Mk64Diagnostics3DSGfxWatchdog(cmd, cmd->words.w0, cmd->words.w1, commandCount);\n            }\n            g_exec_stack.stop();\n            break;\n        }"
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
    // A segmented address is unresolved only when SegAddr returns the original
    // tagged command word unchanged.
    if ((cmd->words.w1 & 1U) != 0 && i == static_cast<uintptr_t>(cmd->words.w1)) {
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
        string(REPLACE
            "#define TEXTURE_CACHE_MAX_SIZE 1024"
            "#define TEXTURE_CACHE_MAX_SIZE 96"
            SPAGHETTIKART_3DS_INTERPRETER_TEXT
            "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}"
        )
        if(SPAGHETTIKART_3DS_INTERPRETER_TEXT STREQUAL SPAGHETTIKART_3DS_TEXTURE_CACHE_ORIGINAL)
            message(FATAL_ERROR "The upstream Fast3D texture cache limit insertion point changed")
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
