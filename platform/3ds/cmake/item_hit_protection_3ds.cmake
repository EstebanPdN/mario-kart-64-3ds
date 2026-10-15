# Native star-vs-racer collisions share the same post-hit shield as shell actors.
set(ITEM_HIT_RACE_LOGIC "${SPAGHETTIKART_ROOT}/src/racing/race_logic.c")
set(ITEM_HIT_RACE_LOGIC_3DS "${CMAKE_CURRENT_BINARY_DIR}/generated/race_logic_3ds.c")
list(REMOVE_ITEM SPAGHETTIKART_GAME_RECURSIVE "${ITEM_HIT_RACE_LOGIC}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ITEM_HIT_RACE_LOGIC}")
file(READ "${ITEM_HIT_RACE_LOGIC}" ITEM_HIT_TEXT)
foreach(ITEM_HIT_VICTIM playerOne playerTwo)
    set(ITEM_HIT_OLD "        ${ITEM_HIT_VICTIM}->triggers |= HIT_BY_STAR_TRIGGER;")
    string(FIND "${ITEM_HIT_TEXT}" "${ITEM_HIT_OLD}" ITEM_HIT_ANCHOR)
    if(ITEM_HIT_ANCHOR LESS 0)
        message(FATAL_ERROR "Upstream star collision changed: ${ITEM_HIT_VICTIM}")
    endif()
    string(REPLACE "${ITEM_HIT_OLD}"
        "        if (Mk64ItemHit3DSTry(${ITEM_HIT_VICTIM})) ${ITEM_HIT_VICTIM}->triggers |= HIT_BY_STAR_TRIGGER;"
        ITEM_HIT_TEXT "${ITEM_HIT_TEXT}")
endforeach()
# Reset the port clock at the same authoritative RACE_INIT boundary as the
# native course timer. Track-load reset happens before the old timer/gate clear.
set(MK64_RACE_CLOCK_RESET_ANCHOR "            D_8015011E = 0;\n            gCourseTimer = 0.0f;")
string(FIND "${ITEM_HIT_TEXT}" "${MK64_RACE_CLOCK_RESET_ANCHOR}" MK64_RACE_CLOCK_RESET_FOUND)
if(MK64_RACE_CLOCK_RESET_FOUND LESS 0)
    message(FATAL_ERROR "Upstream native RACE_INIT timer reset changed")
endif()
string(REPLACE "${MK64_RACE_CLOCK_RESET_ANCHOR}"
    "${MK64_RACE_CLOCK_RESET_ANCHOR}\n            Mk64RaceClock3DSReset();"
    ITEM_HIT_TEXT "${ITEM_HIT_TEXT}")
file(WRITE "${ITEM_HIT_RACE_LOGIC_3DS}"
    "#include \"item_rules_3ds.h\"\nextern void Mk64RaceClock3DSReset(void);\n${ITEM_HIT_TEXT}")
list(APPEND SPAGHETTIKART_GAME_RECURSIVE "${ITEM_HIT_RACE_LOGIC_3DS}")
