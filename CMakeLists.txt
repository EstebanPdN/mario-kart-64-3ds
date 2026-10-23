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
option(MK64_3DS_AUTODUMP_LAB "Owner-only six-race full-memory diagnostic session" OFF)
set(MK64_3DS_COURSE_PRELOADS "" CACHE PATH "Optional local validated course dependency manifests")
if(MK64_3DS_AUTODUMP_LAB)
    add_compile_definitions(MK64_3DS_AUTODUMP_LAB=1)
    set_source_files_properties(platform/3ds/source/autodump_lab_3ds.cpp
        PROPERTIES COMPILE_DEFINITIONS "MK64_3DS_VERSION=\"${MK64_3DS_VERSION}\"")
endif()
option(MK64_3DS_NATIVE_KART_LAB "Development-only native kart packet integration" OFF)
set(MK64_3DS_NATIVE_KART_PACK "" CACHE FILEPATH "Private preconverted native kart resources")
set(MK64_3DS_NATIVE_WHEEL_PALETTES "" CACHE FILEPATH "Private exact Original wheel palette pool")
set(MK64_3DS_NATIVE_ORIGINAL_TILES "" CACHE FILEPATH "Private GPU-native Original sprite tile dictionary")
set(MK64_3DS_NATIVE_SHADOW_IMAGE "" CACHE FILEPATH "Private pre-tiled native ground-shadow image")
set(MK64_3DS_NATIVE_CREDITS_SHADOW_IMAGE "" CACHE FILEPATH "Private pre-tiled 64x32 shadow crop")
option(MK64_3DS_NATIVE_CHARACTER_SCENARIOS "Development-only forced character/Original rendering fixture" OFF)
option(MK64_3DS_NATIVE_PROJECTILE_LAB "Development-only native projectiles and fake boxes" OFF)
option(MK64_3DS_NATIVE_CACHE_LAB "Development-only matched CPU-to-GPU cache-transfer probe" OFF)
option(MK64_3DS_NATIVE_COMPARE_LAB "Development-only matched native/interpreted static-course replay" OFF)
option(MK64_3DS_NATIVE_BATCH_COMPARE_LAB "Development-only matched original/adjacent native draw plans" OFF)
option(MK64_3DS_NATIVE_PARTICLE_LAB "Development-only typed native player particles" OFF)
option(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB "Development-only fixed-frame particle pipeline comparison" OFF)
set(MK64_3DS_NATIVE_PARTICLE_ASSETS "" CACHE PATH "Private pre-tiled particle images and native quads")
option(MK64_3DS_NATIVE_ITEM_BOX_LAB "Development-only native item-box family" OFF)
option(MK64_3DS_NATIVE_VEGETATION_LAB "Development-only native vegetation family" OFF)
set(MK64_3DS_NATIVE_VEGETATION_ASSETS "" CACHE PATH "Private preconverted vegetation resources")
option(MK64_3DS_NATIVE_LUIGI_LAB "Development-only native Luigi race integration" OFF)
option(MK64_3DS_NATIVE_PLAYTEST "Manual local native Luigi playtest with bundled resources" OFF)
set(MK64_3DS_NATIVE_LUIGI_ASSETS "" CACHE PATH "Private offline-converted Luigi resources")
option(MK64_3DS_NATIVE_FULL_GAME "Enable prepared native families in normal game flow" OFF)
option(MK64_3DS_NATIVE_DYNAMIC "Record native typed dynamic producers instead of graphics opcodes" ${MK64_3DS_NATIVE_FULL_GAME})
set(MK64_3DS_NATIVE_COURSES "" CACHE PATH "Private prepared stock-course working sets")
set(MK64_3DS_NATIVE_PROGRAMS "" CACHE PATH "Private ahead-of-time native resource procedures")
set(MK64_3DS_NATIVE_DEPENDENCIES "" CACHE PATH "Private per-course preload manifests")
if(MK64_3DS_NATIVE_FULL_GAME)
    if(NOT EXISTS "${MK64_3DS_NATIVE_COURSES}/luigi_raceway/scene.nlc")
        message(FATAL_ERROR "Full-game integration requires the private course working sets")
    endif()
    if(NOT MK64_3DS_NATIVE_PLAYTEST)
        message(FATAL_ERROR "Full-game candidate requires the bundled-resource candidate configuration")
    endif()
    if(NOT EXISTS "${MK64_3DS_NATIVE_PROGRAMS}/native_programs.generated.hpp")
        message(FATAL_ERROR "Full-game integration requires ahead-of-time native procedures")
    endif()
    foreach(course banshee_boardwalk big_donut block_fort bowsers_castle choco_mountain
            dks_jungle_parkway double_deck frappe_snowland kalimari_desert koopa_troopa_beach
            luigi_raceway mario_raceway moo_moo_farm rainbow_road royal_raceway sherbet_land
            skyscraper toads_turnpike wario_stadium yoshi_valley)
        if(NOT EXISTS "${MK64_3DS_NATIVE_DEPENDENCIES}/${course}/resources.npr")
            message(FATAL_ERROR "Full-game integration requires the preload manifest for ${course}")
        endif()
    endforeach()
    add_compile_definitions(MK64_3DS_NATIVE_FULL_GAME=1)
