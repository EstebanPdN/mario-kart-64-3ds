#include "native_sprite_pack_3ds.hpp"
#include "native_sprite_cache_budget_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace mk64_3ds::native_scene;
int main(int argc,char** argv) {
 assert(mk64_3ds::NativeSpriteCacheSlots(false,64*1024*1024)==128);
 assert(mk64_3ds::NativeSpriteCacheSlots(false,6*1024*1024-1)==32);
 assert(mk64_3ds::NativeSpriteCacheSlots(false,6*1024*1024)==128);
 assert(mk64_3ds::NativeSpriteCacheSlots(true,8*1024*1024)==128);
 assert(mk64_3ds::NativeSpriteCacheSlots(true,8*1024*1024-1)==32);
 SpriteResidency<128> bounded;
 assert(!bounded.SetCapacity(0) && !bounded.SetCapacity(129));
 assert(bounded.SetCapacity(32));bounded.BeginAfterCompletion();
 assert(!bounded.SetCapacity(128));
 for(unsigned i=0;i<32;++i){assert(bounded.Candidate()==int(i));assert(bounded.Publish(i,i));}
 assert(bounded.Candidate()==-1 && !bounded.Publish(32,32));
 // Revisit 96 immutable image identities over many completed submissions.
 // This is a cache algorithm fixture, not a game/hardware FPS benchmark.
 auto misses=[](unsigned capacity) {
     SpriteResidency<128> resident;assert(resident.SetCapacity(capacity));unsigned misses=0;
     for(unsigned pass=0;pass<10;++pass)for(unsigned key=0;key<96;++key) {
         resident.BeginAfterCompletion();
         if(resident.Find(key)<0){++misses;const int slot=resident.Candidate();assert(slot>=0 && unsigned(slot)<capacity);assert(resident.Publish(slot,key));}
     }
     return misses;
 };
 assert(misses(32)==960 && misses(128)==96);
 unsigned wheel=9;assert(SpriteIdentity("__OTR__textures/karts/mario_kart/mario_kart_frame001_wheel2",wheel)==SpriteIdentity("textures/karts/mario_kart/mario_kart_frame001",wheel));
 SpriteIdentity("a_wheel3",wheel);assert(wheel==3);
 uint8_t pal[512]{},mask[32]{};mask[0]=1;auto h=SpritePaletteHash(pal,mask);pal[99]=3;assert(SpritePaletteHash(pal,mask)==h);pal[0]=1;assert(SpritePaletteHash(pal,mask)!=h);
 SpriteResidency<3> cache;assert(cache.Candidate()==-1);cache.BeginAfterCompletion();
 for(unsigned i=0;i<3;++i){int slot=cache.Candidate();assert(slot>=0);cache.Publish(slot,i);}
 assert(cache.Candidate()==-1);assert(!cache.Publish(1,99));assert(!cache.Publish(3,99));assert(cache.Find(1)>=0);cache.BeginAfterCompletion();assert(cache.Find(0)>=0 && cache.Find(2)>=0);int slot=cache.Candidate();assert(slot>=0);cache.Publish(slot,3);assert(cache.Candidate()==-1 && cache.Find(1)==-1);
 for(unsigned frame=0;frame<20000;++frame){cache.BeginAfterCompletion();for(unsigned i=0;i<3;++i){unsigned key=frame*3+i;int found=cache.Find(key);if(found<0){found=cache.Candidate();assert(found>=0);cache.Publish(found,key);}assert(cache.Find(key)==found);}assert(cache.Candidate()==-1);}

 std::vector<uint8_t> directory(32+sizeof(SpriteEntry));std::memcpy(directory.data(),"NSP1",4);
 uint32_t header[]={1,1,sizeof(SpriteEntry),64,64,uint32_t(directory.size()),uint32_t(directory.size()+10)};std::memcpy(directory.data()+4,header,sizeof header);
 SpriteEntry entry{};entry.identity=1;entry.paletteMask[0]=1;
 for(unsigned i=0;i<8;++i){entry.offset[i]=directory.size();entry.packed[i]=10;}
 std::memcpy(directory.data()+32,&entry,sizeof entry);std::vector<SpriteEntry> validated;
 assert(ValidateSpriteIndex(directory.data(),directory.size(),directory.size()+10,validated));
 for(size_t size:{size_t(0),size_t(31),directory.size()-1})assert(!ValidateSpriteIndex(directory.data(),size,directory.size()+10,validated) && validated.empty());
 const auto good=directory;
 auto reject=[&](size_t at,uint32_t value){directory=good;std::memcpy(directory.data()+at,&value,4);assert(!ValidateSpriteIndex(directory.data(),directory.size(),good.size()+10,validated));};
 reject(4,2);reject(8,0xffffffff);reject(12,0);reject(16,128);reject(24,0);reject(28,0);
 reject(32,0);reject(32+offsetof(SpriteEntry,offset),0xffffffff);reject(32+offsetof(SpriteEntry,packed),0);
 reject(32+offsetof(SpriteEntry,packed),11);
 if(argc==2){SpritePack pack;assert(pack.Open(argv[1]));assert(pack.IndexBytes()==2568*sizeof(SpriteEntry));unsigned wheel;uint32_t index;
 const auto* e=pack.Find(SpriteIdentity("textures/karts/mario_kart/mario_kart_frame000_wheel0",wheel),index);assert(e);
 std::array<uint8_t,17408> bytes;size_t packed=0;assert(pack.ReadPacked(index,4,bytes.data(),bytes.size(),packed));assert(packed>0);
 std::ifstream f(argv[1],std::ios::binary);std::array<uint8_t,32> header;f.read((char*)header.data(),32);uint32_t n=ReadLE32(header.data()+24);std::vector<uint8_t> data(n);f.seekg(0);f.read((char*)data.data(),n);std::vector<SpriteEntry> entries;
 uint32_t length=ReadLE32(data.data()+28);assert(ValidateSpriteIndex(data.data(),data.size(),length,entries));data.pop_back();assert(!ValidateSpriteIndex(data.data(),data.size(),length,entries));}
}
