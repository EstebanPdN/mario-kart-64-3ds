#pragma once
#include "native_particle_contract_3ds.h"
#include "native_sprite_batch_3ds.hpp"
#include "alpha_policy_3ds.hpp"

namespace mk64_3ds::native_scene {
struct ParticleState {
    uint32_t high,low,geometry;
    uint64_t combine;
    SpriteStyle style;
};
// Typed migration state, deliberately separate from GBI command encoding.
// The final native scene will supply these values directly from its camera
// and material; the development bridge preserves following legacy consumers.
inline ParticleState ResolveParticleState(const Mk64NativeParticle3DS& p,
        Mk64NativeParticleQuad quad,uint32_t high,uint32_t low,uint32_t geometry,bool mirror) {
    // Smoke setup clears LUT; the converted grass caller explicitly does too.
    high=(high&~((3u<<12)|(3u<<14)))|(2u<<12)|(1u<<19);
    geometry&=~0x20000u;
    if(p.setup==MK64_PARTICLE_GRASS_SETUP)low=(low&4u)|0x00553078u;
    if(p.compare!=MK64_PARTICLE_INHERIT_COMPARE) {
        const unsigned compare=p.compare==MK64_PARTICLE_DITHER_COMPARE?3:
            (p.compare==MK64_PARTICLE_THRESHOLD_COMPARE?1:0);
        low=(low&~3u)|compare;
    }
    if(p.raster!=MK64_PARTICLE_INHERIT_RASTER)
        low=(low&7u)|(p.raster==MK64_PARTICLE_BLEND_RASTER?0x00504a50u:0x00553078u);
    // The authored standard quad lists set this mode after the caller's mode.
    if(quad!=MK64_PARTICLE_EXPLICIT_QUAD)low=(low&7u)|0x00504b50u;
    const bool blend=(((low>>20)&3)==1 && ((low>>16)&3)==0) ||
        (((low>>22)&3)==1 && ((low>>18)&3)==0);
    const auto alpha=ResolveFast3DAlpha(blend,(low&0x1000u)!=0,(low&3u)==1);
    SpriteStyle style;style.quads=1;style.blend=alpha.blend;style.cutout=alpha.cutout;
    style.alphaTest=alpha.alphaTest;style.logicalWidth=p.width;style.logicalHeight=p.height;style.clampLogical=true;
    style.cull=(geometry&0x1000?1:0)|(geometry&0x2000?2:0);
    if(mirror && (style.cull==1 || style.cull==2))style.cull=3-style.cull;
    std::memcpy(style.primitive,p.primitive,4);std::memcpy(style.environment,p.environment,4);
    style.technique=p.color==MK64_PARTICLE_TEXTURE_TINT?SpriteStyle::TextureTint:SpriteStyle::ColorLerp;
    const uint64_t rgb=p.color==MK64_PARTICLE_TEXTURE_TINT?
        (1u|(15u<<4)|(3u<<8)|(7u<<13)):(3u|(5u<<4)|(1u<<8)|(5u<<13));
    const uint64_t a=1u|(7u<<3)|(3u<<6)|(7u<<9);
    return {high,low,geometry,rgb|(a<<16)|(rgb<<28)|(a<<44),style};
}
}
