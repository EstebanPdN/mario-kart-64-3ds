#!/usr/bin/env python3
"""Execute the real TEV builder; verify original fog, distance fog and alpha."""
import argparse
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
#include "alpha_policy_3ds.hpp"
using GPU_COMBINEFUNC=int;using GPU_TEVSRC=int;using GPU_TEVOP_RGB=int;using C3D_TexEnvMode=int;using GPU_WRITEMASK=unsigned;
enum {GPU_REPLACE=1,GPU_MODULATE,GPU_INTERPOLATE,GPU_MULTIPLY_ADD,GPU_SUBTRACT,
 GPU_TEXTURE0,GPU_TEXTURE1,GPU_TEXTURE2,GPU_PRIMARY_COLOR,GPU_CONSTANT,GPU_PREVIOUS,GPU_PREVIOUS_BUFFER,
 GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA,GPU_TEVOP_RGB_SRC_R,GPU_TEVOP_RGB_SRC_G,GPU_TEVOP_RGB_SRC_B,
 GPU_TEVOP_A_SRC_ALPHA,GPU_GREATER,GPU_GEQUAL,GPU_ALWAYS,GPU_VERTEX_SHADER,C3D_RGB,C3D_Alpha,C3D_Both};
constexpr unsigned GPU_WRITE_COLOR=15,GPU_WRITE_DEPTH=16;
struct C3D_Tex{};
struct C3D_TexEnv {int source[2][3]{},operand[2][3]{},function[2]{};uint32_t color=0xffffffff;};
std::array<C3D_TexEnv,6> environments{};unsigned updates=0,binds=0;bool originalEnabled=false;
C3D_TexEnv* C3D_GetTexEnv(int i){return &environments[i];}
void C3D_TexEnvInit(C3D_TexEnv* e){*e={};++updates;e->source[0][0]=e->source[1][0]=GPU_PREVIOUS;e->function[0]=e->function[1]=GPU_REPLACE;}
void C3D_TexEnvSrc(C3D_TexEnv* e,int mode,int x,int y=0,int z=0){auto& a=e->source[mode==C3D_Alpha];a[0]=x;a[1]=y;a[2]=z;}
void C3D_TexEnvFunc(C3D_TexEnv* e,int mode,int f){e->function[mode==C3D_Alpha]=f;}
void C3D_TexEnvOpRgb(C3D_TexEnv* e,int x,int y=GPU_TEVOP_RGB_SRC_COLOR,int z=GPU_TEVOP_RGB_SRC_COLOR){e->operand[0][0]=x;e->operand[0][1]=y;e->operand[0][2]=z;}
void C3D_TexEnvOpAlpha(C3D_TexEnv* e,int x,int y=GPU_TEVOP_A_SRC_ALPHA,int z=GPU_TEVOP_A_SRC_ALPHA){e->operand[1][0]=x;e->operand[1][1]=y;e->operand[1][2]=z;}
void C3D_TexEnvColor(C3D_TexEnv* e,uint32_t c){e->color=c;}
void C3D_TexEnvBufUpdate(int,int){}void C3D_TexEnvBufColor(uint32_t){}
void C3D_TexBind(int,C3D_Tex*){++binds;}
void C3D_BoolUnifSet(int,int,bool v){originalEnabled=v;}
void C3D_AlphaTest(bool,int,int){}void C3D_DepthTest(bool,int,unsigned){}
float gMk64DistanceFar3DS=0;unsigned gMk64DistanceFogBypassed3DS=0,gMk64DistanceFogBinds3DS=0,gMk64DistanceFogDraws3DS=0,gMk64DistanceFogSkipped3DS=0;
'''
for name in ['CompiledTevOperation3DS','ShaderProgram']:
 pre+=block(h,'struct '+name+' {')+';\n'
pre+=r'''
struct Impl {
 bool depthTest=true,depthWrite=true,decal=false,tevStateValid=false,tevStateDistanceFog=false,distanceFogTextureBound=false;
 void* activeTarget=nullptr;void* gameTarget=nullptr;int originalFogUniform=0;C3D_Tex distanceFogTexture;
 uint32_t distanceFogColor=0,tevStateOriginalFog=0,tevStateGrayscale=0;ShaderProgram* tevStateProgram=nullptr;
 int tevStateVaryingInput=0;std::array<uint32_t,7> tevStateConstants{};bool tevStateDepthTest=false,tevStateDepthWrite=false;
};
class GfxRenderingAPICitro3D {public:Impl state;Impl* mImpl=&state;
 void ApplyCombinerState(ShaderProgram*,int,const std::array<std::array<float,4>,7>&,const std::array<float,4>&,bool,const std::array<float,4>&);
};
'''
helpers=block(s,'enum CombinerSource : int')+';\n'+block(s,'struct ChannelOperation {')+';\n'
for marker in ['uint8_t ToByte(', 'uint32_t PackColor(', 'ChannelOperation PassPrevious(', 'GPU_TEVSRC SourceToTev(', 'GPU_TEVOP_RGB SourceToRgbOperand(', 'std::array<float, 4> ConstantForSource(', 'bool IsConstantSource(', 'void ConfigureChannel(', 'void ConfigureOperands(', 'void ConfigureAlphaPass(']:helpers+=block(s,marker)+'\n'
body=block(s,'void GfxRenderingAPICitro3D::ApplyCombinerState(')
post=r'''
using Color=std::array<float,4>;
Color Unpack(uint32_t c){return {float(c&255)/255,float((c>>8)&255)/255,float((c>>16)&255)/255,float(c>>24)/255};}
Color Evaluate(Color primary,Color texture,Color fogSample){
 Color previous{};
 for(const auto& e:environments){Color output{};const auto constant=Unpack(e.color);
  for(int c=0;c<4;++c){int ch=c==3;float args[3]{};
   for(int i=0;i<3;++i){Color v{};switch(e.source[ch][i]){case GPU_PRIMARY_COLOR:v=primary;break;case GPU_TEXTURE0:v=texture;break;case GPU_TEXTURE2:v=fogSample;break;case GPU_CONSTANT:v=constant;break;case GPU_PREVIOUS:v=previous;break;}
    int component=c;const auto operand=e.operand[ch][i];if(operand==GPU_TEVOP_RGB_SRC_ALPHA)component=3;else if(operand==GPU_TEVOP_RGB_SRC_R)component=0;else if(operand==GPU_TEVOP_RGB_SRC_G)component=1;else if(operand==GPU_TEVOP_RGB_SRC_B)component=2;args[i]=v[component];}
   switch(e.function[ch]){case GPU_REPLACE:output[c]=args[0];break;case GPU_MODULATE:output[c]=args[0]*args[1];break;case GPU_INTERPOLATE:output[c]=args[0]*args[2]+args[1]*(1-args[2]);break;default:assert(false);}
  }previous=output;
 }return previous;
}
int main(){
 GfxRenderingAPICitro3D api;ShaderProgram program;program.fog=true;program.alpha=true;program.twoCycle=true;program.numInputs=2;program.usedTextures[0]=program.usedTextures[1]=program.sampledTextures[0]=true;
 program.tevOperationCount[0][0]=program.tevOperationCount[0][1]=program.tevOperationCount[1][0]=program.tevOperationCount[1][1]=1;
 program.tevOperations[0][0][0]={GPU_MODULATE,{SourceTexel0,SourceInput1,SourceZero}};
 program.tevOperations[0][1][0]={GPU_REPLACE,{SourceOne,SourceZero,SourceZero}};
 program.tevOperations[1][0][0]={GPU_REPLACE,{SourceCombined,SourceZero,SourceZero}};
 program.tevOperations[1][1][0]={GPU_REPLACE,{SourceInput1+1,SourceZero,SourceZero}};
 std::array<std::array<float,4>,7> constants{};Color shade={0.3f,0.6f,0.9f,1},texture={0.7f,0.5f,0.2f,0.4f},fog={73/255.f,101/255.f,191/255.f,1};
 unsigned cases=0;
 for(int distance=0;distance<2;++distance){gMk64DistanceFar3DS=distance?5000:0;api.state.distanceFogColor=0xff405060;
  const auto distanceColor=Unpack(api.state.distanceFogColor);
  for(int alpha=0;alpha<256;++alpha){constants[1][3]=alpha/255.f;
   api.ApplyCombinerState(&program,0,constants,{1,1,1,0},true,fog);assert(originalEnabled);
   const auto update=updates;api.ApplyCombinerState(&program,0,constants,{1,1,1,0},true,fog);assert(updates==update);
   // Original fog follows both cycles, passes alpha unchanged, and samples R.
   assert(environments[2].source[0][2]==GPU_TEXTURE2 && environments[2].operand[0][2]==GPU_TEVOP_RGB_SRC_R);
   assert(environments[2].source[1][0]==GPU_PREVIOUS && environments[2].function[1]==GPU_REPLACE);
   if(distance)assert(environments[3].operand[0][2]==GPU_TEVOP_RGB_SRC_G);
   for(int factor=0;factor<256;++factor)for(int farFactor:{0,127,255}){
    Color sample={factor/255.f,farFactor/255.f,0,1};auto out=Evaluate(shade,texture,sample);
    for(int c=0;c<3;++c){float expected=fog[c]*sample[0]+texture[c]*shade[c]*(1-sample[0]);if(distance)expected=distanceColor[c]*sample[1]+expected*(1-sample[1]);assert(std::fabs(out[c]-expected)<1e-6f);}
    assert(std::fabs(out[3]-alpha/255.f)<1e-6f);++cases;
   }
  }
 }
 program.usedTextures[1]=program.sampledTextures[1]=true;api.ApplyCombinerState(&program,0,constants,{1,1,1,0},true,fog);assert(!originalEnabled);
 std::printf("PASS: production TEV builder, %u exact-algebra original/distance fog cases; all 256 independent second-cycle alpha values, separate ramp channels, state reuse and dual-texture boundary. GPU quantization/pixels remain a hardware gate.\n",cases);
}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(port/'include')]
for name,code,success in [('fog-tev',body,True),('negative-fog-channel',body.replace('GPU_TEVOP_RGB_SRC_R','GPU_TEVOP_RGB_SRC_G'),False)]:
 cpp=out/(name+'.cpp');exe=out/name;cpp.write_text(pre+helpers+code+post);subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert (r.returncode==0)==success,r.stdout+r.stderr
 if success:print(r.stdout,end='')
print('PASS: substituting distance fog for original fog fails the negative control.')
