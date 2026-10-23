#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace mk64_3ds::native_scene {
struct SpriteStyle {
    uint8_t primitive[4]{},environment[4]{};
    bool blend=false,cutout=true,alphaTest=true;
    uint8_t cull=0,quads=2;
    enum Technique : uint8_t { Colorize, ConstantRgb, TextureTint, ColorLerp } technique=Colorize;
    // Logical image bounds inside the common 64x64 backing. Unbounded keeps
    // the original character path byte-for-byte in its established UV policy.
    uint8_t logicalWidth=64,logicalHeight=64;
    bool clampLogical=false;
};
inline bool SameSpriteMaterial(const SpriteStyle& a,const SpriteStyle& b) {
    return a.blend==b.blend && a.cutout==b.cutout && a.alphaTest==b.alphaTest &&
        a.cull==b.cull && a.technique==b.technique &&
        ((a.technique==SpriteStyle::TextureTint || a.technique==SpriteStyle::ColorLerp) ||
         std::memcmp(a.primitive,b.primitive,4)==0) &&
        (a.technique==SpriteStyle::ConstantRgb || a.technique==SpriteStyle::TextureTint ||
         std::memcmp(a.environment,b.environment,4)==0);
}
// Each submitted range owns its indices until the GPU completion fence. The
// shared vertex base remains fixed throughout a batch; no submitted bytes move.
inline unsigned SpriteIndices(unsigned base,unsigned quads,uint16_t* output,size_t capacity) {
    if(!output || !quads || quads>2 || capacity<quads*6 || base>65536-quads*4)return 0;
    constexpr unsigned pattern[6]={0,1,2,0,2,3};
    for(unsigned q=0;q<quads;++q)for(unsigned i=0;i<6;++i)output[q*6+i]=base+q*4+pattern[i];
    return quads*6;
}
// Every native sprite is an ordered sequence of independent quads. Prepare
// this immutable index stream once; frame draws only select a subrange.
inline bool PrepareSpriteIndices(unsigned quads,uint16_t* output,size_t capacity) {
    if(!output || !quads || quads>16384 || capacity<size_t(quads)*6)return false;
    for(unsigned q=0;q<quads;++q)
        if(SpriteIndices(q*4,1,output+q*6,6)!=6)return false;
    return true;
}

}
