#!/usr/bin/env python3
"""Exercise the actual loading-audio scope entry/exit with mocked NDSP work."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];source=(port/'source/audio_runtime_3ds.cpp').read_text()
start=source.index('extern "C" void Mk64GameAudio3DSBeginLoading()');end=source.index('extern "C" void Mk64GameAudio3DSSetPaused',start);actual=source[start:end]
cpp=r'''
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <cstdint>
bool sReady=true,sGameStateReady=true,sSuspended=false,sLoadingAudio=false;
unsigned sLoadingAudioDepth=0,waits=0,services=0;uint64_t sLastLoadingAudioMs=0;
void WaitForWorkerJob(){++waits;}
uint64_t osGetTime(){return 100;}
extern "C" void Mk64GameAudio3DSServiceLoading(){if(sLoadingAudio)++services;}
ACTUAL
struct Scope {Scope(){Mk64GameAudio3DSBeginLoading();}~Scope(){Mk64GameAudio3DSEndLoading();}};
int main(){
 Mk64GameAudio3DSEndLoading();assert(!sLoadingAudioDepth && !services);
 {Scope a;assert(sLoadingAudio && sLoadingAudioDepth==1 && waits==1 && services==1);
  {Scope b;assert(sLoadingAudioDepth==2 && waits==1 && services==1);}
  assert(sLoadingAudio && sLoadingAudioDepth==1 && services==1);
 }
 assert(!sLoadingAudio && !sLoadingAudioDepth && services==2);
 try{Scope a;throw std::runtime_error("load failed");}catch(const std::runtime_error&){}
 assert(!sLoadingAudio && !sLoadingAudioDepth && waits==2 && services==4);
 for(unsigned mode=0;mode<3;++mode){sReady=mode!=0;sGameStateReady=mode!=1;sSuspended=mode==2;{Scope a;assert(!sLoadingAudio && !sLoadingAudioDepth);}}
 assert(waits==2 && services==4);
 std::puts("PASS: actual loading-audio entry/exit, nested credits/course scopes, exception teardown, unavailable/suspended guards.");
}
'''.replace('ACTUAL',actual)
f=a.output/'scope.cpp';f.write_text(cpp);exe=a.output/'scope';subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(f),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
bridge=(port/'cmake/world_tracks_bridge_3ds.cmake').read_text();assert 'struct LoadingAudioScope' in bridge and '~LoadingAudioScope() { Mk64GameAudio3DSEndLoading(); }' in bridge
