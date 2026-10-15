# Keep DK's tree changes confined to its generated function. Stable spawn IDs
# must not change when an earlier tree is culled. Keep geometry, culling and
# the original variant-6 goto/pointer advancement unchanged.
string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "void render_palm_trees(" JUNGLE_START)
string(FIND "${SPAGHETTIKART_3DS_ACTORS_TEXT}" "#include \"actors/trees/render.inc.c\"" JUNGLE_END)
if(JUNGLE_START LESS 0 OR JUNGLE_END LESS JUNGLE_START)
    message(FATAL_ERROR "Upstream jungle tree rendering boundary changed")
endif()
math(EXPR JUNGLE_LENGTH "${JUNGLE_END} - ${JUNGLE_START}")
string(SUBSTRING "${SPAGHETTIKART_3DS_ACTORS_TEXT}" ${JUNGLE_START} ${JUNGLE_LENGTH} JUNGLE_ORIGINAL)
set(JUNGLE_FIXED "${JUNGLE_ORIGINAL}")
macro(mk64_jungle_replace old new)
    string(FIND "${JUNGLE_FIXED}" "${old}" JUNGLE_FOUND)
    if(JUNGLE_FOUND LESS 0)
        message(FATAL_ERROR "Upstream jungle tree anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" JUNGLE_FIXED "${JUNGLE_FIXED}")
endmacro()
mk64_jungle_replace("    size_t i = 0;" "    const struct UnkActorSpawnData* spawnStart = var_s1;")
mk64_jungle_replace("TAG_ITEM_ADDR((i << 4) | (camera - cameras))"
    "TAG_ITEM_ADDR(((size_t)(var_s1 - spawnStart) << 4) | (camera - cameras))")
mk64_jungle_replace("        i++;" "        // Identity is the stable spawn index, not the visible-tree ordinal.")
string(REPLACE "${JUNGLE_ORIGINAL}" "${JUNGLE_FIXED}"
    SPAGHETTIKART_3DS_ACTORS_TEXT "${SPAGHETTIKART_3DS_ACTORS_TEXT}")
