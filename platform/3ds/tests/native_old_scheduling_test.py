#!/usr/bin/env python3
"""Exercise actual production Old audio routing and manual lab tick functions."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);port=Path(__file__).resolve().parents[1]
def function(text,marker):
 start=text.index(marker);begin=text.index('{',start);depth=1;i=begin+1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
def run(name,text,flags=()):
 src=out/(name+'.cpp');src.write_text(text);exe=out/name
 subprocess.run(['clang++','-std=c++20','-O1','-fsanitize=address,undefined',*flags,str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
s=(port/'source/audio_runtime_3ds.cpp').read_text()
prefix=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
using Thread=void*;using LightEvent=int;using Result=int;using __3ds_u32=uint32_t;
#define R_SUCCEEDED(r) ((r)>=0)
#define R_FAILED(r) ((r)<0)
bool aptGrants=false;__3ds_u32 aptLimit=30;bool sCpuLimitChanged=false;__3ds_u32 sPreviousCpuLimit=0;
Result APT_GetAppCpuTimeLimit(__3ds_u32* out){*out=aptLimit;return 0;}
bool aptSticks=true;
Result APT_SetAppCpuTimeLimit(__3ds_u32 value){if(!aptGrants)return -1;if(aptSticks)aptLimit=value;return 0;}
constexpr int MK64_PERFORMANCE_PROFILE_NEW_3DS=1,RESET_ONESHOT=1;
constexpr unsigned kAudioWorkerStackSize=65536,kAudioWorkerPriority=24,kTargetBufferedFrames=1792,kMaxSynthesisBlocksPerPump=2;
bool isNew=false,failThread=false,sGameStateReady=true,sReady=true,sSuspended=false,sAudioPrimed=false,sEmptyObservedActive=false;
unsigned sPumpCallCount=0,sObservedEmptyTransitionCount=0,sQueueFailureCount=0,buffered=0,synthCalls=0,threadCalls=0,restores=0;
int sWorkerCore=-1;Thread sWorkerThread=nullptr;LightEvent sWorkerStart,sWorkerDone;std::string checkpoint;
std::atomic<bool> sWorkerRunning=false,sJobOutstanding=false,sPaused=false,sLastJobQueued=false,sRenderWindowOpen=false;
std::atomic<unsigned> sLastJobBlocks=0,sVolumePercent=100;
int Mk64Graphics3DSResolvedPerformanceProfile(){return isNew?1:0;}
void RestoreCpuLimit(){++restores;}
void Mk64Diagnostics3DSCheckpoint(const char* s){checkpoint=s;}
void LightEvent_Init(LightEvent*,int){}
void LightEvent_Signal(LightEvent*){}
void AudioWorkerMain(void*){}
int lastCore=-9;
Thread threadCreate(void(*)(void*),void*,unsigned stack,int priority,int core,bool){assert(stack==65536 && priority==24 && (core==1 || core==2));lastCore=core;++threadCalls;return failThread?nullptr:(Thread)1;}
unsigned Mk64Audio3DSBufferedFrames(){return buffered;}
unsigned Mk64Settings3DSGetMasterVolumePercent(){return 100;}
bool WaitForWorkerJob(){assert(false);return true;}
void FinishPumpTelemetry(unsigned,unsigned,bool){}
bool SynthesizeAndQueue(unsigned){assert(!sWorkerThread && !sJobOutstanding.load());++synthCalls;buffered+=896;return true;}
namespace mk64_3ds {struct Tick{unsigned audio_synth_us=0,audio_blocks=0;};Tick tick;Tick& PerformanceCurrent(){return tick;}struct PerformanceTimer{PerformanceTimer(unsigned&){};};}
'''
body='\n'.join(function(s,name) for name in ['bool StartAudioWorker()','bool ScheduleWorkerJob(','bool NeedsSynthesis()','extern "C" void Mk64GameAudio3DSPump()'])
main=r'''
int main(){
 // Old without a core-1 grant keeps the serialized main-thread synthesis path.
 assert(!StartAudioWorker());assert(sWorkerCore==-1 && !sWorkerThread && threadCalls==0 && restores==1);assert(checkpoint=="audio-worker-sync-fallback");
 Mk64GameAudio3DSPump();assert(synthCalls==2 && buffered==1792 && mk64_3ds::tick.audio_blocks==2);Mk64GameAudio3DSPump();assert(synthCalls==2);
 buffered=896;Mk64GameAudio3DSPump();assert(synthCalls==3 && buffered==1792);sPaused=true;buffered=0;Mk64GameAudio3DSPump();assert(synthCalls==3);sPaused=false;
 // Old with a grant reserves the largest share and runs the worker on core 1.
 aptGrants=true;assert(StartAudioWorker() && sWorkerCore==1 && lastCore==1 && threadCalls==1);
 assert(aptLimit==80 && sCpuLimitChanged && sPreviousCpuLimit==30 && checkpoint=="audio-worker-core1");
 sWorkerThread=nullptr;sWorkerRunning=false;sWorkerCore=-1;
 // A reported success that verifies below 40% must fall back, not starve audio on core 1.
 aptLimit=5;sCpuLimitChanged=false;aptSticks=false;restores=0;assert(!StartAudioWorker() && sWorkerCore==-1 && threadCalls==1);
 assert(checkpoint=="audio-worker-sync-fallback" && restores==6);aptSticks=true;
 isNew=true;assert(StartAudioWorker() && sWorkerCore==2 && lastCore==2 && threadCalls==2);sWorkerThread=nullptr;failThread=true;
 const unsigned before=restores;assert(!StartAudioWorker() && sWorkerCore==-1 && restores==before+1);
}
'''
run('old-audio',prefix+body+main)
s=(port/'source/native_course_lab_3ds.cpp').read_text();tick=function(s,'extern "C" bool Mk64NativeLuigiLabTick()')
prefix='''#include <cassert>\nunsigned writes=0,dumps=0;void Save(unsigned){++writes;}void Mk64NativeKartSaveStats(unsigned){++writes;}void Mk64NativeVegetationSaveStats(unsigned){++writes;}void Mk64Diagnostics3DSRequestFullDump(){++dumps;}\n'''
run('manual-tick',prefix+tick+'\nint main(){for(int i=0;i<2000;++i)assert(Mk64NativeLuigiLabTick());assert(writes==0 && dumps==0);}', ['-DMK64_3DS_NATIVE_PLAYTEST','-DMK64_3DS_NATIVE_KART_LAB','-DMK64_3DS_NATIVE_VEGETATION_LAB'])
run('automatic-lab-tick',prefix+tick+'\nint main(){for(int i=0;i<899;++i)assert(Mk64NativeLuigiLabTick());assert(!Mk64NativeLuigiLabTick());assert(writes==3 && dumps==2);}', ['-DMK64_3DS_NATIVE_KART_LAB','-DMK64_3DS_NATIVE_VEGETATION_LAB'])
print('PASS: actual Old audio uses core 1 when granted and serialized synthesis otherwise, New retains core 2/failure fallback; manual 2000 ticks export nothing, bounded lab retains diagnostics.')
