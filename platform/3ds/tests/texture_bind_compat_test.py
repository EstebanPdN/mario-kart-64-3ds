#!/usr/bin/env python3
"""Exercise the actual patched SDK bind and native entry functions under sanitizers."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1]

def function(source,start):
    begin=source.index(start);brace=source.index('{',begin);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]+'\n'

bind=function((port/'compat/citro3d-1.7.1/texture.c').read_text(),'void C3D_TexBind(')
gfx=(port/'source/gfx_citro3d.cpp').read_text()
scene=function(gfx,'void GfxRenderingAPICitro3D::BeginNativeSceneBatch(')
sprite=function(gfx,'void GfxRenderingAPICitro3D::BeginNativeSpriteBatch(')
pre=r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
using u32=uint32_t;
constexpr u32 C3DiF_Active=1;
#define C3DiF_Tex(n) (1u<<(23+(n)))
enum GPU_TEXTURE_MODE_PARAM { GPU_TEX_2D=0, GPU_TEX_CUBE_MAP=1 };
struct C3D_Tex { u32 param=0; };
// Same C3D_TexGetType operation as the SDK header: null is not valid here.
GPU_TEXTURE_MODE_PARAM C3D_TexGetType(C3D_Tex* tex){return GPU_TEXTURE_MODE_PARAM((tex->param>>28)&7);}
struct C3D_Context {u32 flags=0;C3D_Tex* tex[3]={};};
C3D_Context context;
C3D_Context* C3Di_GetContext(){return &context;}
bool procedural=false;unsigned flushed=0,begun=0;
void C3D_ProcTexBind(int unit,void* value){assert(unit==0&&!value);procedural=false;}
namespace mk64_3ds::native_scene {
struct GpuScene {void BeginBatch(){assert(!context.tex[1]&&!context.tex[2]&&!procedural&&flushed);++begun;}};
struct GpuSprites {void BeginBatch(){assert(!context.tex[1]&&!context.tex[2]&&!procedural&&flushed);++begun;}};
}
struct GfxRenderingAPICitro3D {
 mk64_3ds::native_scene::GpuScene* mNativeSceneBatch=nullptr;
 mk64_3ds::native_scene::GpuSprites* mNativeSpriteBatch=nullptr;
 void FlushPackedVertices(){++flushed;}
 void BeginNativeSceneBatch(mk64_3ds::native_scene::GpuScene&);
 void BeginNativeSpriteBatch(mk64_3ds::native_scene::GpuSprites&);
};
'''
post=r'''
int main(){
 C3D_Tex image,cube;cube.param=1u<<28;
 context.flags=C3DiF_Active;
 // Exact failing call from the physical dump: r0=1, r1=0.
 C3D_TexBind(1,nullptr);assert(context.tex[1]==nullptr&&(context.flags&C3DiF_Tex(1)));
 for(int unit=0;unit<3;++unit){
  context={};context.flags=C3DiF_Active;
  C3D_TexBind(unit,&image);assert(context.tex[unit]==&image);
  assert(context.flags==(C3DiF_Active|C3DiF_Tex(unit)));
  C3D_TexBind(unit,&cube);assert(context.tex[unit]==(unit?&image:&cube));
  C3D_TexBind(unit,nullptr);assert(!context.tex[unit]);
 }
 for(int unit:{-1,3,100}){
  context={};context.flags=C3DiF_Active;C3D_TexBind(unit,nullptr);
  assert(context.flags==C3DiF_Active&&!context.tex[0]&&!context.tex[1]&&!context.tex[2]);
 }
 context={};C3D_TexBind(1,&image);assert(!context.flags&&!context.tex[1]);
 for(unsigned mask=0;mask<8;++mask)for(bool proc:{false,true}){
  GfxRenderingAPICitro3D gfx;mk64_3ds::native_scene::GpuScene scene;mk64_3ds::native_scene::GpuSprites sprites;
  auto reset=[&](){context={};context.flags=C3DiF_Active;for(int u=0;u<3;++u)context.tex[u]=(mask&(1u<<u))?&image:nullptr;procedural=proc;};
  reset();gfx.BeginNativeSceneBatch(scene);
  assert(context.tex[0]==((mask&1)?&image:nullptr)&&!procedural&&gfx.mNativeSceneBatch==&scene);
  bool rejected=false;try{gfx.BeginNativeSpriteBatch(sprites);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
  gfx.mNativeSceneBatch=nullptr;reset();gfx.BeginNativeSpriteBatch(sprites);
  assert(context.tex[0]==((mask&1)?&image:nullptr)&&!procedural&&gfx.mNativeSpriteBatch==&sprites);
 }
 assert(begun==32);
}
'''
flags=['clang++','-std=c++20','-DMK64_3DS_NATIVE_KART_LAB','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all']
for name,body in [('patched',bind),('negative-control',bind.replace('if (tex && unitId > 0','if (unitId > 0'))]:
    cpp=out/(name+'.cpp');exe=out/name
    cpp.write_text(pre+body+scene+sprite+post)
    subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (out/(name+'.log')).write_text(result.stdout+result.stderr)
    if name=='patched':
        assert result.returncode==0,result.stderr
    else:
        assert result.returncode!=0 and ('null pointer' in result.stderr or 'AddressSanitizer' in result.stderr),result.stderr
print('PASS: actual SDK bind covers null/2D/cube/inactive/invalid units; both native entries cover 16 stale states each. Original null guard negative control fails under sanitizer.')
