#pragma once
#include <cstdint>
namespace mk64_3ds {
// Match LUS default.shader.glsl: cutouts discard alpha <= .19 and render
// surviving RGB opaque. Ordinary threshold/blended surfaces retain 8/256.
constexpr std::uint8_t Fast3DAlphaReference(bool cutout) { return cutout ? 48 : 8; }
constexpr bool Fast3DAlphaBlend(bool useAlpha, bool cutout) { return useAlpha && !cutout; }
}
