#!/usr/bin/env python3
"""Exercise the production course loading bridge with bounded manifests and heap pressure."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);port=Path(__file__).resolve().parents[1]
(out/'3ds.h').write_text('#pragma once\n#include <cstddef>\nextern size_t testHeap; inline size_t envGetHeapSize(){return testHeap;}\n')
(out/'malloc.h').write_text('#pragma once\n#include <cstddef>\nextern size_t testUsed; struct mallinfo { size_t uordblks; }; inline struct mallinfo mallinfo(){return {testUsed};}\n')
cpp=out/'resource-loading.cpp'
cpp.write_text(r'''
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"
#include "game_runtime_3ds.h"
#include "resource_loading_3ds.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>
#include <string>
size_t testHeap=64*1024*1024,testUsed=16*1024*1024;
unsigned scope=0,services=0,compressed=0,textures=0,configure=0,residencyAttempts=0;
bool resident=false,grantResidency=false;
bool pressureDuringWarm=false;
size_t streamPayload=0,lastStreamBudget=0,lastResidencyBudget=0;
unsigned streamResets=0;
std::string report;
extern "C" void Mk64GameAudio3DSBeginLoading(){++scope;}
extern "C" void Mk64GameAudio3DSEndLoading(){assert(scope);--scope;}
extern "C" void Mk64GameAudio3DSServiceLoading(){assert(scope);++services;}
extern "C" Mk64UiTextures3DS Mk64Settings3DSGetUiTextures(){return MK64_UI_TEXTURES_3DS_IMPROVED;}
extern "C" bool Mk64Graphics3DSResolvedNewModel(){return true;}
extern "C" bool Mk64Improved3DSConfigure(int mode){assert(scope && mode==1);++configure;return true;}
extern "C" bool Mk64Resource3DSIsResident(){return resident;}
extern "C" void Mk64Resource3DSSetStreamingCacheBudget(size_t budget){assert(!budget);testUsed-=streamPayload;streamPayload=0;++streamResets;}
extern "C" bool Mk64Resource3DSMakeResident(size_t budget,void(*progress)(unsigned)){
 const size_t limit=testHeap-8*1024*1024;
 assert(scope && budget==(testUsed<limit?limit-testUsed:0));++residencyAttempts;
 lastResidencyBudget=budget;assert(!streamPayload&&streamResets==residencyAttempts);
 progress(1);resident=grantResidency;return resident;
}
extern "C" bool Mk64Improved3DSPrepare(size_t budget,bool isNew){assert(scope && isNew);return budget>0;}
extern "C" size_t Mk64Improved3DSResidentBytes(){return 0;}
extern "C" size_t Mk64Improved3DSPageBytes(){return 0;}
extern "C" const char* ResourceGetNameByCrc(uint64_t crc){return crc==1?"texture":crc==2?"vertices":crc==3?"light":nullptr;}
extern "C" void Mk64Resource3DSGetCacheStats(Mk64ResourceCacheStats3DS* stats){*stats={};stats->streamPayloadBytes=streamPayload;stats->streamAllocationBytes=streamPayload+1024;}
extern "C" size_t Mk64Resource3DSPrefetchNames(const char*const*,size_t count,size_t budget){
 assert(scope&&budget<=2*1024*1024);
 assert(budget<=streamPayload+testHeap-testUsed-8*1024*1024);
 lastStreamBudget=budget;compressed+=count;return count;
}
extern "C" size_t Mk64Improved3DSPreload(const char*const* names,size_t count){
 assert(scope&&count==1&&std::string(*names)=="texture");textures+=count;
 if(pressureDuringWarm)testUsed=testHeap-8*1024*1024-4096;
 return count;
}
extern "C" void Mk64Diagnostics3DSCheckpoint(const char* text){report=text;}
void write32(FILE* f,uint32_t x){for(unsigned i=0;i<4;++i)std::fputc((x>>(i*8))&255,f);}
void manifest(bool corrupt){
 std::filesystem::create_directories("romfs:/course-preloads/test");
 FILE* f=std::fopen("romfs:/course-preloads/test/resources.npr","wb");assert(f);
 std::fwrite(corrupt?"BAD!":"NPR1",1,4,f);write32(f,3);
 for(unsigned i=1;i<=3;++i){write32(f,i);write32(f,0);write32(f,i);write32(f,64);}
 assert(!std::fclose(f));
}
int main(){
 Mk64Resource3DSPrepareCourse(nullptr);assert(!scope&&!configure);
 Mk64Resource3DSPrepareCourse("missing");assert(!scope&&configure==1&&!textures);
 manifest(false);Mk64Resource3DSPrepareCourse("test");
 assert(!scope&&compressed==3&&textures==1&&report.find("dependencies=3")!=std::string::npos);
 grantResidency=true;Mk64Resource3DSPrepareCourse("test");assert(!scope&&compressed==3&&textures==2);
 unsigned previous=residencyAttempts;Mk64Resource3DSPrepareCourse("test");assert(residencyAttempts==previous);
 manifest(true);Mk64Resource3DSPrepareCourse("test");assert(!scope&&textures==3);
 manifest(false);testUsed=testHeap-8*1024*1024;Mk64Resource3DSPrepareCourse("test");assert(!scope&&textures==3);
 resident=grantResidency=false;testUsed=16*1024*1024;streamPayload=64*1024;pressureDuringWarm=true;
 Mk64Resource3DSPrepareCourse("test");assert(!scope&&textures==4&&lastStreamBudget==streamPayload+4096);
 assert(lastResidencyBudget==40*1024*1024+64*1024);
 pressureDuringWarm=false;testUsed=testHeap-8*1024*1024+32*1024;streamPayload=64*1024;
 Mk64Resource3DSPrepareCourse("test");assert(!scope&&lastResidencyBudget==32*1024&&!resident);
 testUsed=testHeap-8*1024*1024+128*1024;streamPayload=64*1024;
 Mk64Resource3DSPrepareCourse("test");assert(!scope&&lastResidencyBudget==0&&!resident);
 std::puts("PASS: actual course preparation validates manifests, preserves reserve, separates texture types, retains fallback and balances loading audio");
}
''')
exe=out/'resource-loading'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-I',str(out),'-I',str(port/'include'),str(cpp),str(port/'source/resource_loading_3ds.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],cwd=out,check=True)
