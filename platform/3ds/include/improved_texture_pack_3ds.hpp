#pragma once

#include "native_sprite_page_3ds.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
#include "performance_trace_3ds.hpp"
#endif
#ifndef __3DS__
#include <zlib.h>
#endif

namespace mk64_3ds::improved {
#ifdef __3DS__
bool DecodePayload(const uint8_t* input,size_t packed,uint8_t* output,size_t bytes,uint32_t crc);
uint32_t PixelCrc(const uint8_t* input,size_t bytes);
#else
inline uint32_t PixelCrc(const uint8_t* input,size_t bytes) { return crc32(0,input,bytes); }
inline bool DecodePayload(const uint8_t* input,size_t packed,uint8_t* output,size_t bytes,uint32_t crc) {
    uLongf size=bytes;
    return uncompress(output,&size,input,packed)==Z_OK && size==bytes && PixelCrc(output,size)==crc;
}
#endif
// Optional owner-installed artwork. Every returned image owns its pixels,
// including across catalog changes, eviction and pack shutdown.
struct Image {
    unsigned width=0, height=0, logicalWidth=0, logicalHeight=0;
    std::shared_ptr<std::vector<char>> pixels;
    bool nativeGpuTiled=false;
    explicit operator bool() const { return pixels && pixels->size()==size_t(width)*height*4; }
};
inline uint64_t Hash(const char* name) {
    if (!name) return 0;
    if (std::char_traits<char>::length(name)>=7 && std::equal(name,name+7,"__OTR__")) name+=7;
    uint64_t h=14695981039346656037ULL;
    while (*name) {h^=uint8_t(*name++);h*=1099511628211ULL;}
    return h;
}
class Pack {
    struct FreePreparation { void operator()(void* bytes)const{std::free(bytes);} };
    template<class T> using PreparationBuffer=std::unique_ptr<T[],FreePreparation>;
    struct Entry { uint64_t hash; uint32_t offset, packed, crc; uint16_t width,height,lw,lh; uint32_t flags; };
    static_assert(sizeof(Entry)==32);
    struct Cached {
        size_t index;
        std::shared_ptr<std::vector<char>> bytes;
        // A wrapper may be retained only while this same bounded pixel entry
        // is retained. It cannot keep an evicted image alive on its own.
        std::shared_ptr<void> owner;
        uint64_t ownerTag=0;
    };
    using Cache=std::list<Cached>;
    FILE* file=nullptr;
    FILE* nativeFile=nullptr;
    uint32_t nativeSize=0,nativeStart=0,nativeVersion=0;
    uint32_t fileSize=0,catalogCount=0;
    std::unique_ptr<Entry[]> entries;
    size_t entryCount=0;
    Cache cache;
    std::unordered_map<size_t,typename Cache::iterator> cacheIndex;
    size_t cachedBytes=0;
    PreparationBuffer<uint8_t> resident;
    uint32_t residentStart=0;
    size_t residentBytes=0;
    native_scene::SpritePageCache<8> pages;
    bool aliases=true,busy=false,closePending=false;
    uint32_t generation=1;
    void (*onRelease)(const void*)=nullptr;
    static constexpr size_t kCacheLimit=2*1024*1024;
    struct Scope {
        Pack& pack;
        explicit Scope(Pack& p):pack(p){pack.busy=true;}
        ~Scope(){pack.busy=false;if(pack.closePending){pack.closePending=false;pack.Close();}}
    };
    static uint32_t U32(const uint8_t* p) {
        return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
    }
    static uint16_t U16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
    void AdvanceGeneration() {if(!++generation)++generation;}
    bool OpenNative(const char* path) {
        const std::string source(path);const auto slash=source.find_last_of("/\\");
        const auto companion=source.substr(0,slash==std::string::npos?0:slash+1)+"native-karts.nsp";
        nativeFile=std::fopen(companion.c_str(),"rb");
        std::array<uint8_t,32> h{};
        if(!nativeFile || std::fread(h.data(),1,32,nativeFile)!=32 ||
           !((U32(h.data())==0x3150534eU && U32(h.data()+4)==1) ||
             (U32(h.data())==0x3250534eU && U32(h.data()+4)==2)) ||
           !U32(h.data()+8) || U32(h.data()+8)>4096 || U32(h.data()+12)!=168 ||
           U32(h.data()+16)!=64 || U32(h.data()+20)!=64 ||
           U32(h.data()+24)!=32+U32(h.data()+8)*168 || std::fseek(nativeFile,0,SEEK_END)!=0)return false;
        const long actual=std::ftell(nativeFile);
        nativeStart=U32(h.data()+24);nativeSize=U32(h.data()+28);nativeVersion=U32(h.data()+4);
        return actual>=0 && uint64_t(actual)==nativeSize && nativeSize>=nativeStart;
    }
    // Two passes validate the complete on-disk index before allocating exactly
    // the selected metadata. reserve(fullCount)+filter would save no heap.
    bool ReadCatalog(bool includeAliases,const char* path,std::unique_ptr<Entry[]>& pending,size_t& selected,size_t budget=SIZE_MAX) {
        selected=0;
        for(unsigned pass=0;pass<2;++pass) {
            if(std::fseek(file,16,SEEK_SET)!=0)return false;
            uint64_t previous=0;size_t at=0;
            for(uint32_t i=0;i<catalogCount;++i) {
                std::array<uint8_t,32> b{};
                if(std::fread(b.data(),1,b.size(),file)!=b.size())return false;
                const Entry e{uint64_t(U32(b.data())) | uint64_t(U32(b.data()+4))<<32,
                    U32(b.data()+8),U32(b.data()+12),U32(b.data()+16),
                    U16(b.data()+20),U16(b.data()+22),U16(b.data()+24),U16(b.data()+26),U32(b.data()+28)};
                if(e.flags && !nativeFile && (!path || !OpenNative(path)))return false;
                if(!e.width || !e.height || e.width>512 || e.height>512 ||
                   !e.lw || !e.lh || e.lw>512 || e.lh>512 || !e.packed || e.packed>2*1024*1024 ||
                   (e.flags!=0 && e.flags!=1 && !native_scene::IsSpritePageAlias(e.flags)) ||
                   (e.flags && nativeVersion!=(e.flags==1?1u:2u)) ||
                   (e.flags && (e.width!=64 || e.height!=64 || e.lw!=64 || e.lh!=64)) ||
                   e.offset<(e.flags?nativeStart:16+catalogCount*32) ||
                   uint64_t(e.offset)+e.packed>uint64_t(e.flags?nativeSize:fileSize) ||
                   (i && e.hash<previous))return false;
                previous=e.hash;
                if(includeAliases || !e.flags) {
                    if(!pass)++selected;
                    else {if(at>=selected)return false;pending[at++]=e;}
                }
            }
            if(!pass){if(selected>budget/sizeof(Entry))return false;pending.reset(selected?new Entry[selected]:nullptr);}
            else if(at!=selected)return false;
        }
        return true;
    }
    size_t Find(const char* name,unsigned width,unsigned height) const {
        if(!file || !name || !entryCount)return entryCount;
        const uint64_t hash=Hash(name);
        const Entry* begin=entries.get();const auto* end=begin+entryCount;
        const auto* first=std::lower_bound(begin,end,hash,[](const Entry& e,uint64_t h){return e.hash<h;});
        size_t best=entryCount;unsigned score=std::numeric_limits<unsigned>::max();
        for(auto* it=first;it!=end && it->hash==hash;++it) {
            const unsigned delta=unsigned(std::abs(int(it->width)-int(width)))+unsigned(std::abs(int(it->height)-int(height)));
            if(delta<score){score=delta;best=size_t(it-entries.get());}
        }
        return best;
    }
    void ClearImages() {
        cachedBytes=0;cacheIndex.clear();
        // Detach before invoking last-owner callbacks; reentrant mutations are
        // rejected/deferred until the enclosing operation has finished.
        Cache released;released.swap(cache);
    }
    bool ReadPacked(const Entry& e,std::vector<uint8_t>& packed,const uint8_t*& input) {
        if(e.flags && resident && e.offset>=residentStart &&
           e.offset-residentStart<=residentBytes && e.packed<=residentBytes-(e.offset-residentStart)) {
            input=resident.get()+e.offset-residentStart;return true;
        }
        packed.resize(e.packed);
        FILE* source=e.flags?nativeFile:file;
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
        const auto start=mk64_3ds::PerformanceNow();
#endif
        const bool ok=source && std::fseek(source,e.offset,SEEK_SET)==0 &&
            std::fread(packed.data(),1,packed.size(),source)==packed.size();
        ++statistics.fileReads;if(ok)statistics.fileBytes+=packed.size();
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
        auto& diagnostic=mk64_3ds::PerformanceCurrent();
        diagnostic.art_io_us+=static_cast<uint32_t>(mk64_3ds::PerformanceNow()-start);
        if(ok)diagnostic.art_io_bytes+=packed.size();
#endif
        input=packed.data();return ok;
    }
    bool DecodePage(const Entry& e,uint8_t* output) {
        std::vector<uint8_t> packed;const uint8_t* input=nullptr;
        if(!ReadPacked(e,packed,input))return false;
        ++statistics.pageDecodes;
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
        mk64_3ds::PerformanceTimer timer(mk64_3ds::PerformanceCurrent().art_decode_us);
#endif
        return native_scene::DecodeSpritePage(input,e.packed,output,DecodePayload);
    }
    void Retain(size_t index,const std::shared_ptr<std::vector<char>>& pixels) {
        if(pixels->size()>kCacheLimit)return;
        while(!cache.empty() && cachedBytes>kCacheLimit-pixels->size()) {
            auto oldest=std::prev(cache.end());
            cachedBytes-=oldest->bytes->size();cacheIndex.erase(oldest->index);
            cache.erase(oldest);++statistics.imageEvictions;
        }
        // Cache allocation failure must not turn a successful decode into a
        // missing replacement. The caller already owns the validated pixels.
        try {
            cache.push_front({index,pixels,{},0});
            try {cacheIndex.emplace(index,cache.begin());}
            catch(...){cache.pop_front();throw;}
            cachedBytes+=pixels->size();
        } catch(const std::bad_alloc&) {}
    }
    std::pair<uint32_t,size_t> NativeRange() const {
        uint32_t start=nativeSize,end=0;
        for(size_t i=0;i<entryCount;++i)if(entries[i].flags){start=std::min(start,entries[i].offset);end=std::max(end,entries[i].offset+entries[i].packed);}
        return end?std::pair<uint32_t,size_t>{start,size_t(end-start)}:std::pair<uint32_t,size_t>{0,0};
    }
public:
    struct Statistics {
        uint64_t requests=0,imageHits=0,absent=0,failures=0,fileReads=0,fileBytes=0;
        uint64_t pageHits=0,pageDecodes=0,validationDecodes=0,imageEvictions=0,residencyReleases=0;
    };
private:
    Statistics statistics{};
public:
    struct Request {
        uint64_t hash=0;
        uint32_t generation=0,index=0,offset=0,packed=0,readUs=0,decodeUs=0;
        uint16_t width=0,height=0;
        uint8_t lane=0,outcome=0; // 0 absent, 1 image hit, 2 decoded, 3 failed, 4 page hit
    };
private:
    std::array<Request,32> history{};
    size_t historyCount=0,historyNext=0;
    void Record(const Request& request) {
        history[historyNext]=request;historyNext=(historyNext+1)%history.size();
        historyCount=std::min(historyCount+1,history.size());
    }
public:
    size_t HistorySize()const{return historyCount;}
    const Request& HistoryAt(size_t index)const{return history[(historyNext+history.size()-historyCount+index)%history.size()];}
    Pack()=default;
    Pack(const Pack&)=delete;
    Pack& operator=(const Pack&)=delete;
    ~Pack(){Close();}
    void SetReleaseCallback(void (*callback)(const void*)){onRelease=callback;}
    void Close() {
        if(busy){closePending=true;return;}
        Scope scope(*this);
        if(file)std::fclose(file);if(nativeFile)std::fclose(nativeFile);
        file=nativeFile=nullptr;nativeSize=nativeStart=nativeVersion=fileSize=catalogCount=0;
        entries.reset();entryCount=0;resident.reset();residentBytes=residentStart=0;pages.Clear();
        AdvanceGeneration();ClearImages();
    }
    bool Open(const char* path,bool includeAliases=true) {
        if(busy || !path)return false;
        Close();Scope scope(*this);
        file=std::fopen(path,"rb");if(!file)return false;
        bool ok=false;
        try {
            std::array<uint8_t,16> h{};
            if(std::fread(h.data(),1,h.size(),file)==h.size() && U32(h.data())==0x3349554dU &&
               U32(h.data()+4)==1 && U32(h.data()+8)<=20000 && U32(h.data()+12)==32 && std::fseek(file,0,SEEK_END)==0) {
                const long actual=std::ftell(file);catalogCount=U32(h.data()+8);
                if(actual>=0 && uint64_t(actual)>=16+catalogCount*32 && uint64_t(actual)<=512*1024*1024ULL) {
                    fileSize=uint32_t(actual);
                    std::unique_ptr<Entry[]> pending;size_t selected=0;
                    if(ReadCatalog(includeAliases,path,pending,selected)) {
                        entries=std::move(pending);entryCount=selected;aliases=includeAliases;ok=true;
                    }
                }
            }
        } catch(const std::bad_alloc&) {}
        if(!ok)closePending=true;
        return ok;
    }
    bool SetKartAliasesEnabled(bool enabled,size_t budget=SIZE_MAX) {
        if(busy || !file)return false;
        if(aliases==enabled)return true;
        Scope scope(*this);
        try {
            std::unique_ptr<Entry[]> pending;size_t selected=0;
            if(!ReadCatalog(enabled,nullptr,pending,selected,budget))return false;
            entries=std::move(pending);entryCount=selected;aliases=enabled;
            resident.reset();residentBytes=residentStart=0;pages.Clear();AdvanceGeneration();ClearImages();return true;
        } catch(const std::bad_alloc&) {return false;}
    }
    bool IsOpen()const{return file!=nullptr;}
    bool KartAliasesEnabled()const{return aliases;}
    uint32_t Generation()const{return generation;}
    size_t MetadataBytes()const{return entryCount*sizeof(Entry);}
    size_t FullMetadataBytes()const{return size_t(catalogCount)*sizeof(Entry);}
    size_t EntryCount()const{return entryCount;}
    bool Contains(const char* name,unsigned width,unsigned height)const{return Find(name,width,height)!=entryCount;}
    // Call only at loading boundaries, with an additional-allocation budget
    // computed from actual heap use after subtracting the 8 MiB reserve.
    bool PreparePages(size_t count,size_t budget) {
        if(busy || !aliases || (count!=4 && count!=8) || count*native_scene::kSpritePageBytes>budget)return false;
        Scope scope(*this);return pages.Resize(count);
    }
    size_t RequiredResidentBytes()const{return NativeRange().second;}
    size_t ResidentBytes()const{return residentBytes;}
    size_t PageCacheBytes()const{return pages.Bytes();}
    size_t RequiredPreparationBytes(size_t count)const {
        if(count>SIZE_MAX/native_scene::kSpritePageBytes || entryCount>SIZE_MAX/sizeof(uint32_t))return SIZE_MAX;
        size_t required=RequiredResidentBytes();
        for(const size_t bytes:{count*native_scene::kSpritePageBytes,native_scene::kSpritePageBytes,entryCount*sizeof(uint32_t)}) {
            if(bytes>SIZE_MAX-required)return SIZE_MAX;
            required+=bytes;
        }
        return required;
    }
    bool MakeResident(size_t budget,size_t pageCount=4,void (*progress)(unsigned)=nullptr) {
        if(busy || !file || !nativeFile || !aliases || (pageCount!=4 && pageCount!=8))return false;
        if(resident)return true; // Never allocate a second compressed mirror.
        const auto range=NativeRange();
        const size_t required=RequiredPreparationBytes(pageCount);
        if(!range.second || required==SIZE_MAX || required>budget)return false;
        Scope scope(*this);
        try {
            // Optional preparation must fail locally. Even nothrow new calls
            // the process new_handler, which could surrender resident O2R
            // while this busy Pack cannot release its old optional cache.
            // All three candidates use malloc/free, without global recovery.
            PreparationBuffer<uint8_t> pending(static_cast<uint8_t*>(std::malloc(range.second)));
            if(!pending)return false;
            if(std::fseek(nativeFile,range.first,SEEK_SET)!=0)return false;
            for(size_t at=0;at<range.second;) {
                const auto bytes=std::min(size_t(256*1024),range.second-at);
                const auto read=std::fread(pending.get()+at,1,bytes,nativeFile);
                ++statistics.fileReads;statistics.fileBytes+=read;
                if(read!=bytes)return false;at+=read;
                if(progress)progress(static_cast<unsigned>(at*50ULL/range.second));
            }
            PreparationBuffer<uint32_t> order(static_cast<uint32_t*>(std::malloc(entryCount*sizeof(uint32_t))));
            if(!order)return false;
            size_t orderCount=0;
            for(size_t i=0;i<entryCount;++i)if(entries[i].flags)order[orderCount++]=uint32_t(i);
            std::sort(order.get(),order.get()+orderCount,[&](uint32_t a,uint32_t b){return entries[a].offset<entries[b].offset;});
            PreparationBuffer<uint8_t> page(static_cast<uint8_t*>(std::malloc(native_scene::kSpritePageBytes)));
            if(!page)return false;
            uint32_t offset=0,packed=0,end=range.first;bool paged=false;
            size_t checked=0;
            for(size_t i=0;i<orderCount;++i) {
                const auto index=order[i];
                if(progress && (++checked%16==0 || checked==orderCount))
                    progress(50+static_cast<unsigned>(checked*50ULL/orderCount));
                const Entry e=entries[index];
                const auto* input=pending.get()+e.offset-range.first;
                if(e.offset!=offset || e.packed!=packed) {
                    if(e.offset<end)return false;
                    offset=e.offset;packed=e.packed;end=e.offset+e.packed;paged=native_scene::IsSpritePageAlias(e.flags);
                    ++statistics.validationDecodes;
                    if(paged) {if(!native_scene::DecodeSpritePage(input,e.packed,page.get(),DecodePayload))return false;}
                    else if(!DecodePayload(input,e.packed,page.get(),native_scene::kSpriteImageBytes,e.crc))return false;
                }
                if(paged!=native_scene::IsSpritePageAlias(e.flags) ||
                   (paged && native_scene::SpritePageU32(page.get()+(e.flags>>8)*4)!=e.crc))return false;
            }
            if(!pages.Resize(pageCount))return false;
            resident=std::move(pending);residentStart=range.first;residentBytes=range.second;return true;
        } catch(const std::bad_alloc&) {return false;}
    }
    bool Reclaim() {
        if(busy)return false;
        Scope scope(*this);
        const bool had=cachedBytes || residentBytes || pages.Bytes();
        ClearImages();resident.reset();residentBytes=residentStart=0;pages.Clear();
        if(had)++statistics.residencyReleases;
        return had;
    }
    const Statistics& Stats()const{return statistics;}
    void ResetStats(){statistics={};}
    Image Load(const char* name,unsigned width,unsigned height,bool retain=false) {
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
        ++mk64_3ds::PerformanceCurrent().art_requests;
#endif
        ++statistics.requests;
        if(busy)return {};
        Scope scope(*this);
        struct RequestScope {
            Pack& pack;Request event{};
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
            uint32_t read=mk64_3ds::PerformanceCurrent().art_io_us;
            uint32_t decode=mk64_3ds::PerformanceCurrent().art_decode_us;
#endif
            ~RequestScope() {
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
                event.readUs=mk64_3ds::PerformanceCurrent().art_io_us-read;
                event.decodeUs=mk64_3ds::PerformanceCurrent().art_decode_us-decode;
#endif
                pack.Record(event);
            }
        } request{*this};
        request.event.hash=Hash(name);request.event.generation=generation;
        request.event.width=uint16_t(std::min(width,65535u));request.event.height=uint16_t(std::min(height,65535u));
        const size_t index=Find(name,width,height);
        if(index==entryCount){++statistics.absent;return {};}
        // Copy the immutable record before any allocation or release callback.
        const Entry e=entries[index];
        request.event.index=uint32_t(index);request.event.offset=e.offset;request.event.packed=e.packed;
        request.event.lane=uint8_t(e.flags>>8);request.event.outcome=3;
        if(const auto found=cacheIndex.find(index);found!=cacheIndex.end()) {
            cache.splice(cache.begin(),cache,found->second);++statistics.imageHits;request.event.outcome=1;
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
            ++mk64_3ds::PerformanceCurrent().art_cache_hits;
#endif
            return {e.width,e.height,e.lw,e.lh,found->second->bytes,e.flags!=0};
        }
        try {
            const auto release=onRelease;
            auto pixels=std::shared_ptr<std::vector<char>>(new std::vector<char>(size_t(e.width)*e.height*4),
                [release](std::vector<char>* bytes){if(release)release(bytes->data());delete bytes;});
            auto* output=reinterpret_cast<uint8_t*>(pixels->data());
            bool ok=false;
            if(native_scene::IsSpritePageAlias(e.flags)) {
                bool loaded=false;
                const auto loader=[&](uint8_t* page){loaded=true;return DecodePage(e,page);};
                std::vector<uint8_t> scratch;
                const uint8_t* page=nullptr;
                if(pages.ActiveCapacity())page=pages.Load(uint64_t(e.offset)<<32 | e.packed,loader);
                else {scratch.resize(native_scene::kSpritePageBytes);if(loader(scratch.data()))page=scratch.data();}
                if(page && !loaded){++statistics.pageHits;request.event.outcome=4;}
                ok=page && native_scene::ExtractSpritePage(page,e.flags>>8,e.crc,output) && PixelCrc(output,pixels->size())==e.crc;
            } else {
                std::vector<uint8_t> packed;const uint8_t* input=nullptr;
                if(ReadPacked(e,packed,input)) {
#if defined(__3DS__) || defined(MK64_3DS_AUTODUMP_LAB)
                    mk64_3ds::PerformanceTimer timer(mk64_3ds::PerformanceCurrent().art_decode_us);
#endif
                    ok=DecodePayload(input,e.packed,output,pixels->size(),e.crc);
                }
            }
            if(!ok){++statistics.failures;request.event.outcome=3;return {};}
            if(request.event.outcome!=4)request.event.outcome=2;
            if(retain)Retain(index,pixels);
            return {e.width,e.height,e.lw,e.lh,std::move(pixels),e.flags!=0};
        } catch(const std::bad_alloc&) {++statistics.failures;return {};}
    }
    std::shared_ptr<void> CachedOwner(const char* name,unsigned width,unsigned height,uint64_t tag) {
        if(busy)return {};
        const auto found=cacheIndex.find(Find(name,width,height));
        if(found==cacheIndex.end() || found->second->ownerTag!=tag)return {};
        cache.splice(cache.begin(),cache,found->second);return found->second->owner;
    }
    void TouchImage(const char* name,unsigned width,unsigned height) {
        if(busy)return;
        const auto found=cacheIndex.find(Find(name,width,height));
        if(found!=cacheIndex.end())cache.splice(cache.begin(),cache,found->second);
    }
    void RememberOwner(const char* name,unsigned width,unsigned height,uint64_t tag,std::shared_ptr<void> owner) {
        if(busy)return;
        Scope scope(*this);
        const auto found=cacheIndex.find(Find(name,width,height));
        if(found!=cacheIndex.end()){found->second->ownerTag=tag;found->second->owner=std::move(owner);}
    }
    size_t CachedBytes()const{return cachedBytes;}
};
inline Pack& GlobalPack(){static Pack pack;return pack;}
inline bool& LocalKartAliasesEnabled(){static bool enabled=true;return enabled;}
inline Pack& LocalPack() {
    static const bool opened=[] {
        return GlobalPack().Open("romfs:/improved-textures.bin",LocalKartAliasesEnabled()) ||
               GlobalPack().Open("sdmc:/3ds/MK64/improved-textures.bin",LocalKartAliasesEnabled());
    }();
    (void)opened;return GlobalPack();
}
inline bool ConfigureLocalCatalog(bool aliases,size_t budget=SIZE_MAX) {
    LocalKartAliasesEnabled()=aliases;
    return LocalPack().SetKartAliasesEnabled(aliases,budget);
}
} // namespace mk64_3ds::improved
