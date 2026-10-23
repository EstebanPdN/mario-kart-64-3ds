#pragma once
#include <array>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#ifdef __3DS__
#include <3ds.h>
#endif
namespace mk64_3ds::native_scene {
struct SpriteVramStorage {
    static size_t Available() {
#ifdef __3DS__
        constexpr size_t reserve=1536u*1024u;const size_t available=vramSpaceFree();return available>reserve?available-reserve:0;
#else
        return 0;
#endif
    }
    static uint8_t* Allocate(size_t bytes) {
#ifdef __3DS__
        return static_cast<uint8_t*>(vramAlloc(bytes));
#else
        (void)bytes;return nullptr;
#endif
    }
    static void Release(uint8_t* pointer) {
#ifdef __3DS__
        vramFree(pointer);
#else
        (void)pointer;
#endif
    }
};
// Only immutable compressed bytes spill into otherwise unused VRAM. The owner
// replaces this storage after GPU completion; GPU textures never alias it.
template<class Spill=SpriteVramStorage> class SpriteResidentStorage {
    struct HeapFree {void operator()(uint8_t* p)const{std::free(p);}};
    struct SpillFree {void operator()(uint8_t* p)const{Spill::Release(p);}};
    std::unique_ptr<uint8_t,HeapFree> heap_;
    std::array<std::unique_ptr<uint8_t,SpillFree>,4> spill_;
    std::array<size_t,4> sizes_{};
    size_t heapBytes_=0,bytes_=0;
public:
    size_t Bytes()const{return bytes_;}
    size_t HeapBytes()const{return heapBytes_;}
    bool Load(FILE* file,size_t offset,size_t bytes,size_t budget) {
        if(!file || !bytes)return false;
        SpriteResidentStorage pending;
        pending.heapBytes_=bytes<=budget?bytes:budget&~size_t(3);
        size_t remaining=bytes-pending.heapBytes_;
        // Round the heap split down to aligned word boundaries. The last VRAM
        // chunk may contain byte data; all writes pass through aligned staging.
        if(remaining>Spill::Available() || remaining>4u*1024u*1024u)return false;
        for(unsigned i=0;remaining && i<4;++i) {
            const size_t n=std::min(remaining,size_t(1024u*1024u));
            pending.spill_[i].reset(Spill::Allocate((n+3)&~size_t(3)));if(!pending.spill_[i])return false;
            pending.sizes_[i]=n;remaining-=n;
        }
        if(pending.heapBytes_) {
            pending.heap_.reset(static_cast<uint8_t*>(std::malloc(pending.heapBytes_)));if(!pending.heap_)return false;
        }
        if(std::fseek(file,offset,SEEK_SET))return false;
        if(pending.heapBytes_ && std::fread(pending.heap_.get(),1,pending.heapBytes_,file)!=pending.heapBytes_)return false;
        alignas(128) std::array<uint8_t,4096> staging{};
        for(unsigned i=0;i<4;++i)for(size_t at=0;at<pending.sizes_[i];at+=staging.size()) {
            const size_t n=std::min(staging.size(),pending.sizes_[i]-at);
            if(std::fread(staging.data(),1,n,file)!=n)return false;
            // VRAM rejects byte stores. The allocation and destination offset
            // are word aligned, including the padded final word.
            auto* dst=reinterpret_cast<volatile uint32_t*>(pending.spill_[i].get()+at);
            const auto* src=reinterpret_cast<const uint32_t*>(staging.data());
            for(size_t word=0;word<(n+3)/4;++word)dst[word]=src[word];
        }
        pending.bytes_=bytes;*this=std::move(pending);return true;
    }
    bool Copy(size_t offset,void* output,size_t count)const {
        if(!output || offset>bytes_ || count>bytes_-offset)return false;
        auto* dst=static_cast<uint8_t*>(output);
        if(offset<heapBytes_){const size_t n=std::min(count,heapBytes_-offset);std::memcpy(dst,heap_.get()+offset,n);dst+=n;count-=n;offset+=n;}
        if(!count)return true;
        offset-=heapBytes_;
        for(unsigned i=0;i<4 && count;++i) {
            if(offset>=sizes_[i]){offset-=sizes_[i];continue;}
            const size_t n=std::min(count,sizes_[i]-offset);
            // Reads may begin at an arbitrary compressed-byte offset. Aligned
            // word reads also work on hardware where byte VRAM reads do not.
            const auto* src=reinterpret_cast<const volatile uint32_t*>(spill_[i].get());
            size_t k=0;
            while(k<n) {
                const size_t at=offset+k;const uint32_t word=src[at/4];
                const size_t take=std::min(n-k,4-(at&3));
                for(size_t b=0;b<take;++b)dst[k+b]=uint8_t(word>>(((at&3)+b)*8));
                k+=take;
            }
            dst+=n;count-=n;offset=0;
        }
        return count==0;
    }
};
}
