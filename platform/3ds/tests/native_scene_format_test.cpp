#include "native_scene_format_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

using namespace mk64_3ds::native_scene;
static void Write32(std::vector<std::uint8_t>& bytes,size_t at,std::uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes[at+i]=value>>(8*i);
}
int main(int argc,char** argv) {
    if (argc==3) {
        auto read=[](const char* path) {
            std::ifstream f(path,std::ios::binary);
            assert(f);
            return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)),{});
        };
        auto mesh=read(argv[1]),scene=read(argv[2]);
        MeshView m;SceneView s;
        assert(ValidateMesh(mesh.data(),mesh.size(),m));
        assert(ValidateScene(scene.data(),scene.size(),m.indexCount,s));
        return 0;
    }
    std::vector<std::uint8_t> bytes(126,0);
    std::memcpy(bytes.data(),"NLC3",4);
    for (unsigned i=4;i<28;i+=4) Write32(bytes,i,i==4 ? 3 : 1);
    Write32(bytes,32,8);Write32(bytes,36,8);Write32(bytes,40,256);
    Material material{};material.kind=1;material.cull=2;
    material.uvScaleOffset[0]=material.uvScaleOffset[1]=1.f/256.f;
    material.uvClamp[0]=material.uvClamp[1]=1e6;
    std::memcpy(bytes.data()+44,&material,sizeof material);
    Write32(bytes,96,3); // range index count; first/material/reserved remain zero
    Write32(bytes,112,1); // view reference count
    SceneView view;
    assert(ValidateScene(bytes.data(),bytes.size(),3,view));
    for (size_t n=0;n<bytes.size();++n) {
        assert(!ValidateScene(bytes.data(),n,3,view));
        assert(!view.textures && !view.materials && !view.viewCount);
    }
    for (size_t at:{size_t(8),size_t(12),size_t(16),size_t(20),size_t(24),size_t(28),
                    size_t(44),size_t(48),size_t(52),size_t(56),size_t(84),size_t(88),
                    size_t(92),size_t(96),size_t(100),size_t(104),size_t(108),size_t(112),
                    size_t(116),size_t(120)}) {
        auto bad=bytes;Write32(bad,at,0xffffffff);
        assert(!ValidateScene(bad.data(),bad.size(),3,view));
    }
    auto bad=bytes;bad[124]=1;
    assert(!ValidateScene(bad.data(),bad.size(),3,view));
    bad=bytes;Write32(bad,60,0x7fc00000); // NaN UV
    assert(!ValidateScene(bad.data(),bad.size(),3,view));
    bad=bytes;bad.push_back(0);
    assert(!ValidateScene(bad.data(),bad.size(),3,view));
    assert(!ValidateScene(bytes.data(),bytes.size(),2,view));
    bad=bytes;bad.insert(bad.begin(),0);
    assert(ValidateScene(bad.data()+1,bytes.size(),3,view));
    for(unsigned alpha=2;alpha<=3;++alpha) {
        bad=bytes;Write32(bad,56,alpha);assert(!ValidateScene(bad.data(),bad.size(),3,view));
        std::memcpy(bad.data(),"NLC4",4);Write32(bad,4,4);
        assert(ValidateScene(bad.data(),bad.size(),3,view));
    }
    Write32(bad,56,4);assert(!ValidateScene(bad.data(),bad.size(),3,view));
    Write32(bad,56,3);Write32(bad,4,3);assert(!ValidateScene(bad.data(),bad.size(),3,view));
    auto native=bytes;
    native.insert(native.begin()+92,sizeof(RasterState),0);
    std::memcpy(native.data(),"NLC5",4);Write32(native,4,5);
    RasterState raster;std::memcpy(native.data()+92,&raster,sizeof raster);
    assert(ValidateScene(native.data(),native.size(),3,view) && view.materialStride==56);
    for(unsigned kind=0;kind<=6;++kind) {
        auto supported=native;Write32(supported,48,kind);
        assert(ValidateScene(supported.data(),supported.size(),3,view));
        if(kind>2) {
            auto legacy=bytes;Write32(legacy,48,kind);
            assert(!ValidateScene(legacy.data(),legacy.size(),3,view));
        }
    }
    auto unknown=native;Write32(unknown,48,7);
    assert(!ValidateScene(unknown.data(),unknown.size(),3,view));
    for(unsigned legacyAlpha=1;legacyAlpha<=3;++legacyAlpha) {
        auto conflicting=native;Write32(conflicting,56,legacyAlpha);
        assert(!ValidateScene(conflicting.data(),conflicting.size(),3,view));
    }
    for(unsigned depth=0;depth<2;++depth)for(unsigned write=0;write<2;++write)
    for(unsigned decal=0;decal<2;++decal)for(unsigned test=0;test<2;++test)
    for(unsigned blend=0;blend<2;++blend)for(unsigned filter=0;filter<2;++filter)
    for(unsigned reference:{8u,48u})for(unsigned output=0;output<2;++output) {
        raster={uint8_t(depth),uint8_t(write),uint8_t(decal),uint8_t(test),uint8_t(reference),uint8_t(blend),uint8_t(filter),uint8_t(output)};
        std::memcpy(native.data()+92,&raster,sizeof raster);
        assert(ValidateScene(native.data(),native.size(),3,view));
    }
    for(unsigned at=0;at<sizeof(RasterState);++at) {
        auto invalid=native;invalid[92+at]=at==4?9:2;
        assert(!ValidateScene(invalid.data(),invalid.size(),3,view) && !view.materials);
    }
    // Two interleaved records must advance by the NLC5 stride. The second
    // range selects the second material, not the first record's raster bytes.
    auto two=native;
    two.insert(two.begin()+100,native.begin()+44,native.begin()+100);
    Write32(two,12,2);Write32(two,104,2); // second material kind
    Write32(two,164,1); // range material id after both 56-byte records
    assert(ValidateScene(two.data(),two.size(),3,view) && view.materialCount==2);
    assert(ReadLE32(view.materials+view.materialStride+4)==2);
    two[100+48+7]=2;
    assert(!ValidateScene(two.data(),two.size(),3,view));
    for(size_t n=0;n<native.size();++n)assert(!ValidateScene(native.data(),n,3,view));
}
