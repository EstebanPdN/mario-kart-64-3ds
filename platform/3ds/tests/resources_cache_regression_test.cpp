// Actual production cache code; synthetic pixels, isolated disposable files.
#include "improved_texture_pack_3ds.hpp"
#include "read_ahead_file_3ds.hpp"
#include "texture_retention_3ds.hpp"
#include "heap_policy_3ds.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>

using namespace mk64_3ds;
using namespace mk64_3ds::native_scene;
using mk64_3ds::improved::Pack;
static size_t failSize=SIZE_MAX, observeSize=SIZE_MAX;
static void (*onAllocation)()=nullptr;
void* operator new(size_t n) {
    if(n==observeSize && onAllocation) {auto hook=onAllocation;onAllocation=nullptr;hook();}
    if(n==failSize){failSize=SIZE_MAX;throw std::bad_alloc();}
    if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](size_t n){return ::operator new(n);}
void* operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void operator delete[](void* p,size_t)noexcept{std::free(p);}
static void Put(std::vector<uint8_t>& out,size_t at,uint64_t v,unsigned bytes=4) {
    for(unsigned b=0;b<bytes;++b)out[at+b]=uint8_t(v>>(b*8));
}
static void Save(const char* name,const std::vector<uint8_t>& bytes) {
    std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(f.good());
}
struct ImageSpec {std::string name;unsigned width,height;uint32_t flags,crc,offset;std::vector<uint8_t> packed;};
static std::vector<ImageSpec> specs;
static std::vector<uint8_t> native;
static std::vector<uint8_t> mui;
static void MakeFixture() {
    specs.clear();native.assign(200,0);std::memcpy(native.data(),"NSP2",4);
    const uint32_t header[]={2,1,168,64,64,200,0};std::memcpy(native.data()+4,header,sizeof(header));
    for(unsigned p=0;p<12;++p) {
        std::vector<uint8_t> page(kSpritePageBytes);
        std::array<std::vector<uint8_t>,4> image;
        for(unsigned lane=0;lane<4;++lane) {
            image[lane].resize(kSpriteImageBytes);
            for(unsigned i=0;i<kSpriteImageBytes;++i)image[lane][i]=uint8_t((i*37+i/19+p*23+lane*61)^(i>>(p%5)));
            Put(page,lane*4,crc32(0,image[lane].data(),image[lane].size()));
        }
        std::copy(image[0].begin(),image[0].end(),page.begin()+16);
        for(unsigned lane=1;lane<4;++lane)for(unsigned i=0;i<kSpriteImageBytes;++i)page[16+lane*kSpriteImageBytes+i]=image[0][i]^image[lane][i];
        uLongf size=compressBound(page.size());std::vector<uint8_t> packed(size+4);
        Put(packed,0,crc32(0,page.data(),page.size()));
        assert(compress2(packed.data()+4,&size,page.data(),page.size(),1)==Z_OK);packed.resize(size+4);
        auto offset=native.size();native.insert(native.end(),packed.begin(),packed.end());
        for(unsigned lane=0;lane<4;++lane)specs.push_back({"kart"+std::to_string(p)+"/wheel"+std::to_string(lane),64,64,2|(lane<<8),uint32_t(crc32(0,image[lane].data(),image[lane].size())),uint32_t(offset),packed});
    }
    Put(native,28,native.size());
    for(unsigned n=0;n<10;++n) {
        std::vector<uint8_t> image(512*128*4,uint8_t(n+1));
        uLongf size=compressBound(image.size());std::vector<uint8_t> packed(size);
        assert(compress2(packed.data(),&size,image.data(),image.size(),1)==Z_OK);packed.resize(size);
        specs.push_back({"direct"+std::to_string(n),512,128,0,uint32_t(crc32(0,image.data(),image.size())),0,std::move(packed)});
    }
    std::sort(specs.begin(),specs.end(),[](const auto& a,const auto& b){return improved::Hash(a.name.c_str())<improved::Hash(b.name.c_str());});
    mui.assign(16+specs.size()*32,0);std::memcpy(mui.data(),"MUI3",4);Put(mui,4,1);Put(mui,8,specs.size());Put(mui,12,32);
    for(size_t i=0;i<specs.size();++i) {
        auto& e=specs[i];const size_t at=16+i*32;
        if(!e.flags){e.offset=mui.size();mui.insert(mui.end(),e.packed.begin(),e.packed.end());}
        Put(mui,at,improved::Hash(e.name.c_str()),8);Put(mui,at+8,e.offset);Put(mui,at+12,e.packed.size());Put(mui,at+16,e.crc);
        for(unsigned pos:{20U,24U})Put(mui,at+pos,e.width,2);
        for(unsigned pos:{22U,26U})Put(mui,at+pos,e.height,2);
        Put(mui,at+28,e.flags);
    }
    Save("native-karts.nsp",native);Save("fixture.mui",mui);
}
static Pack* active=nullptr;
static bool callbackClose=false,callbackReclaim=false;
static unsigned callbacks=0;
static void Released(const void* address) {
    assert(address);++callbacks;
    if(callbackClose){callbackClose=false;active->Close();}
    if(callbackReclaim){callbackReclaim=false;assert(active->Reclaim());assert(active->SetKartAliasesEnabled(false));}
}
static void ReentrantPackAllocation(){assert(!active->Reclaim());assert(!active->SetKartAliasesEnabled(false));}
static ReadAheadFile* activeFile=nullptr;
static void ReentrantRangeAllocation(){assert(!activeFile->TryReclaimCache());activeFile->ClearCache();activeFile->TrimCache(0);assert(!activeFile->CacheRange(0,1,1024));}

