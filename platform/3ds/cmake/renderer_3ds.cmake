# Graphics target and interpreter adaptation owner. Transform build-local text,
# then write the generated interpreter; the upstream checkout remains pinned.
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
        "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
        "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
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
        mk64-3ds-platform
        citro3d
        ctru
        m
    )

    if(MK64_3DS_BUILD_RENDERER_PROBE)
        add_executable(mk64-3ds-renderer-probe
            platform/3ds/source/renderer_probe.cpp
            platform/3ds/source/performance_trace_3ds.cpp
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
            "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/src/fast/interpreter.cpp")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${SPAGHETTIKART_INTERPRETER_SOURCE}")
        set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/interpreter_3ds.cpp")
        file(READ "${SPAGHETTIKART_INTERPRETER_SOURCE}" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
        set(SPAGHETTIKART_3DS_INTERPRETER_ORIGINAL "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    include(platform/3ds/cmake/interpreter_patches_3ds.cmake)

        add_library(mk64-3ds-fast3d-interpreter STATIC
            platform/3ds/source/gfx_window_manager_3ds.cpp
            third_party/SpaghettiKart/libultraship/src/fast/debug/GfxDebugger.cpp
            "${SPAGHETTIKART_3DS_INTERPRETER_SOURCE}"
        )
        target_include_directories(mk64-3ds-fast3d-interpreter PUBLIC
            "${MK64_3DS_SOURCE_ROOT}/platform/3ds/include"
            "${MK64_3DS_SOURCE_ROOT}/platform/3ds/probe_stubs"
            "${MK64_3DS_SOURCE_ROOT}/third_party/SpaghettiKart/libultraship/include"
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
            platform/3ds/source/performance_trace_3ds.cpp
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

