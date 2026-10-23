#pragma once
#include "native_exit_state_3ds.hpp"
namespace mk64_3ds::native_scene {
struct VegetationExit {
    uint32_t geometryClear,geometrySet,highMask,highValue,lowMask,lowValue;
    uint32_t primitiveWritten,primitive,paletteWritten,paletteTexture;
};
static_assert(sizeof(VegetationExit)==40);
inline bool ValidateVegetationExits(const void* data,size_t bytes,uint32_t textures,std::vector<VegetationExit>& out,uint32_t count=12,uint32_t geometryMask=0x23000u) {
    out.clear();if(!data || !count || count>256 || (geometryMask&~0x23200u) || bytes!=8+count*sizeof(VegetationExit))return false;
    const auto* p=static_cast<const uint8_t*>(data);
    if(std::memcmp(p,"NVX1",4) || ReadLE32(p+4)!=count)return false;
    std::vector<VegetationExit> checked(count);std::memcpy(checked.data(),p+8,bytes-8);
    for(const auto& s:checked) {
        if((s.geometryClear|s.geometrySet)&~geometryMask || (s.geometryClear&s.geometrySet) ||
           s.highMask&~0x8f000u || (s.highValue&~s.highMask) || (s.lowValue&~s.lowMask) ||
           s.primitiveWritten>1 || s.paletteWritten>1 || (s.paletteWritten && s.paletteTexture>=textures))return false;
    }
    out=std::move(checked);return true;
}
}
