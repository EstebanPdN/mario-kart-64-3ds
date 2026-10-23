#pragma once
#include "native_mesh_format_3ds.hpp"

namespace mk64_3ds::native_scene {

// Development-only selection of two native plans over the same resident mesh,
// materials and images. Neither route enters the source-list interpreter.
struct BatchComparison {
    uint32_t roots=0, referenceBase=0, optimizedBase=0;
};
inline bool ValidateBatchComparison(const void* bytes,size_t size,uint32_t roots,
                                   uint32_t views,BatchComparison& out) {
    out={};
    if(!bytes || size!=20 || !roots || roots>1024 || views!=roots*4)return false;
    const auto* p=static_cast<const uint8_t*>(bytes);
    if(std::memcmp(p,"NBP1",4) || ReadLE32(p+4)!=1 || ReadLE32(p+8)!=roots ||
       ReadLE32(p+12)!=0 || ReadLE32(p+16)!=roots*2)return false;
    out={roots,0,roots*2};return true;
}
}
