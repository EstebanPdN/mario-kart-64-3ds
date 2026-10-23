// Native playtest only: retain queue pointers, export evidence on a failed fence.
#include <3ds.h>
#include <citro3d.h>
extern "C" {
#include "../compat/citro3d-1.7.1/internal.h"
extern C3D_Context __C3D_Context;
extern u32 __ctru_linear_heap,__ctru_linear_heap_size;
bool __real_C3D_FrameBegin(u8);
void __real_C3D_FrameEnd(u8);
}
#include "system_3ds.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
namespace {
struct Chunk {u32* address;u32 bytes;u8 flags;};
Chunk chunks[64]{};unsigned count=0,epoch=0;bool overflow=false;
char root[192]="sdmc:/3ds/MK64/dump";
bool Save(const char* name,const void* data,size_t bytes) {
 char path[192];std::snprintf(path,sizeof path,"%s/%s",root,name);
 FILE* f=std::fopen(path,"wb");if(!f)return false;
 bool ok=std::fwrite(data,1,bytes,f)==bytes && std::fflush(f)==0;
 if(ok)ok=fsync(fileno(f))==0;
 return std::fclose(f)==0 && ok;
}
bool ForceCapture() {
 static int enabled=-1;
 if(enabled<0){FILE* f=std::fopen("sdmc:/3ds/MK64/gpu-startup-capture","rb");enabled=f?1:0;if(f)std::fclose(f);}
 return enabled==1;
}
void Export(const char* reason,bool memory) {
 if(memory){
  // Preserve previous failed launches instead of overwriting the same epoch.
  for(unsigned serial=0;serial<1000;++serial){
   std::snprintf(root,sizeof root,"sdmc:/3ds/MK64/dump/gpu-fault-%03u",serial);
   if(mkdir(root,0777)==0)break;
   if(errno!=EEXIST || serial==999)return;
  }
 }
 char name[80];std::snprintf(name,sizeof name,"gpu-queue-%u.txt",epoch);
 char path[192];std::snprintf(path,sizeof path,"%s/%s",root,name);
 FILE* f=std::fopen(path,"wb");if(!f)return;
 const auto& ctx=__C3D_Context;const auto& q=ctx.gxQueue;
 std::fprintf(f,"build=%s reason=%s epoch=%u chunks=%u overflow=%d cache=%s\n",MK64_3DS_VERSION,reason,epoch,count,overflow,Mk64System3DSDataCacheMode());
 std::fprintf(f,"queue capacity=%u count=%u submitted=%u completed=%u flags=%08lx\n",q.maxEntries,q.numEntries,q.curEntry,q.lastEntry,(unsigned long)ctx.flags);
 std::fprintf(f,"command_base=%08lx command_bytes=%lu tex_config=%08lx\n",(unsigned long)ctx.cmdBuf,(unsigned long)ctx.cmdBufSize*4,(unsigned long)ctx.texConfig);
 for(unsigned i=0;i<count;++i){
  std::snprintf(name,sizeof name,"gpu-%u-chunk-%u.pica",epoch,i);
  const bool saved=Save(name,chunks[i].address,chunks[i].bytes);
  std::fprintf(f,"chunk %u virtual=%08lx physical=%08lx bytes=%lu flags=%u saved=%d\n",i,(unsigned long)chunks[i].address,(unsigned long)osConvertVirtToPhys(chunks[i].address),(unsigned long)chunks[i].bytes,chunks[i].flags,saved);
 }
 if(q.entries && q.numEntries<=q.maxEntries && q.numEntries<=256){
  for(unsigned i=0;i<q.numEntries;++i){std::fprintf(f,"gx %u",i);for(auto word:q.entries[i].data)std::fprintf(f," %08lx",(unsigned long)word);std::fputc('\n',f);}
 }
 for(unsigned i=0;i<3;++i)if(ctx.tex[i]){const auto& t=*ctx.tex[i];std::fprintf(f,"texture %u ptr=%08lx data=%08lx phys=%08lx width=%u height=%u bytes=%lu param=%08lx\n",i,(unsigned long)ctx.tex[i],(unsigned long)t.data,(unsigned long)osConvertVirtToPhys(t.data),t.width,t.height,(unsigned long)t.size,(unsigned long)t.param);}
 if(memory){
  std::snprintf(name,sizeof name,"gpu-%u-linear.bin",epoch);
  const bool saved=Save(name,(const void*)(uintptr_t)__ctru_linear_heap,__ctru_linear_heap_size);
  std::fprintf(f,"linear base=%08lx bytes=%lu saved=%d\n",(unsigned long)__ctru_linear_heap,(unsigned long)__ctru_linear_heap_size,saved);
 }
 std::snprintf(name,sizeof name,"gpu-%u-context.bin",epoch);Save(name,&ctx,sizeof ctx);
 std::fflush(f);fsync(fileno(f));std::fclose(f);
}
}
extern "C" bool __wrap_C3D_FrameBegin(u8 flags) {
 const bool ready=__real_C3D_FrameBegin(flags);
 if(ready){++epoch;count=0;overflow=false;}
 return ready;
}
extern "C" void Mk64Gpu3DSRecordCommands(u32* commands,u32 bytes,u8 flags) {
 if(count<64)chunks[count++]={commands,bytes,flags};else overflow=true;
}
extern "C" void __wrap_C3D_FrameEnd(u8 flags) {
 __real_C3D_FrameEnd(flags);
 if(epoch<=8 && ForceCapture())Export("requested-startup-capture",false);
}
extern "C" void Mk64Gpu3DSFenceFailure(const char* reason) { Export(reason,true); }
