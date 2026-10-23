#include "native_mesh_draw_3ds.hpp"
#include <citro3d.h>
#include <array>
#include <cassert>
#include <cstring>
#include <random>
using namespace mk64_3ds::native_scene;
namespace {
C3D_AttrInfo attributes; C3D_BufInfo buffers;
struct Field {int reg;unsigned bytes,count;};
std::array<Field,12> fields;unsigned fieldCount;std::size_t stride;int components;std::uint64_t permutation;
const void* meshAddress=nullptr;const void* indexAddress=nullptr;int drawCount=0;
}
C3D_AttrInfo* C3D_GetAttrInfo(){return &attributes;}
void AttrInfo_Init(C3D_AttrInfo*){fieldCount=0;}
int AttrInfo_AddLoader(C3D_AttrInfo*,int reg,GPU_FORMATS format,int count){fields[fieldCount++]={reg,format==GPU_SHORT?2u:1u,unsigned(count)};return fieldCount-1;}
C3D_BufInfo* C3D_GetBufInfo(){return &buffers;}
void BufInfo_Init(C3D_BufInfo*){}
int BufInfo_Add(C3D_BufInfo*,const void* data,std::ptrdiff_t size,int count,std::uint64_t order){meshAddress=data;stride=size;components=count;permutation=order;return 0;}
void C3D_DrawElements(GPU_Primitive_t,int count,int type,const void* indices){assert(type==C3D_UNSIGNED_SHORT);drawCount=count;indexAddress=indices;}
int main(){
 ConfigureVertexLayout();Vertex vertices[4]{};std::uint16_t indices[]={0,1,2,0,2,3};
 GpuMeshView mesh{vertices,indices,6};BindMesh(mesh);
 assert(fieldCount==4&&components==4&&stride==sizeof(Vertex)&&meshAddress==vertices);
 std::array<std::size_t,4> offsets{};std::size_t consumed=0;
 for(unsigned i=0;i<fieldCount;++i){const auto id=(permutation>>(i*4))&15;const auto& f=fields[id];consumed=(consumed+f.bytes-1)&~(f.bytes-1);offsets[f.reg]=consumed;consumed+=f.bytes*f.count;}
 assert(consumed==stride); // No unrepresented tail bytes in the hardware stream.
 assert(offsets[0]==offsetof(Vertex,position)&&offsets[1]==offsetof(Vertex,uv));
 assert(offsets[2]==offsetof(Vertex,color)&&offsets[3]==offsetof(Vertex,reserved));
 std::mt19937 random(0x3d526);
 for(unsigned trial=0;trial<200000;++trial){
  Vertex v;auto* bytes=reinterpret_cast<unsigned char*>(&v);for(unsigned i=0;i<sizeof(v);++i)bytes[i]=random();
  assert(!std::memcmp(bytes+offsets[0],v.position,6));assert(!std::memcmp(bytes+offsets[1],v.uv,4));assert(!std::memcmp(bytes+offsets[2],v.color,4));
 }
 DrawMesh(mesh);assert(drawCount==6&&indexAddress==indices);
 DrawIndices(indices+3,3);assert(drawCount==3&&indexAddress==indices+3);
}
