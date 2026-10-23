#include "alpha_policy_3ds.hpp"
#include <cassert>
#include <cstdint>
int main() {
    struct Case {bool useAlpha,edge,threshold,blend,test,cutout;uint8_t reference;};
    // Shader-option normalization followed by the renderer's backend contract.
    constexpr Case cases[]={
        {false,false,false,false,false,false,8},
        {false,false,true, false,true, false,8},
        {false,true, false,false,true, true,48},
        {false,true, true, false,true, true,48},
        {true, false,false,true, false,false,8},
        {true, false,true, true, true, false,8},
        {true, true, false,true, true, false,8},
        {true, true, true, true, true, false,8},
    };
    for(const auto& c:cases) {
        const auto state=mk64_3ds::ResolveFast3DAlpha(c.useAlpha,c.edge,c.threshold);
        assert(state.blend==c.blend && state.alphaTest==c.test && state.cutout==c.cutout);
        assert(mk64_3ds::Fast3DAlphaReference(state.cutout)==c.reference);
    }
    // A partially transparent edge must not acquire the opaque-cutout cutoff.
    constexpr auto ghost=mk64_3ds::ResolveFast3DAlpha(true,true,false);
    static_assert(ghost.blend && ghost.alphaTest && !ghost.cutout);
    assert(16>mk64_3ds::Fast3DAlphaReference(ghost.cutout));
    assert(16<=mk64_3ds::Fast3DAlphaReference(true));
}
