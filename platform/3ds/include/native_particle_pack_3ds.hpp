#pragma once
#include "native_mesh_format_3ds.hpp"
#include <tuple>

namespace mk64_3ds::native_scene {
struct ParticleImage {
    uint64_t identity;
    uint32_t nameOffset;
    uint16_t nameBytes;
    uint8_t format,reserved;
    uint16_t width,height;
    uint32_t reserved2;
};
static_assert(sizeof(ParticleImage)==24);
struct ParticlePackView {
    const uint8_t* images=nullptr;
    const uint8_t* quads=nullptr;
    uint32_t imageCount=0;
};
inline uint64_t ParticleImageIdentity(const char* name,size_t bytes) {
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=0;i<bytes;++i){hash^=static_cast<uint8_t>(name[i]);hash*=1099511628211ULL;}
    return hash;
}
// Startup-only borrowed view. This cache contains native quad data and image
// identities; it contains no source-list instructions or runtime decode plan.
// Individual NSI1 payloads are checked/decompressed before GPU publication.
// Quad UVs retain authored integer values; capture applies the declared half
// scale exactly once, before this shared sprite shader consumes them.
inline bool ValidateParticlePack(const void* bytes,size_t size,ParticlePackView& out) {
    out={};if(!bytes || size<32 || size>32*1024)return false;
    const auto* p=static_cast<const uint8_t*>(bytes);
    const uint32_t n=ReadLE32(p+8),quads=ReadLE32(p+12),images=ReadLE32(p+16),
        geometry=ReadLE32(p+20),names=ReadLE32(p+24);
    if(std::memcmp(p,"NPF1",4) || ReadLE32(p+4)!=1 || !n || n>64 || quads!=3 ||
       images!=32 || geometry!=32+n*24 || names!=geometry+3*4*sizeof(Vertex) ||
       names>size || ReadLE32(p+28)!=size)return false;
    size_t cursor=names;
    std::tuple<uint64_t,unsigned,unsigned,unsigned> previous{};
    for(uint32_t i=0;i<n;++i) {
        ParticleImage image;std::memcpy(&image,p+images+i*24,sizeof(image));
        const auto key=std::tuple(image.identity,unsigned(image.format),unsigned(image.width),unsigned(image.height));
        if(!image.identity || (i && key<=previous) || image.format>2 || image.reserved || image.reserved2 ||
           !image.width || image.width>64 || !image.height || image.height>64 ||
           image.nameOffset!=cursor || image.nameBytes<2 || image.nameBytes>256 ||
           image.nameBytes>size-cursor)return false;
        const char* name=reinterpret_cast<const char*>(p+cursor);
        if(name[image.nameBytes-1] || std::memchr(name,0,image.nameBytes-1) ||
           ParticleImageIdentity(name,image.nameBytes-1)!=image.identity)return false;
        cursor+=image.nameBytes;previous=key;
    }
    if(cursor!=size)return false;
    for(unsigned i=0;i<12;++i) {
        Vertex vertex;std::memcpy(&vertex,p+geometry+i*sizeof(Vertex),sizeof(Vertex));
        if(vertex.reserved)return false;
    }
    out={p+images,p+geometry,n};return true;
}
}
