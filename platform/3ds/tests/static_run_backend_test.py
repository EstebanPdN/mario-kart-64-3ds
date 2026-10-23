#!/usr/bin/env python3
"""Exercise the production indexed draw with a register-level Citro3D mock."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];source=(port/'source/gfx_citro3d.cpp').read_text();header=(port/'include/gfx_citro3d.h').read_text()
def block(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
def struct(name):return block(header,'struct '+name+' {')+';\n'
body=block(source,'void GfxRenderingAPICitro3D::DrawStaticRun(')+'\n'+block(source,'void GfxRenderingAPICitro3D::BindPackedVertices(')+'\n'+block(source,'void GfxRenderingAPICitro3D::FlushPackedVertices(')
pre=r'''
#include "native_geometry_3ds.hpp"
#include "static_runs_3ds.hpp"
#include "original_fog_3ds.hpp"
#include <stdexcept>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
using uint8_t=std::uint8_t;using uint16_t=std::uint16_t;using uint32_t=std::uint32_t;
namespace mk64_3ds {constexpr int kGpuCommandTailWords=64,kGpuCommandDrawWords=32;}
constexpr int GPU_VERTEX_SHADER=0,GPU_TRIANGLES=0,C3D_UNSIGNED_SHORT=0,kMaxVertexStrideFloats=64;
enum GPU_CULLMODE {GPU_CULL_NONE,GPU_CULL_FRONT_CCW,GPU_CULL_BACK_CCW};
using PackedVertex=mk64_3ds::StaticRunVertex;
constexpr size_t kVertexBufferCapacity=65536;
struct C3D_BufInfo {};
struct Binding{const void* pointer;size_t stride;int count,permutation;};std::vector<Binding> bindings;
const void* cleaned=nullptr;size_t cleanBytes=0;
bool Mk64System3DSCleanDataCache(const void* p,size_t n){cleaned=p;cleanBytes=n;return true;}
struct Registers {std::array<std::array<float,4>,32> floats{};std::array<bool,8> bits{};GPU_CULLMODE cull=GPU_CULL_NONE;
 bool operator==(const Registers&) const=default;};
Registers hardware;unsigned uploads=0;std::vector<Registers> draws;
void C3D_FVUnifSet(int,int slot,float a,float b,float c,float d){hardware.floats[slot]={a,b,c,d};++uploads;}
void C3D_BoolUnifSet(int,int slot,bool value){hardware.bits[slot]=value;++uploads;}
void C3D_CullFace(GPU_CULLMODE mode){hardware.cull=mode;++uploads;}
C3D_BufInfo* C3D_GetBufInfo(){static C3D_BufInfo info;return &info;}
void BufInfo_Init(C3D_BufInfo*){bindings.clear();}
void BufInfo_Add(C3D_BufInfo*,const void* p,size_t stride,int count,int permutation){bindings.push_back({p,stride,count,permutation});}
void C3D_DrawElements(int,int,int,const void*){draws.push_back(hardware);}
'''
pre+=block(source,'struct StaticFogVertex {')+';\n'
pre+=struct('CompiledTevOperation3DS')+struct('StaticRunDraw3DS')+struct('ShaderProgram')
pre+=r'''
struct Impl {
 bool frameActive=true,textureTransformValid=false,staticVertexStateValid=false,packedBufferBound=false,nativeVertexStateDirty=false;
 ShaderProgram* currentProgram=nullptr;StaticRunDraw3DS staticVertexState{};
 std::array<float,4> textureScaleState{},textureClampState{};
 int textureScaleUniform=0,textureClampUniform=1,nativeMatrixUniform=2,staticUvUniform=6,staticColorUniform=8,staticColorMaskUniform=9;
 int nativeGeometryUniform=0,nativeLightingUniform=1,staticMeshUniform=2,staticConstantColorUniform=3;
 PackedVertex* packedVertices=nullptr;unsigned drawCallCount=0,triangleCount=0;
 size_t packedVertexCount=0,dirtyVertexBegin=0,dirtyVertexEnd=0;uint64_t vertexUploadCount=0,vertexUploadBytes=0;
};
class GfxRenderingAPICitro3D {
public:
 Impl state;Impl* mImpl=&state;
 void DrawStaticRun(const StaticRunDraw3DS&);void BindPackedVertices();void FlushPackedVertices();
 bool DrawTargetVisible(){return true;}
 void RequireGpuCommandRoom(int){}
 std::array<float,4> SelectedTextureScale(ShaderProgram*,float){return {1,1,1,1};}
 void ApplyCombinerState(ShaderProgram*,int,const std::array<std::array<float,4>,7>&,const std::array<float,4>&,bool,const std::array<float,4>& = {}){}
};
'''
post=r'''
std::pair<std::vector<Registers>,unsigned> run(bool cold) {
 hardware={};uploads=0;draws.clear();
 GfxRenderingAPICitro3D renderer;ShaderProgram program;program.strideFloats=4;renderer.state.currentProgram=&program;
 PackedVertex vertices[3]{};uint16_t indices[3]={0,1,2};float material[4]={0,0,0,1};
 StaticRunDraw3DS draw;draw.vertices=vertices;draw.indices=indices;draw.indexCount=3;draw.materialVertex=material;
 for(int frame=0;frame<5;++frame) {
  for(int pass=0;pass<4;++pass) {
   draw.rows[0][0]=float(frame+1);draw.rows[3][3]=float(pass+1);
   draw.cull=static_cast<mk64_3ds::NativeGeometryCull>(pass%3);draw.constantColor=pass!=0;
   draw.color[3]=float(200-pass);draw.colorMask[0]=pass%2;draw.uvAffine[0][2]=float(frame%2);
   for(int repeated=0;repeated<20;++repeated) {
    if(cold)renderer.state.staticVertexStateValid=false;
    renderer.DrawStaticRun(draw);
   }
   // Interpreted geometry uses the same matrix and culling registers.
   renderer.BindPackedVertices();
   for(int row=2;row<6;++row)hardware.floats[row]={-7,-8,-9,-10};
   hardware.bits[0]=hardware.bits[1]=true;hardware.cull=GPU_CULL_FRONT_CCW;
  }
 }
 assert(renderer.state.drawCallCount==400 && renderer.state.triangleCount==400);
 return {draws,uploads};
}

void fogRun(){
 GfxRenderingAPICitro3D renderer;ShaderProgram program;program.strideFloats=8;program.fog=true;program.fogOffset=4;
 renderer.state.currentProgram=&program;std::vector<PackedVertex> stream(65536);renderer.state.packedVertices=stream.data();
 PackedVertex vertices[4]{};for(int i=0;i<4;++i){vertices[i].position[0]=float(i*4-8);vertices[i].position[2]=float(3+i);vertices[i].position[3]=1;vertices[i].color[0]=20+i;vertices[i].color[1]=80-i;vertices[i].color[2]=140+i;vertices[i].color[3]=30+i*50;}
 const auto original=std::array{vertices[0],vertices[1],vertices[2],vertices[3]};uint16_t indices[6]={0,1,2,0,2,3};float material[8]={0,0,0,1,0.3f,0.4f,0.5f,0.75f};
 StaticRunDraw3DS draw;draw.vertices=vertices;draw.indices=indices;draw.indexCount=6;draw.vertexCount=4;draw.materialVertex=material;draw.center[0]=100;draw.center[2]=50;
 draw.fogTransform.matrix[2][2]=1;draw.fogTransform.matrix[2][3]=0.125f;draw.fogTransform.matrix[3][3]=100;draw.fogTransform.fogMul=319;draw.fogTransform.fogOffset=-17;
 for(int scenario=0;scenario<64;++scenario){
  draw.fogTransform.fog=scenario&1;draw.fogFromVertex=scenario&2;draw.fogTransform.matrix[3][2]=float(scenario-32);
  const auto before=renderer.state.packedVertexCount;renderer.DrawStaticRun(draw);
  assert(bindings.size()==2 && bindings[0].pointer==vertices && bindings[0].stride==40 && bindings[0].count==2 && bindings[0].permutation==0x10);
  assert(bindings[1].stride==16 && bindings[1].count==3 && bindings[1].permutation==0x432);
  const auto* fog=static_cast<const StaticFogVertex*>(bindings[1].pointer);
  assert(fog==reinterpret_cast<const StaticFogVertex*>(stream.data()+before));
  for(int i=0;i<4;++i){float object[3]={vertices[i].position[0]+100,vertices[i].position[1],vertices[i].position[2]+50};auto expected=draw.fogTransform.fog?mk64_3ds::OriginalFogVertex(object,draw.fogTransform.matrix,319,-17):vertices[i].color[3];
   assert(fog[i].color[3]==expected && std::memcmp(fog[i].color,vertices[i].color,3)==0);
   assert(fog[i].coordinate[0]==mk64_3ds::OriginalFogCoordinate(draw.fogFromVertex?expected/255.f:material[7]));
  }
  assert(std::memcmp(vertices,original.data(),sizeof(vertices))==0);
  assert(renderer.state.packedVertexCount==before+2);renderer.FlushPackedVertices();
  assert(cleaned==stream.data()+before && cleanBytes==80 && renderer.state.dirtyVertexBegin==renderer.state.dirtyVertexEnd);
 }
 std::puts("PASS: 64 production static fog submissions retain immutable position/UV/index storage, bind the 16-byte color/fog stream, preserve load-time fog/alpha and publish complete owned ranges.");
}

int main(){fogRun();auto reference=run(true);auto cached=run(false);
 assert(reference.first==cached.first);assert(cached.second<reference.second/5);
 std::printf("PASS: 400 production indexed draws preserve shader/cull registers across material and interpreted transitions; uniform/state writes %u -> %u.\n",reference.second,cached.second);
}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(port/'include')]
for name,code in [('cached',body),('negative-control',body.replace('mImpl->staticVertexStateValid = false;',''))]:
    cpp=a.output/(name+'.cpp');exe=a.output/name;cpp.write_text(pre+code+post)
    subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/(name+'.log')).write_text(result.stdout+result.stderr)
    assert (result.returncode==0)==(name=='cached'),result.stdout+result.stderr
    if name=='cached':print(result.stdout,end='')
print('PASS: removing ownership invalidation fails the negative control.')
