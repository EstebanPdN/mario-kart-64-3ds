#include <3ds.h>
#include "native_gpu_scene_3ds.hpp"
#include "gpu_command_budget_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace mk64_3ds::native_scene;
struct DrawEvent {
    std::vector<std::uint16_t> indices;
    std::array<C3D_TexEnv,6> environment;
    std::array<int,9> raster;
    C3D_Mtx matrix;
};
static std::vector<DrawEvent> events;
void C3D_DrawElements(int,int count,int,const void* input) {
    const auto* indices=static_cast<const std::uint16_t*>(input);
    events.push_back({{indices,indices+count},mock::env,mock::raster,mock::matrix});
}
extern "C" bool Mk64System3DSCleanDataCache(const void*,std::size_t){return true;}
template<class T> static void Append(std::vector<std::uint8_t>& bytes,const T& object) {
    const auto* p=reinterpret_cast<const std::uint8_t*>(&object);bytes.insert(bytes.end(),p,p+sizeof object);
}
static void Write(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(output);
}
int main() {
    const std::filesystem::path directory="native-scene-submission-fixture";
    std::filesystem::create_directories(directory);
    std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;
    const std::int16_t centers[]={0,40,3,-40,1,50,2,60};
    for(unsigned triangle=0;triangle<8;++triangle)for(unsigned corner=0;corner<3;++corner) {
        Vertex vertex{};vertex.position[0]=centers[triangle]+(corner==1?1:0);
        vertex.position[1]=corner==2?1:0;vertex.color[0]=triangle*24;vertex.color[3]=255;
        indices.push_back(vertices.size());vertices.push_back(vertex);
    }
    std::vector<std::uint8_t> mesh{'N','C','M','1'};
    for(std::uint32_t value:{std::uint32_t(vertices.size()),std::uint32_t(indices.size()),16u})Append(mesh,value);
    for(const auto& vertex:vertices)Append(mesh,vertex);for(auto index:indices)Append(mesh,index);
    Write(directory/"mesh.ncm",mesh);
    std::vector<std::uint8_t> scene{'N','L','C','5'};
    for(std::uint32_t value:{5u,1u,2u,8u,1u,8u,0u})Append(scene,value);
    Append(scene,TextureInfo{8,8,256});
    for(unsigned kind:{1u,6u}) {
        Material material{};material.kind=kind;material.uvClamp[0]=material.uvClamp[1]=1;
        Append(scene,material);RasterState raster{};raster.depthTest=raster.depthWrite=1;
        raster.blend=kind==6;raster.alphaOutput=kind==6;Append(scene,raster);
    }
    for(unsigned i=0;i<8;++i)Append(scene,DrawRange{i*3,3,i%2,0});
    Append(scene,View{0,8,0,0});for(std::uint16_t i=0;i<8;++i)Append(scene,i);
    Write(directory/"scene.nlc",scene);Write(directory/"texture-000.ctex",std::vector<std::uint8_t>(256,255));
    GpuScene gpu;assert(gpu.Load(directory.c_str()) && gpu.VisibilityBytes()>0);
    Camera camera{};camera.rows[0][0]=camera.rows[1][1]=camera.rows[2][2]=1;
    camera.rows[2][3]=-5;camera.rows[3][3]=10;
    std::uint64_t triangles=0;gpu.SetVisibilityEnabled(false);
    assert(gpu.Draw(0,camera,triangles)==8 && triangles==8);const auto reference=events;
    events.clear();triangles=0;gpu.SetVisibilityEnabled(true);
    assert(gpu.Draw(0,camera,triangles)==4 && triangles==4 && events.size()==4);
    for(unsigned i=0;i<4;++i) {
        const auto& a=reference[i*2];const auto& b=events[i];
        assert(a.indices==b.indices && a.raster==b.raster);
        assert(!std::memcmp(&a.matrix,&b.matrix,sizeof a.matrix));
        assert(!std::memcmp(a.environment.data(),b.environment.data(),sizeof a.environment));
    }
    // Repeated views reuse the MVP only within a batch. A changed camera and
    // a new batch must both cause a fresh upload.
    gpu.BeginBatch();events.clear();triangles=0;const auto start=mock::matrices;
    gpu.Draw(0,camera,triangles,true);gpu.Draw(0,camera,triangles,true);
    assert(mock::matrices==start+1 && events.size()==8);
    camera.rows[0][3]=1;gpu.Draw(0,camera,triangles,true);assert(mock::matrices==start+2);
    gpu.BeginBatch();gpu.Draw(0,camera,triangles,true);assert(mock::matrices==start+3);
    // A wholly clipped root performs no native program/material/matrix setup.
    camera.rows[0][3]=10000;events.clear();const auto programs=mock::programs,matrices=mock::matrices;
    assert(gpu.Draw(0,camera,triangles)==0 && events.empty());
    assert(mock::programs==programs && mock::matrices==matrices);
    // The ordinary command headroom gate remains active for visible work.
    camera.rows[0][3]=0;mock::commandCapacity=1;bool rejected=false;
    try{gpu.Draw(0,camera,triangles);}catch(const mk64_3ds::GpuCommandPressure&){rejected=true;}
    assert(rejected);mock::commandCapacity=262144;
    gpu.ResetAfterCompletion();assert(gpu.VisibilityBytes()==0);
    std::puts("PASS: actual GpuScene load/draw with ordered reference submissions, material state, clipped roots, matrix reuse/invalidation, command-pressure rejection and metadata release. SDK calls only, no GPU pixel/FPS claim.");
}
