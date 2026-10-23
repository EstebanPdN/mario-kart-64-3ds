#!/usr/bin/env python3
"""Compare the factored production material resolver with its pre-factor body."""
import argparse,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];root=port.parents[1];up=root/'third_party/SpaghettiKart/libultraship'
static=(port/'cmake/static_runs_3ds.cmake').read_text();(out/'static-reference.cmake').write_text(static.split('# Factor the actual production material resolver')[0])
patch=(port/'cmake/interpreter_patches_3ds.cmake').read_text()
for name,body in [('reference',patch.replace('include(platform/3ds/cmake/static_runs_3ds.cmake)',f'include("{out}/static-reference.cmake")')),('current',patch)]:
 driver=out/(name+'.cmake');driver.write_text(f'''set(MK64_3DS_SOURCE_ROOT "{root}")
set(CMAKE_CURRENT_BINARY_DIR "{out}")
set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE "{out}/{name}.cpp")
file(READ "{up}/src/fast/interpreter.cpp" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
'''+body);subprocess.run(['cmake','-P',str(driver)],cwd=root,check=True)
s=(out/'current.cpp').read_text();baseline=(out/'reference.cpp').read_text()
def block(text,marker):
 start=text.index(marker);i=text.index('{',start)+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
old=baseline[baseline.index('    struct TriangleMaterial3DS {'):baseline.index('    const auto comb = material3DS.comb;')]
for member in ['mRsp','mRdp','mRapi','mRenderingState','mShaderStack']:old=re.sub(r'\b'+member+r'\b','gfx->'+member,old)
for method in ['Flush','ImportTexture','ImportTextureMask','LookupOrCreateColorCombiner','LookupOrCreateShaderProgram']:old=old.replace(method+'(','gfx->'+method+'(')
old=re.sub(r'\bthis\b','gfx',old)
reference='static auto Reference(Interpreter* gfx,bool is_rect){\n'+old+'return material3DS;\n}\n'
api=(up/'include/fast/backends/gfx_rendering_api.h').read_text();stubs=[]
for signature in re.findall(r'virtual\s+([^;{}]+?)\s*=\s*0;',api,re.S):
 name=re.search(r'(\w+)\(',signature)[1]
 body=''
 if name=='ShaderGetInfo':body='*numInputs=prg->numInputs;usedTextures[0]=prg->usedTextures[0];usedTextures[1]=prg->usedTextures[1];events.push_back("info");'
 elif name in ['UnloadShader','LoadShader','SetUseAlpha','SetDepthTestAndMask','SetZmodeDecal','SetViewport','SetScissor','SetSamplerParameters']:body=f'events.push_back("{name}");'
 elif not signature.strip().startswith('void '):body='return {};'
 stubs.append(signature+' override {'+body+'}')
