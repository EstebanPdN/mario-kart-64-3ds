# CPU item strategies also use the player-local effect/voice entry points.
# Their dummy (0,0,0) listener-space source never attenuates. Route only CPU
# racers through the existing bounded positional pool; human/UI audio stays local.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SPAGHETTIKART_AUDIO_EXTERNAL_SOURCE}")
file(READ "${SPAGHETTIKART_AUDIO_EXTERNAL_SOURCE}" SPATIAL_AUDIO_TEXT)
set(SPATIAL_AUDIO_TEXT "#include \"item_rules_3ds.h\"\n#include \"auto_mode_3ds.h\"\n#include \"audio_feedback_3ds.h\"\n${SPATIAL_AUDIO_TEXT}")
# Only external.c uses the bank storage. Give the enlarged private array a new
# name rather than violating external.h's original 20-entry row-stride ABI.
function(mk64_audio_feedback_replace old new)
    string(FIND "${SPATIAL_AUDIO_TEXT}" "${old}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Upstream audio feedback anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" replaced "${SPATIAL_AUDIO_TEXT}")
    set(SPATIAL_AUDIO_TEXT "${replaced}" PARENT_SCOPE)
endfunction()
mk64_audio_feedback_replace("struct Unk_8018EFD8 D_8018EFD8[50];"
    "struct Unk_8018EFD8 D_8018EFD8[MK64_POSITIONAL_SOUND_SLOTS_3DS];")
mk64_audio_feedback_replace("struct SoundCharacteristics sSoundBanks[SOUND_BANK_COUNT][20];"
    "static struct SoundCharacteristics sMk64SoundBanks3DS[SOUND_BANK_COUNT][MK64_SOUND_BANK_SLOTS_3DS];\n#define sSoundBanks sMk64SoundBanks3DS")
mk64_audio_feedback_replace("var_v0 < 49;" "var_v0 < MK64_POSITIONAL_SOUND_SLOTS_3DS - 1;")
mk64_audio_feedback_replace("var_v1 < 19;" "var_v1 < MK64_SOUND_BANK_SLOTS_3DS - 1;")
# Pending request capacity is independent of playback channel capacity. The
# original sound_player M64 script hardcodes bank-specific channel scripts and
# initialization masks for ALL four modes. Changing D_800EA188 shifts later
# banks onto incompatible scripts and even uninitialized channels. Preserve
# both native playback/per-source tables; never validate by sum <=16 alone.
mk64_audio_feedback_replace("(u32) (sSoundBanks[bank][soundIndex].soundBits & 0xFF00)"
    "Mk64FeedbackPriority3DS(sSoundBanks[bank][soundIndex].soundBits)")
mk64_audio_feedback_replace("(u32) (var_t3 & 0xFF00)" "Mk64FeedbackPriority3DS(var_t3)")
mk64_audio_feedback_replace("(u32) (var_a2->soundBits & 0xFF00)" "Mk64FeedbackPriority3DS(var_a2->soundBits)")
mk64_audio_feedback_replace("((u32) (temp_t2->soundBits & 0xFF00) >> 8) & 0xFF"
    "Mk64FeedbackPriority3DS(temp_t2->soundBits) >> 8")
