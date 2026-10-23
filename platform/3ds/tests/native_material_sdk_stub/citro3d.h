#pragma once
// Minimal recording API for the asset-free material regression. This is not a
// GPU emulator; tests compare calls and ideal algebra with a source reference.
#include <array>
#include <map>
#include <vector>
#include <cstdlib>
#include <cstdint>
enum {GPU_RGBA8,GPU_LINEAR,GPU_CLAMP_TO_EDGE,GPU_VERTEX_SHADER,C3D_Both,C3D_RGB,C3D_Alpha,
GPU_TEXTURE0,GPU_CONSTANT,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR,
GPU_MODULATE,GPU_REPLACE,GPU_PREVIOUS,GPU_ADD,GPU_GREATER,GPU_BLEND_ADD,
GPU_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA,GPU_ZERO,GPU_CULL_BACK_CCW,GPU_CULL_FRONT_CCW,GPU_CULL_NONE,
GPU_PRIMARY_COLOR,GPU_REPEAT,GPU_MIRRORED_REPEAT,GPU_NEAREST,GPU_GEQUAL,GPU_ALWAYS,GPU_TEVOP_RGB_SRC_ALPHA,GPU_INTERPOLATE};
using GPU_TEXTURE_WRAP_PARAM=int;using GPU_WRITEMASK=unsigned;
constexpr unsigned GPU_WRITE_COLOR=15,GPU_WRITE_DEPTH=16;
struct C3D_Tex{void* data=nullptr;unsigned width=0,height=0;int filter=0,wrapS=0,wrapT=0;};
struct C3D_TexEnv{int src[2][3]{},op[3]{},func[2]{};uint32_t color=0;};
namespace mock{
inline std::array<C3D_TexEnv,6> env{};inline std::array<int,9> raster{};
inline unsigned envCalls=0,depthCalls=0,filterCalls=0,alphaCalls=0;
inline std::array<unsigned,3> depth{};inline std::array<float,3> depthMap{};
inline std::map<int,std::array<float,4>> uniforms;
}
inline bool C3D_TexInit(C3D_Tex* t,unsigned w,unsigned h,int){t->width=w;t->height=h;t->data=std::calloc(1,w*h*4);return t->data;}
inline void C3D_TexDelete(C3D_Tex* t){std::free(t->data);}
inline void C3D_TexSetFilter(C3D_Tex* t,int filter,int){++mock::filterCalls;t->filter=filter;}
inline void C3D_TexSetWrap(C3D_Tex* t,int s,int u){t->wrapS=s;t->wrapT=u;}
inline void C3D_TexBind(unsigned,C3D_Tex*){}
inline C3D_TexEnv* C3D_GetTexEnv(unsigned stage){return &mock::env[stage];}
inline void C3D_TexEnvInit(C3D_TexEnv* e){++mock::envCalls;*e={};e->src[0][0]=e->src[1][0]=GPU_PREVIOUS;e->func[0]=e->func[1]=GPU_REPLACE;e->color=0xffffffff;}
inline void C3D_TexEnvSrc(C3D_TexEnv* e,int mode,int a,int b=0,int c=0){auto& s=e->src[mode==C3D_Alpha];s[0]=a;s[1]=b;s[2]=c;}
inline void C3D_TexEnvFunc(C3D_TexEnv* e,int mode,int function){e->func[mode==C3D_Alpha]=function;}
inline void C3D_TexEnvOpRgb(C3D_TexEnv* e,int a,int b=0,int c=0){e->op[0]=a;e->op[1]=b;e->op[2]=c;}
inline void C3D_CullFace(int cull){mock::raster[7]=cull;}
inline void C3D_AlphaTest(bool enabled,int function,int reference){++mock::alphaCalls;mock::raster[0]=enabled;mock::raster[1]=function;mock::raster[2]=reference;}
inline void C3D_AlphaBlend(int,int,int a,int b,int c,int d){mock::raster[3]=a;mock::raster[4]=b;mock::raster[5]=c;mock::raster[6]=d;}
inline void C3D_DepthMap(bool z,float scale,float offset){mock::depthMap={float(z),scale,offset};}
inline void C3D_DepthTest(bool enabled,int function,unsigned write){++mock::depthCalls;mock::depth={unsigned(enabled),unsigned(function),write};}
inline void C3D_FVUnifSet(int,int location,float x,float y,float z,float w){mock::uniforms[location]={x,y,z,w};}
