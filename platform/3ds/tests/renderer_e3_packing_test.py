#!/usr/bin/env python3
"""Differentially execute actual DrawTriangles with and without conversion reuse."""
import argparse,re
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];s=(port/'source/gfx_citro3d.cpp').read_text();h=(port/'include/gfx_citro3d.h').read_text()
def block(text,marker):
 start=text.index(marker);i=text.index('{',start)+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
pre=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "native_geometry_3ds.hpp"
#include "depth_precision_3ds.hpp"
#include "original_fog_3ds.hpp"
#include "packed_vertex_cache_3ds.hpp"
#include "performance_trace_3ds.hpp"
constexpr size_t kMaxVertexStrideFloats=64,kMaxSourceVertices=768,kMaxDrawVertices=1536,kVertexBufferCapacity=65536;
constexpr float kClipWEpsilon=1e-4f,kFullscreenBoundsEpsilon=0.03f,kFullscreenCoverScale=1.25f;
constexpr int kNativeWidth=320,kNativeHeight=240,kTopLogicalWidth=400,GPU_VERTEX_SHADER=0,GPU_TRIANGLES=0;
namespace mk64_3ds {constexpr int kGpuCommandTailWords=64,kGpuCommandDrawWords=32;PerformanceTick sCurrent;bool sTickOpen=true,sWriting=false;unsigned clockReads=0;PerformanceTick& PerformanceCurrent(){return sCurrent;}uint64_t PerformanceNow(){return ++clockReads;}}
float gMk64DistanceFar3DS=5000;unsigned colorConversions=0,conversionProbes=0;
void C3D_FVUnifSet(int,int,float,float,float,float){}void C3D_DrawArrays(int,int,int){}
'''
trace=(port/'source/performance_trace_3ds.cpp').read_text()
pre+='namespace mk64_3ds {\n'+block(trace,'bool DetailedField(')+'\n'+block(trace,'bool PerformanceTimerEnabled(')+'\n}\n'
pre+=block(s,'struct PackedVertex {')+';\n'
for name in ['CompiledTevOperation3DS','ShaderProgram']:pre+=block(h,'struct '+name+' {')+';\n'
pre+=r'''
struct Impl {
 struct Texture{bool initialized=true,hasTransparency=false;unsigned sourceWidth=320,sourceHeight=240;};
 bool frameActive=true,originalAspect=false,textureTransformValid=false;ShaderProgram* currentProgram=nullptr;
 mk64_3ds::NativeGeometryDraw nativeGeometry;void* activeTarget=nullptr;void* gameTarget=nullptr;
 std::array<int,2> selectedFramebuffers{};std::array<unsigned,2> selectedTextures{};std::vector<Texture> textures={Texture{}};
 std::vector<float> clipScratch;PackedVertex* packedVertices=nullptr;size_t packedVertexCount=0,dirtyVertexBegin=0,dirtyVertexEnd=0;
 unsigned drawCallCount=0,triangleCount=0;int textureScaleUniform=0,textureClampUniform=1;
 std::array<float,4> textureScaleState{},textureClampState{};
};
struct MaterialEvent {int varying;std::array<std::array<float,4>,7> constants;std::array<float,4> gray,fog;bool distance;bool operator==(const MaterialEvent&)const=default;};
class GfxRenderingAPICitro3D {public:
 Impl state;Impl* mImpl=&state;std::vector<MaterialEvent> materials;
 bool DrawTargetVisible(){return true;}void RequireGpuCommandRoom(int){}void BindPackedVertices(){}void ApplyNativeVertexState(){}
 std::array<float,4> SelectedTextureScale(ShaderProgram*,float){return {1,1,1,1};}
 void ApplyCombinerState(ShaderProgram*,int varying,const std::array<std::array<float,4>,7>& inputs,const std::array<float,4>& gray,bool distance,const std::array<float,4>& fog){materials.push_back({varying,inputs,gray,fog,distance});}
 void DrawTriangles(float*,size_t,size_t);void Reference(float*,size_t,size_t);
};
'''
helpers=block(s,'uint8_t FloatColorToByte(').replace('{','{++colorConversions;',1)+'\n'+block(s,'size_t ClipTriangleAgainstW(')+'\n'+block(s,'bool IsNativeFullscreenQuad(')+'\n'
body=block(s,'void GfxRenderingAPICitro3D::DrawTriangles(')
reference=body.replace('GfxRenderingAPICitro3D::DrawTriangles','GfxRenderingAPICitro3D::Reference')
reference=re.sub(r'^.*conversions\.Copy\(source, destination\).*\n','',reference,flags=re.M)
reference=re.sub(r'^.*conversions\.Store\(source, destination\).*\n','',reference,flags=re.M)
assert 'conversions.Copy' not in reference and 'conversions.Store' not in reference
# Count the actual production call sites, preserving their short-circuit gate.
body=body.replace('conversions.Copy(source, destination)','(++conversionProbes, conversions.Copy(source, destination))')
post=r'''
struct Result {std::vector<PackedVertex> packed;std::vector<MaterialEvent> materials;unsigned conversions=0,clocks=0;size_t triangles=0;};
Result Run(bool reference,bool sampled=false){
 Result result;colorConversions=conversionProbes=mk64_3ds::clockReads=0;mk64_3ds::sCurrent.detail_sampled=sampled;GfxRenderingAPICitro3D api;std::vector<PackedVertex> packed(65536);api.state.packedVertices=packed.data();ShaderProgram program;api.state.currentProgram=&program;
 program.strideFloats=28;program.numInputs=2;program.textureOffsets[0]=4;program.textureOffsets[1]=8;program.inputOffsets[0]=20;program.inputOffsets[1]=24;program.fogOffset=12;program.grayscaleOffset=16;
 for(int scenario=0;scenario<256;++scenario){
  api.state.packedVertexCount=api.state.dirtyVertexBegin=api.state.dirtyVertexEnd=0;api.state.clipScratch.clear();
  program.usedTextures[0]=program.sampledTextures[0]=scenario&1;program.usedTextures[1]=program.sampledTextures[1]=scenario&2;program.alpha=scenario&4;program.fog=scenario&8;program.grayscale=scenario&16;api.state.nativeGeometry.enabled=scenario&32;
  for(int r=0;r<4;++r)for(int c=0;c<4;++c)api.state.nativeGeometry.matrix[r][c]=r==c?1:0;
  program.clamp[0][0]=program.clamp[1][1]=scenario&64;
  std::vector<float> source;
  for(int quad=0;quad<64;++quad)for(int index:{0,1,2,0,2,3}){
   std::array<float,28> vertex{};vertex[0]=(index==1||index==2)?0.8f:-0.8f;vertex[1]=index>=2?1:-1;vertex[2]=0.1f*quad;
   vertex[3]=(scenario&128)&&index==0?0.00001f:1.0f;
   for(int i=4;i<12;++i)vertex[i]=(quad%5+index+i)/32.f;
   for(int i=12;i<28;++i)vertex[i]=float((index*13+i*7+quad)%256)/255;
   source.insert(source.end(),vertex.begin(),vertex.end());
  }
  if(reference)api.Reference(source.data(),source.size(),128);else api.DrawTriangles(source.data(),source.size(),128);
  assert(api.state.dirtyVertexBegin==0 && api.state.dirtyVertexEnd==api.state.packedVertexCount);
  result.packed.insert(result.packed.end(),packed.begin(),packed.begin()+api.state.packedVertexCount);
 }
 result.materials=std::move(api.materials);result.conversions=colorConversions;result.clocks=mk64_3ds::clockReads;result.triangles=api.state.triangleCount;return result;
}
int main(){auto reference=Run(true);auto cached=Run(false);assert(reference.packed.size()==cached.packed.size());
 assert(std::memcmp(reference.packed.data(),cached.packed.data(),reference.packed.size()*sizeof(PackedVertex))==0);
 assert(reference.materials==cached.materials && reference.triangles==cached.triangles && cached.conversions<reference.conversions);
 assert(reference.clocks==0 && cached.clocks==0);
 auto sampled=Run(false,true);assert(sampled.clocks==512 && sampled.packed.size()==cached.packed.size());
 assert(std::memcmp(sampled.packed.data(),cached.packed.data(),cached.packed.size()*sizeof(PackedVertex))==0 && sampled.materials==cached.materials);
 // A single triangle bypasses all hash/memcmp/store work. Six vertices use
 // one bounded cache probe per vertex, even when each full input is unique.
 GfxRenderingAPICitro3D api;ShaderProgram program;program.strideFloats=4;api.state.currentProgram=&program;std::vector<PackedVertex> packed(6);api.state.packedVertices=packed.data();
 float triangle[12]={-.2f,-.2f,0,1,.2f,-.2f,0,1,0,.2f,0,1};conversionProbes=0;api.DrawTriangles(triangle,12,1);assert(conversionProbes==0);
 float unique[24];std::copy_n(triangle,12,unique);std::copy_n(triangle,12,unique+12);unique[12]+=.01f;api.state.packedVertexCount=0;api.DrawTriangles(unique,24,2);assert(conversionProbes==6);
 std::puts("PASS: actual DrawTriangles and production PerformanceTimer: 0 unsampled clock reads, 512 sampled reads across 256 draws; identical packed output. Single triangle: 0 cache probes; six unique vertices: 6 bounded probes.");
 std::printf("PASS: production DrawTriangles, 256 shader/native/clamp/fog/alpha/near-eye cases; %zu byte-identical packed vertices and material/dirty-span events; color conversions %u -> %u. Host operation counts are not physical timing.\n",cached.packed.size(),reference.conversions,cached.conversions);
}
'''
cpp=out/'packing.cpp';exe=out/'packing';cpp.write_text(pre+helpers+reference+body+post)
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(port/'include'),str(cpp),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True);(out/'packing.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr;print(r.stdout,end='')