pre=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "fast/interpreter.h"
#include "gfx_citro3d.h"
#include "fast3d_math_3ds.hpp"
#include "triangle_reuse_3ds.hpp"
namespace Fast {
struct Api:GfxRenderingAPI {std::vector<std::string> events;
'''+ '\n'.join(stubs)+r'''
};
static mk64_3ds::TriangleRun sTriangleRun3DS;
static bool textures[2]{};static unsigned inputs=1;static unsigned imports=0,flushes=0;
Interpreter::Interpreter(){mRsp=new RSP{};mRdp=new RDP{};}
Interpreter::~Interpreter(){delete mRsp;delete mRdp;}
void Interpreter::Flush(){++flushes;}
void Interpreter::ImportTexture(int,int,bool){++imports;}
void Interpreter::ImportTextureMask(int,int){++imports;}
ColorCombiner* Interpreter::LookupOrCreateColorCombiner(const ColorCombinerKey& key){
 auto& c=mColorCombinerPool[key];c.shader_id1=key.options;c.shader_id0=key.combine_mode;
 c.usedTextures[0]=textures[0];c.usedTextures[1]=textures[1];
 c.shader_input_mapping[0][0]=G_CCMUX_SHADE;c.shader_input_mapping[1][0]=G_CCMUX_PRIMITIVE;
 c.shader_input_mapping[0][1]=G_CCMUX_ENVIRONMENT;c.shader_input_mapping[1][1]=G_CCMUX_ENVIRONMENT;return &c;
}
static std::map<std::pair<uint64_t,uint64_t>,ShaderProgram> programs;
ShaderProgram* Interpreter::LookupOrCreateShaderProgram(uint64_t id0,uint64_t id1){
 auto& p=programs[{id0,id1}];p.numInputs=inputs;p.usedTextures[0]=p.sampledTextures[0]=textures[0];p.usedTextures[1]=p.sampledTextures[1]=textures[1];p.alpha=id1&SHADER_OPT(ALPHA);p.fog=id1&SHADER_OPT(FOG);p.twoCycle=id1&SHADER_OPT(_2CYC);p.strideFloats=4;
 for(int t=0;t<2;++t)if(textures[t]){p.textureOffsets[t]=p.strideFloats;p.strideFloats+=2;}
 if(p.fog){p.fogOffset=p.strideFloats;p.strideFloats+=4;}
 for(unsigned i=0;i<inputs;++i){p.inputOffsets[i]=p.strideFloats;p.strideFloats+=p.alpha?4:3;}
 p.tevOperationCount[0][0]=p.tevOperationCount[0][1]=p.tevOperationCount[1][0]=p.tevOperationCount[1][1]=1;return &p;
}
static void Mk64NoteStaticReject3DS(uint32_t){}
'''
pre+=block(s,'struct Mk64StaticMaterial3DS {')+';\nstatic Mk64StaticMaterial3DS sStaticMaterial3DS;\n'
# The helper uses the unchanged tile-coordinate function from production.
pre+=block(s,'static inline uint32_t GetTileSizeFromCoordinates(')+'\n' if 'static inline uint32_t GetTileSizeFromCoordinates(' in s else block(s,'uint32_t GetTileSizeFromCoordinates(')+'\n'
helper=block(s,'struct TriangleMaterial3DS {')+';\n'+block(s,'static TriangleMaterial3DS Mk64ResolveTriangleMaterial3DS(')+'\n'+block(s,'static void Mk64FillStaticMaterial3DS(')+'\n'
post=r'''
int main(){
 for(unsigned scenario=0;scenario<1024;++scenario){Interpreter a,b;Api apiA,apiB;a.mRapi=&apiA;b.mRapi=&apiB;
  textures[0]=scenario&1;textures[1]=scenario&2;inputs=1+(scenario%2);
  a.mRsp->geometry_mode=scenario&4?G_ZBUFFER:0;
  a.mRdp->other_mode_l=(scenario&8)?((G_BL_CLR_MEM<<20)|(G_BL_1MA<<16)):0;
  if(scenario&16)a.mRdp->other_mode_l|=uint32_t(G_BL_CLR_FOG)<<30;
  if(scenario&32)a.mRdp->other_mode_l|=Z_UPD|Z_CMP;
  if(scenario&64)a.mRdp->other_mode_l|=ZMODE_DEC;
  a.mRdp->other_mode_h=scenario&128?G_CYC_2CYCLE:0;a.mRdp->combine_mode=scenario;
  a.mRdp->prim_color={41,72,183,97};a.mRdp->env_color={192,145,88,231};a.mRdp->fog_color={101,121,141,161};
  a.mRdp->first_tile_index=scenario%4;a.mRdp->viewport_or_scissor_changed=true;a.mRdp->viewport={1,2,300,200};a.mRdp->scissor={2,3,298,198};
  for(auto& tile:a.mRdp->texture_tile){tile.siz=G_IM_SIZ_16b;tile.line_size_bytes=32;tile.lrs=60;tile.lrt=60;tile.cms=tile.cmt=G_TX_CLAMP;tile.shifts=scenario%16;tile.shiftt=(scenario/16)%16;}
  for(auto& loaded:a.mRdp->loaded_texture){loaded.line_size_bytes=32;loaded.size_bytes=loaded.orig_size_bytes=512;loaded.full_image_line_size_bytes=32;}
  a.mRdp->textures_changed[0]=a.mRdp->textures_changed[1]=true;*b.mRsp=*a.mRsp;*b.mRdp=*a.mRdp;
  sTriangleRun3DS.Invalidate();const auto before=imports;const auto oldFlush=flushes;auto result=Mk64ResolveTriangleMaterial3DS(&a,false);auto imported=imports-before,flushed=flushes-oldFlush;
  const auto beforeRef=imports,flushRef=flushes;auto reference=Reference(&b,false);assert(imports-beforeRef==imported && flushes-flushRef==flushed && apiA.events==apiB.events);
  assert(result.use_alpha==reference.use_alpha && result.use_fog==reference.use_fog && result.use_blend_color==reference.use_blend_color && result.use_grayscale==reference.use_grayscale && result.tm==reference.tm && result.numInputs==reference.numInputs);
  assert(std::memcmp(result.tex_width,reference.tex_width,sizeof(result.tex_width))==0 && std::memcmp(result.tex_height,reference.tex_height,sizeof(result.tex_height))==0 && std::memcmp(result.effective_tile,reference.effective_tile,sizeof(result.effective_tile))==0);
  // Direct static plans preserve independent shade RGB/primitive alpha and
  // standard fog's forced opaque shade alpha through one or two cycles.
  Mk64FillStaticMaterial3DS(&a,result.comb,result.numInputs,result.use_alpha,result.use_fog,result.use_grayscale,result.tm,result.tex_width,result.tex_height,result.tex_width2,result.tex_height2,result.effective_tile,result.usedTextures,false);
  if(result.use_fog && textures[1])assert(!sStaticMaterial3DS.ok);
  else {assert(sStaticMaterial3DS.ok);assert(sStaticMaterial3DS.colorMask[0]==1 && sStaticMaterial3DS.colorMask[3]==0);assert(sStaticMaterial3DS.color[3]==(result.use_alpha?97:255));}
 }
 std::puts("PASS: 1024 actual factored/pre-factor material resolutions: exact state/import/flush events, dimensions, tiles, fog/two-cycle alpha and dual-texture fallback.");return 0;
}
} // namespace Fast
int main(){return Fast::main();}
'''
cpp=out/'material.cpp';exe=out/'material';cpp.write_text(pre+reference+helper+post)
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-DF3DEX_GBI=1','-I',str(port/'include'),'-I',str(port/'probe_stubs'),'-I',str(up/'include')]
subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True);r=subprocess.run([str(exe)],capture_output=True,text=True);(out/'material.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr;print(r.stdout,end='')
