#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace mk64_3ds {
// Raw game palettes have caller-owned storage. Resource-backed palettes have
// an authoritative length; never read a nonexistent tail to fill 256 entries.
inline bool CopyPaletteBytes(void* destination,const uint8_t* source,size_t count,
                             const uint8_t* ownerBase,size_t ownerBytes) {
    if(!destination || !source || count>512)return false;
    size_t available=count;
    if(ownerBase) {
        const uintptr_t begin=reinterpret_cast<uintptr_t>(ownerBase),at=reinterpret_cast<uintptr_t>(source);
        if(at<begin || at-begin>ownerBytes)return false;
        available=std::min(count,ownerBytes-(at-begin));
    }
    if(available)std::memcpy(destination,source,available);
    if(available<count)std::memset(static_cast<uint8_t*>(destination)+available,0,count-available);
    return true;
}
}
