#pragma once
#include <cstdint>
namespace mk64_3ds {
// Match LUS default.shader.glsl: cutouts discard alpha <= .19 and render
// surviving RGB opaque. Ordinary threshold/blended surfaces retain 8/256.
constexpr std::uint8_t Fast3DAlphaReference(bool cutout) { return cutout ? 48 : 8; }
constexpr bool Fast3DAlphaBlend(bool useAlpha, bool cutout) { return useAlpha && !cutout; }
struct Fast3DAlphaState { bool blend,alphaTest,cutout; };
// Match interpreter shader-option normalization before the backend policy.
// A blended texture edge becomes an ordinary alpha threshold, not a cutout.
constexpr Fast3DAlphaState ResolveFast3DAlpha(bool useAlpha,bool textureEdge,bool threshold) {
    if(textureEdge) {
        if(useAlpha){threshold=true;textureEdge=false;}
        useAlpha=true;
    }
    return {Fast3DAlphaBlend(useAlpha,textureEdge),textureEdge||threshold,textureEdge};
}
}
