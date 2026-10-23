#!/usr/bin/env python3
"""Exercise all three actual patched image handlers and their weak owners."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);port=Path(__file__).resolve().parents[1];root=port.parents[1]
driver=out/'apply.cmake';driver.write_text(f'''set(MK64_3DS_SOURCE_ROOT "{root}")
set(CMAKE_CURRENT_BINARY_DIR "{out}")
set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE "{out}/interpreter.cpp")
file(READ "{root}/third_party/SpaghettiKart/libultraship/src/fast/interpreter.cpp" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
include(platform/3ds/cmake/interpreter_patches_3ds.cmake)
''');subprocess.run(['cmake','-P',str(driver)],cwd=root,check=True)
s=(out/'interpreter.cpp').read_text()
def block(marker):
 start=s.index(marker);i=s.index('{',start)+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
helpers=s[s.index('namespace {\nstruct Mk64TextureImageEntry3DS'):s.index('bool gfx_set_timg_handler_rdp(')]
handlers='\n'.join(block(x) for x in ['bool gfx_set_timg_handler_rdp(', 'bool gfx_set_timg_otr_hash_handler_custom(', 'bool gfx_set_timg_otr_filepath_handler_custom('])
pre=r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#define __3DS__ 1
#define SPDLOG_ERROR(...) ((void)0)
namespace mk64_3ds {struct Tick{uint32_t timg_us=0;};Tick tick;Tick& PerformanceCurrent(){return tick;}struct PerformanceTimer{explicit PerformanceTimer(uint32_t&) {}};}
static uint32_t generation=1;static int mode=0;static unsigned loads=0;static bool missing=false;
uint32_t Mk64Resource3DSGeneration(){return generation;}
int Mk64Settings3DSGetUiTextures(){return mode;}
struct Init{std::string Path;};
namespace Fast {struct Texture {
 std::shared_ptr<Init> init=std::make_shared<Init>();std::array<uint8_t,64> pixels{};
 uint8_t* ImageData=pixels.data();uint32_t Flags=0;uint16_t Width=8,Height=8;float HByteScale=1,VPixelScale=1;int Type=1;
 std::shared_ptr<Init> GetInitData(){return init;}
};}
namespace Ship {
struct Archive {const char* HashToCString(uint64_t hash){return hash==1?"textures/one":hash==2?"textures/two":nullptr;}};
struct Manager {Archive archive;Archive* GetArchiveManager(){return &archive;}
 std::shared_ptr<Fast::Texture> LoadResourceProcess(const char* name){++loads;if(missing)return nullptr;
  auto t=std::make_shared<Fast::Texture>();t->init->Path=std::strncmp(name,"__OTR__",7)==0?name+7:name;
  t->pixels.fill(uint8_t(mode+generation));t->Flags=77;t->Width=mode?16:8;t->HByteScale=mode?2:1;t->Type=2;return t;}
};
struct Context{Manager manager;static Context* GetRawInstance(){static Context c;return &c;}Manager* GetResourceManager(){return &manager;}};
}
struct RawTexMetadata{uint16_t width=0,height=0;float h_byte_scale=1,v_pixel_scale=1;int type=0;std::shared_ptr<Fast::Texture> resource;};
struct F3DGfx{struct {uintptr_t w0,w1;} words;};
struct Event {unsigned fmt,size,width,flags;const void* data;RawTexMetadata meta;};
struct Interpreter {
 std::vector<Event> events;void* segment=nullptr;
 void* SegAddr(uintptr_t p){return p&1?segment:reinterpret_cast<void*>(p);}
 void GfxDpSetTextureImage(unsigned f,unsigned s,unsigned w,const char*,unsigned flags,RawTexMetadata meta,const void* data){events.push_back({f,s,w,flags,data,std::move(meta)});}
};
Interpreter interpreter;Interpreter* mInstance=&interpreter;
int gfx_check_image_signature(const char* p){return p && std::strncmp(p,"__OTR__",7)==0;}
#define C0(pos,width) ((cmd->words.w0>>(pos))&((1U<<(width))-1))
'''
post=r'''
int main(){
 alignas(8) char name[128]="__OTR__textures/one";F3DGfx cmd{{(2U<<21)|(1U<<19)|7,reinterpret_cast<uintptr_t>(name)}};F3DGfx* cursor=&cmd;
 for(int i=0;i<2000;++i)gfx_set_timg_handler_rdp(&cursor);
 assert(loads==1 && interpreter.events.size()==2000);auto owner=interpreter.events[0].meta.resource;assert(owner);
 for(const auto& e:interpreter.events)assert(e.data==owner->ImageData && e.meta.resource==owner && e.flags==77 && e.fmt==2 && e.size==1 && e.width==8 && e.meta.width==8 && e.meta.h_byte_scale==1);
 interpreter.events.clear();
 mode=1;gfx_set_timg_handler_rdp(&cursor);assert(loads==2 && interpreter.events.back().meta.resource!=owner && interpreter.events.back().meta.width==16);
 ++generation;gfx_set_timg_handler_rdp(&cursor);assert(loads==3);
 mode=2;gfx_set_timg_handler_rdp(&cursor);assert(loads==4);
 std::strcpy(name,"__OTR__textures/two");gfx_set_timg_handler_rdp(&cursor);assert(loads==5 && interpreter.events.back().meta.resource->init->Path=="textures/two");
 interpreter.events.clear();gfx_set_timg_handler_rdp(&cursor);assert(loads==6); // Expired weak owner must reload.
 auto lease=interpreter.events.back().meta.resource;interpreter.events.clear();
 // Segmented resource names use the same route, raw buffers never enter it.
 interpreter.segment=name;cmd.words.w1=0x01000001;gfx_set_timg_handler_rdp(&cursor);assert(loads==6 && interpreter.events.back().meta.resource==lease);
 alignas(8) char raw[64]="raw framebuffer pixels";interpreter.segment=raw;gfx_set_timg_handler_rdp(&cursor);assert(loads==6 && interpreter.events.back().data==raw && !interpreter.events.back().meta.resource);
 interpreter.segment=nullptr;gfx_set_timg_handler_rdp(&cursor);assert(loads==6 && !interpreter.events.back().data);
 // Custom filepath and archive-hash handlers retain exact metadata/owners.
 cmd.words.w1=reinterpret_cast<uintptr_t>(name);gfx_set_timg_otr_filepath_handler_custom(&cursor);assert(loads==6);
 F3DGfx hashed[2]={{{0,0}},{{0,1}}};cursor=hashed;gfx_set_timg_otr_hash_handler_custom(&cursor);const auto hashOwner=interpreter.events.back().meta.resource;assert(hashOwner && cursor==hashed+1);
 const auto before=loads;cursor=hashed;gfx_set_timg_otr_hash_handler_custom(&cursor);assert(loads==before && interpreter.events.back().meta.resource==hashOwner);
 // Content mismatch is checked even when the pointer bucket collides.
 std::vector<std::shared_ptr<Fast::Texture>> pinned;
 for(unsigned i=0;i<1024;++i){alignas(8) char recycled[64];std::snprintf(recycled,sizeof(recycled),"__OTR__textures/%u",i);auto t=Mk64TextureByName3DS(recycled);assert(t->init->Path==std::string(recycled+7));pinned.push_back(t);}
 missing=true;std::strcpy(name,"__OTR__textures/missing");cmd.words.w1=reinterpret_cast<uintptr_t>(name);cursor=&cmd;gfx_set_timg_handler_rdp(&cursor);assert(!interpreter.events.back().data && !interpreter.events.back().meta.resource);
 hashed[1].words.w1=99;cursor=hashed;gfx_set_timg_otr_hash_handler_custom(&cursor);assert(!interpreter.events.back().data);
 uint64_t stats[6];Mk64TextureImages3DSStats(stats);assert(stats[0]>=2000 && stats[1]>=1999 && stats[3]>=1 && stats[5]>=1);
 std::puts("PASS: three actual image handlers, 2000 ordinary named calls/one load, raw/segmented/missing data, weak expiry, owners, policy/generation changes, 1024 recycled names and per-route counters.");return 0;
}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-Wno-tautological-pointer-compare']
for name,code,success in [('texture-handlers',helpers+handlers,True),('negative-mode',helpers.replace(' ||\n        entry.mode != Mk64TextureImageMode3DS()','')+handlers,False)]:
 cpp=out/(name+'.cpp');exe=out/name;cpp.write_text(pre+code+post);subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert (r.returncode==0)==success,r.stdout+r.stderr
 if success:print(r.stdout,end='')
print('PASS: policy-stale negative control fails the actual ordinary handler.')
