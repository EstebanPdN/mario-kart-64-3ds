# Keep the original update order and both internal logic ticks. These spans
# record exclusive time on each profiled tick; they do not infer that
# all work outside the graphics interpreter is physics.
string(PREPEND SPAGHETTIKART_3DS_MAIN_TEXT "#include \"performance_trace_3ds.h\"\n")
function(mk64_game_section anchor section)
    string(FIND "${SPAGHETTIKART_3DS_MAIN_TEXT}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Game preparation timing anchor changed: ${anchor}")
    endif()
    string(REPLACE "${anchor}"
        "{ uint32_t section = Mk64Perf3DSGameSectionBegin(${section}); ${anchor} Mk64Perf3DSGameSectionEnd(section); }"
        changed "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    set(SPAGHETTIKART_3DS_MAIN_TEXT "${changed}" PARENT_SCOPE)
endfunction()
mk64_game_section("func_802909F0();" MK64_PERF_GAME_PHYSICS)
mk64_game_section("evaluate_collision_for_players_and_actors();" MK64_PERF_GAME_COLLISION)
mk64_game_section("handle_a_press_for_all_players_during_race();" MK64_PERF_GAME_PLAYERS)
mk64_game_section("CM_TickCameras();" MK64_PERF_GAME_CAMERA)
mk64_game_section("func_80028F70();" MK64_PERF_GAME_PLAYERS)
mk64_game_section("func_8028F474();" MK64_PERF_GAME_PHYSICS)
mk64_game_section("func_80059AC8();" MK64_PERF_GAME_ACTORS)
mk64_game_section("update_course_actors();" MK64_PERF_GAME_ACTORS)
mk64_game_section("CM_TickActors();" MK64_PERF_GAME_ACTORS)
mk64_game_section("CM_TickTrack();" MK64_PERF_GAME_ACTORS)
mk64_game_section("func_8028FCBC();" MK64_PERF_GAME_PHYSICS)
set(start "    profiler_log_thread5_time(LEVEL_SCRIPT_EXECUTE);\n    sNumVBlanks = 0;")
set(finish "    // End of frame cleanup of actors, objects, etc.\n    CM_RunGarbageCollector();")
foreach(anchor "${start}" "${finish}")
    string(FIND "${SPAGHETTIKART_3DS_MAIN_TEXT}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Race visual preparation timing boundary changed")
    endif()
endforeach()
string(REPLACE "${start}"
    "    uint32_t visualSection = Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_VISUAL);\n${start}"
    SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
string(REPLACE "${finish}"
    "    Mk64Perf3DSGameSectionEnd(visualSection);\n${finish}"
    SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
