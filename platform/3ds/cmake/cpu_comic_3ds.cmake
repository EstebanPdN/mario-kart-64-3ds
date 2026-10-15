# Reuse the original fixed per-kart comic slots, textures and animation.
# Keep the camera-visible mask and the human-own-viewport rule unchanged.
function(mk64_cpu_comic_replace old new)
    string(FIND "${SPAGHETTIKART_3DS_HUD_TEXT}" "${old}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Upstream CPU comic anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" replaced "${SPAGHETTIKART_3DS_HUD_TEXT}")
    set(SPAGHETTIKART_3DS_HUD_TEXT "${replaced}" PARENT_SCOPE)
endfunction()
mk64_cpu_comic_replace(
    "        if ((player->type & PLAYER_HUMAN) == PLAYER_HUMAN) {\n            func_8006D194(player, arg1, arg2);"
    "        if (player->type & (PLAYER_HUMAN | PLAYER_CPU)) {\n            func_8006D194(player, arg1, arg2);")
mk64_cpu_comic_replace(
    "        if (((player->type & PLAYER_HUMAN) == PLAYER_HUMAN) && (screenId == playerId)) {\n            FrameInterpolation_RecordOpenChild(\"onomatopoeia\""
    "        if ((player->type & PLAYER_CPU) ||\n            (((player->type & PLAYER_HUMAN) == PLAYER_HUMAN) && (screenId == playerId))) {\n            FrameInterpolation_RecordOpenChild(\"onomatopoeia\"")
# The native typo was hidden for player zero: playerId was used as slot index.
mk64_cpu_comic_replace(
    "    player->particlePool2[arg1 /* arg1 instead of arg3 */].scale = 0.2f;"
    "    player->particlePool2[index].scale = 0.2f;")
# BOING/POOMP were designed for the human's rear camera. Opponents can be
# viewed from any angle; their existing camera-facing billboard remains valid.
mk64_cpu_comic_replace(
    "(player->animFrameSelector[arg3] < 0xD)"
    "((player->animFrameSelector[arg3] < 0xD) || (player->type & PLAYER_CPU))")
mk64_cpu_comic_replace(
    "((s32) player->animFrameSelector[arg3] < 0xD)"
    "(((s32) player->animFrameSelector[arg3] < 0xD) || (player->type & PLAYER_CPU))")
