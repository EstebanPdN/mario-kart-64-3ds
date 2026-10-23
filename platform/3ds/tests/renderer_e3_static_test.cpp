#include "static_runs_3ds.hpp"
#include "original_fog_3ds.hpp"
#include "packed_vertex_cache_3ds.hpp"
#include "position_cache_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using Cache=mk64_3ds::StaticRunCache<68>;
static unsigned live=0;
static uint32_t generation=1;
static bool immutable=false;
void* Allocate(size_t n){++live;return std::malloc(n);}
void Release(void* p){--live;std::free(p);}
bool Publish(const void*,size_t){return true;}
uint32_t Generation(){return generation;}
bool Immutable(const void*,size_t){return immutable;}
struct Source {int16_t xyz[3];uint16_t flag;int16_t uv[2];uint8_t color[4];};
static_assert(sizeof(Source)==16);
struct Cmd {uint32_t op,arg;};
mk64_3ds::StaticRunVertex Vertex(const Source& src){
 mk64_3ds::StaticRunVertex v{};
 for(int i=0;i<3;++i)v.position[i]=src.xyz[i];v.position[3]=1;
 v.texcoord0[0]=v.texcoord1[0]=src.uv[0];v.texcoord0[1]=v.texcoord1[1]=src.uv[1];
 std::memcpy(v.color,src.color,4);return v;
}
void Load(Cache& cache,Source* v,size_t slot,const mk64_3ds::StaticRunTransform& camera={}){
 cache.NoteLoadRange(v,3,slot);
 for(size_t i=0;i<3;++i)cache.NoteLoad(slot+i,v+i,7,Vertex(v[i]),camera);
}
void Capture(Cache& cache,Cmd* command,Source* v,size_t slot=0){
 cache.BeginCapture(command,7);Load(cache,v,slot);cache.CaptureTriangle(slot,slot+1,slot+2);assert(cache.EndCapture(command+1));
}
uint8_t ReferenceFog(float z,float w,int mul,int offset){
 if(fabsf(w)<0.001f)w=0.001f;
 float inv=1.0f/w;if(inv<0)inv=std::numeric_limits<int16_t>::max();
 float f=z*inv*mul+offset;f=std::clamp(f,0.0f,255.0f);return f;
}
int main(){
 Source source[3]={{{100,0,100},0,{0,1},{20,30,40,50}},{{110,1,120},0,{10,11},{120,130,140,150}},{{120,2,130},0,{20,21},{220,230,240,250}}};
 Cmd command[2]{};
 {
  Cache cache(1<<20,768);cache.SetAllocator({Allocate,Release,Publish});cache.SetValidation({Generation,Immutable});
  immutable=true;Capture(cache,command,source);auto* e=cache.Find(command,7);assert(e);
  auto hashed=cache.Counters().hashedBytes;
  for(int i=0;i<1000;++i)assert(cache.Find(command,7)==e);
  assert(cache.Counters().hashedBytes==hashed);
  // A generation change rejects before reading an unmapped retired source.
  ++generation;assert(!cache.Find(command,7));cache.Clear();
  immutable=false;Capture(cache,command,source);e=cache.Find(command,7);assert(e);
  cache.Replayed(*e);auto saved=Vertex(source[0]);source[0].xyz[0]+=99;
  assert(!cache.Find(command,7));assert(std::memcmp(&saved,&cache.Snapshot(0),sizeof(saved))==0);
  source[0].xyz[0]-=99;cache.Clear();Capture(cache,command,source);command[0].arg=1;
  assert(!cache.Find(command,7));command[0].arg=0;
 }
 // Retire an entry whose vertices are still live in virtual slots 0..2.
 {
  Cache cache(640,768);cache.SetAllocator({Allocate,Release,Publish});Cmd lists[8][2]{};
  cache.BeginFrame(1);Capture(cache,lists[0],source);auto* first=cache.Find(lists[0],7);assert(first);cache.Replayed(*first);
  for(int i=1;i<8;++i){cache.BeginFrame(i+1);Capture(cache,lists[i],source,4);}
  assert(cache.Counters().evictions>0);
  for(int i=0;i<3;++i){auto expected=Vertex(source[i]);assert(cache.IsVirtual(i));assert(std::memcmp(&cache.Snapshot(i),&expected,sizeof(expected))==0);}
  cache.SetEnabled(false);assert(cache.IsVirtual(0) && live==0);
 }
 // Every RSP slot can own a different camera. Thousands of replacements may
 // recycle unreferenced matrices but may never overwrite a referenced one.
 {
  Cache cache(1<<20,768);cache.SetAllocator({Allocate,Release,Publish});
  mk64_3ds::StaticRunTransform transforms[68];
  for(int slot=0;slot<68;++slot){transforms[slot].matrix[3][0]=float(slot+1);cache.NoteLoad(slot,source,7,Vertex(source[0]),transforms[slot]);}
  for(int iteration=0;iteration<5000;++iteration){auto& camera=transforms[iteration%68];camera.matrix[3][0]+=0.25f;cache.NoteLoad(iteration%68,source,7,Vertex(source[0]),camera);
   for(int slot=0;slot<68;++slot)assert(cache.Transform(slot)==transforms[slot]);}
 }
 const float ws[]={-100.0f,-0.001f,-0.00099f,-0.0f,0.0f,0.00099f,0.001f,0.1f,1,100,10000};
 unsigned comparisons=0;
 for(float w:ws)for(int z=-1024;z<=1024;z+=4)for(int mul:{-319,0,1,319,32767})for(int offset:{-32768,-17,0,127,255,32767}){
  assert(mk64_3ds::OriginalFogAlpha(float(z),w,float(mul),float(offset))==ReferenceFog(float(z),w,mul,offset));++comparisons;
 }
 // The 256 ramp samples reproduce each truncated factor exactly; fractional
 // interpolation is deferred to texture sampling, independent of shade alpha.
 for(unsigned a=0;a<256;++a){const float x=mk64_3ds::OriginalFogCoordinate(a/255.0f)*256-0.5f;assert(std::fabs(x-float(a))<0.0001f);}
 struct Packed {uint32_t words[10];};
 mk64_3ds::PackedVertexConversionCache<Packed,64> conversion(64);
 float input[64]{};Packed packed{};for(int c=0;c<10;++c)packed.words[c]=unsigned(c+31);
 Packed output{};assert(!conversion.Copy(input,output));conversion.Store(input,packed);assert(conversion.Copy(input,output));assert(std::memcmp(&packed,&output,sizeof(packed))==0);
 for(int component=0;component<64;++component){input[component]=float(component+1);assert(!conversion.Copy(input,output));conversion.Store(input,packed);assert(conversion.Copy(input,output));input[component]=0;if(!conversion.Copy(input,output))conversion.Store(input,packed);else assert(std::memcmp(&packed,&output,sizeof(packed))==0);}
 // The four centred corners previously mapped to one direct-mapped bucket:
 // 64 identical quads missed on every request, including shared corners.
 mk64_3ds::PackedVertexConversionCache<Packed,64> corners(8);
 float quad[4][8]{};
 for(int i=0;i<4;++i){quad[i][0]=(i==1||i==2)?0.8f:-0.8f;quad[i][1]=i>=2?1.0f:-1.0f;quad[i][3]=1;quad[i][4]=float(i);}
 unsigned cornerHits=0,cornerMisses=0;
 for(int repeat=0;repeat<64;++repeat)for(int i:{0,1,2,0,2,3}){
  if(corners.Copy(quad[i],output))++cornerHits;
  else {++cornerMisses;corners.Store(quad[i],packed);}
 }
 assert(cornerHits==380 && cornerMisses==4);
 // Unique UV/colour inputs must never reuse position-only matches. Capacity
 // and lookup work stay bounded even when every input misses.
 mk64_3ds::PackedVertexConversionCache<Packed,64> unique(8);
 for(unsigned i=0;i<384;++i){quad[i%4][6]=float(i+1);assert(!unique.Copy(quad[i%4],output));unique.Store(quad[i%4],packed);}
 static_assert(sizeof(unique)<=5*1024);
 assert(live==0);
 // UV, color and flags do not participate in Z/W. Relevant object coordinates
 // and camera columns still invalidate, including in-place scroll writers.
 struct DepthVertex {struct {int16_t ob[3];uint16_t flag;int16_t tc[2];uint8_t cn[4];} v;};
 static_assert(sizeof(DepthVertex)==16);
 mk64_3ds::Fast3DCompactPositionCache depth;
 DepthVertex depthVertices[32]{};float matrix[4][4]{};
 for(int iteration=0;iteration<2000;++iteration){
  for(int r=0;r<4;++r)for(int c=0;c<4;++c)matrix[r][c]=float(iteration%17+r*7-c*2)/13;
  for(int i=0;i<32;++i)for(int c=0;c<3;++c)depthVertices[i].v.ob[c]=int16_t(iteration*7+i*13-c*37);
  const auto count=iteration%2?16:32;const auto* expected=mk64_3ds::BeginFast3DPositionBlock(depthVertices,count,matrix);
  auto* actual=depth.Load(depthVertices,count,matrix);assert(expected && actual && std::memcmp(expected,actual,count*sizeof(*actual))==0);
  const auto hits=mk64_3ds::gFast3DReuseStats.positionHits;
  depthVertices[0].v.tc[0]++;depthVertices[0].v.cn[2]++;depthVertices[0].v.flag++;
  assert(depth.Load(depthVertices,count,matrix)==actual && mk64_3ds::gFast3DReuseStats.positionHits==hits+1);
  // X/Y matrix columns are similarly outside this cache's dependency set.
  matrix[0][0]+=1;matrix[3][1]-=1;assert(depth.Load(depthVertices,count,matrix)==actual);
  const auto misses=mk64_3ds::gFast3DReuseStats.positionMisses;depthVertices[count-1].v.ob[2]++;
  assert(depth.Load(depthVertices,count,matrix) && mk64_3ds::gFast3DReuseStats.positionMisses==misses+1);
 }
 assert(!depth.Load(depthVertices,15,matrix));
 std::printf("PASS: 2000 actual compact/baseline Z/W blocks; exact relevant-input invalidation, independent UV/color/flag updates, unchanged small-load bypass; compact cache %zu bytes on host.\n",sizeof(depth));
 std::printf("PASS: immutable-generation/unknown-writer validation, real-free eviction, 68 simultaneous cameras, 5000 replacements, %u original-fog boundary cases, all 64 packing inputs.\n",comparisons);
 std::printf("PASS: centred quad cache %u hits/%u misses over 384 requests; 384 unique full inputs never reused; fixed cache %zu bytes, one bucket probe per request.\n",cornerHits,cornerMisses,sizeof(unique));
}
