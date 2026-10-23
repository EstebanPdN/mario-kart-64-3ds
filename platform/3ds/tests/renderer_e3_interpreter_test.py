#!/usr/bin/env python3
"""Compile real generated vertex/consumer/material functions under ASan/UBSan."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--fixtures',type=Path,help='Private captured-run fixture; never store it in Source')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];root=port.parents[1];up=root/'third_party/SpaghettiKart/libultraship'
driver=out/'apply.cmake'
driver.write_text(f'''set(MK64_3DS_SOURCE_ROOT "{root}")
set(CMAKE_CURRENT_BINARY_DIR "{out}")
set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE "{out}/interpreter.cpp")
file(READ "{up}/src/fast/interpreter.cpp" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
include(platform/3ds/cmake/interpreter_patches_3ds.cmake)
''')
subprocess.run(['cmake','-P',str(driver)],cwd=root,check=True)
s=(out/'interpreter.cpp').read_text()
def block(marker):
 start=s.index(marker);i=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
pre=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "fast/interpreter.h"
#include "gfx_citro3d.h"
#include "native_lighting_3ds.h"
#include "fast3d_reuse_3ds.hpp"
#include "position_cache_3ds.hpp"
#include "fast3d_culling_3ds.hpp"
#include "static_runs_3ds.hpp"
#include "original_fog_3ds.hpp"
#include "ship/utils/Utils.h"
#include "performance_trace_3ds.hpp"
namespace mk64_3ds {
uint64_t PerformanceNow(){return 0;}
bool PerformanceTimerEnabled(const uint32_t*){return false;}
PerformanceTick& PerformanceCurrent(){static PerformanceTick p;return p;}
}
#define SUPPORT_CHECK(x) assert(x)
namespace Fast {
using Cache=mk64_3ds::StaticRunCache<MAX_VERTICES+4>;
static Cache sStaticRuns3DS(1024*1024,768);
static mk64_3ds::NativeLightingVertices<MAX_VERTICES+4> sNativeLights3DS;
static bool resourceFrame=true;
static bool Mk64InResourceFrame3DS(){return resourceFrame;}
static void Mk64DisableStaticLight3DS(size_t slot){sNativeLights3DS.Disable(slot);}
static float aspect=0.75f;
Interpreter::Interpreter(){mRsp=new RSP{};mRdp=new RDP{};}
Interpreter::~Interpreter(){delete mRsp;delete mRdp;}
float Interpreter::AdjXForAspectRatio(float x) const{return x*aspect;}
void Interpreter::CalculateNormalDir(const F3DLight_t*,float[3]){assert(false);}
void Interpreter::TransposedMatrixMul(float[3],const float[3],const float[4][4]){assert(false);}
#define C0(pos,width) ((cmd->words.w0>>(pos))&((1U<<(width))-1))
#define C1(pos,width) ((cmd->words.w1>>(pos))&((1U<<(width))-1))
static Interpreter* mInstance;
static UcodeHandlers ucode_handler_index=ucode_f3dex;
static unsigned returns=0,branches=0;
GfxExecStack g_exec_stack;
F3DGfx* GfxExecStack::ret(){++returns;return nullptr;}
void GfxExecStack::branch(F3DGfx*){++branches;}
static F3DGfx target[2]{};
static F3DGfx* Mk64TimedRawPointer3DS(uint64_t){return target;}
static void Mk64MarkResourceFrame3DS(){}
'''
functions='\n'.join(block(x) for x in ['static uint32_t Mk64StaticLoadSignature3DS(',
 'static bool Mk64StaticRecordable3DS(', 'static ::mk64_3ds::StaticRunTransform Mk64StaticTransform3DS(',
 'static void Mk64MaterializeSlot3DS(', 'void Interpreter::GfxSpVertex(',
 'void Interpreter::GfxSpModifyVertex(', 'bool gfx_cull_dl_handler_f3dex2(',
 'bool gfx_branch_z_otr_handler_f3dex2(', 'bool gfx_tri1_handler_f3dex(', 'bool gfx_tri2_handler_f3dex(', 'bool gfx_quad_handler_f3dex('])
post=r'''
void Interpreter::GfxSpTri1(uint8_t a,uint8_t b,uint8_t c,bool rectangle){assert(!rectangle);sStaticRuns3DS.CaptureTriangle(a,b,c);}
void* Allocate(size_t n){return std::malloc(n);}void Release(void* p){std::free(p);}bool Publish(const void*,size_t){return true;}
static uint32_t sourceGeneration=1;
uint32_t Generation(){return sourceGeneration;}
bool Immutable(const void*,size_t){return false;}
void Camera(Interpreter& gfx,int n,bool fog){
 std::memset(gfx.mRsp->MP_matrix,0,sizeof(gfx.mRsp->MP_matrix));
 gfx.mRsp->MP_matrix[0][0]=0.01f;gfx.mRsp->MP_matrix[1][1]=0.02f;
 gfx.mRsp->MP_matrix[2][2]=1;gfx.mRsp->MP_matrix[2][3]=0.125f;
 gfx.mRsp->MP_matrix[3][0]=n*0.125f;gfx.mRsp->MP_matrix[3][3]=20+n;
 gfx.mRsp->geometry_mode=fog?G_FOG:0;
 gfx.mRsp->fog_mul=319;gfx.mRsp->fog_offset=-17;
 gfx.mRsp->texture_scaling_factor.s=0x8000;gfx.mRsp->texture_scaling_factor.t=0xffff;
}
void Equal(const LoadedVertex& a,const LoadedVertex& b){
 if(!(a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w && a.u==b.u && a.v==b.v)) std::fprintf(stderr,"expected %g %g %g %g %g %g got %g %g %g %g %g %g\n",a.x,a.y,a.z,a.w,a.u,a.v,b.x,b.y,b.z,b.w,b.u,b.v);
 assert(a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w && a.u==b.u && a.v==b.v);
 assert(std::memcmp(&a.color,&b.color,sizeof(a.color))==0 && a.clip_rej==b.clip_rej);
}

void ReplayCaptures(Interpreter& gfx){
 const char* path=std::getenv("MK64_RENDERER_FIXTURES");if(!path)return;
 FILE* file=std::fopen(path,"rb");assert(file);
 auto read=[&](void* p,size_t n){assert(std::fread(p,1,n,file)==n);};
 auto word=[&](){uint32_t n;read(&n,4);return n;};
 const auto records=word();uint64_t triangles=0;
 for(unsigned record=0;record<records;++record){
  const auto sig=word(),bytes=word(),loads=word(),nv=word(),ni=word();
  std::vector<uint8_t> commands(bytes);read(commands.data(),bytes);
  std::vector<mk64_3ds::StaticRunVertex> expected(nv);read(expected.data(),nv*sizeof(expected[0]));
  std::vector<uint16_t> indices(ni);read(indices.data(),ni*2);
  struct Load {unsigned count,first;std::vector<F3DVtx> vertices;};std::vector<Load> ranges;
  for(unsigned i=0;i<loads;++i){Load load;load.count=word();load.first=word();load.vertices.resize(load.count);read(load.vertices.data(),load.count*16);ranges.push_back(std::move(load));}
  for(int fog=0;fog<2;++fog){
   sStaticRuns3DS.Clear();Camera(gfx,int(record%97),fog);
   gfx.mRsp->texture_scaling_factor.s=uint16_t(sig);gfx.mRsp->texture_scaling_factor.t=uint16_t(sig>>16);
   auto transform=Mk64StaticTransform3DS(&gfx);sStaticRuns3DS.BeginCapture(commands.data(),sig,transform);
   unsigned load=0;
   for(unsigned offset=0;offset<bytes;offset+=8){F3DGfx command{};uint32_t words[2];std::memcpy(words,commands.data()+offset,8);command.words.w0=words[0];command.words.w1=words[1];auto* cursor=&command;auto op=words[0]>>24;
    if(op==0x32 || op==0x24 || op==0x04){assert(load<ranges.size());auto& range=ranges[load++];
     if(op==0x32){assert(((words[0]>>12)&255)==range.count);assert((((words[0]>>1)&127)-range.count)==range.first);offset+=8;}
     else if(op==0x24)offset+=8;
     else {assert(((words[0]>>10)&63)==range.count);assert(((words[0]>>17)&127)==range.first);}
     gfx.GfxSpVertex(range.count,range.first,range.vertices.data());
    }else if(op==0xbf)gfx_tri1_handler_f3dex(&cursor);
    else if(op==0xb1)gfx_tri2_handler_f3dex(&cursor);
    else if(op==0xb5)gfx_quad_handler_f3dex(&cursor);
    else assert(false);
   }
   assert(load==ranges.size() && sStaticRuns3DS.EndCapture(commands.data()+bytes));
   auto* entry=sStaticRuns3DS.Find(commands.data(),sig,transform);assert(entry && entry->vertexCount==nv && entry->indexCount==ni);
   assert(std::memcmp(entry->vertices,expected.data(),nv*sizeof(expected[0]))==0);
   assert(std::memcmp(entry->indices,indices.data(),ni*2)==0);
   std::array<LoadedVertex,68> reference{};
   for(unsigned slot=0;slot<64;++slot){mk64_3ds::gNativeGeometry3DS.Materialize(slot,gfx.mRsp->loaded_vertices[slot]);reference[slot]=gfx.mRsp->loaded_vertices[slot];}
   for(int repeat=0;repeat<10;++repeat)sStaticRuns3DS.Replayed(*entry,transform);
   Camera(gfx,200,!fog);sStaticRuns3DS.Clear();
   for(const auto& range:ranges)for(unsigned k=0;k<range.count;++k){const auto slot=range.first+k;Mk64MaterializeSlot3DS(&gfx,slot);mk64_3ds::gNativeGeometry3DS.Materialize(slot,gfx.mRsp->loaded_vertices[slot]);Equal(reference[slot],gfx.mRsp->loaded_vertices[slot]);}
  }
  triangles+=ni/3;
 }
 assert(std::fgetc(file)==EOF);std::fclose(file);
 std::printf("PASS: %u physical captured display-list runs / %llu ordered triangles; byte-identical cached geometry/indices, two fog modes, 20 replays each and retired-slot CPU equality.\n",records,(unsigned long long)triangles);
}

int main(){
 sStaticRuns3DS.SetAllocator({Allocate,Release,Publish});
 sStaticRuns3DS.SetValidation({Generation,Immutable});
 Interpreter gfx;mInstance=&gfx;ReplayCaptures(gfx);
 F3DVtx vertices[4]{};
 for(int i=0;i<4;++i){vertices[i].v.ob[0]=100+i*17;vertices[i].v.ob[1]=20-i*2;vertices[i].v.ob[2]=10+i*5;
  vertices[i].v.tc[0]=-100+i*21;vertices[i].v.tc[1]=320-i*41;
  vertices[i].v.cn[0]=20+i*3;vertices[i].v.cn[1]=200-i*7;vertices[i].v.cn[2]=i*51;vertices[i].v.cn[3]=15+i*70;}
 F3DGfx commands[4]{};
 for(int fog=0;fog<2;++fog){
  sStaticRuns3DS.Clear();mk64_3ds::gNativeGeometry3DS.ResetVertices();Camera(gfx,0,fog);
  const auto transform=Mk64StaticTransform3DS(&gfx);const auto signature=Mk64StaticLoadSignature3DS(gfx.mRsp);
  sStaticRuns3DS.BeginCapture(commands,signature,transform);
  gfx.GfxSpVertex(4,0,vertices);sStaticRuns3DS.CaptureTriangle(0,1,2);sStaticRuns3DS.CaptureTriangle(0,2,3);
  assert(sStaticRuns3DS.EndCapture(commands+3));
  LoadedVertex reference[4];for(int i=0;i<4;++i){mk64_3ds::gNativeGeometry3DS.Materialize(i,gfx.mRsp->loaded_vertices[i]);reference[i]=gfx.mRsp->loaded_vertices[i];}
  auto* entry=sStaticRuns3DS.Find(commands,signature,transform);assert(entry);
  const auto copies=sStaticRuns3DS.Counters().snapshotCopies;
  for(int repeat=0;repeat<1000;++repeat)sStaticRuns3DS.Replayed(*entry,transform);
  assert(sStaticRuns3DS.Counters().snapshotCopies==copies);
  // Replays keep owned values through source mutation and unrelated cameras.
  auto old=vertices[0];vertices[0].v.ob[0]+=200;Camera(gfx,200,!fog);
  for(int i=0;i<4;++i){Mk64MaterializeSlot3DS(&gfx,i);mk64_3ds::gNativeGeometry3DS.Materialize(i,gfx.mRsp->loaded_vertices[i]);Equal(reference[i],gfx.mRsp->loaded_vertices[i]);}
  vertices[0]=old;
  // Retirement detaches the outstanding slots before the cache's actual free.
  sStaticRuns3DS.Replayed(*entry,transform);sStaticRuns3DS.Clear();
  for(int i=0;i<4;++i){Mk64MaterializeSlot3DS(&gfx,i);mk64_3ds::gNativeGeometry3DS.Materialize(i,gfx.mRsp->loaded_vertices[i]);Equal(reference[i],gfx.mRsp->loaded_vertices[i]);}
  Camera(gfx,0,fog);sStaticRuns3DS.BeginCapture(commands,signature,transform);gfx.GfxSpVertex(4,0,vertices);
  sStaticRuns3DS.CaptureTriangle(0,1,2);assert(sStaticRuns3DS.EndCapture(commands+3));entry=sStaticRuns3DS.Find(commands,signature,transform);assert(entry);
  sStaticRuns3DS.Replayed(*entry,transform);gfx.GfxSpModifyVertex(0,G_MWO_POINT_ST,0xffe00019);
  assert(gfx.mRsp->loaded_vertices[0].u==-32 && gfx.mRsp->loaded_vertices[0].v==25 && !sStaticRuns3DS.IsVirtual(0));
  // Actual BranchZ and CULLDL handlers must read the original load camera.
  sStaticRuns3DS.Replayed(*entry,transform);Camera(gfx,100,!fog);
  F3DGfx branch[2]{};branch[0].words.w1=uint32_t(reference[0].z+1);auto* cursor=branch;
  assert(gfx_branch_z_otr_handler_f3dex2(&cursor) && cursor==target && branches==unsigned(fog+1));
  sStaticRuns3DS.Replayed(*entry,transform);
  F3DGfx cull{};cull.words.w1=3*2;cursor=&cull;
  assert(!gfx_cull_dl_handler_f3dex2(&cursor));
  for(int i=0;i<4;++i){Mk64MaterializeSlot3DS(&gfx,i);mk64_3ds::gNativeGeometry3DS.Materialize(i,gfx.mRsp->loaded_vertices[i]);Equal(reference[i],gfx.mRsp->loaded_vertices[i]);}
  // A new generation rejects before touching freed command/source memory.
  ++sourceGeneration;assert(!sStaticRuns3DS.Find(commands,signature,transform));
 }
 sStaticRuns3DS.Clear();
 std::puts("PASS: actual patched vertex/ModifyVertex/BranchZ/CULLDL functions; 2000 lazy replays; exact load-time color/fog/UV/depth/cull state after mutation, retirement and camera changes.");
 return 0;
}
} // namespace Fast
int main(){return Fast::main();}
'''
code=pre+functions+post
cpp=out/'interpreter-consumers.cpp';cpp.write_text(code)
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DF3DEX_GBI=1','-I',str(port/'include'),'-I',str(port/'probe_stubs'),'-I',str(up/'include')]
exe=out/'interpreter-consumers'
subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
env=__import__('os').environ.copy()
if a.fixtures:env['MK64_RENDERER_FIXTURES']=str(a.fixtures.resolve())
r=subprocess.run([str(exe)],capture_output=True,text=True,env=env);(out/'interpreter-consumers.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr;print(r.stdout,end='')