endif()
if(MK64_3DS_NATIVE_LUIGI_LAB)
    if(NOT EXISTS "${MK64_3DS_NATIVE_LUIGI_ASSETS}/exit.ncx" OR
       NOT EXISTS "${MK64_3DS_NATIVE_LUIGI_ASSETS}/mutable.nmd")
        message(FATAL_ERROR "Native Luigi lab requires private converted resources")
    endif()
    set(MK64_3DS_VERSION "2.0-N4-local")
    if(MK64_3DS_NATIVE_KART_LAB)
        if(NOT EXISTS "${MK64_3DS_NATIVE_KART_PACK}" OR NOT EXISTS "${MK64_3DS_NATIVE_SHADOW_IMAGE}" OR
           NOT EXISTS "${MK64_3DS_NATIVE_CREDITS_SHADOW_IMAGE}")
            message(FATAL_ERROR "Native kart lab requires its private preconverted kart/shadow resources")
        endif()
        add_compile_definitions(MK64_3DS_NATIVE_KART_LAB=1)
        set(MK64_3DS_VERSION "2.0-N7b-local")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_LUIGI_LAB=1)
endif()
if(MK64_3DS_NATIVE_CHARACTER_SCENARIOS)
    if(NOT MK64_3DS_NATIVE_KART_LAB)
        message(FATAL_ERROR "Character scenarios require the native kart development build")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_CHARACTER_SCENARIOS=1)
endif()
if(MK64_3DS_NATIVE_VEGETATION_LAB)
    if(NOT MK64_3DS_NATIVE_KART_LAB OR NOT EXISTS "${MK64_3DS_NATIVE_VEGETATION_ASSETS}/extra.nvx")
        message(FATAL_ERROR "Native vegetation requires the native race and private converted resources")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_VEGETATION_LAB=1)
    set(MK64_3DS_VERSION "2.0-N8-local")
endif()

if(MK64_3DS_NATIVE_ITEM_BOX_LAB)
    if(NOT MK64_3DS_NATIVE_VEGETATION_LAB)
        message(FATAL_ERROR "Native item boxes require the ordered native object integration")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_ITEM_BOX_LAB=1)
    set(MK64_3DS_VERSION "2.0-N9b-local")
endif()

if(MK64_3DS_NATIVE_PROJECTILE_LAB)
    if(NOT MK64_3DS_NATIVE_ITEM_BOX_LAB OR NOT EXISTS "${MK64_3DS_NATIVE_VEGETATION_ASSETS}/variants.ntr")
        message(FATAL_ERROR "Native projectiles require the object integration and private variant map")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_PROJECTILE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N10b-local")
endif()
if(MK64_3DS_NATIVE_CACHE_LAB)
    if(NOT MK64_3DS_NATIVE_PROJECTILE_LAB)
        message(FATAL_ERROR "Native cache probe requires the prepared native projectile integration")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_CACHE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N15-local")
endif()
if(MK64_3DS_NATIVE_COMPARE_LAB)
    if(NOT MK64_3DS_NATIVE_CACHE_LAB)
        message(FATAL_ERROR "Native comparison requires the prepared cache/projectile experiment")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_COMPARE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N15-compare-local")
endif()
if(MK64_3DS_NATIVE_BATCH_COMPARE_LAB)
    if(NOT MK64_3DS_NATIVE_COMPARE_LAB OR NOT EXISTS "${MK64_3DS_NATIVE_LUIGI_ASSETS}/batch.nbp")
        message(FATAL_ERROR "Native batch comparison requires both native plans and the prepared replay")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_BATCH_COMPARE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N16-compare-local")
endif()
if(MK64_3DS_NATIVE_PARTICLE_LAB)
    if(NOT MK64_3DS_NATIVE_PROJECTILE_LAB OR NOT EXISTS "${MK64_3DS_NATIVE_PARTICLE_ASSETS}/index.npf")
        message(FATAL_ERROR "Native particles require the ordered native object integration and private resources")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_PARTICLE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N17-local")
