#include "native_frame_comparison_3ds.hpp"
#include <array>
#include <cassert>
using namespace mk64_3ds;
int main() {
    std::array<uint8_t,256> a{},b{};NativeFrameDifference out;
    assert(CompareNativeFrames(a.data(),b.data(),a.size(),out));
    assert(!out.pixels && !out.channels && !out.maxDelta && !out.absoluteDelta);
    assert(!NativeFrameHasVariation(a.data(),a.size()));
    for(unsigned i=0;i<64;++i) {
        a[i*4]=uint8_t(i);a[i*4+1]=uint8_t(255-i);a[i*4+2]=uint8_t(i*3);a[i*4+3]=255;
    }
    assert(NativeFrameHasVariation(a.data(),a.size()));b=a;
    b[0]=255;b[1]-=3;b[4*31+2]-=1;
    assert(CompareNativeFrames(a.data(),b.data(),a.size(),out));
    assert(out.pixels==2 && out.channels==3 && out.maxDelta==255 && out.absoluteDelta==259);
    assert(!CompareNativeFrames(nullptr,b.data(),a.size(),out) && !out.pixels);
    assert(!CompareNativeFrames(a.data(),nullptr,a.size(),out));
    assert(!CompareNativeFrames(a.data(),b.data(),0,out));
    assert(!CompareNativeFrames(a.data(),b.data(),255,out));
    assert(!CompareNativeFrames(a.data(),b.data(),1024*1024+4,out));
    assert(!NativeFrameHasVariation(nullptr,256));
    assert(!NativeFrameHasVariation(a.data(),255));
    assert(!NativeFrameHasVariation(a.data(),1024*1024+4));
    // Compare both directions: unsigned byte arithmetic must not wrap deltas.
    assert(CompareNativeFrames(b.data(),a.data(),a.size(),out));
    assert(out.pixels==2 && out.channels==3 && out.maxDelta==255 && out.absoluteDelta==259);
}
