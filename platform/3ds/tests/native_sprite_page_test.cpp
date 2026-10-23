#include "native_sprite_pack_3ds.hpp"
#include "heap_policy_3ds.hpp"
#include "improved_texture_pack_3ds.hpp"
#include "native_sprite_page_trace_3ds.hpp"
#include <cassert>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <filesystem>
using namespace mk64_3ds::native_scene;
using mk64_3ds::improved::DecodePayload;
static void U32(std::vector<uint8_t>& b,size_t i,uint32_t v){std::memcpy(b.data()+i,&v,4);}
static void Save(const std::string& name,const std::vector<uint8_t>& b){std::ofstream f(name,std::ios::binary);f.write((const char*)b.data(),b.size());assert(f.good());}
int main(int argc,char** argv){
 // A replacement reuses its old allocation but must first repay a reserve deficit.
 static_assert(mk64_3ds::ArchiveResidencyBudget(32u<<20,28u<<20,16u<<20)==12u<<20);
 static_assert(mk64_3ds::ArchiveResidencyBudget(32u<<20,20u<<20,16u<<20)==20u<<20);
 static_assert(mk64_3ds::ArchiveResidencyBudget(32u<<20,32u<<20,4u<<20)==0);

 std::vector<uint8_t> page(kSpritePageBytes);std::array<std::array<uint8_t,kSpriteImageBytes>,4> images;
 for(unsigned lane=0;lane<4;++lane){for(unsigned i=0;i<kSpriteImageBytes;++i)images[lane][i]=uint8_t(i*17+lane*61);U32(page,lane*4,crc32(0,images[lane].data(),kSpriteImageBytes));}
 std::memcpy(page.data()+16,images[0].data(),kSpriteImageBytes);
 for(unsigned lane=1;lane<4;++lane)for(unsigned i=0;i<kSpriteImageBytes;++i)page[16+lane*kSpriteImageBytes+i]=images[0][i]^images[lane][i];
 uLongf cap=compressBound(page.size());std::vector<uint8_t> payload(cap+4);U32(payload,0,crc32(0,page.data(),page.size()));assert(compress2(payload.data()+4,&cap,page.data(),page.size(),6)==Z_OK);payload.resize(cap+4);
 std::vector<uint8_t> decoded(kSpritePageBytes);assert(DecodeSpritePage(payload.data(),payload.size(),decoded.data(),DecodePayload));assert(decoded==page);
 std::array<uint8_t,kSpriteImageBytes> output{};
 for(unsigned lane=0;lane<4;++lane){assert(ExtractSpritePage(decoded.data(),lane,SpritePageU32(page.data()+lane*4),output.data()));assert(output==images[lane]);}
 assert(!ExtractSpritePage(page.data(),4,0,output.data()));assert(!ExtractSpritePage(page.data(),0,1234,output.data()));
 auto damaged=payload;damaged[0]^=1;assert(!DecodeSpritePage(damaged.data(),damaged.size(),decoded.data(),DecodePayload));
 for(size_t size=0;size<payload.size();++size)assert(!DecodeSpritePage(payload.data(),size,decoded.data(),DecodePayload));
 SpritePageCache<2> cache;assert(cache.Initialize());unsigned loads=0;
 auto loader=[&](uint8_t* out){++loads;return DecodeSpritePage(payload.data(),payload.size(),out,DecodePayload);};
 for(unsigned i=0;i<1000;++i){assert(cache.Load(i%2,loader));}assert(loads==2);
 assert(!cache.Load(2,[](uint8_t*){return false;}));assert(cache.Load(2,loader));assert(loads==3);
 // A real eight-page repeated working set thrashes four slots. Growth at a
 // loading boundary eliminates its subsequent decodes without changing bytes.
 SpritePageCache<16> adaptive;assert(adaptive.Initialize(4));assert(!adaptive.Initialize());
 unsigned decodedPages=0;
 auto request=[&](unsigned key){auto* p=adaptive.Load(key,[&](uint8_t* dst){++decodedPages;std::memset(dst,key,kSpritePageBytes);return true;});
     assert(p && p[0]==key && p[kSpritePageBytes-1]==key);};
 for(unsigned i=0;i<80;++i)request(i%8);assert(decodedPages==80);
 assert(!adaptive.Resize(0) && !adaptive.Resize(17) && adaptive.ActiveCapacity()==4);
 assert(adaptive.Resize(8) && adaptive.Bytes()==8*kSpritePageBytes);
 for(unsigned i=0;i<80;++i)request(i%8);assert(decodedPages==88);
 assert(adaptive.Resize(4));for(unsigned i=0;i<80;++i)request(i%8);assert(decodedPages==168);
 static_assert(DecodedSpritePageCapacity(false,64u<<20,0)==4);
 static_assert(DecodedSpritePageCapacity(true,16*kSpritePageBytes-1,0)==8);
 static_assert(DecodedSpritePageCapacity(true,16*kSpritePageBytes,0)==16);
 static_assert(DecodedSpritePageCapacity(true,16*kSpritePageBytes,16*kSpritePageBytes)==4);
 static_assert(DecodedSpritePageCapacity(true,16*kSpritePageBytes,32*kSpritePageBytes)==16);
 NativeSpritePageTrace trace;
 for(unsigned i=0;i<2049;++i)trace.Record(i%500,8,(i%3)==0);
 assert(trace.requests==2049 && (trace.events[0]&65535)==2048%500);
 assert((trace.events[0]>>16&255)==8 && (trace.events[0]>>31)==0);
 // Real SpritePack file path, budget failure, selected-mode residency, mode
 // replacement and release: resident reads must report zero physical bytes.
 const uint32_t start=32+168;std::vector<uint8_t> pack(start+payload.size()*2);std::memcpy(pack.data(),"NSP2",4);
 const uint32_t header[]={2,1,168,64,64,start,uint32_t(pack.size())};std::memcpy(pack.data()+4,header,sizeof header);
 SpriteEntry e{};e.identity=1;e.paletteMask[0]=1;
 for(unsigned v=0;v<8;++v){e.offset[v]=start+(v/4)*payload.size();e.packed[v]=payload.size();e.crc[v]=SpritePageU32(page.data()+(v%4)*4);}
 std::memcpy(pack.data()+32,&e,sizeof e);std::memcpy(pack.data()+start,payload.data(),payload.size());std::memcpy(pack.data()+start+payload.size(),payload.data(),payload.size());
 std::vector<SpriteEntry> entries;assert(ValidateSpriteIndex(pack.data(),start,pack.size(),entries));auto bad=pack;U32(bad,32+40+4,start+1);assert(!ValidateSpriteIndex(bad.data(),start,bad.size(),entries));
 Save("native-karts.nsp",pack);SpritePack source;assert(source.Open("native-karts.nsp") && source.IsPaged());
 assert(source.RequiredResidentBytes(false)==payload.size() && source.RequiredResidentBytes(true)==payload.size());
 std::vector<uint8_t> packed(kSpritePackedCapacity);size_t bytes=0;
 assert(!source.MakeResident(true,payload.size()-1));assert(source.ReadPacked(0,4,packed.data(),packed.size(),bytes));assert(source.LastReadBytes()==payload.size());
 assert(source.MakeResident(true,payload.size()) && source.ResidentBytes()==payload.size());
 for(unsigned i=0;i<1000;++i){assert(source.ReadPacked(0,4+i%4,packed.data(),packed.size(),bytes));assert(source.LastReadBytes()==0);assert(std::equal(payload.begin(),payload.end(),packed.begin()));}
 assert(source.ReadPacked(0,0,packed.data(),packed.size(),bytes) && source.LastReadBytes()==payload.size());
 assert(source.MakeResident(false,payload.size()));assert(source.ReadPacked(0,0,packed.data(),packed.size(),bytes) && source.LastReadBytes()==0);
 assert(source.DropResident() && !source.DropResident());assert(source.ReadPacked(0,0,packed.data(),packed.size(),bytes) && source.LastReadBytes()==payload.size());
 // Actual Improved pack alias reader consumes the same page representation.
 struct Alias{uint64_t hash;unsigned lane;};std::vector<Alias> aliases;
 for(unsigned lane=0;lane<4;++lane)aliases.push_back({mk64_3ds::improved::Hash(("native"+std::to_string(lane)).c_str()),lane});
 std::sort(aliases.begin(),aliases.end(),[](auto a,auto b){return a.hash<b.hash;});std::vector<uint8_t> mui(16+4*32);std::memcpy(mui.data(),"MUI3",4);U32(mui,4,1);U32(mui,8,4);U32(mui,12,32);
 for(unsigned i=0;i<4;++i){unsigned lane=aliases[i].lane,b=16+i*32;std::memcpy(mui.data()+b,&aliases[i].hash,8);U32(mui,b+8,start+payload.size());U32(mui,b+12,payload.size());U32(mui,b+16,e.crc[lane]);const uint16_t dimensions[]={64,64,64,64};std::memcpy(mui.data()+b+20,dimensions,8);U32(mui,b+28,2|(lane<<8));}
 Save("improved.bin",mui);mk64_3ds::improved::Pack ui;assert(ui.Open("improved.bin"));
 for(unsigned lane=0;lane<4;++lane){auto image=ui.Load(("native"+std::to_string(lane)).c_str(),64,64);assert(image && image.nativeGpuTiled);assert(!std::memcmp(image.pixels->data(),images[lane].data(),kSpriteImageBytes));}
 if(argc==2){SpritePack actual;assert(actual.Open(argv[1]) && actual.IsPaged());std::array<uint8_t,kSpriteImageBytes> result;unsigned verified=0;
  for(bool improved:{false,true}){assert(actual.MakeResident(improved,32*1024*1024));for(unsigned i=0;const auto* entry=actual.At(i);++i){for(unsigned lane=0;lane<4;++lane){unsigned v=improved*4+lane;assert(actual.ReadPacked(i,v,packed.data(),packed.size(),bytes) && actual.LastReadBytes()==0);assert(DecodeSpritePage(packed.data(),bytes,decoded.data(),DecodePayload));assert(ExtractSpritePage(decoded.data(),lane,entry->crc[v],result.data()));assert(crc32(0,result.data(),result.size())==entry->crc[v]);++verified;}}}assert(verified==20544);}
 std::filesystem::remove("improved.bin");std::filesystem::remove("native-karts.nsp");
}
