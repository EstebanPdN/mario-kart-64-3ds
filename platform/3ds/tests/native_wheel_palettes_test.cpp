#include "native_wheel_palettes_3ds.hpp"
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
using mk64_3ds::native_scene::WheelPalettes;
static void put(std::vector<uint8_t>& b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=n>>(i*8);}
static void save(const std::vector<uint8_t>& b){std::ofstream f("wheel-fixture.nwp",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
int main(){
 std::vector<uint8_t> b(32+WheelPalettes::kBytes);std::memcpy(b.data(),"NWP1",4);
 unsigned h[]={1,8,289,4,128,unsigned(b.size()),0};for(unsigned i=0;i<7;++i)put(b,4+i*4,h[i]);
 for(size_t i=32;i<b.size();++i)b[i]=uint8_t((i-32)/128);
 uint32_t crc=~0u;for(size_t i=32;i<b.size();++i){crc^=b[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}put(b,28,~crc);save(b);
 WheelPalettes pool;assert(!pool.Find(nullptr));assert(pool.Load("wheel-fixture.nwp"));assert(pool.ResidentBytes()==WheelPalettes::kBytes);
 const char* names[]={"mario","luigi","yoshi","toad","donkeykong","wario","peach","bowser"};
 for(unsigned c=0;c<8;++c)for(unsigned frame=0;frame<289;++frame)for(unsigned lane=0;lane<4;++lane){
  char name[128];std::snprintf(name,sizeof name,"textures/karts/%s_kart/%s_kart_%03u_tlut_wheel_%u",names[c],names[c],frame,lane);
  const auto* p=pool.Find(name);assert(p && std::memcmp(p,b.data()+32+((c*289+frame)*4+lane)*128,128)==0);
  std::string otr="__OTR__";otr+=name;assert(pool.Find(otr.c_str())==p);
 }
 for(const char* bad:{"", "textures/karts/mario_kart/mario_kart_289_tlut_wheel_0", "textures/karts/mario_kart/mario_kart_000_tlut_wheel_4", "textures/karts/mario_kart/mario_kart_000_tlut_wheel_0x", "textures/karts/mario_kart/luigi_kart_000_tlut_wheel_0", "textures/karts/mario_kart/mario_kart_0a0_tlut_wheel_0"})assert(!pool.Find(bad));
 auto original=b;
 const auto reject=[&](){save(b);WheelPalettes invalid;assert(!invalid.Load("wheel-fixture.nwp"));assert(!invalid.ResidentBytes());assert(!invalid.Find("textures/karts/mario_kart/mario_kart_000_tlut_wheel_0"));};
 b.back()^=1;reject();b=original;b.pop_back();reject();b=original;b.push_back(0);reject();b=original;put(b,8,9);reject();
 assert(pool.Load("missing-file")); // Published immutable data is stable for its owner lifetime.
 std::remove("wheel-fixture.nwp");std::puts("PASS: all 9248 palette addresses, exact bytes, strict canonical names and malformed pool rejection.");
}