endif()
if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
    if(NOT MK64_3DS_NATIVE_PARTICLE_LAB OR NOT MK64_3DS_NATIVE_BATCH_COMPARE_LAB)
        message(FATAL_ERROR "Particle comparison requires typed particles and a fixed optimized native course plan")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB=1)
    set(MK64_3DS_VERSION "2.0-N21-raster-local")
endif()
if(MK64_3DS_NATIVE_PLAYTEST)
    if(NOT MK64_3DS_NATIVE_PARTICLE_LAB OR MK64_3DS_NATIVE_COMPARE_LAB OR
       MK64_3DS_NATIVE_CACHE_LAB OR MK64_3DS_NATIVE_CHARACTER_SCENARIOS)
        message(FATAL_ERROR "Native playtest requires the native particle family without comparison/cache/forced-character fixtures")
    endif()
    add_compile_definitions(MK64_3DS_NATIVE_PLAYTEST=1)
    set(MK64_3DS_VERSION "2.0-N28b-playtest")
    if(MK64_3DS_NATIVE_FULL_GAME)
        set(MK64_3DS_VERSION "2.0-N39-resident-fast-pages")
    endif()
endif()

option(MK64_3DS_ENABLE_ON_DEVICE_EXTRACTOR "Build Torch so the 3DS can generate mk64.o2r from a legal ROM" ON)

set(MK64_3DS_ROMFS_DIR "${CMAKE_CURRENT_BINARY_DIR}/romfs")
set(MK64_3DS_TORCH_ROMFS_DIR "${MK64_3DS_ROMFS_DIR}/torch")
if(MK64_3DS_NATIVE_PARTICLE_LAB)
    file(COPY "${MK64_3DS_NATIVE_PARTICLE_ASSETS}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/native-particles")
else()
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/native-particles")
endif()
if(MK64_3DS_NATIVE_VEGETATION_LAB)
    file(COPY "${MK64_3DS_NATIVE_VEGETATION_ASSETS}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/native-vegetation")
else()
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/native-vegetation")
endif()
file(MAKE_DIRECTORY "${MK64_3DS_TORCH_ROMFS_DIR}")
if(MK64_3DS_NATIVE_KART_LAB)
    if(NOT MK64_3DS_NATIVE_LUIGI_LAB)
        message(FATAL_ERROR "Native kart lab requires the Luigi native development build")
    endif()
    configure_file("${MK64_3DS_NATIVE_KART_PACK}" "${MK64_3DS_ROMFS_DIR}/native-karts.nsp" COPYONLY)
    if(MK64_3DS_NATIVE_ORIGINAL_TILES)
        configure_file("${MK64_3DS_NATIVE_ORIGINAL_TILES}" "${MK64_3DS_ROMFS_DIR}/native-karts.nsp.original.ntl" COPYONLY)
    else()
        file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-karts.nsp.original.ntl")
    endif()
    if(MK64_3DS_NATIVE_WHEEL_PALETTES)
        configure_file("${MK64_3DS_NATIVE_WHEEL_PALETTES}" "${MK64_3DS_ROMFS_DIR}/native-wheel-palettes.nwp" COPYONLY)
    else()
        file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-wheel-palettes.nwp")
    endif()
    configure_file("${MK64_3DS_NATIVE_SHADOW_IMAGE}" "${MK64_3DS_ROMFS_DIR}/native-shadow.nsi" COPYONLY)
    configure_file("${MK64_3DS_NATIVE_CREDITS_SHADOW_IMAGE}" "${MK64_3DS_ROMFS_DIR}/native-credits-shadow.nsi" COPYONLY)
else()
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-wheel-palettes.nwp")
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-karts.nsp")
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-karts.nsp.original.ntl")
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-shadow.nsi")
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/native-credits-shadow.nsi")
endif()
if(MK64_3DS_NATIVE_LUIGI_LAB)
    file(COPY "${MK64_3DS_NATIVE_LUIGI_ASSETS}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/native-luigi")
else()
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/native-luigi")
endif()
if(MK64_3DS_NATIVE_FULL_GAME)
    file(COPY "${MK64_3DS_NATIVE_COURSES}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/native-courses")
    if(MK64_3DS_NATIVE_DEPENDENCIES)
        file(COPY "${MK64_3DS_NATIVE_DEPENDENCIES}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/native-program-dependencies")
    endif()
else()
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/native-courses")
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/native-program-dependencies")
endif()
file(COPY "${MK64_3DS_SOURCE_ROOT}/platform/3ds/romfs/"
    DESTINATION "${MK64_3DS_ROMFS_DIR}")
