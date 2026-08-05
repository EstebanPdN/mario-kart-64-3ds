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

add_library(mk64-3ds-platform STATIC
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
target_compile_options(mk64-3ds-platform PRIVATE
    -O3
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
    # translation unit with a bounded 16 MiB arena while leaving upstream clean.
    set(SPAGHETTIKART_MEMORY_SOURCE "${SPAGHETTIKART_ROOT}/src/racing/memory.c")
    set(SPAGHETTIKART_3DS_MEMORY_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/memory_3ds.c")
    list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${SPAGHETTIKART_MEMORY_SOURCE}")
    file(READ "${SPAGHETTIKART_MEMORY_SOURCE}" SPAGHETTIKART_MEMORY_TEXT)
    string(REPLACE
        "static u8 sMemoryPool[0x10000000] ALIGNED4096;"
        "static u8 sMemoryPool[0x01000000] ALIGNED4096;"
        SPAGHETTIKART_3DS_MEMORY_TEXT
        "${SPAGHETTIKART_MEMORY_TEXT}"
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
        SPAGHETTI_VERSION="3DS-dev"
    )
    target_compile_options(mk64-3ds-game-core PRIVATE
        -O3
        -ffast-math
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -Wno-incompatible-pointer-types
        -Wno-error=int-conversion
        -Wno-unused-parameter
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
        platform/3ds/source/game_state_3ds.cpp
        platform/3ds/source/gbi_middleware_3ds.cpp
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
    )
    target_compile_options(mk64-3ds-game-engine PRIVATE
        -O3
        -ffast-math
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -fpermissive
        -include
        engine_3ds_prelude.h
        -Wno-unused-parameter
    )
    target_link_libraries(mk64-3ds-game-engine PRIVATE nlohmann_json::nlohmann_json)
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
    target_compile_options(mk64-3ds-o2r-reader PRIVATE
        -O3
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
        add_library(mk64-3ds-fast3d-interpreter STATIC
            platform/3ds/source/gfx_window_manager_3ds.cpp
            third_party/SpaghettiKart/libultraship/src/fast/debug/GfxDebugger.cpp
            third_party/SpaghettiKart/libultraship/src/fast/interpreter.cpp
        )
        target_include_directories(mk64-3ds-fast3d-interpreter PUBLIC
            "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/include"
            "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/probe_stubs"
            "${CMAKE_CURRENT_LIST_DIR}/third_party/SpaghettiKart/libultraship/include"
        )
        target_compile_features(mk64-3ds-fast3d-interpreter PUBLIC cxx_std_20)
        target_compile_definitions(mk64-3ds-fast3d-interpreter PRIVATE F3DEX_GBI_2)
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
        target_compile_definitions(mk64-3ds-interpreter-probe PRIVATE F3DEX_GBI_2)
        target_compile_options(mk64-3ds-interpreter-probe PRIVATE
            -O3
            -ffunction-sections
            -fdata-sections
            -Wno-unused-parameter
        )
        target_link_options(mk64-3ds-interpreter-probe PRIVATE -Wl,--gc-sections)
        target_link_libraries(mk64-3ds-interpreter-probe PRIVATE
            mk64-3ds-fast3d-interpreter
            mk64-3ds-platform
        )

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
    target_compile_definitions(mk64-3ds-game-runtime PRIVATE F3DEX_GBI_2)
    target_compile_options(mk64-3ds-game-runtime PRIVATE
        -O3
        -ffast-math
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
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -fpermissive
        -include
        engine_3ds_prelude.h
    )
    target_link_options(mk64-3ds-game PRIVATE -Wl,--gc-sections)
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
    )
endif()
