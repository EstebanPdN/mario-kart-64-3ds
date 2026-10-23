#include "native_fast_page_3ds.hpp"
#include "native_sprite_storage_3ds.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace mk64_3ds::native_scene;
struct Spill {
    static inline size_t live=0,limit=4*1024*1024,attempts=0,fail=0;
    static size_t Available(){return limit;}
    static uint8_t* Allocate(size_t n){++attempts;if(attempts==fail)return nullptr;auto* p=static_cast<uint8_t*>(std::malloc(n));if(p)++live;return p;}
    static void Release(uint8_t* p){if(p){--live;std::free(p);}}
};
int main() {
    std::array<uint8_t,256> out{};
    const uint8_t literal[]={0x40,1,2,3,4};
    assert(DecodeFastSpriteBlock(literal,sizeof literal,out.data(),4));
    assert(out[0]==1 && out[3]==4);
    const uint8_t run[]={0x1f,42,1,0,5,0x50,1,2,3,4,5};
    assert(DecodeFastSpriteBlock(run,sizeof run,out.data(),30));
    for(unsigned i=0;i<25;++i)assert(out[i]==42);
    for(unsigned i=0;i<5;++i)assert(out[25+i]==i+1);
    const uint8_t repeated[]={0x48,1,2,3,4,4,0,0x50,5,6,7,8,9};
    assert(DecodeFastSpriteBlock(repeated,sizeof repeated,out.data(),21));
    for(unsigned i=0;i<16;++i)assert(out[i]==i%4+1);
    for(size_t n=0;n<sizeof repeated;++n)assert(!DecodeFastSpriteBlock(repeated,n,out.data(),21));
    auto broken=std::array<uint8_t,sizeof repeated>{};std::memcpy(broken.data(),repeated,sizeof repeated);
    broken[5]=0;assert(!DecodeFastSpriteBlock(broken.data(),broken.size(),out.data(),21));
    broken[5]=5;assert(!DecodeFastSpriteBlock(broken.data(),broken.size(),out.data(),21));
    assert(!DecodeFastSpriteBlock(repeated,sizeof repeated,out.data(),20));
    assert(!DecodeFastSpriteBlock(repeated,sizeof repeated,out.data(),22));
    const uint8_t text[]="123456789";assert(FastSpriteChecksum(text,9)==0xcbf43926);
    // Fuzz malformed blocks under ASan/UBSan; successful decodes are bounded.
    uint32_t random=1234;
    for(unsigned trial=0;trial<20000;++trial) {
        std::array<uint8_t,128> input{};
        for(auto& v:input){random=random*1664525+1013904223;v=random>>24;}
        DecodeFastSpriteBlock(input.data(),trial%128,out.data(),trial%256);
    }
    FILE* file=std::tmpfile();assert(file);
    std::vector<uint8_t> bytes(2*1024*1024+19);
    for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(i*31+i/117);
    assert(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size());
    {
        SpriteResidentStorage<Spill> storage;
        assert(storage.Load(file,0,bytes.size(),65539));assert(storage.HeapBytes()==65536);
        assert(storage.Bytes()==bytes.size() && Spill::live==2);
        std::vector<uint8_t> actual(bytes.size());assert(storage.Copy(0,actual.data(),actual.size()));assert(actual==bytes);
        for(size_t start:{size_t(65533),size_t(65536+1024*1024-3),bytes.size()-31}) {
            std::array<uint8_t,31> part{};assert(storage.Copy(start,part.data(),part.size()));
            assert(std::memcmp(part.data(),bytes.data()+start,part.size())==0);
        }
        assert(!storage.Copy(bytes.size(),out.data(),1));assert(!storage.Copy(SIZE_MAX,out.data(),1));
        // Failed replacements retain the published storage and free partial spills.
        Spill::fail=Spill::attempts+2;assert(!storage.Load(file,0,bytes.size(),65536));assert(Spill::live==2);
        Spill::fail=0;assert(!storage.Load(file,bytes.size()-1,bytes.size(),65536));assert(Spill::live==2);
        Spill::limit=1;assert(!storage.Load(file,0,bytes.size(),65536));assert(Spill::live==2);Spill::limit=4*1024*1024;
        assert(storage.Copy(0,actual.data(),actual.size()) && actual==bytes);
        assert(storage.Load(file,0,19,100));assert(storage.HeapBytes()==19 && Spill::live==0);
        assert(storage.Copy(0,out.data(),19) && !std::memcmp(out.data(),bytes.data(),19));
    }
    assert(Spill::live==0);std::fclose(file);
    std::puts("PASS: bounded fast blocks, CRC, 20000 malformed inputs, segmented odd-offset reads, allocation/truncation rollback and spill lifetimes.");
}
