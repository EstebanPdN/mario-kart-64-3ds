#pragma once
#include "native_sprite_page_3ds.hpp"
#include <algorithm>

namespace mk64_3ds::native_scene {
// Bounded LZ4 block decoding for offline-prepared character pages. This is an
// independent block-format implementation; no frame library or allocator runs
// during drawing. The final CRC covers the original interleaved page bytes.
inline bool DecodeFastSpriteBlock(const uint8_t* input,size_t bytes,uint8_t* output,size_t capacity) {
    if(!input || !output)return false;
    size_t in=0,out=0;
    auto length=[&](size_t& n) {
        if(n!=15)return true;
        unsigned b;
        do {if(in==bytes)return false;b=input[in++];if(n>capacity || b>capacity-n)return false;n+=b;} while(b==255);
        return true;
    };
    while(in<bytes) {
        const unsigned token=input[in++];size_t literal=token>>4;
        if(!length(literal) || literal>bytes-in || literal>capacity-out)return false;
        std::memcpy(output+out,input+in,literal);in+=literal;out+=literal;
        if(in==bytes)return out==capacity;
        if(bytes-in<2)return false;
        const size_t distance=size_t(input[in])|(size_t(input[in+1])<<8);in+=2;
        if(!distance || distance>out)return false;
        size_t match=token&15;
        if(!length(match) || capacity-out<4 || match>capacity-out-4)return false;
        match+=4;
        // Copy complete non-overlapping spans, preserving repeated short runs.
        if(distance==1){std::memset(output+out,output[out-1],match);out+=match;}
        else {size_t span=distance;while(match) {const size_t n=std::min(match,span);std::memcpy(output+out,output+out-span,n);out+=n;match-=n;span+=n;}}
    }
    return false;
}
inline uint32_t FastSpriteChecksum(const uint8_t* data,size_t size) {
    static constexpr auto table=[] {
        std::array<uint32_t,256> values{};
        for(unsigned i=0;i<256;++i){uint32_t c=i;for(unsigned bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320u:0);values[i]=c;}
        return values;
    }();
    uint32_t crc=~0u;for(size_t i=0;i<size;++i)crc=(crc>>8)^table[(crc^data[i])&255];return ~crc;
}
inline bool DecodeFastSpritePage(const uint8_t* packed,size_t bytes,uint8_t* output,uint8_t* scratch) {
    if(!packed || bytes<=4 || bytes>kSpritePackedCapacity || !output || !scratch ||
       !DecodeFastSpriteBlock(packed+4,bytes-4,scratch,kSpritePageBytes))return false;
    std::memcpy(output,scratch,16);
    constexpr size_t plane=(kSpritePageBytes-16)/4;
    for(size_t i=0;i<plane;++i)for(unsigned c=0;c<4;++c)output[16+i*4+c]=scratch[16+c*plane+i];
    return FastSpriteChecksum(output,kSpritePageBytes)==SpritePageU32(packed);
}
}
