#include "native_exit_state_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace mk64_3ds::native_scene;
int main(int argc,char** argv) {
    std::vector<uint8_t> bytes(16+123*sizeof(ExitState));
    std::memcpy(bytes.data(),"NCX1",4);uint32_t header[]={1,123,1};std::memcpy(bytes.data()+4,header,sizeof header);
    std::vector<ExitState> result;uint32_t textures=0;
    assert(ValidateExits(bytes.data(),bytes.size(),1,result,textures));assert(result.size()==123 && textures==1);
    for(size_t n:{size_t(0),size_t(15),bytes.size()-1})assert(!ValidateExits(bytes.data(),n,1,result,textures) && result.empty() && textures==0);
    const auto good=bytes;
    auto reject=[&](size_t at,uint32_t value) {bytes=good;std::memcpy(bytes.data()+16+at,&value,4);assert(!ValidateExits(bytes.data(),bytes.size(),1,result,textures));};
    reject(offsetof(ExitState,shader),3);reject(offsetof(ExitState,renderTile),8);
    reject(offsetof(ExitState,tileMask),256);reject(offsetof(ExitState,loadedMask),4);
    reject(offsetof(ExitState,tiles)+offsetof(ExitTile,memory),512);
    bytes=good;ExitState state{};state.sourceWritten=1;state.sourceTexture=1;std::memcpy(bytes.data()+16,&state,sizeof state);
    assert(!ValidateExits(bytes.data(),bytes.size(),1,result,textures));
    state={};state.slotMaskHigh=0x80000000;state.vertices[63]=1;std::memcpy(bytes.data()+16,&state,sizeof state);
    assert(!ValidateExits(bytes.data(),bytes.size(),1,result,textures));
    state.vertices[63]=0;std::memcpy(bytes.data()+16,&state,sizeof state);assert(ValidateExits(bytes.data(),bytes.size(),1,result,textures));
    if(argc==3) {
        auto read=[](const char* path){std::ifstream f(path,std::ios::binary);assert(f);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});};
        bytes=read(argv[1]);const auto mesh=read(argv[2]);MeshView view;assert(ValidateMesh(mesh.data(),mesh.size(),view));
        assert(ValidateExits(bytes.data(),bytes.size(),view.vertexCount,result,textures));assert(textures==7);
    }
}