if(MK64_3DS_COURSE_PRELOADS)
    file(COPY "${MK64_3DS_COURSE_PRELOADS}/" DESTINATION "${MK64_3DS_ROMFS_DIR}/course-preloads")
else()
    file(REMOVE_RECURSE "${MK64_3DS_ROMFS_DIR}/course-preloads")
endif()
set(MK64_3DS_IMPROVED_TEXTURE_PACK "" CACHE FILEPATH "Optional local native-size texture bundle; never part of public source")
set(MK64_3DS_LAB_TEXTURE_COMPANION "" CACHE FILEPATH "Legacy local shared texture payload option")
set(MK64_3DS_TEXTURE_COMPANION "${MK64_3DS_LAB_TEXTURE_COMPANION}" CACHE FILEPATH "Optional local shared artwork payload; never part of public source")
if(MK64_3DS_AUTODUMP_LAB)
    if(NOT EXISTS "${MK64_3DS_IMPROVED_TEXTURE_PACK}" OR NOT EXISTS "${MK64_3DS_TEXTURE_COMPANION}")
        message(FATAL_ERROR "The diagnostic lab requires both approved artwork bundles")
    endif()
endif()
if(MK64_3DS_TEXTURE_COMPANION)
    configure_file("${MK64_3DS_TEXTURE_COMPANION}" "${MK64_3DS_ROMFS_DIR}/native-karts.nsp" COPYONLY)
endif()
if(MK64_3DS_IMPROVED_TEXTURE_PACK)
    configure_file("${MK64_3DS_IMPROVED_TEXTURE_PACK}" "${MK64_3DS_ROMFS_DIR}/improved-textures.bin" COPYONLY)
else()
    file(REMOVE "${MK64_3DS_ROMFS_DIR}/improved-textures.bin")
endif()
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
    if(MK64_3DS_NATIVE_LUIGI_LAB)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_course_lab_3ds.cpp)
        target_sources(mk64-3ds-game-engine PRIVATE platform/3ds/source/native_luigi_boot_3ds.cpp)
    endif()
    if(MK64_3DS_NATIVE_FULL_GAME)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_program_3ds.cpp)
        set_source_files_properties(platform/3ds/source/native_program_3ds.cpp
            PROPERTIES COMPILE_OPTIONS "-Os;-fno-unroll-loops")
        target_include_directories(mk64-3ds-game-runtime PRIVATE "${MK64_3DS_NATIVE_PROGRAMS}")
    endif()
    if(MK64_3DS_NATIVE_KART_LAB)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_kart_lab_3ds.cpp)
        # Model-range checks must not be optimized away by runtime fast-math.
        set_source_files_properties(platform/3ds/source/native_kart_lab_3ds.cpp
            PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
    endif()
    if(MK64_3DS_NATIVE_CACHE_LAB)
        target_sources(mk64-3ds-game-engine PRIVATE platform/3ds/source/native_cache_probe_3ds.cpp)
    endif()
    if(MK64_3DS_NATIVE_COMPARE_LAB)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_comparison_lab_3ds.cpp)
        set_source_files_properties(platform/3ds/source/native_comparison_lab_3ds.cpp
            PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
    endif()
    if(MK64_3DS_NATIVE_VEGETATION_LAB)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_vegetation_lab_3ds.cpp)
        set_source_files_properties(platform/3ds/source/native_vegetation_lab_3ds.cpp
            PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
    endif()
    if(MK64_3DS_NATIVE_PARTICLE_LAB)
        target_sources(mk64-3ds-game-runtime PRIVATE platform/3ds/source/native_particle_lab_3ds.cpp)
        set_source_files_properties(platform/3ds/source/native_particle_lab_3ds.cpp
            PROPERTIES COMPILE_OPTIONS "-fno-fast-math")
    endif()
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
        platform/3ds/source/resource_loading_3ds.cpp
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
    target_sources(mk64-3ds-game PRIVATE platform/3ds/source/gpu_command_publication_3ds.cpp)
    target_link_options(mk64-3ds-game PRIVATE -Wl,--wrap=GX_ProcessCommandList)
    if(MK64_3DS_NATIVE_PLAYTEST)
        target_sources(mk64-3ds-game PRIVATE platform/3ds/source/gpu_startup_evidence_3ds.cpp)
        target_link_options(mk64-3ds-game PRIVATE
            -Wl,--wrap=C3D_FrameBegin -Wl,--wrap=C3D_FrameEnd)
    endif()
    target_link_libraries(mk64-3ds-game PRIVATE
        mk64-3ds-updater
        -Wl,--start-group
        mk64-3ds-game-runtime
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

include(platform/3ds/cmake/native_dynamic_3ds.cmake)
