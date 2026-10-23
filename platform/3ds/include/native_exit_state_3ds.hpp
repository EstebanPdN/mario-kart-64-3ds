#pragma once
#include "native_mesh_format_3ds.hpp"
#include <vector>

namespace mk64_3ds::native_scene {
// Migration-only compatibility state. These are final typed values, never
// an opcode stream. Native actors will remove the need for this boundary.
struct ExitTile {uint32_t format,size,line,memory,wrapS,wrapT,maskS,maskT,shiftS,shiftT,palette,lowS,lowT,highS,highT;};
struct ExitLoaded {uint32_t texture,size,width,bytes;};
struct ExitState {
    uint32_t shader,cull,scaleWritten,sourceWritten,scaleS,scaleT,renderTile,sourceTexture,sourceSize,sourceWidth;
    uint32_t tileMask,boundsMask,loadedMask,slotMaskLow,slotMaskHigh;
    ExitTile tiles[8];ExitLoaded loaded[2];uint32_t vertices[64];
};
static_assert(sizeof(ExitState)==828);
inline bool ValidateExits(const void* bytes,size_t size,uint32_t vertices,std::vector<ExitState>& out,uint32_t& textures,
                          uint32_t expectedCount=123,uint32_t maxShader=2,uint32_t maxTextures=64) {
    out.clear();textures=0;if(!bytes || size<16)return false;
    const auto* p=static_cast<const uint8_t*>(bytes);const uint32_t count=ReadLE32(p+8),ntex=ReadLE32(p+12);
    if(std::memcmp(p,"NCX1",4) || ReadLE32(p+4)!=1 || expectedCount>4096 || !expectedCount || maxShader>3 ||
       count!=expectedCount || ntex>maxTextures || !ntex || size!=16+count*sizeof(ExitState))return false;
    std::vector<ExitState> checked(count);std::memcpy(checked.data(),p+16,count*sizeof(ExitState));
    for(const auto& s:checked) {
        if((s.shader>maxShader && s.shader!=~0u) || (s.cull>3 && s.cull!=~0u) || s.scaleWritten>1 || s.sourceWritten>1 ||
           s.scaleS>65535 || s.scaleT>65535 || s.renderTile>7 || s.tileMask>255 || s.boundsMask>255 || s.loadedMask>3 ||
           (s.sourceWritten && (s.sourceTexture>=ntex || s.sourceSize>3 || s.sourceWidth>65536)))return false;
        for(unsigned i=0;i<8;++i) {
            const auto& t=s.tiles[i];
            if(t.format>4 || t.size>3 || t.line>511 || t.memory>511 || t.wrapS>3 || t.wrapT>3 ||
               t.maskS>15 || t.maskT>15 || t.shiftS>15 || t.shiftT>15 || t.palette>15 ||
               t.lowS>4095 || t.lowT>4095 || t.highS>4095 || t.highT>4095)return false;
        }
        for(unsigned i=0;i<2;++i)if(s.loadedMask&(1u<<i)) {
            const auto& l=s.loaded[i];if(l.texture>=ntex || l.size>3 || l.width>65536 || !l.bytes || l.bytes>65536)return false;
        }
        for(unsigned i=0;i<64;++i)if(((i<32?s.slotMaskLow:s.slotMaskHigh)>>(i&31))&1u) {
            if(s.vertices[i]>=vertices)return false;
        }
    }
    out=std::move(checked);textures=ntex;return true;
}
}
