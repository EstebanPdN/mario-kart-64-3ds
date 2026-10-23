#pragma once
#include "native_mesh_format_3ds.hpp"
#include <vector>
namespace mk64_3ds {
struct NativeDependency {uint64_t hash;uint32_t kind,bytes;};
static_assert(sizeof(NativeDependency)==16);
inline bool ValidateNativeDependencies(const uint8_t* data,size_t bytes,std::vector<NativeDependency>& result) {
    result.clear();if(!data || bytes<8 || std::memcmp(data,"NPR1",4))return false;
    const uint32_t count=native_scene::ReadLE32(data+4);
    if(count>8192 || bytes!=8+size_t(count)*16)return false;
    std::vector<NativeDependency> checked(count);
    if(count)std::memcpy(checked.data(),data+8,count*16);
    uint64_t previous=0;
    for(size_t i=0;i<count;++i) {
        const auto& item=checked[i];if((i && item.hash<=previous) || !item.hash || item.kind<1 || item.kind>3 ||
            !item.bytes || item.bytes>8*1024*1024)return false;
        previous=item.hash;
    }
    result=std::move(checked);return true;
}
constexpr size_t NativePreloadBudget(size_t heap,size_t used) {
    constexpr size_t reserve=8u*1024u*1024u,cap=2u*1024u*1024u;
    if(used>=heap || heap-used<=reserve)return 0;
    const size_t available=heap-used-reserve;return available<cap?available:cap;
}
}
