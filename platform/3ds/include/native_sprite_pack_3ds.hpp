#pragma once
#include "native_mesh_format_3ds.hpp"
#include "native_sprite_page_3ds.hpp"
#include "native_sprite_storage_3ds.hpp"
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace mk64_3ds::native_scene {
struct SpriteEntry {
    uint64_t identity,paletteHash[4];
    uint32_t offset[8],packed[8],crc[8];
    uint8_t paletteMask[32];
};
static_assert(sizeof(SpriteEntry)==168);
inline uint64_t SpriteHash(const uint8_t* bytes,size_t n,uint64_t initial=14695981039346656037ULL) {
    for(size_t i=0;i<n;++i){initial^=bytes[i];initial*=1099511628211ULL;}return initial;
}
inline uint64_t SpriteIdentity(const char* name,unsigned& wheel) {
    wheel=0;if(!name)return 0;if(std::strncmp(name,"__OTR__",7)==0)name+=7;
    size_t n=std::strlen(name);
    if(n>=7 && std::memcmp(name+n-7,"_wheel",6)==0 && name[n-1]>='0' && name[n-1]<='3') {wheel=name[n-1]-'0';n-=7;}
    return SpriteHash(reinterpret_cast<const uint8_t*>(name),n);
}
inline uint64_t SpritePaletteHash(const uint8_t palette[512],const uint8_t mask[32]) {
    uint64_t hash=14695981039346656037ULL;
    for(unsigned i=0;i<256;++i)if(mask[i>>3]&(1u<<(i&7)))hash=SpriteHash(palette+i*2,2,hash);
    return hash;
}
inline bool ValidateSpriteIndex(const void* bytes,size_t size,uint32_t fileBytes,std::vector<SpriteEntry>& out) {
    out.clear();if(!bytes || size<32)return false;const auto* p=static_cast<const uint8_t*>(bytes);
    const uint32_t n=ReadLE32(p+8),stride=ReadLE32(p+12),start=ReadLE32(p+24);
    const bool paged=(!std::memcmp(p,"NSP2",4) && ReadLE32(p+4)==2) ||
                     (!std::memcmp(p,"NSP3",4) && ReadLE32(p+4)==3);
    if((!paged && (std::memcmp(p,"NSP1",4) || ReadLE32(p+4)!=1)) || !n || n>4096 || stride!=sizeof(SpriteEntry) ||
       ReadLE32(p+16)!=64 || ReadLE32(p+20)!=64 || start!=32+n*stride || size!=start || ReadLE32(p+28)!=fileBytes || fileBytes<start)return false;
    std::vector<SpriteEntry> checked(n);std::memcpy(checked.data(),p+32,n*stride);
    uint64_t previous=0;
    for(const auto& e:checked) {
        if(!e.identity || e.identity<=previous)return false;
        previous=e.identity;
        bool mask=false;for(uint8_t v:e.paletteMask)mask|=v!=0;if(!mask)return false;
        for(unsigned i=0;i<8;++i)if(e.offset[i]<start || !e.packed[i] || e.packed[i]>(paged?kSpritePackedCapacity:16384+1024) ||
            e.offset[i]>fileBytes || e.packed[i]>fileBytes-e.offset[i])return false;
    }
    if(paged)for(const auto& e:checked)for(unsigned mode=0;mode<2;++mode)
        for(unsigned lane=1;lane<4;++lane)if(e.offset[mode*4+lane]!=e.offset[mode*4] || e.packed[mode*4+lane]!=e.packed[mode*4])return false;
    out=std::move(checked);return true;
}
class SpritePack {
public:
    ~SpritePack(){if(file_)std::fclose(file_);}
    SpritePack()=default;SpritePack(const SpritePack&)=delete;SpritePack& operator=(const SpritePack&)=delete;
    bool Open(const char* path) {
        if(file_)return false;
        file_=std::fopen(path,"rb");if(!file_)return false;
        if(std::fseek(file_,0,SEEK_END))return false;
        long n=std::ftell(file_);
        if(n<32 || uint64_t(n)>512u*1024u*1024u || std::fseek(file_,0,SEEK_SET))return false;
        std::array<uint8_t,32> header{};if(std::fread(header.data(),1,32,file_)!=32)return false;
        const uint32_t start=ReadLE32(header.data()+24);if(start<32 || start>32+4096*sizeof(SpriteEntry) || start>uint32_t(n))return false;
        std::vector<uint8_t> bytes(start);std::memcpy(bytes.data(),header.data(),32);
        if(std::fread(bytes.data()+32,1,start-32,file_)!=start-32)return false;
        fastPaged_=ReadLE32(header.data()+4)==3;
        paged_=ReadLE32(header.data()+4)==2 || fastPaged_;
        return ValidateSpriteIndex(bytes.data(),bytes.size(),n,entries_);
    }
    const SpriteEntry* Find(uint64_t identity,uint32_t& index) const {
        auto it=std::lower_bound(entries_.begin(),entries_.end(),identity,[](const SpriteEntry& e,uint64_t h){return e.identity<h;});
        if(it==entries_.end() || it->identity!=identity)return nullptr;
        index=it-entries_.begin();return &*it;
    }
    const SpriteEntry* At(uint32_t index) const {return index<entries_.size()?&entries_[index]:nullptr;}
    bool ReadPacked(uint32_t index,unsigned variant,uint8_t* buffer,size_t capacity,size_t& packed) {
        lastReadBytes_=0;
        const auto* e=At(index);if(!file_ || !e || variant>=8 || !buffer || e->packed[variant]>capacity)return false;
        packed=e->packed[variant];const uint32_t offset=e->offset[variant];
        if(resident_.Bytes() && offset>=residentStart_ && offset-residentStart_<=residentBytes_ && packed<=residentBytes_-(offset-residentStart_)) {
            return resident_.Copy(offset-residentStart_,buffer,packed);
        }
        if(std::fseek(file_,offset,SEEK_SET)!=0 || std::fread(buffer,1,packed,file_)!=packed)return false;
        lastReadBytes_=packed;return true;
    }
    bool IsPaged() const{return paged_;}
    bool IsFastPaged() const{return fastPaged_;}
    size_t LastReadBytes() const{return lastReadBytes_;}
    size_t ResidentBytes() const{return residentBytes_;}
    size_t ResidentHeapBytes() const{return resident_.HeapBytes();}
    size_t RequiredResidentBytes(bool improved)const {
        if(!paged_ || !file_)return 0;
        uint32_t first=UINT32_MAX,last=0;
        for(const auto& e:entries_){const unsigned v=improved?4:0;first=std::min(first,e.offset[v]);last=std::max(last,e.offset[v]+e.packed[v]);}
        return first==UINT32_MAX || last<first?0:size_t(last-first);
    }
    bool DropResident(){const bool had=resident_.Bytes()!=0;resident_=SpriteResidentStorage<>();residentBytes_=residentStart_=0;return had;}
    // Call at a loading/logic boundary, never during GPU recording. Only the
    // selected Original/Improved range is resident; all other bytes stay on SD.
    // Budget is the total replacement allowance after reclaiming this range,
    // with the caller's heap reserve already deducted. Never hold two modes.
    bool MakeResident(bool improved,size_t budget) {
        if(!paged_ || !file_)return false;
        if(resident_.Bytes() && residentMode_==improved)return true;
        uint32_t first=UINT32_MAX,last=0;
        for(const auto& e:entries_){const unsigned v=improved?4:0;first=std::min(first,e.offset[v]);last=std::max(last,e.offset[v]+e.packed[v]);}
        DropResident();
        if(first==UINT32_MAX || last<first)return false;
        SpriteResidentStorage<> pending;
        if(!pending.Load(file_,first,last-first,budget))return false;
        resident_=std::move(pending);residentStart_=first;residentBytes_=last-first;residentMode_=improved;return true;
    }
    size_t IndexBytes() const {return entries_.size()*sizeof(SpriteEntry);}
private:
    struct Free {void operator()(uint8_t* p)const{std::free(p);}};
    FILE* file_=nullptr;std::vector<SpriteEntry> entries_;
    SpriteResidentStorage<> resident_;
    size_t residentBytes_=0,lastReadBytes_=0;uint32_t residentStart_=0;
    bool paged_=false,fastPaged_=false,residentMode_=false;
};
// Residency changes are legal only after the owner's GPU-completion fence.
// Slots touched in the current submission cannot be reused within that frame.
// Exhaustion is explicit; it never evicts a texture referenced by queued draws.
template<size_t Capacity> class SpriteResidency {
    struct Slot {uint64_t age=0,epoch=0;uint32_t key=0;bool occupied=false;};
    std::array<Slot,Capacity> slots_{};uint64_t epoch_=0,age_=0;
    size_t activeCapacity_=Capacity;
public:
    // Configure once, before any GPU ownership epoch. Never return an index
    // belonging to an unallocated texture on the smaller hardware profile.
    bool SetCapacity(size_t capacity) {
        if(epoch_ || !capacity || capacity>Capacity)return false;
        activeCapacity_=capacity;return true;
    }
    void BeginAfterCompletion(){++epoch_;}
    int Find(uint32_t key) {
        if(!epoch_)return -1;
        for(size_t i=0;i<activeCapacity_;++i)if(slots_[i].occupied && slots_[i].key==key){slots_[i].epoch=epoch_;slots_[i].age=++age_;return i;}
        return -1;
    }
    int Candidate() const {
        if(!epoch_)return -1;
        int best=-1;
        for(size_t i=0;i<activeCapacity_;++i) {
            if(!slots_[i].occupied)return i;
            if(slots_[i].epoch!=epoch_ && (best<0 || slots_[i].age<slots_[best].age))best=i;
        }
        return best;
    }
    bool Publish(unsigned slot,uint32_t key) {
        if(!epoch_ || slot>=activeCapacity_ || (slots_[slot].occupied && slots_[slot].epoch==epoch_ && slots_[slot].key!=key))return false;
        slots_[slot]={++age_,epoch_,key,true};return true;
    }
};
}
