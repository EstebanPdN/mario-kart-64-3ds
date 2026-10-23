#include <3ds.h>
#include <array>
#include <cassert>
#include <cstring>
extern "C" Result __wrap_GX_ProcessCommandList(u32*,u32,u8);
namespace {std::array<u32,32> gpuMemory{},cpuCache{};u8 requested,observed,recorded;unsigned calls,records;}
extern "C" void Mk64Gpu3DSRecordCommands(u32* address,u32 bytes,u8 flags){assert(address==cpuCache.data()&&bytes==sizeof(cpuCache));recorded=flags;++records;}
extern "C" Result __real_GX_ProcessCommandList(u32* address,u32 bytes,u8 flags){
 assert(address==cpuCache.data()&&bytes==sizeof(cpuCache));observed=flags;++calls;
 if(flags&GX_CMDLIST_FLUSH)std::memcpy(gpuMemory.data(),address,bytes);
 // The physical consumer sees published memory, independently of C2D buffers.
 assert(gpuMemory==cpuCache);assert((flags&~GX_CMDLIST_FLUSH)==(requested&~GX_CMDLIST_FLUSH));
 return -37;
}
int main(){
 for(unsigned f=0;f<256;++f){
  requested=f;for(unsigned i=0;i<cpuCache.size();++i){cpuCache[i]=f*12345+i;gpuMemory[i]=~cpuCache[i];}
  assert(__wrap_GX_ProcessCommandList(cpuCache.data(),sizeof(cpuCache),f)==-37);
  assert((observed&GX_CMDLIST_FLUSH)&&recorded==observed);
 }
 assert(calls==256&&records==256);
}
