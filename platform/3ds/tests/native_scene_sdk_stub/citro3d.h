#pragma once
#include "../native_material_sdk_stub/citro3d.h"
#include <cstring>
#include <string>
using GPU_FORMATS=int;
enum {GPU_SHORT=100,GPU_UNSIGNED_BYTE,GPU_TRIANGLES};
constexpr int C3D_UNSIGNED_SHORT=1;
struct C3D_AttrInfo {};
struct C3D_BufInfo {};
struct C3D_FVec {float x=0,y=0,z=0,w=0;};
struct C3D_Mtx {C3D_FVec r[4];};
struct DVLB_s {int DVLE[1]{};};
struct shaderProgram_s {void* vertexShader=nullptr;};
namespace mock {
inline C3D_Mtx matrix;
inline unsigned matrices=0,programs=0,layouts=0;
inline std::uint32_t commands[1]{},commandCapacity=262144,commandOffset=0;
inline const void* vertices=nullptr;
}
inline DVLB_s* DVLB_ParseFile(std::uint32_t*,std::size_t){return new DVLB_s;}
inline void DVLB_Free(DVLB_s* value){delete value;}
inline int shaderProgramInit(shaderProgram_s*){return 0;}
inline int shaderProgramSetVsh(shaderProgram_s*,int*){return 0;}
inline void shaderProgramFree(shaderProgram_s*){}
inline int shaderInstanceGetUniformLocation(void*,const char* name){return !std::strcmp(name,"mvp")?0:!std::strcmp(name,"uvScaleOffset")?4:5;}
inline void C3D_BindProgram(shaderProgram_s*){++mock::programs;}
inline void C3D_FVUnifMtx4x4(int,int,const C3D_Mtx* value){mock::matrix=*value;++mock::matrices;}
inline void C3D_TexFlush(C3D_Tex*){}
inline void C3D_TexEnvBufUpdate(int,int){}
inline void C3D_TexEnvBufColor(std::uint32_t){}
inline C3D_AttrInfo* C3D_GetAttrInfo(){static C3D_AttrInfo a;return &a;}
inline void AttrInfo_Init(C3D_AttrInfo*){++mock::layouts;}
inline int AttrInfo_AddLoader(C3D_AttrInfo*,int,int,int){return 0;}
inline C3D_BufInfo* C3D_GetBufInfo(){static C3D_BufInfo b;return &b;}
inline void BufInfo_Init(C3D_BufInfo*){}
inline int BufInfo_Add(C3D_BufInfo*,const void* data,std::ptrdiff_t,int,std::uint64_t){mock::vertices=data;return 0;}
inline void GPUCMD_GetBuffer(std::uint32_t** p,std::uint32_t* size,std::uint32_t* offset){*p=mock::commands;*size=mock::commandCapacity;*offset=mock::commandOffset;}
void C3D_DrawElements(int,int,int,const void*);
