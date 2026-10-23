#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mk64_3ds {
// Resource metadata flag, independent of the archive's original N64 format.
inline constexpr uint32_t kNativeGpuTextureFlag=1u<<16;
constexpr size_t MortonTexel(unsigned x,unsigned y,unsigned width) {
    const unsigned m=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
    return ((size_t(y/8)*(width/8)+x/8)*64+m)*4;
}
// Copy existing GPU-ready tiles. Full images and aligned vertical strips use
// one memcpy; no image reconstruction or per-frame format conversion.
inline bool CopyNativeKartUpload(const uint8_t* source,uint8_t* output,
        unsigned bw,unsigned bh,unsigned width,unsigned height,
        size_t logicalOffset,size_t lineBytes,size_t sourceStep,size_t loadedBytes) {
    if(!source || !output || !width || !height || bw<width || bh<height ||
       bw>512 || bh>512 || bw<8 || bh<8 || (bw&(bw-1)) || (bh&(bh-1)) ||
       !lineBytes || !sourceStep || logicalOffset>16384 || lineBytes>16384 ||
       sourceStep>16384 || loadedBytes>16384 || ((logicalOffset|lineBytes|sourceStep|loadedBytes)&3)) return false;
    // Validate every logical row before publishing anything to the GPU.
    for(size_t row=0;row*lineBytes<loadedBytes;++row) {
        const size_t count=loadedBytes-row*lineBytes<lineBytes?loadedBytes-row*lineBytes:lineBytes;
        if(row>16384/sourceStep || logicalOffset>16384 ||
           row*sourceStep>16384-logicalOffset || count>16384-logicalOffset-row*sourceStep) return false;
    }
    if(width==64 && bw==64 && bh==height && lineBytes==256 && sourceStep==256 &&
       loadedBytes==size_t(width)*height*4 && logicalOffset%256==0 &&
       logicalOffset/256+height<=64 && height>=8) {
        std::memcpy(output,source+(64-logicalOffset/256-height)*256,size_t(width)*height*4);
        return true;
    }
    for(unsigned y=0;y<bh;++y) for(unsigned x=0;x<bw;++x) {
        const unsigned sy=(bh-1-y)%height,sx=x%width;
        const size_t linear=(size_t(sy)*width+sx)*4;
        auto* dst=output+MortonTexel(x,y,bw);
        if(linear>=loadedBytes) {std::memset(dst,0,4);continue;}
        const size_t logical=logicalOffset+(linear/lineBytes)*sourceStep+linear%lineBytes;
        if(logical>16380)return false;
        std::memcpy(dst,source+MortonTexel((logical/4)%64,63-(logical/4)/64,64),4);
    }
    return true;
}
}
