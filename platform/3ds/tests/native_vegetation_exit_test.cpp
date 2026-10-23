#include "native_vegetation_exit_3ds.hpp"
#include <cassert>
using namespace mk64_3ds::native_scene;
int main() {
    std::vector<uint8_t> bytes(8+12*sizeof(VegetationExit));std::memcpy(bytes.data(),"NVX1",4);
    const uint32_t count=12;std::memcpy(bytes.data()+4,&count,4);std::vector<VegetationExit> checked;
    assert(ValidateVegetationExits(bytes.data(),bytes.size(),3,checked) && checked.size()==12);
    const auto valid=bytes;
    auto reject=[&](unsigned offset,unsigned value){bytes=valid;std::memcpy(bytes.data()+8+offset,&value,4);assert(!ValidateVegetationExits(bytes.data(),bytes.size(),3,checked) && checked.empty());};
    reject(offsetof(VegetationExit,geometryClear),0x400000);
    reject(offsetof(VegetationExit,highMask),1);
    reject(offsetof(VegetationExit,highValue),0x8000);
    reject(offsetof(VegetationExit,primitiveWritten),2);
    reject(offsetof(VegetationExit,paletteWritten),2);
    bytes=valid;VegetationExit item{};item.paletteWritten=1;item.paletteTexture=3;std::memcpy(bytes.data()+8,&item,sizeof item);
    assert(!ValidateVegetationExits(bytes.data(),bytes.size(),3,checked));
    item.paletteTexture=2;std::memcpy(bytes.data()+8,&item,sizeof item);assert(ValidateVegetationExits(bytes.data(),bytes.size(),3,checked));
    std::vector<uint8_t> exit(16+12*sizeof(ExitState));std::memcpy(exit.data(),"NCX1",4);
    const uint32_t header[3]={1,12,3};std::memcpy(exit.data()+4,header,sizeof header);
    ExitState state{};state.shader=3;std::memcpy(exit.data()+16,&state,sizeof state);
    std::vector<ExitState> decoded;uint32_t textures=0;
    assert(!ValidateExits(exit.data(),exit.size(),1,decoded,textures));
    assert(ValidateExits(exit.data(),exit.size(),1,decoded,textures,12,3));
    state.shader=4;std::memcpy(exit.data()+16,&state,sizeof state);
    assert(!ValidateExits(exit.data(),exit.size(),1,decoded,textures,12,3));
    // Extended object tables must opt into their count and smooth-shading bit.
    std::vector<uint8_t> extended(8+21*sizeof(VegetationExit));std::memcpy(extended.data(),"NVX1",4);
    const uint32_t extendedCount=21;std::memcpy(extended.data()+4,&extendedCount,4);
    VegetationExit smooth{};smooth.geometrySet=0x200;std::memcpy(extended.data()+8,&smooth,sizeof smooth);
    assert(!ValidateVegetationExits(extended.data(),extended.size(),3,checked));
    assert(!ValidateVegetationExits(extended.data(),extended.size(),3,checked,21));
    assert(ValidateVegetationExits(extended.data(),extended.size(),3,checked,21,0x23200));
    assert(!ValidateVegetationExits(extended.data(),extended.size(),3,checked,0));
    assert(!ValidateVegetationExits(extended.data(),extended.size(),3,checked,257));
    assert(!ValidateVegetationExits(extended.data(),extended.size(),3,checked,21,~0u));

}
