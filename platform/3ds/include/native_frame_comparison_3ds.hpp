#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace mk64_3ds {
struct NativeFrameDifference {
    uint32_t pixels=0,channels=0,maxDelta=0;
    uint64_t absoluteDelta=0;
};
inline bool CompareNativeFrames(const uint8_t* a,const uint8_t* b,size_t bytes,NativeFrameDifference& out) {
    out={};
    if(!a || !b || !bytes || (bytes&3) || bytes>1024*1024)return false;
    for(size_t i=0;i<bytes;i+=4) {
        bool changed=false;
        for(unsigned c=0;c<4;++c) {
            const unsigned x=a[i+c],y=b[i+c],d=x>y?x-y:y-x;
            if(d){changed=true;++out.channels;out.absoluteDelta+=d;if(d>out.maxDelta)out.maxDelta=d;}
        }
        out.pixels+=changed;
    }
    return true;
}
inline bool NativeFrameHasVariation(const uint8_t* pixels,size_t bytes) {
    if(!pixels || bytes<256 || (bytes&3) || bytes>1024*1024)return false;
    size_t different=0;
    for(size_t i=4;i<bytes;i+=4)different+=std::memcmp(pixels,pixels+i,4)!=0;
    return different>bytes/64;
}
}