foreach(SPATIAL_FUNCTION func_800C9060 func_800C90F4)
    set(SPATIAL_FINISH_VOICE "")
    if(SPATIAL_FUNCTION STREQUAL "func_800C90F4")
        # Native func_800C76C0 schedules this once at audio tick 30. Preserve
        # its winner/top-four/bottom-four selection and original listener ABI;
        # do not extend the spectator input lock or unmute overtaking voices.
        set(SPATIAL_FINISH_VOICE [=[
            || (Mk64Auto3DSFinishAudio(playerId) && D_800E9EA4[playerId] == 30 &&
                ((soundBits & ~0xF0U) == 0x29008007U ||
                 (soundBits & ~0xF0U) == 0x2900800DU ||
                 (soundBits & ~0xF0U) == 0x29008003U))]=])
    endif()
    set(SPATIAL_ANCHOR "void ${SPATIAL_FUNCTION}(u8 playerId, u32 soundBits) {")
    mk64_audio_feedback_replace("${SPATIAL_ANCHOR}" "${SPATIAL_ANCHOR}
    if (playerId >= NUM_PLAYERS) return;
    if (gGamestate == RACING && (Mk64NewItems3DSEnabled() || Mk64Auto3DSActive() || Mk64SoloBattle3DSSpectating()) && (gPlayers[playerId].type & PLAYER_CPU) && !(gPlayers[playerId].type & PLAYER_HUMAN)) {
        if (Mk64Auto3DSWatchedAudio(playerId)${SPATIAL_FINISH_VOICE}) playerId = 0; // Native listener slot, not racer ID.
        else {
            func_800C98B8(gPlayers[playerId].pos, gPlayers[playerId].velocity, soundBits);
            return;
        }
    }")
endforeach()
# Character confirmation is menu-local, even when the previous race left its
# racer type, effects and finish-audio gates behind. Keep the original sample
# and unity gain, rather than treating it as a distant CPU in the old course.
mk64_audio_feedback_replace("void func_800C90F4(u8 playerId, u32 soundBits) {"
    "void func_800C90F4(u8 playerId, u32 soundBits) {\n    if (gGamestate != RACING && (soundBits & ~0xF0U) == 0x2900800EU) {\n        play_sound2(soundBits);\n        return;\n    }")
mk64_audio_feedback_replace("void func_800C90F4(u8 playerId, u32 soundBits) {"
    "void func_800C90F4(u8 playerId, u32 soundBits) {\n    if (Mk64SoloBattle3DSSpectating() && playerId == 0) return; // Spent human bomb is not the watched voice source.")
mk64_audio_feedback_replace("void func_800C92CC(u8 playerId, u32 soundBits) {"
    "void func_800C92CC(u8 playerId, u32 soundBits) {\n    if (playerId >= NUM_PLAYERS) return;\n    if (Mk64Auto3DSWatchedAudio(playerId)) {\n        if (soundBits == SOUND_ARG_LOAD(0x29, 0x00, 0x80, 0x0D)) return; // Watched driver is silent on overtaking, like a human driver.\n        func_800C90F4(playerId, (gPlayers[playerId].characterId * 0x10) + soundBits);\n        return;\n    }")
# Extend only the targeted shell-warning attenuation, never all item sounds.
set(SPATIAL_WARNING_ANCHOR "    temp_f0 = temp_v0->distance;")
string(FIND "${SPATIAL_AUDIO_TEXT}" "${SPATIAL_WARNING_ANCHOR}" SPATIAL_WARNING_FOUND)
if(SPATIAL_WARNING_FOUND LESS 0)
    message(FATAL_ERROR "Upstream warning attenuation anchor changed")
endif()
string(REPLACE "${SPATIAL_WARNING_ANCHOR}"
    "${SPATIAL_WARNING_ANCHOR}\n    if (Mk64NewItems3DSEnabled() && temp_v0->soundBits == SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08))\n        temp_f0 *= (0.35f / 1.5f); // Modern warning reach: 50% farther, still attenuated."
    SPATIAL_AUDIO_TEXT "${SPATIAL_AUDIO_TEXT}")
# Keep the extended warning reach and sound/stop identity, but tune only this
# modern rear-warning's gain. Other positional effects and original OFF audio
# retain their native levels; do not change playback channels or bank scripts.
mk64_audio_feedback_replace("    return var_f2;\n}\n\ns8 func_800C15D0"
    "    if (Mk64NewItems3DSEnabled() && temp_v0->soundBits == SOUND_ARG_LOAD(0x51, 0x01, 0x80, 0x08))\n        var_f2 *= 0.70f; // Rear red/blue/Star alarm: 70% of the original extended-warning gain.\n    return var_f2;\n}\n\ns8 func_800C15D0")
# Repeated item/voice requests at the same live source used to consume a new
# positional slot each time. The bounded usable slots silently drop sounds when
# full. Reuse only active, identical one-shots for the same listener; never
# steal another source or reuse a slot marked complete by the audio thread.
set(SPATIAL_ALLOC_ANCHOR "    if (D_8018EFD8[D_8018FB90].next != 0xFF) {")
string(FIND "${SPATIAL_AUDIO_TEXT}" "${SPATIAL_ALLOC_ANCHOR}" SPATIAL_ALLOC_FOUND)
if(SPATIAL_ALLOC_FOUND LESS 0)
    message(FATAL_ERROR "Upstream positional sound allocation changed")
endif()
string(REPLACE "${SPATIAL_ALLOC_ANCHOR}" [=[
    if ((soundBits >> 28) == 1 || (soundBits >> 28) == 2) {
        u8 it = D_8018EFD8[0].next;
        u8 visited = 0;
        while (it != 0xFF && it > 0 && it < MK64_POSITIONAL_SOUND_SLOTS_3DS && visited++ < MK64_POSITIONAL_SOUND_SLOTS_3DS - 1) {
            struct Unk_8018EFD8* existing = &D_8018EFD8[it];
            if (existing->posX == position && existing->cameraId == arg4 &&
                existing->soundBits == soundBits && existing->unk18[1] != 100000.0f)
                return existing;
            it = existing->next;
        }
    }
    if (D_8018EFD8[D_8018FB90].next != 0xFF) {
]=] SPATIAL_AUDIO_TEXT "${SPATIAL_AUDIO_TEXT}")
set(SPATIAL_IMPACT_ANCHOR "void func_800C98B8(Vec3f position, Vec3f velocity, u32 soundBits) {")
string(FIND "${SPATIAL_AUDIO_TEXT}" "${SPATIAL_IMPACT_ANCHOR}" SPATIAL_IMPACT_FOUND)
if(SPATIAL_IMPACT_FOUND LESS 0)
    message(FATAL_ERROR "Upstream positional impact sound entry changed")
endif()
string(REPLACE "${SPATIAL_IMPACT_ANCHOR}" "${SPATIAL_IMPACT_ANCHOR}
    if (Mk64Auto3DSActive() || Mk64SoloBattle3DSSpectating()) {
        const int watched = Mk64Auto3DSObservedPlayer();
        if (Mk64Auto3DSWatchedAudio(watched) && position == gPlayers[watched].pos && (soundBits >> 28) == 1) {
            func_800C9060(watched, soundBits);
            return;
        }
    }
    if (soundBits == SOUND_ARG_LOAD(0x19, 0x01, 0x80, 0x10)) {
        for (u8 playerId = 0; playerId < NUM_PLAYERS; ++playerId) {
            if (position == gPlayers[playerId].pos && (gPlayers[playerId].type & PLAYER_HUMAN)) {
                func_800C9060(playerId, soundBits); // Player impact cannot be lost to a full positional pool.
                return;
            }
        }
    }" SPATIAL_AUDIO_TEXT "${SPATIAL_AUDIO_TEXT}")
file(WRITE "${SPAGHETTIKART_3DS_AUDIO_EXTERNAL_SOURCE}" "${SPATIAL_AUDIO_TEXT}")
