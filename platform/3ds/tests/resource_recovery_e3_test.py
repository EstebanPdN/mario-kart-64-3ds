#!/usr/bin/env python3
"""Compile production recovery functions with actual Pack and failure fixtures."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];text=(port/'source/resource_runtime_3ds.cpp').read_text()
def function(signature):
 start=text.index(signature);brace=text.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
functions='\n'.join(function(s) for s in ('ReclaimedCache ReclaimOptionalCaches(uint32_t allowed) {','extern "C" bool Mk64Resource3DSReclaimPerformanceCache(void) {','extern "C" const char* Mk64Resource3DSLastReclaimedCache(void) {','LoadedResource* LoadResolvedPath(size_t archiveIndex, CrcEntry* crcEntry) {'))
prelude=r'''
#include "improved_texture_pack_3ds.hpp"
#include "resource_runtime_3ds.h"
#include <cassert>
#include <fstream>
#include <cstring>
using mk64_3ds::improved::Pack;
struct LoadedResource {};
struct CrcEntry {};
enum class ReclaimedCache : uint8_t {None,Improved,Retained,Native,Archive,Streaming};
ReclaimedCache sLastReclaimedCache=ReclaimedCache::None;
bool sReclaimingCaches=false;
unsigned drops=0,attempts=0,loading=0,nativeDrops=0,retainedDrops=0;
bool retained=false,native=false,alwaysFail=false,repopulate=false,reenter=false;
unsigned failThrough=0;
Pack pack;
std::vector<std::string> checkpoints;
struct Archive {
 bool resident=true,stream=true;
 bool IsResident()const{return resident;}
 bool ReclaimPerformanceCache(){if(resident){resident=false;++drops;return true;}if(stream){stream=false;++drops;return true;}return false;}
};
std::unique_ptr<Archive> sArchive;
extern "C" bool Mk64Improved3DSReclaim() __attribute__((weak));
extern "C" bool Mk64NativeKartReclaimCache() __attribute__((weak));
extern "C" void Mk64GameAudio3DSServiceLoading() __attribute__((weak));
extern "C" void Mk64Diagnostics3DSCheckpoint(const char*) __attribute__((weak));
extern "C" bool Mk64Improved3DSReclaim(){return pack.Reclaim();}
extern "C" bool Mk64NativeKartReclaimCache(){if(!native)return false;native=false;++nativeDrops;return true;}
bool ReleaseRetainedTextures(){if(!retained)return false;retained=false;++retainedDrops;return true;}
extern "C" void Mk64GameAudio3DSServiceLoading(){++loading;}
extern "C" void Mk64Diagnostics3DSCheckpoint(const char* message){checkpoints.emplace_back(message);}
void Released(const void*){if(reenter)assert(!Mk64Resource3DSReclaimPerformanceCache());}
LoadedResource resource;
LoadedResource* LoadResolvedPathOnce(size_t,CrcEntry*) {
 ++attempts;
 if(repopulate)assert(pack.Load("test",2,2,true));
 if(alwaysFail || attempts<=failThrough)throw std::bad_alloc();
 return &resource;
}
void Put(std::vector<uint8_t>& b,size_t at,uint64_t v,unsigned n=4){for(unsigned i=0;i<n;++i)b[at+i]=uint8_t(v>>(i*8));}
void Fixture(){
 std::vector<uint8_t> pixels(16,123),compressed(compressBound(pixels.size()));uLongf n=compressed.size();assert(compress2(compressed.data(),&n,pixels.data(),pixels.size(),1)==Z_OK);compressed.resize(n);
 std::vector<uint8_t> b(48+n);std::memcpy(b.data(),"MUI3",4);Put(b,4,1);Put(b,8,1);Put(b,12,32);Put(b,16,mk64_3ds::improved::Hash("test"),8);Put(b,24,48);Put(b,28,n);Put(b,32,crc32(0,pixels.data(),pixels.size()));for(unsigned o:{36,38,40,42})Put(b,o,2,2);std::copy(compressed.begin(),compressed.end(),b.begin()+48);
 std::ofstream f("fixture.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());
}
void Reset(){
 reenter=false;pack.Close();assert(pack.Open("fixture.bin"));pack.SetReleaseCallback(Released);sArchive=std::make_unique<Archive>();
 drops=attempts=loading=nativeDrops=retainedDrops=failThrough=0;retained=native=alwaysFail=repopulate=false;checkpoints.clear();
}
'''
checks=r'''
int main(){
 Fixture();Reset();assert(pack.Load("test",2,2,true));auto live=pack.Load("test",2,2,true);
 failThrough=1;reenter=true;assert(LoadResolvedPath(0,nullptr)==&resource);reenter=false;
 assert(attempts==2 && loading==1 && !drops && sArchive->resident && !pack.CachedBytes() && live);
 assert(checkpoints.size()==1 && checkpoints[0]=="improved-cache-released-for-memory");live={};
 Reset();assert(pack.Load("test",2,2,true));reenter=true;assert(Mk64Resource3DSReclaimPerformanceCache());reenter=false;
 assert(!drops && sArchive->resident && !std::strcmp(Mk64Resource3DSLastReclaimedCache(),"improved-cache-released-for-memory"));
 Reset();retained=true;failThrough=1;assert(LoadResolvedPath(0,nullptr)==&resource);assert(!drops && retainedDrops==1 && attempts==2);
 Reset();native=true;failThrough=2;assert(LoadResolvedPath(0,nullptr)==&resource);assert(nativeDrops==1 && drops==1 && attempts==3 && !sArchive->resident);
 // A failing retry can reconstruct Improved. It cannot repeatedly reclaim
 // that same tier forever, or surrender live borrowers through reentrancy.
 Reset();retained=native=alwaysFail=repopulate=true;assert(pack.Load("test",2,2,true));
 bool failed=false;try{LoadResolvedPath(0,nullptr);}catch(const std::bad_alloc&){failed=true;}
 assert(failed && attempts==6 && retainedDrops==1 && nativeDrops==1 && drops==2);
 Reset();sArchive.reset();alwaysFail=true;failed=false;
 try{LoadResolvedPath(0,nullptr);}catch(const std::bad_alloc&){failed=true;}assert(failed && attempts==1 && !drops);
 pack.Close();std::remove("fixture.bin");std::puts("PASS: production new_handler/resource recovery, Improved-first O2R preservation, borrowed pixels, reentrant callbacks, per-tier retries and bounded persistent failure.");
}
'''
source=out/'resource_recovery.cpp';source.write_text(prelude+functions+checks);binary=out/'resource-recovery'
command=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(port/'include'),str(source),'-lz','-o',str(binary)]
(out/'commands.txt').write_text(' '.join(command)+'\n'+str(binary)+'\n');subprocess.run(command,check=True);subprocess.run([str(binary)],cwd=out,check=True)
