cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/version.txt" MK64_3DS_VERSION LIMIT_COUNT 1)
project(mk64_3ds LANGUAGES C)

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

set(MK64_3DS_SMDH "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds.smdh")
ctr_generate_smdh(
    OUTPUT "${MK64_3DS_SMDH}"
    NAME "Mario Kart 64 3DS v${MK64_3DS_VERSION}"
    DESCRIPTION "Native 3DS port bootstrap"
    AUTHOR "EstebanPdN"
    ICON "${CMAKE_CURRENT_LIST_DIR}/platform/3ds/assets/icon-48.png"
)

ctr_create_3dsx(mk64-3ds-3dsx
    TARGET mk64-3ds
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mk64-3ds-v${MK64_3DS_VERSION}.3dsx"
    SMDH "${MK64_3DS_SMDH}"
)
