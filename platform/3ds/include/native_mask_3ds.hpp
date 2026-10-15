#pragma once

#include "native_sprite_3ds.hpp"
#include <cstddef>

namespace mk64_3ds::native_art {
// Each token stores a 1..4095 pixel run in its upper twelve bits and a
// four-bit alpha in its lower bits. Color is white and remains vertex-tintable.
struct MaskSprite {
    unsigned width, height;
    const uint16_t* runs;
    size_t runCount;
};

inline native_sprite::Image DecodeMask(const MaskSprite& sprite) {
    if (!sprite.width || !sprite.height || sprite.width>512 || sprite.height>512 ||
        !sprite.runs || !sprite.runCount || sprite.runCount>size_t(sprite.width)*sprite.height) return {};
    const size_t size=size_t(sprite.width)*sprite.height;
    native_sprite::Image image{sprite.width,sprite.height,std::vector<uint32_t>(size)};
    size_t cursor=0;
    for(size_t i=0;i<sprite.runCount;++i) {
        const auto token=sprite.runs[i];
        const size_t count=token>>4;
        if (!count || count>size-cursor) return {};
        const auto alpha=uint32_t(token&15)*17;
        std::fill_n(image.pixels.begin()+cursor,count,alpha ? (alpha<<24)|0x00ffffffU : 0);
        cursor+=count;
    }
    return cursor==size ? image : native_sprite::Image{};
}
} // namespace mk64_3ds::native_art
