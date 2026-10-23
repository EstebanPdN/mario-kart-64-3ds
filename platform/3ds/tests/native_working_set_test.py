#!/usr/bin/env python3
"""Run actual native working-set preparation against instrumented resources."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1]
source=(port/'source/native_program_3ds.cpp').read_text()
functions=source[source.index('struct PreparedResource {'):source.index('struct Context {')]
release=source[source.index('extern "C" void Mk64NativeProgramReleaseWorkingSet()'):source.index('extern "C" void Mk64NativeProgramPreload(')]
gpu=(port/'source/native_gpu_sprites_3ds.cpp').read_text()
retry=gpu[gpu.index('bool GpuSprites::PrepareTextureMode('):gpu.index('bool GpuSprites::LoadEffects(')]
test=r'''
#include "native_preload_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "native_sprite_page_3ds.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
size_t used=6*1024*1024,heap=16*1024*1024;
int mode=0;unsigned loads=0,hashes=0,pins=0,releases=0,logs=0;
bool failTexture=false,failData=false;
size_t textureCost=256;
struct Info {size_t uordblks;};
Info mallinfo(){return {used};}
size_t envGetHeapSize(){return heap;}
int Mk64Settings3DSGetUiTextures(){return mode;}
void Mk64Diagnostics3DSCheckpoint(const char*){++logs;}
namespace Fast {
struct Texture {
    unsigned char pixel=42;
    void* ImageData=&pixel;
    int selectedMode=mode;
    size_t cost=textureCost;
    Texture(){used+=cost;}
    ~Texture(){used-=cost;}
};
}
namespace Ship {
struct Manager {
    Manager* GetArchiveManager(){return this;}
    const char* HashToCString(uint64_t){++hashes;return "test/texture";}
    std::shared_ptr<Fast::Texture> LoadResourceProcess(const char*) {
        ++loads;if(failTexture)throw std::bad_alloc();
        return std::make_shared<Fast::Texture>();
    }
};
struct Context {
    static Context* GetRawInstance(){static Context c;return &c;}
    Manager* GetResourceManager(){static Manager m;return &m;}
};
}
std::array<unsigned,2> raw{17,29};
extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireDataByCrc(uint64_t hash,void** data,uint64_t* token) {
    *data=nullptr;*token=0;
    if(failData)return MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS;
    ++pins;used+=256;*data=&raw.at(hash==20?0:1);*token=hash;return MK64_RESOURCE_LOAD_OK_3DS;
}
extern "C" void Mk64Resource3DSReleaseData(uint64_t token) {
    assert(token==20 || token==30);++releases;used-=256;
}
FUNCTIONS
RELEASE
namespace mk64_3ds::native_scene {
struct FakePack {
    unsigned attempts=0;size_t resident=0;bool succeed=false;
    size_t ResidentBytes()const{return resident;}
    size_t RequiredResidentBytes(bool)const{return resident;}
    bool DropResident(){const bool had=resident!=0;resident=0;return had;}
    bool MakeResident(bool,size_t budget){++attempts;resident=succeed?budget:0;return resident!=0;}
};
struct GpuSprites {
    int preparedMode_=-1;FakePack pack_;
    SpritePageCache<16> pageCache_;
    struct Tiles {bool Load(const char*,size_t,const FakePack&){return false;}bool Drop(){return false;}} originalTiles_;
    std::string originalTilesPath_;
    size_t ResidentBytes()const{return pack_.ResidentBytes();}
    bool PrepareTextureMode(bool,size_t,bool=false);
};
RETRY
}
int main(){
    mk64_3ds::native_scene::GpuSprites sprites;
    assert(!sprites.PrepareTextureMode(true,256) && sprites.pack_.attempts==1);
    sprites.pack_.succeed=true;
    for(unsigned i=0;i<1000;++i)assert(!sprites.PrepareTextureMode(true,256));
    assert(sprites.pack_.attempts==1); // No frame-time I/O retry after reclamation/failure.
    assert(sprites.PrepareTextureMode(true,256,true) && sprites.pack_.attempts==2);
    assert(sprites.PrepareTextureMode(true,256,true) && sprites.pack_.attempts==2);
    assert(sprites.PrepareTextureMode(false,256) && sprites.pack_.attempts==3);
    const auto initial=used;
    dependencies={{10,1,256},{20,2,256},{30,3,256}};
    PrepareWorkingSet();
    assert(prepared.size()==3 && used==initial+768 && pins==2 && loads==1);
    assert(Prepared(10)->owner->selectedMode==0 && Prepared(10)->name);
    assert(*static_cast<unsigned*>(Prepared(20)->data)==17 && !Prepared(99));
    raw[0]=103;assert(*static_cast<unsigned*>(Prepared(20)->data)==103); // Mutable data is never snapshotted.
    const auto before=logs;
    Mk64NativeProgramPrepareMode();assert(logs==before && loads==1 && hashes==1);
    mode=2;Mk64NativeProgramPrepareMode();
    assert(prepared.size()==3 && preparedMode==2 && Prepared(10)->owner->selectedMode==2);
    assert(loads==2 && pins==4 && releases==2 && used==initial+768);
    Mk64NativeProgramReleaseWorkingSet();assert(prepared.empty() && dependencies.empty() && used==initial && pins==releases);
    dependencies={{10,1,256},{20,2,256},{30,3,256}};
    heap=used+8*1024*1024;PrepareWorkingSet();assert(prepared.empty() && used==initial);
    heap=16*1024*1024;failTexture=true;PrepareWorkingSet();assert(prepared.empty() && used==initial);
    failTexture=false;failData=true;PrepareWorkingSet();assert(prepared.size()==1 && used==initial+256);
    Mk64NativeProgramReleaseWorkingSet();assert(used==initial && pins==releases);
    failData=false;dependencies={{10,1,256},{20,2,256},{30,3,256}};
    // Actual allocation exceeding the estimate is not retained in the working set.
    textureCost=3*1024*1024;PrepareWorkingSet();assert(prepared.empty() && used==initial);
    Mk64NativeProgramReleaseWorkingSet();
    std::puts("PASS: actual native preparation retains live resources, reuses an unchanged mode, releases old modes, respects the reserve, and tolerates missing/OOM/oversized optional resources.");
}
'''.replace('FUNCTIONS',functions).replace('RELEASE',release).replace('RETRY',retry)
cpp=a.output/'native-working-set.cpp';cpp.write_text(test)
binary=a.output/'native-working-set'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
    '-I',str(port/'include'),str(cpp),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
