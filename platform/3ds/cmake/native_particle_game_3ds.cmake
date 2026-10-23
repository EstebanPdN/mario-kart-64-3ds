# Native producer conversion, invoked only by the prepared particle experiment.
# Keep the original game logic and replace the complete emission contract in
# each selected function. Unsupported source edits stop configuration.
macro(mk64_convert_native_particle_producers)
    macro(mk64_particle_replace statement recipe)
        string(REPLACE "${statement}" "${recipe}" after "${after}")
        if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
            string(REPLACE "${statement}" "${recipe}\n                ${statement}" dual "${dual}")
        endif()
    endmacro()
    set(nativeParticleFunctions func_8006538C func_800658A0
        render_player_drift_particles render_player_ground_particles
        func_800664E0 func_80066998 render_player_boost_spark_particles
        func_80069444 func_80069BA8)
    foreach(function IN LISTS nativeParticleFunctions)
        string(FIND "${SPAGHETTIKART_3DS_HUD_TEXT}" "void ${function}(" begin)
        if(begin LESS 0)
            message(FATAL_ERROR "Native particle producer missing: ${function}")
        endif()
        string(SUBSTRING "${SPAGHETTIKART_3DS_HUD_TEXT}" ${begin} -1 tail)
        string(FIND "${tail}" "\n}\n" end)
        if(end LESS 0)
            message(FATAL_ERROR "Native particle function boundary changed: ${function}")
        endif()
        math(EXPR length "${end}+3")
        string(SUBSTRING "${tail}" 0 ${length} before)
        set(after "${before}")
        string(FIND "${after}" "{" brace)
        math(EXPR brace "${brace}+1")
        string(SUBSTRING "${after}" 0 ${brace} prefix)
        string(SUBSTRING "${after}" ${brace} -1 body)
        set(after "${prefix}\n    Mk64NativeParticle3DS nativeParticle={0};${body}")
        if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
            set(dual "${prefix}\n    Mk64NativeParticle3DS nativeParticle={0};\n    const void* nativeCompareBegin=gDisplayListHead;\n    const void* nativeCompareMatrixKey=0;${body}")
            string(REGEX REPLACE "func_800652D4\\(([^;]+)\\);"
                "mk64_native_particle_transform(&nativeParticle, \\1);\n        func_800652D4(\\1);\n        nativeCompareMatrixKey=Mk64NativeParticleCompareLastMatrix();" dual "${dual}")
            string(REGEX REPLACE "func_8004B72C\\(([^;]+)\\);"
                "Mk64NativeParticleLerp(&nativeParticle, \\1);\n        func_8004B72C(\\1);" dual "${dual}")
            string(REGEX REPLACE "func_8004B35C\\(([^;]+)\\);"
                "Mk64NativeParticleTint(&nativeParticle, \\1);\n        func_8004B35C(\\1);" dual "${dual}")
        endif()
        string(REPLACE "func_800652D4(" "mk64_native_particle_transform(&nativeParticle, " after "${after}")
        foreach(setup DB8 C90)
            if(setup STREQUAL "DB8")
                set(kind SMOKE)
            else()
                set(kind GRASS)
            endif()
            mk64_particle_replace("gSPDisplayList(gDisplayListHead++, D_0D008${setup});"
                "Mk64NativeParticleSetSetup(&nativeParticle, MK64_PARTICLE_${kind}_SETUP);")
        endforeach()
        set(space "[ \t\r\n]*")
        set(load "gDPLoadTextureBlock\\(gDisplayListHead\\+\\+,${space}([^,]+),${space}G_IM_FMT_([A-Z]+),${space}G_IM_SIZ_([0-9]+)b,${space}([0-9]+),${space}([0-9]+),${space}0,${space}G_TX_NOMIRROR \\| G_TX_(WRAP|CLAMP),${space}G_TX_NOMIRROR \\| G_TX_(WRAP|CLAMP),${space}G_TX_NOMASK,${space}G_TX_NOMASK,${space}G_TX_NOLOD,${space}G_TX_NOLOD\\);")
        # No masks means the production tile policy clamps both axes. Preserve
        # the logical image bounds in the native sampler, including padding.
        while(after MATCHES "${load}")
            set(original "${CMAKE_MATCH_0}")
            set(image "${CMAKE_MATCH_1}")
            set(format "${CMAKE_MATCH_2}")
            set(bits "${CMAKE_MATCH_3}")
            set(width "${CMAKE_MATCH_4}")
            set(height "${CMAKE_MATCH_5}")
            if(format STREQUAL "I" AND bits STREQUAL "8")
                set(format INTENSITY8)
            elseif(format STREQUAL "IA" AND bits STREQUAL "8")
                set(format INTENSITY_ALPHA8)
            elseif(format STREQUAL "RGBA" AND bits STREQUAL "16")
                set(format RGBA5551)
            else()
                message(FATAL_ERROR "Native particle image format changed: ${function}")
            endif()
            mk64_particle_replace("${original}"
                "Mk64NativeParticleSetImage(&nativeParticle, (const char*)(${image}), MK64_PARTICLE_${format}, ${width}, ${height});")
        endwhile()
        string(REPLACE "func_8004B72C(" "Mk64NativeParticleLerp(&nativeParticle, " after "${after}")
        string(REPLACE "func_8004B35C(" "Mk64NativeParticleTint(&nativeParticle, " after "${after}")
        mk64_particle_replace("gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);" "/* Native image has no palette. */")
        foreach(mode NONE DITHER THRESHOLD)
            if(mode STREQUAL "NONE")
                set(kind NO)
            else()
                set(kind ${mode})
            endif()
            mk64_particle_replace("gDPSetAlphaCompare(gDisplayListHead++, G_AC_${mode});"
                "nativeParticle.compare=MK64_PARTICLE_${kind}_COMPARE;")
        endforeach()
        foreach(mode XLU TEX_EDGE)
            if(mode STREQUAL "XLU")
                set(kind BLEND)
                set(mode0 G_RM_ZB_XLU_SURF)
                set(mode1 G_RM_ZB_XLU_SURF2)
            else()
                set(kind EDGE)
                set(mode0 G_RM_AA_ZB_TEX_EDGE)
                set(mode1 G_RM_AA_ZB_TEX_EDGE2)
            endif()
            mk64_particle_replace("gDPSetRenderMode(gDisplayListHead++, ${mode0}, ${mode1});"
                "nativeParticle.raster=MK64_PARTICLE_${kind}_RASTER;")
        endforeach()
        mk64_particle_replace("gSPVertex(gDisplayListHead++, D_800E8C00, 4, 0);"
            "nativeParticle.vertices=D_800E8C00;")
        foreach(draw E48 E70 DF8 DA0)
            if(draw STREQUAL "E48")
                set(kind QUAD_32)
            elseif(draw STREQUAL "E70")
                set(kind QUAD_REVERSED_32)
            elseif(draw STREQUAL "DF8")
                set(kind QUAD_16)
            else()
                set(kind EXPLICIT_QUAD)
            endif()
            string(REPLACE "gSPDisplayList(gDisplayListHead++, D_0D008${draw});"
                "Mk64NativeParticleEmit(gDisplayListHead, &nativeParticle, MK64_PARTICLE_${kind});" after "${after}")
            if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
                string(REPLACE "gSPDisplayList(gDisplayListHead++, D_0D008${draw});"
                    "gSPDisplayList(gDisplayListHead++, D_0D008${draw});\n        Mk64NativeParticleEmitCompared(nativeCompareBegin,gDisplayListHead,&nativeParticle,MK64_PARTICLE_${kind},nativeCompareMatrixKey);" dual "${dual}")
            endif()
        endforeach()
        if(after MATCHES "g[SD]P[A-Za-z]+\\(" OR after MATCHES "func_800652D4\\(" OR after MATCHES "func_8004B(35C|72C)\\(")
            message(FATAL_ERROR "Native particle producer has an unconverted emission: ${function}")
        endif()
        if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
            string(REPLACE "${before}" "${dual}" SPAGHETTIKART_3DS_HUD_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
        else()
            string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_HUD_TEXT "${SPAGHETTIKART_3DS_HUD_TEXT}")
        endif()
    endforeach()
    string(PREPEND SPAGHETTIKART_3DS_HUD_TEXT [=[#include "native_particle_contract_3ds.h"
static void mk64_native_particle_transform(Mk64NativeParticle3DS*,const float*,const short*,float);
]=])
    if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
        string(PREPEND SPAGHETTIKART_3DS_HUD_TEXT "#include \"native_particle_compare_3ds.h\"\n")
    endif()
    string(APPEND SPAGHETTIKART_3DS_HUD_TEXT [=[
static void mk64_native_particle_transform(Mk64NativeParticle3DS* particle,
        const float* position,const short* rotation,float scale) {
    Mat4 matrix;
    mtxf_translate_rotate(matrix, (float*)position, (short*)rotation);
    mtxf_scale(matrix,scale);
    Mk64NativeParticleSetModel(particle,&matrix[0][0]);
}
]=])
endmacro()
