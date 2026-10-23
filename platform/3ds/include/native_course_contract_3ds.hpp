#pragma once
#include "native_exit_state_3ds.hpp"
namespace mk64_3ds::native_scene {
// Read-before-write masks compiled from the complete immutable root. Different
// callers must use their own state path, rather than silently inheriting Luigi.
struct IncomingCourseState {
    uint32_t geometry=0,low=0,high=0,combine=0,scale=0;
    bool Matches(uint32_t g,uint32_t l,uint32_t h,uint64_t c,uint64_t preset,
                 uint16_t s,uint16_t t) const {
        return !((g^0x2205u)&geometry) && !((l^0x552078u)&low) &&
            !((h^0x82000u)&high) && (!combine || c==preset) &&
            (!scale || (s==65535 && t==65535));
    }
};
static_assert(sizeof(IncomingCourseState)==20);
struct CourseExitExtra {
    uint32_t geometryMask,geometry,highMask,high,lowMask,low,flags,numLights;
    uint64_t combine;
    int32_t fogMul,fogOffset;
    uint32_t fog,primitive,environment;
};
// Disk encoding is packed explicitly; ARM structure tail padding is ignored.
constexpr size_t kCourseExitExtraBytes=60;
inline bool ValidateCourseExits(const uint8_t* bytes,size_t size,uint32_t vertices,
        uint32_t roots,std::vector<ExitState>& out,std::vector<CourseExitExtra>& extra,uint32_t& textures) {
    out.clear();extra.clear();textures=0;
    if(!bytes || size<16 || !roots || roots>4096 || std::memcmp(bytes,"NCX2",4) ||
       ReadLE32(bytes+4)!=2 || ReadLE32(bytes+8)!=roots ||
       size!=16+size_t(roots)*(sizeof(ExitState)+kCourseExitExtraBytes))return false;
    // Reuse the established bounds validation for all source-owner references.
    std::vector<uint8_t> base(16+size_t(roots)*sizeof(ExitState));
    std::memcpy(base.data(),bytes,16);std::memcpy(base.data(),"NCX1",4);base[4]=1;
    std::vector<CourseExitExtra> checked(roots);
    for(uint32_t i=0;i<roots;++i) {
        const auto* p=bytes+16+size_t(i)*(sizeof(ExitState)+kCourseExitExtraBytes);
        std::memcpy(base.data()+16+size_t(i)*sizeof(ExitState),p,sizeof(ExitState));
        std::memcpy(&checked[i],p+sizeof(ExitState),kCourseExitExtraBytes);
        const auto& e=checked[i];
        if(e.flags>63 || e.numLights>33 || e.numLights==0 ||
           e.fogMul<-32768 || e.fogMul>32767 || e.fogOffset<-32768 || e.fogOffset>32767)return false;
    }
    if(!ValidateExits(base.data(),base.size(),vertices,out,textures,roots,3,512))return false;
    extra=std::move(checked);return true;
}
}
