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
    # load_track owns scene teardown, including cameras. Credits used to bind
    # cameras first and immediately destroy them during that load (dump 72).
    string(FIND "${SPAGHETTIKART_3DS_ENDING_TEXT}" "void load_credits(void) {" credits_start)
    if(credits_start LESS 0)
        message(FATAL_ERROR "The upstream credits camera boundary changed")
    endif()
    string(SUBSTRING "${SPAGHETTIKART_3DS_ENDING_TEXT}" ${credits_start} -1 credits_text)
    string(FIND "${credits_text}" "    gCurrentCourseId = gCreditsCourseId;" credits_camera_offset)
    if(credits_camera_offset LESS 0)
        message(FATAL_ERROR "The upstream credits track selection changed")
    endif()
    math(EXPR credits_camera_end "${credits_start} + ${credits_camera_offset}")
    string(LENGTH "void load_credits(void) {\n" credits_header_length)
    math(EXPR credits_camera_start "${credits_start} + ${credits_header_length}")
    math(EXPR credits_camera_length "${credits_camera_end} - ${credits_camera_start}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_ENDING_TEXT}" ${credits_camera_start}
        ${credits_camera_length} credits_camera_block)
    string(REPLACE "void load_credits(void) {\n${credits_camera_block}"
        "void load_credits(void) {\n" SPAGHETTIKART_3DS_ENDING_TEXT
        "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    string(REPLACE "    gFreeMemoryCourseAnchor = gNextFreeMemoryAddress;\n"
        "    gFreeMemoryCourseAnchor = gNextFreeMemoryAddress;\n${credits_camera_block}"
        SPAGHETTIKART_3DS_ENDING_TEXT "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    if(NOT SPAGHETTIKART_3DS_ENDING_TEXT MATCHES "load_track\\(gCurrentCourseId\\);[^}]*CM_CleanCameras")
        message(FATAL_ERROR "Credits camera construction must follow track teardown")
    endif()
    string(REPLACE "void load_credits(void) {"
        "extern void Mk64GameAudio3DSBeginLoading(void);\nextern void Mk64GameAudio3DSEndLoading(void);\nvoid load_credits(void) {\n    Mk64GameAudio3DSBeginLoading();"
        SPAGHETTIKART_3DS_ENDING_TEXT "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    string(REPLACE "    D_801625FC = ((f32) D_801625F8 / 1000.0f);"
        "    D_801625FC = ((f32) D_801625F8 / 1000.0f);\n    Mk64GameAudio3DSEndLoading();"
        SPAGHETTIKART_3DS_ENDING_TEXT "${SPAGHETTIKART_3DS_ENDING_TEXT}")
    foreach(credits_audio_hook IN ITEMS BeginLoading EndLoading)
        string(REGEX MATCHALL "Mk64GameAudio3DS${credits_audio_hook}\\(\\)"
            credits_audio_calls "${SPAGHETTIKART_3DS_ENDING_TEXT}")
        list(LENGTH credits_audio_calls credits_audio_call_count)
        if(NOT credits_audio_call_count EQUAL 1)
            message(FATAL_ERROR "The credits loading audio scope changed: ${credits_audio_hook}")
        endif()
    endforeach()
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
    if (asset == NULL || gGamestate == CREDITS_SEQUENCE) {
        // Resource pressure can reject a course-section load. Leave the
        // collision mesh empty instead of dereferencing a failed C lookup.
        // Credits discard this mesh immediately after load_track anyway;
        // do not build/decode the entire racing collision scene for each cut.
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

    include(platform/3ds/cmake/menu_details_3ds.cmake)
    include(platform/3ds/cmake/time_trial_hud_3ds.cmake)
    include(platform/3ds/cmake/auto_mode_3ds.cmake)
    include(platform/3ds/cmake/solo_modes_3ds.cmake)
