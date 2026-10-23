#pragma once
#include "native_sprite_pack_3ds.hpp"
#ifdef __3DS__
#include <3ds.h>
#endif

namespace mk64_3ds::native_scene {
// Immutable GPU-native RGB5A1 tiles. Original N64 colors already have five
// bits per channel and one alpha bit; conversion is performed by the packer.
// The complete dictionary/index is loaded and validated at a loading boundary.
class OriginalSpriteTiles {
    static uint32_t Checksum(const uint8_t* data,size_t bytes,uint32_t crc=~0u) {
        static constexpr auto table=[] {
            std::array<uint32_t,256> out{};
            for(unsigned i=0;i<256;++i){uint32_t v=i;for(unsigned b=0;b<8;++b)v=(v>>1)^((v&1)?0xedb88320u:0);out[i]=v;}
            return out;
        }();
        for(size_t i=0;i<bytes;++i)crc=(crc>>8)^table[(crc^data[i])&255];return crc;
    }
    struct Free {void operator()(uint8_t* p) const {std::free(p);}};
    struct SpillFree {void operator()(uint8_t* p) const {
#ifdef __3DS__
        vramFree(p);
#else
        (void)p;
#endif
    }};
    std::unique_ptr<uint8_t,Free> data_;
    std::array<std::unique_ptr<uint8_t,SpillFree>,4> spill_;
    std::array<size_t,4> spillBytes_{};
    size_t heapBytes_=0,bytes_=0;uint32_t entries_=0,index_=0,tiles_=0;
public:
    size_t ResidentBytes() const {return bytes_;}
    size_t HeapBytes() const {return heapBytes_;}
    bool Drop() {const bool had=bool(data_);data_.reset();for(auto& p:spill_)p.reset();spillBytes_={};heapBytes_=bytes_=0;entries_=index_=tiles_=0;return had;}
    bool Load(const char* path,size_t budget,const SpritePack& pack) {
        if(data_)return true;
        FILE* file=std::fopen(path,"rb");if(!file)return false;
        struct Close {FILE* f;~Close(){std::fclose(f);}} close{file};
        uint8_t h[32];if(std::fread(h,1,32,file)!=32 || std::memcmp(h,"NTL1",4) || ReadLE32(h+4)!=1)return false;
        const uint32_t n=ReadLE32(h+8),tileCount=ReadLE32(h+12),index=ReadLE32(h+16),tiles=ReadLE32(h+20),size=ReadLE32(h+24);
        if(!n || n>4096 || !tileCount || tileCount>1048576 || index!=32+n*16 ||
           tiles!=index+n*4*64*4 || uint64_t(tiles)+uint64_t(tileCount)*128!=size || pack.At(n) || !pack.At(n-1))return false;
        if(budget<tiles)return false;
        OriginalSpriteTiles pending;
        pending.heapBytes_=size<=budget?size:tiles+(budget-tiles)/128*128;
        size_t remaining=size-pending.heapBytes_;
#ifdef __3DS__
        // Preserve the ordinary-heap reserve. An immutable dictionary tail may
        // use otherwise idle VRAM, leaving 1.5 MiB for framebuffer changes.
        constexpr size_t reserve=1536u*1024u;
        if(remaining && (vramSpaceFree()<reserve || remaining>vramSpaceFree()-reserve))return false;
        for(unsigned i=0;remaining && i<pending.spill_.size();++i) {
            const size_t chunk=std::min(remaining,size_t(1024u*1024u));
            pending.spill_[i].reset(static_cast<uint8_t*>(vramAlloc(chunk)));
            if(!pending.spill_[i])return false;
            pending.spillBytes_[i]=chunk;remaining-=chunk;
        }
#endif
        if(remaining)return false;
        pending.data_.reset(static_cast<uint8_t*>(std::malloc(pending.heapBytes_)));
        if(!pending.data_)return false;
        std::memcpy(pending.data_.get(),h,32);
        if(std::fread(pending.data_.get()+32,1,pending.heapBytes_-32,file)!=pending.heapBytes_-32)return false;
        uint32_t checksum=Checksum(pending.data_.get()+32,pending.heapBytes_-32);
        // Never ask stdio to issue byte stores into VRAM. Copy aligned blocks
        // from the existing ordinary stack only during this loading boundary.
        alignas(128) std::array<uint8_t,4096> staging;
        for(unsigned i=0;i<pending.spill_.size();++i)for(size_t at=0;at<pending.spillBytes_[i];at+=staging.size()) {
            const size_t chunk=std::min(staging.size(),pending.spillBytes_[i]-at);
            if(std::fread(staging.data(),1,chunk,file)!=chunk)return false;
            checksum=Checksum(staging.data(),chunk,checksum);
            std::memcpy(pending.spill_[i].get()+at,staging.data(),chunk);
        }
        if(std::fgetc(file)!=EOF || ~checksum!=ReadLE32(h+28))return false;
        for(uint32_t i=0;i<n;++i)for(unsigned lane=0;lane<4;++lane)
            if(ReadLE32(pending.data_.get()+32+(i*4+lane)*4)!=pack.At(i)->crc[lane])return false;
        for(uint32_t at=index;at<tiles;at+=4)if(ReadLE32(pending.data_.get()+at)>=tileCount)return false;
        pending.bytes_=size;pending.entries_=n;pending.index_=index;pending.tiles_=tiles;
        *this=std::move(pending);return true;
    }
    bool Copy(uint32_t key,void* output,size_t capacity) const {
        if(!data_ || !output || capacity<8192 || key%8>=4 || key/8>=entries_)return false;
        const auto* indices=data_.get()+index_+size_t(key/8*4+key%8)*256;
        auto* destination=static_cast<uint8_t*>(output);
        for(unsigned i=0;i<64;++i) {
            size_t offset=tiles_+size_t(ReadLE32(indices+i*4))*128;
            const uint8_t* source=nullptr;
            if(offset<heapBytes_)source=data_.get()+offset;
            else {offset-=heapBytes_;for(unsigned chunk=0;chunk<spill_.size();++chunk) {
                if(offset<spillBytes_[chunk]){source=spill_[chunk].get()+offset;break;}
                offset-=spillBytes_[chunk];
            }}
            if(!source)return false;
            std::memcpy(destination+i*128,source,128);
        }
        return true;
    }
};
}
