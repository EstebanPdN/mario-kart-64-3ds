#pragma once
#include <cstdint>

namespace mk64_3ds::native_scene {
// Native cache state: explicit PICA-facing choices, with no source opcodes or
// source render-mode words. The offline converter resolves their semantics.
struct RasterState {
    std::uint8_t depthTest=0,depthWrite=0,decal=0,alphaTest=0;
    std::uint8_t alphaReference=8,blend=0,linearFilter=1,alphaOutput=0;
};
static_assert(sizeof(RasterState)==8);
inline bool ValidRasterState(const RasterState& state) {
    return state.depthTest<=1 && state.depthWrite<=1 && state.decal<=1 &&
        state.alphaTest<=1 && (state.alphaReference==8 || state.alphaReference==48) &&
        state.blend<=1 && state.linearFilter<=1 && state.alphaOutput<=1;
}
}