int main() {
    MakeFixture();
    {
        Pack p;active=&p;p.SetReleaseCallback(Released);assert(p.Open("fixture.mui",false));
        assert(p.EntryCount()==10 && p.MetadataBytes()==320);
        auto borrower=p.Load("direct0",512,128,true);assert(borrower);
        const auto generation=p.Generation();
        assert(!p.SetKartAliasesEnabled(true,specs.size()*32-1));
        assert(p.Generation()==generation && p.MetadataBytes()==320);
        failSize=specs.size()*32;assert(!p.SetKartAliasesEnabled(true));
        assert(p.Generation()==generation && p.EntryCount()==10 && borrower);
        assert(p.Load("direct0",512,128,true).pixels==borrower.pixels);
        assert(p.SetKartAliasesEnabled(true));assert(p.EntryCount()==58 && p.MetadataBytes()==1856);
        assert(p.Generation()!=generation && !p.CachedBytes());
        assert(p.PreparePages(4,4*kSpritePageBytes));p.ResetStats();
        for(unsigned lane=0;lane<4;++lane)assert(p.Load(("kart0/wheel"+std::to_string(lane)).c_str(),64,64));
        assert(p.Stats().pageDecodes==1 && p.Stats().pageHits==3 && p.Stats().fileReads==1);
        const auto requirement=p.RequiredPreparationBytes(8);
        assert(!p.MakeResident(requirement-1,8) && !p.ResidentBytes());
        // Optional mirror/order/page malloc failures with an installed real
        // production new_handler are covered by resource_admission_e3_test.py.
        assert(p.MakeResident(requirement,8,[](unsigned){assert(!active->Reclaim());assert(!active->SetKartAliasesEnabled(false));}));assert(p.ResidentBytes()==native.size()-200 && p.PageCacheBytes()==8*kSpritePageBytes);
        p.ResetStats();
        for(const auto& e:specs)if(e.flags) {
            auto image=p.Load(e.name.c_str(),e.width,e.height);
            assert(image && improved::PixelCrc(reinterpret_cast<const uint8_t*>(image.pixels->data()),image.pixels->size())==e.crc);
        }
        assert(p.Stats().fileBytes==0 && p.Stats().pageDecodes==12 && p.Stats().pageHits==36);
        auto live=p.Load("kart0/wheel0",64,64,true);assert(live);
        observeSize=kSpriteImageBytes;onAllocation=ReentrantPackAllocation;
        auto uncached=p.Load("kart11/wheel3",64,64);assert(uncached && !onAllocation && p.ResidentBytes());
        callbackReclaim=true;uncached={};callbackReclaim=false;
        for(unsigned repeat=0;repeat<64;++repeat) {
            assert(p.SetKartAliasesEnabled(false));assert(p.EntryCount()==10 && !p.ResidentBytes() && !p.PageCacheBytes());
            assert(!p.Load("kart0/wheel0",64,64));
            assert(p.Load("direct0",512,128));assert(p.SetKartAliasesEnabled(true));
            assert(p.Load("kart0/wheel0",64,64));
        }
        assert(live && borrower);const auto before=callbacks;
        p.Close();assert(live && borrower && callbacks==before);
        live={};borrower={};assert(callbacks==before+2);
    }
    // True image LRU: touch A after filling the cache; B must be evicted.
    {
        Pack p;assert(p.Open("fixture.mui"));
        auto a=p.Load("direct0",512,128,true),b=p.Load("direct1",512,128,true);assert(a&&b);
        for(unsigned n=2;n<8;++n)assert(p.Load(("direct"+std::to_string(n)).c_str(),512,128,true));
        assert(p.Load("direct0",512,128,true).pixels==a.pixels);
        assert(p.Load("direct8",512,128,true));
        assert(p.CachedBytes()==2*1024*1024 && p.Load("direct0",512,128,true).pixels==a.pixels);
        assert(p.Load("direct1",512,128,true).pixels!=b.pixels);
    }
    // Release callbacks can request shutdown while an eviction is in progress.
    {
        Pack p;active=&p;p.SetReleaseCallback(Released);assert(p.Open("fixture.mui"));
        for(unsigned n=0;n<8;++n)assert(p.Load(("direct"+std::to_string(n)).c_str(),512,128,true));
        callbackClose=true;auto live=p.Load("direct8",512,128,true);
        assert(live && !p.IsOpen() && !p.CachedBytes());
    }
    // Failed/truncated mirror and corrupt page never commit partial residency.
    {
        auto bad=native;bad.back()^=1;Save("native-karts.nsp",bad);
        Pack p;assert(p.Open("fixture.mui"));assert(!p.MakeResident(32*1024*1024,4));assert(!p.ResidentBytes());
        p.Close();Save("native-karts.nsp",native);assert(p.Open("fixture.mui"));
        std::filesystem::resize_file("native-karts.nsp",native.size()-1);
        assert(!p.MakeResident(32*1024*1024,4) && !p.ResidentBytes());
        p.Close();Save("native-karts.nsp",native);
        // Menu still validates inactive aliases, rather than trusting/filtering them.
        auto invalid=mui;
        for(size_t i=0;i<specs.size();++i)if(specs[i].flags){Put(invalid,16+i*32+8,1);break;}
        Save("invalid.mui",invalid);assert(!p.Open("invalid.mui",false));
    }
    // Cache count is constrained by actual charged bytes, with stable nodes,
    // exact hits, disjoint ranges and reentrant new_handler immunity.
    {
        std::vector<uint8_t> file(600000);
        for(size_t i=0;i<file.size();++i)file[i]=uint8_t(i*73+i/131);
        Save("stream.bin",file);ReadAheadFile f;activeFile=&f;assert(f.Open("stream.bin"));
        constexpr size_t budget=2*1024*1024;
        for(unsigned i=0;i<5000;++i)assert(f.CacheRange(i*100,64,budget));
        assert(f.CachedRanges()==5000 && f.CacheAllocationBytes()<=budget);
        const auto bytes=f.ReadBytes();std::array<uint8_t,64> out{};
        for(unsigned i=0;i<5000;++i) {assert(f.Read(i*100,out.data(),out.size())==out.size());assert(std::equal(out.begin(),out.end(),file.begin()+i*100));}
        assert(f.ReadBytes()==bytes && f.CacheHits()>=5000);
        assert(!f.CacheRange(1,100,budget)); // Existing ranges cannot be shadowed.
        observeSize=65;onAllocation=ReentrantRangeAllocation;
        assert(f.CacheRange(500000,65,budget));assert(!onAllocation && f.CachedRanges()==5001);
        failSize=66;assert(!f.CacheRange(500100,66,budget));assert(f.CachedRanges()==5001);
        f.TrimCache(320);assert(f.CacheAllocationBytes()<=320 && f.CacheByteEvictions()>0);
        assert(f.TryReclaimCache() && !f.CacheAllocationBytes());
        assert(f.Read(0,out.data(),out.size())==out.size() && std::equal(out.begin(),out.end(),file.begin()));
        observeSize=67;onAllocation=[](){activeFile->Close();};
        assert(f.CacheRange(500200,67,budget));assert(!f.Size() && !f.CachedBytes());
        assert(f.Open("stream.bin"));
        assert(f.MakeResident(file.size(),[](unsigned){assert(!activeFile->MakeResident(SIZE_MAX));activeFile->Close();}));
        assert(!f.Size() && !f.IsResident());
    }
    // Full historical rings must not evict an unrelated live head to make a
    // slot for repeated reacquisition of one small hot resource.
    {
        struct R {bool retained=false;uint32_t retainStamp=0;size_t allocatedBytes=256;bool alive=true;};
        std::array<R,16> r{};TextureRetention3DS<512> q(256*1024);unsigned evicted=0;
        auto lookup=[&](uint32_t i){return r[i].alive?&r[i]:nullptr;};
        auto evict=[&](uint32_t,R& a){assert(!a.retained);a.alive=false;++evicted;return true;};
        for(unsigned i=0;i<r.size();++i)assert(q.Retain(i,r[i],lookup,evict));
        for(unsigned i=0;i<20000;++i){q.Forget(r[15]);assert(q.Retain(15,r[15],lookup,evict));}
        assert(!evicted && q.Bytes()==4096 && q.StaleRecordsRemoved()>19000 && !q.CountEvictions());
        assert(q.EvictOldest(0,0,lookup,evict));assert(evicted==16 && q.Bytes()==0 && !q.Slots());
    }
    static_assert(ArchiveResidencyBudget(49037312,20893952)==19754752);
    static_assert(ArchiveResidencyBudget(49037312,20893952-304128)>=20022979);
    for(const char* path:{"native-karts.nsp","fixture.mui","invalid.mui","stream.bin"})std::filesystem::remove(path);
    std::cout<<"PASS: compact catalog, 64 mode cycles, exact lanes/CRCs, mirror rollback, bounded LRU, callback/new-handler reentrancy, 5000 ranges and 20000 retention cycles.\n";
}
