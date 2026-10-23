#pragma once
#include "native_mesh_format_3ds.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mk64_3ds::native_scene {
struct DynamicTextureRecord {
    uint16_t texture,width,height,stride;
    uint32_t nameOffset,nameBytes;
};
static_assert(sizeof(DynamicTextureRecord)==16);
// A resource table describes mutable source images separately from immutable
// geometry/materials. Names are bounded, owned by the supplied table at load.
inline bool ValidateDynamicTextureTable(const void* bytes,size_t size,uint32_t textureCount,
        std::vector<DynamicTextureRecord>& records) {
    records.clear();if(!bytes || size<16 || size>16384)return false;
    const auto* data=static_cast<const uint8_t*>(bytes);
    const uint32_t count=ReadLE32(data+8),strings=ReadLE32(data+12);
    if(std::memcmp(data,"NMD1",4) || ReadLE32(data+4)!=1 || !count || count>32 ||
       strings!=16+count*sizeof(DynamicTextureRecord) || strings>=size)return false;
    std::vector<DynamicTextureRecord> checked(count);
    std::memcpy(checked.data(),data+16,count*sizeof(DynamicTextureRecord));
    for(size_t i=0;i<count;++i) {
        const auto& r=checked[i];
        if(r.texture>=textureCount || r.width<8 || r.height<8 || r.width>1024 || r.height>1024 ||
           (r.width&(r.width-1)) || (r.height&(r.height-1)) || r.stride<r.width ||
           r.nameOffset<strings || r.nameOffset>=size || r.nameBytes<2 || r.nameBytes>256 ||
           r.nameBytes>size-r.nameOffset || data[r.nameOffset+r.nameBytes-1] ||
           std::memchr(data+r.nameOffset,0,r.nameBytes-1))return false;
        for(size_t j=0;j<i;++j)if(checked[j].texture==r.texture)return false;
    }
    records=std::move(checked);return true;
}

// Big-endian RGBA5551 to vertically inverted, 8x8 Morton-tiled ABGR8, matching
// the production texture decoder and PICA texture memory. No allocation.
inline bool TileRgba5551(const uint8_t* source,size_t sourceBytes,unsigned stride,
        unsigned width,unsigned height,uint8_t* destination,size_t destinationBytes) {
    if(!source || !destination || width<8 || height<8 || width>1024 || height>1024 ||
       (width&(width-1)) || (height&(height-1)) || stride<width || stride>65535 ||
       sourceBytes<size_t(stride)*height*2 || destinationBytes!=size_t(width)*height*4)return false;
    const auto input=reinterpret_cast<uintptr_t>(source),output=reinterpret_cast<uintptr_t>(destination);
    if(input<=output ? output-input<sourceBytes : input-output<destinationBytes)return false;
    for(unsigned tileY=0;tileY<height;tileY+=8)for(unsigned tileX=0;tileX<width;tileX+=8) {
        auto* tile=destination+((size_t(tileY/8)*(width/8)+tileX/8)*64)*4;
        for(unsigned y=0;y<8;++y) {
            const auto* row=source+(size_t(height-1-tileY-y)*stride+tileX)*2;
            for(unsigned x=0;x<8;++x) {
                const uint16_t value=uint16_t(row[x*2])<<8|row[x*2+1];
                const unsigned morton=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
                auto* pixel=tile+morton*4;
                pixel[0]=(value&1)?255:0;
                pixel[1]=((value>>1)&31)*255/31;
                pixel[2]=((value>>6)&31)*255/31;
                pixel[3]=(value>>11)*255/31;
            }
        }
    }
    return true;
}
}
