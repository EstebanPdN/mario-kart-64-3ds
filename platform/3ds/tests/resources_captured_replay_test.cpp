// Offline actual-code replay. All input names/pixels stay outside Source.
#ifdef E3_RESOURCE_BASELINE
#include <cstdio>
#include <cstdint>
static uint64_t baselineBytes=0;
static size_t ResourceReplayFread(void* data,size_t size,size_t count,FILE* file) {
    const auto read=std::fread(data,size,count,file);baselineBytes+=read*size;return read;
}
#endif
#include "improved_texture_pack_3ds.hpp"
#include "read_ahead_file_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using mk64_3ds::improved::Pack;
struct Entry {std::string name;unsigned w,h,lw,lh,flags;uint32_t crc;};
static std::vector<std::string> Fields(const std::string& line) {
    std::vector<std::string> values;std::istringstream s(line);std::string value;
    while(std::getline(s,value,'\t'))values.push_back(value);return values;
}
static void Check(const mk64_3ds::improved::Image& image,const Entry& e) {
    assert(image && image.width==e.w && image.height==e.h && image.logicalWidth==e.lw && image.logicalHeight==e.lh);
    assert(image.nativeGpuTiled==bool(e.flags));
    assert(crc32(0,reinterpret_cast<const uint8_t*>(image.pixels->data()),image.pixels->size())==e.crc);
}
int main(int argc,char** argv) {
    assert(argc==5);
    std::ifstream entriesFile(argv[2]);assert(entriesFile.good());std::string line;
    std::vector<Entry> entries;std::map<std::string,size_t> names;
    while(std::getline(entriesFile,line)) {
        const auto f=Fields(line);assert(f.size()==8);
        Entry e{f[0],unsigned(std::stoul(f[1])),unsigned(std::stoul(f[2])),unsigned(std::stoul(f[3])),unsigned(std::stoul(f[4])),unsigned(std::stoul(f[5])),uint32_t(std::stoul(f[6],nullptr,16))};
        assert(std::stoull(f[7],nullptr,16)==mk64_3ds::improved::Hash(e.name.c_str()));
        names.emplace(e.name,entries.size());entries.push_back(e);
    }
    assert(entries.size()==10502);
    Pack pack;assert(pack.Open(argv[1]));
    for(const auto& e:entries)Check(pack.Load(e.name.c_str(),e.w,e.h),e);
    std::cout<<"{\"verification\":\"all artwork via actual Pack::Load\",\"images\":"<<entries.size()<<"}\n";
#ifndef E3_RESOURCE_BASELINE
    const auto full=pack.MetadataBytes();assert(pack.SetKartAliasesEnabled(false));
    assert(pack.EntryCount()==998 && full-pack.MetadataBytes()==304128);
    for(unsigned mode:{0,2}) {
        for(const auto& e:entries) {
            const auto image=pack.Load(e.name.c_str(),e.w,e.h);
            if(e.flags)assert(!image);else Check(image,e);
        }
        std::cout<<"{\"compact_mode\":"<<mode<<",\"direct_images\":998,\"metadata_bytes\":"<<pack.MetadataBytes()<<"}\n";
    }
    assert(pack.SetKartAliasesEnabled(true));assert(pack.RequiredResidentBytes()==18599716);
    assert(pack.MakeResident(pack.RequiredPreparationBytes(8),8));pack.ResetStats();
    for(const auto& e:entries)if(e.flags)Check(pack.Load(e.name.c_str(),e.w,e.h),e);
    assert(!pack.Stats().fileBytes);
    std::cout<<"{\"resident_alias_images\":9504,\"range_bytes\":"<<pack.ResidentBytes()<<",\"file_bytes\":0,\"decoded_page_bytes\":"<<pack.PageCacheBytes()<<",\"validation_pages\":2562}\n";
#endif
    std::ifstream replay(argv[3]);assert(replay.good());
    mk64_3ds::ReadAheadFile source;
    std::string session;uint64_t requests=0,art=0,checksum=0;
    auto finish=[&] {
        if(session.empty())return;
        std::cout<<"{\"session\":\""<<session<<"\",\"requests\":"<<requests<<",\"artwork_images\":"<<art<<",\"ordered_crc\":"<<checksum
                 <<",\"archive_read_calls\":"<<source.ReadCalls()<<",\"archive_read_bytes\":"<<source.ReadBytes()<<",\"stream_payload_bytes\":"<<source.CachedBytes();
#ifndef E3_RESOURCE_BASELINE
        std::cout<<",\"stream_indexed_ranges\":"<<source.CachedRanges()<<",\"stream_allocation_bytes\":"<<source.CacheAllocationBytes()
                 <<",\"art_file_bytes\":"<<pack.Stats().fileBytes<<",\"art_page_decodes\":"<<pack.Stats().pageDecodes<<",\"art_page_hits\":"<<pack.Stats().pageHits;
#endif
#ifdef E3_RESOURCE_BASELINE
        std::cout<<",\"art_file_bytes\":"<<baselineBytes;
#endif
        std::cout<<"}\n";
    };
    while(std::getline(replay,line)) {
        const auto f=Fields(line);assert(f.size()==5);
        if(session!=f[0]) {
            finish();session=f[0];requests=art=checksum=0;assert(source.Open(argv[4]));assert(pack.Open(argv[1]));
#ifndef E3_RESOURCE_BASELINE
            assert(pack.MakeResident(pack.RequiredPreparationBytes(8),8));pack.ResetStats();
#else
            baselineBytes=0;
#endif
        }
        const uint64_t offset=std::stoull(f[2]);const size_t bytes=std::stoul(f[3]);
        std::vector<uint8_t> compressed(bytes);
        if(bytes<=256*1024)source.CacheRange(offset,bytes,2*1024*1024);
        assert(source.Read(offset,compressed.data(),bytes)==bytes);
        assert(crc32(0,compressed.data(),compressed.size())==std::stoul(f[4],nullptr,16));
        const auto found=names.find(f[1]);
        if(found!=names.end()) {
            const auto& e=entries[found->second];auto image=pack.Load(e.name.c_str(),e.w,e.h,true);Check(image,e);
            checksum=(checksum*1099511628211ULL)^e.crc;++art;
        } else assert(!pack.Load(f[1].c_str(),64,64,true));
        ++requests;
    }
    finish();
}
