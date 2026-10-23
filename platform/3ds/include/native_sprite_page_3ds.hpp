#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <array>
#include <memory>
#include <cstdlib>

namespace mk64_3ds::native_scene {
constexpr size_t kSpriteImageBytes=16384;
constexpr size_t kSpritePageBytes=16+4*kSpriteImageBytes;
constexpr size_t kSpritePackedCapacity=kSpritePageBytes+1024;
inline uint32_t SpritePageU32(const uint8_t* p) {
    return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
template<class Decoder> bool DecodeSpritePage(const uint8_t* packed,size_t bytes,
                                             uint8_t* page,Decoder decode) {
    return packed && page && bytes>4 && bytes<=kSpritePackedCapacity &&
        decode(packed+4,bytes-4,page,kSpritePageBytes,SpritePageU32(packed));
}
inline bool ExtractSpritePage(const uint8_t* page,unsigned lane,uint32_t crc,uint8_t* output) {
    if(!page || !output || lane>=4 || SpritePageU32(page+lane*4)!=crc)return false;
    const auto* base=page+16;
    if(!lane)std::memcpy(output,base,kSpriteImageBytes);
    else {
        const auto* delta=base+lane*kSpriteImageBytes;
        for(size_t i=0;i<kSpriteImageBytes;++i)output[i]=base[i]^delta[i];
    }
    return true;
}
inline bool IsSpritePageAlias(uint32_t flags) {return (flags&0xff)==2 && flags<0x400;}
// The caller has already deducted the ordinary-heap reserve. Prefer keeping
// the complete compressed range; grow only with spare loading-boundary space.
constexpr size_t DecodedSpritePageCapacity(bool improved,size_t budget,size_t packedBytes) {
    if(!improved)return 4;
    const size_t spare=packedBytes<=budget?budget-packedBytes:budget;
    return spare>=16*kSpritePageBytes?16:spare>=8*kSpritePageBytes?8:4;
}
template<size_t Capacity=8> class SpritePageCache {
    static_assert(Capacity>0);
    struct Free {void operator()(uint8_t* p)const{std::free(p);}};
    struct Slot {uint64_t age=0,key=0;bool valid=false;};
    std::unique_ptr<uint8_t,Free> bytes_;
    std::array<Slot,Capacity> slots_{};uint64_t age_=0;size_t capacity_=0;
    bool loading_=false;
public:
    bool Initialize(size_t capacity=Capacity){if(bytes_)return false;return Resize(capacity);}
    // Decoded pages are copied into fenced GPU textures immediately by Load's
    // caller. No page pointer may survive a loading-boundary resize.
    bool Resize(size_t capacity) {
        if(loading_ || !capacity || capacity>Capacity)return false;
        if(capacity_==capacity)return true;
        auto* pending=static_cast<uint8_t*>(std::realloc(bytes_.get(),capacity*kSpritePageBytes));
        if(!pending)return false; // Retain the valid old allocation on failure.
        bytes_.release();bytes_.reset(pending);capacity_=capacity;slots_={};age_=0;return true;
    }
    size_t ActiveCapacity()const{return capacity_;}
    size_t Bytes()const{return capacity_*kSpritePageBytes;}
    void Clear() {if(!loading_){bytes_.reset();slots_={};capacity_=0;age_=0;}}
    template<class Loader> const uint8_t* Load(uint64_t key,Loader load) {
        if(!bytes_ || loading_)return nullptr;
        struct Scope {bool& flag;Scope(bool& f):flag(f){flag=true;}~Scope(){flag=false;}} scope(loading_);
        for(size_t i=0;i<capacity_;++i) {
            if(slots_[i].valid && slots_[i].key==key){slots_[i].age=++age_;return bytes_.get()+i*kSpritePageBytes;}
        }
        size_t slot=0;
        for(size_t i=0;i<capacity_;++i){if(!slots_[i].valid){slot=i;break;}if(slots_[i].age<slots_[slot].age)slot=i;}
        slots_[slot].valid=false;auto* page=bytes_.get()+slot*kSpritePageBytes;
        if(!load(page))return nullptr;
        slots_[slot]={++age_,key,true};return page;
    }
};
}
