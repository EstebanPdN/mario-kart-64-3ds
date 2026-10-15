cmake_minimum_required(VERSION 3.20)
set(MK64_3DS_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}")

file(STRINGS "${MK64_3DS_SOURCE_ROOT}/platform/3ds/version.txt" MK64_3DS_VERSION LIMIT_COUNT 1)
project(mk64_3ds LANGUAGES C CXX)

# Keep local checkout paths out of diagnostics embedded in distributable 3DSX
# files and make builds reproducible across maintainer machines.
add_compile_options("-ffile-prefix-map=${MK64_3DS_SOURCE_ROOT}/=")
add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}/=build/")
foreach(MK64_3DS_FETCH_DEP NLOHMANN_JSON SPDLOG TINYXML2 YAML-CPP)
    if(FETCHCONTENT_SOURCE_DIR_${MK64_3DS_FETCH_DEP})
        string(TOLOWER "${MK64_3DS_FETCH_DEP}" MK64_3DS_FETCH_DEP_NAME)
        add_compile_options("-ffile-prefix-map=${FETCHCONTENT_SOURCE_DIR_${MK64_3DS_FETCH_DEP}}/=third_party/${MK64_3DS_FETCH_DEP_NAME}/")
    endif()
endforeach()

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
file(COPY "${MK64_3DS_SOURCE_ROOT}/platform/3ds/romfs/"
    DESTINATION "${MK64_3DS_ROMFS_DIR}")
file(COPY
    "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/config.yml"
    DESTINATION "${MK64_3DS_TORCH_ROMFS_DIR}"
)
file(COPY
    "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/yamls"
    "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/meta"
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
    "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
    "${MK64_3DS_SOURCE_ROOT}/platform/3ds/compat"
    "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
    "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include/libultraship"
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

include(platform/3ds/cmake/game_core_3ds.cmake)

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

include(platform/3ds/cmake/game_engine_3ds.cmake)

set(MK64_3DS_SMDH "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds.smdh")
ctr_generate_smdh(
    OUTPUT "${MK64_3DS_SMDH}"
    NAME "MK64 3DS Port"
    DESCRIPTION "Mario Kart 64"
    AUTHOR "EstebanPdN"
    ICON "${MK64_3DS_SOURCE_ROOT}/platform/3ds/assets/icon-48.png"
)

add_executable(mk64-3ds
    platform/3ds/source/main.c
)

target_include_directories(mk64-3ds PRIVATE
    "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
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
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
    )
    target_include_directories(mk64-3ds-o2r-reader PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/compat"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/include"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/include/assets"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/src"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include/libultraship"
    )
    target_include_directories(mk64-3ds-o2r-reader SYSTEM PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/torch/lib"
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

include(platform/3ds/cmake/renderer_3ds.cmake)

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
        platform/3ds/source/performance_trace_3ds.cpp
    )
    target_include_directories(mk64-3ds-game-runtime PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/probe_stubs"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
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

    set(MK64_3DS_UPDATE_DEPS "" CACHE PATH "Updater static library prefix")
    add_library(mk64-3ds-updater STATIC
        platform/3ds/update/updater.c
        platform/3ds/update/update_manifest.c)
    target_include_directories(mk64-3ds-updater PUBLIC platform/3ds/update
        PRIVATE "${MK64_3DS_UPDATE_DEPS}/include")
    target_compile_definitions(mk64-3ds-updater PRIVATE MK64_3DS_VERSION="${MK64_3DS_VERSION}")
    foreach(lib curl jansson mbedtls mbedx509 mbedcrypto)
        if(NOT EXISTS "${MK64_3DS_UPDATE_DEPS}/lib/lib${lib}.a")
            message(FATAL_ERROR "Missing updater library ${lib}; run platform/3ds/build_update_deps.py")
        endif()
        target_link_libraries(mk64-3ds-updater PUBLIC "${MK64_3DS_UPDATE_DEPS}/lib/lib${lib}.a")
    endforeach()
    target_link_libraries(mk64-3ds-updater PUBLIC ctru m)

    add_executable(mk64-3ds-game
        platform/3ds/source/bottom_ui_3ds.cpp
        platform/3ds/source/race_clock_3ds.cpp
        platform/3ds/source/diagnostics_3ds.cpp
        platform/3ds/source/game_main_3ds.cpp
        platform/3ds/source/heap_partition_3ds.cpp
        platform/3ds/source/startup_trace_3ds.cpp
        platform/3ds/source/loading_screen_3ds.cpp
        platform/3ds/source/linear_allocation_capture_3ds.cpp
        platform/3ds/source/ndsp_thread_stack_3ds.cpp
    )
    # This one entrypoint runs before normal C/C++ initialization. Keep even
    # optimized 64-bit zero stores off VFP; no pre-init FP state is assumed.
    set_source_files_properties(platform/3ds/source/heap_partition_3ds.cpp
        PROPERTIES COMPILE_OPTIONS "-mgeneral-regs-only")
    target_include_directories(mk64-3ds-game PRIVATE
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/compat"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/include"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/include/assets"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/src"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include/libultraship"
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
    target_link_options(mk64-3ds-game PRIVATE
        -Wl,--gc-sections
        -Wl,--allow-multiple-definition
        -Wl,--wrap=linearAlloc
        -Wl,--wrap=threadCreate
        -Wl,--wrap=__system_allocateHeaps
    )
    target_link_libraries(mk64-3ds-game PRIVATE
        mk64-3ds-updater
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
