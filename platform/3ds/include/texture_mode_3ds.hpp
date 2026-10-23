#pragma once

#include "settings_3ds.h"
#include <cstring>

namespace mk64_3ds {
constexpr bool ImprovedUiTextures(Mk64UiTextures3DS mode) {
    return mode == MK64_UI_TEXTURES_3DS_IMPROVED || mode == MK64_UI_TEXTURES_3DS_MENU;
}
constexpr bool ImprovedRaceTextures(Mk64UiTextures3DS mode) {
    return mode == MK64_UI_TEXTURES_3DS_IMPROVED;
}
inline bool ImprovedResourceTexture(Mk64UiTextures3DS mode, const char* name) {
    if (!ImprovedUiTextures(mode) || !name) return false;
    if (mode == MK64_UI_TEXTURES_3DS_IMPROVED) return true;
    if (!std::strncmp(name, "__OTR__", 7)) name += 7;
    // Menu mode also improves race HUD and maps; world actors keep Originals.
    constexpr const char* prefixes[] = {
        "textures/player_selection/", "textures/texture_tkmk00/",
        "textures/texture_data_2/", "textures/other_textures/minimap_",
        "textures/common_data/common_texture_hud_",
        "textures/common_data/common_texture_portrait_",
        "textures/common_data/common_texture_item_window_",
        "textures/common_data/common_texture_font_",
        "textures/common_data/common_texture_player_emblem_"
    };
    for (const char* prefix : prefixes)
        if (!std::strncmp(name, prefix, std::strlen(prefix))) return true;
    return std::strstr(name, "/logo_mario_kart_64") != nullptr;
}
}
