#include "native_texture_storage_3ds.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <chrono>
int main(){
 using namespace mk64_3ds;
 std::array<uint8_t,16384> linear{},native{},expected{},actual{};
 for(size_t i=0;i<linear.size();++i)linear[i]=uint8_t((i*137+i/7)%256);
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
  for(unsigned c=0;c<4;++c)native[MortonTexel(x,63-y,64)+c]=linear[(y*64+x)*4+3-c];
 unsigned cases=0;
 for(unsigned width:{8,16,32,64})for(unsigned height:{8,16,24,32,48,64})
  for(unsigned y0:{0,8,16,32})for(unsigned x0:{0,8,16,32}){
   if(x0+width>64 || y0+height>64)continue;
   unsigned bh=8;while(bh<height)bh*=2;
   expected.fill(0);actual.fill(0);
   for(unsigned y=0;y<bh;++y)for(unsigned x=0;x<width;++x){
    auto* dst=expected.data()+MortonTexel(x,y,width);
    const auto at=((y0+(bh-1-y)%height)*64+x0+x)*4;
    for(unsigned c=0;c<4;++c)dst[c]=linear[at+3-c];
   }
   assert(CopyNativeKartUpload(native.data(),actual.data(),width,bh,width,height,(y0*64+x0)*4,width*4,256,width*height*4));
   assert(actual==expected);++cases;
  }
 assert(!CopyNativeKartUpload(native.data(),actual.data(),64,64,64,64,4,256,256,16384));
 assert(!CopyNativeKartUpload(native.data(),actual.data(),64,64,64,64,0,0,256,16384));
 std::printf("PASS: %u GPU tile/strip/padded/subrect cases preserve every byte; invalid ranges rejected.\n",cases);
}
