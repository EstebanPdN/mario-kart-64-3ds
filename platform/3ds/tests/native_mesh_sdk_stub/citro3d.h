#pragma once
#include <cstddef>
#include <cstdint>
enum GPU_FORMATS { GPU_SHORT, GPU_UNSIGNED_BYTE };
enum GPU_Primitive_t { GPU_TRIANGLES };
constexpr int C3D_UNSIGNED_SHORT=1;
struct C3D_AttrInfo {};
struct C3D_BufInfo {};
C3D_AttrInfo* C3D_GetAttrInfo();
void AttrInfo_Init(C3D_AttrInfo*);
int AttrInfo_AddLoader(C3D_AttrInfo*,int,GPU_FORMATS,int);
C3D_BufInfo* C3D_GetBufInfo();
void BufInfo_Init(C3D_BufInfo*);
int BufInfo_Add(C3D_BufInfo*,const void*,std::ptrdiff_t,int,std::uint64_t);
void C3D_DrawElements(GPU_Primitive_t,int,int,const void*);
