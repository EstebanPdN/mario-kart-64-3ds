#include "texture_mode_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
    using namespace mk64_3ds;
    for (auto mode : {MK64_UI_TEXTURES_3DS_ORIGINAL, MK64_UI_TEXTURES_3DS_IMPROVED,
                      MK64_UI_TEXTURES_3DS_MENU}) {
        const bool ui = mode != MK64_UI_TEXTURES_3DS_ORIGINAL;
        const bool race = mode == MK64_UI_TEXTURES_3DS_IMPROVED;
        assert(ImprovedUiTextures(mode) == ui);
        assert(ImprovedRaceTextures(mode) == race);
        for (const char* name : {"textures/player_selection/portrait_mario",
                "__OTR__textures/texture_tkmk00/texture_l_option",
                "textures/texture_data_2/texture_time",
                "textures/other_textures/minimap_wario_stadium",
                "textures/common_data/common_texture_hud_lap",
                "textures/common_data/common_texture_portrait_bowser",
                "textures/common_data/common_texture_item_window_shell",
                "textures/common_data/common_texture_font_number",
                "textures/common_data/common_texture_player_emblem_1"})
            assert(ImprovedResourceTexture(mode, name) == ui);
        for (const char* name : {"textures/karts/mario", "__OTR__textures/karts/peach",
                "textures/common_data/common_texture_particle_smoke",
                "textures/common_data/common_texture_tree", "courses/luigi_raceway/road"})
            assert(ImprovedResourceTexture(mode, name) == race);
        assert(!ImprovedResourceTexture(mode, nullptr));
    }
    std::puts("PASS: Menu keeps UI, maps and portraits improved; race drivers/world use originals.");
}
