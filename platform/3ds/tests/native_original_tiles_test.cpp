#include "native_original_tiles_3ds.hpp"
#include <cassert>
#include <fstream>
using namespace mk64_3ds::native_scene;
static void put(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=n>>(i*8);}
static void save(const char* name,const std::vector<uint8_t>& b){std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
static uint32_t crc(const uint8_t* p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^((v&1)?0xedb88320:0);}return ~v;}
int main(){
 std::vector<uint8_t> archive(201);std::memcpy(archive.data(),"NSP2",4);
 const uint32_t h[]={2,1,168,64,64,200,201};std::memcpy(archive.data()+4,h,sizeof h);
 SpriteEntry e{};e.identity=1;e.paletteMask[0]=1;
 for(unsigned i=0;i<8;++i){e.offset[i]=200;e.packed[i]=1;e.crc[i]=i+1;}
 std::memcpy(archive.data()+32,&e,sizeof e);save("tiles-fixture.nsp",archive);SpritePack pack;assert(pack.Open("tiles-fixture.nsp"));
 // Four images, each alternating between two distinct GPU-native tiles.
 constexpr unsigned index=48,tiles=index+1024;
 std::vector<uint8_t> bytes(tiles+256);std::memcpy(bytes.data(),"NTL1",4);
 put(bytes,4,1);put(bytes,8,1);put(bytes,12,2);put(bytes,16,index);put(bytes,20,tiles);put(bytes,24,bytes.size());
 for(unsigned lane=0;lane<4;++lane){put(bytes,32+lane*4,lane+1);for(unsigned t=0;t<64;++t)put(bytes,index+(lane*64+t)*4,(lane+t)%2);}
 for(unsigned i=0;i<256;++i)bytes[tiles+i]=i<128?0x1f:0xe0;
 const auto checksum=[&](){put(bytes,28,crc(bytes.data()+32,bytes.size()-32));};checksum();save("tiles-fixture.ntl",bytes);
 OriginalSpriteTiles resident;assert(!resident.Load("tiles-fixture.ntl",tiles-1,pack));assert(!resident.ResidentBytes());
 assert(resident.Load("tiles-fixture.ntl",bytes.size(),pack));assert(resident.HeapBytes()==bytes.size());
 std::array<uint8_t,8192> out{};
 for(unsigned lane=0;lane<4;++lane){assert(resident.Copy(lane,out.data(),out.size()));for(unsigned t=0;t<64;++t)for(unsigned i=0;i<128;++i)assert(out[t*128+i]==((lane+t)%2?0xe0:0x1f));}
 assert(!resident.Copy(4,out.data(),out.size()));assert(!resident.Copy(8,out.data(),out.size()));assert(!resident.Copy(0,out.data(),8191));assert(!resident.Copy(0,nullptr,8192));assert(resident.Drop());assert(!resident.Drop());
 auto good=bytes;
 const auto reject=[&](){save("tiles-invalid.ntl",bytes);OriginalSpriteTiles invalid;assert(!invalid.Load("tiles-invalid.ntl",65536,pack));assert(!invalid.ResidentBytes());};
 bytes.back()^=1;reject(); // Invalid payload checksum.
 bytes=good;put(bytes,index,2);checksum();reject(); // Valid checksum, invalid tile index.
 bytes=good;put(bytes,32,99);checksum();reject(); // Stale pack with different image identity.
 bytes=good;put(bytes,8,0xffffffff);reject();
 bytes=good;bytes.pop_back();reject(); // Truncation must not publish partial storage.
 bytes=good;bytes.push_back(0);reject();
 std::remove("tiles-fixture.nsp");std::remove("tiles-fixture.ntl");std::remove("tiles-invalid.ntl");
 std::puts("PASS: original native tiles, exact image/lane/tile mapping, budget and lifetime, malformed/stale/truncated data rejected.");
}
