#!/usr/bin/env python3
"""Exercise the actual lab controller with host platform/game substitutes."""
from pathlib import Path
import re, subprocess, sys, tempfile

platform = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
runtime = (platform / 'source/autodump_lab_3ds.cpp').read_text()
runtime = '\n'.join(line for line in runtime.splitlines() if not line.startswith('#include'))
runtime = runtime.replace('statvfs("sdmc:/3ds/MK64", &info)', 'HostStatvfs("sdmc:/3ds/MK64", &info)')
registry = (platform.parents[1] / 'third_party/SpaghettiKart/src/engine/registry/RegisterTracks.cpp').read_text()
ids = re.findall(r'"(mk:[a-z0-9_]+)"', runtime.split('static_assert')[0])
assert len(ids) == len(set(ids)) == 16
assert all('"' + name + '"' in registry for name in ids)
prefix = r'''
#include "autodump_schedule_3ds.hpp"
#include "dump_crc_3ds.hpp"
#include "diagnostic_export_3ds.hpp"
#include "autodump_lab_3ds.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/statvfs.h>
#include <sys/stat.h>
using s32=int;
enum {GRAND_PRIX=0, MUSHROOM_CUP=0, SCREEN_MODE_1P=0, CC_50=0, MARIO=0,
      RACING=4, MAIN_MENU_FROM_QUIT=1, RACE_SETUP=1, RACE_IN_PROGRESS=3};
int gPlayerCount, gPlayerCountSelection1, gScreenModeSelection, gModeSelection, gCCSelection,
    gDemoMode, gIsGamePaused, gCupSelection, gCourseIndexInCup, gIsMirrorMode,
    gGamestateNext, gGamestate, gRaceState, gCharacterSelections[8], gLapCountByPlayerId[8];
float gCourseTimer=0;
struct Cup {
    std::vector<std::string> mTracks;
    Cup(const char*,const char*,std::vector<std::string> t):mTracks(t){}
};
struct World { Cup* cup=nullptr;void SetCupIndex(int){} void SetCurrentCup(Cup*c){cup=c;} } world;
World* GetWorld(){return &world;}
bool packAvailable=true, exitRequested=false, storageAvailable=true;
bool audioPaused=false, writeSuccess=true, flushSuccess=true, historySuccess=true, requestAllowed=true;
unsigned epochs=0, exports=0, fpsResets=0, graphicsResets=0, pauses=0, abandonments=0;
mk64_3ds::RaceClock gameClock;
unsigned requests=0, autoEnables=0; std::uint64_t now=0, freeBytes=10ULL<<30;
std::string title;
int HostStatvfs(const char*,struct statvfs* out){out->f_frsize=1024;out->f_bavail=freeBytes/1024;return 0;}
namespace mk64_3ds::improved {
struct Pack { bool Load(const char*,unsigned,unsigned){return packAvailable;} } pack;
Pack& LocalPack(){return pack;}
}
namespace mk64_3ds {
PerformanceTick current;
std::size_t PerformanceStorageBytes(){return storageAvailable?624672:0;}
std::uint64_t PerformanceNow(){return now*1000;}
void PerformanceAbandon(){++abandonments;}
void PerformanceResume(PerformancePause reason,std::uint64_t start){
    assert(reason==PerformancePause::LabExport && audioPaused && !gameClock.Running());
    assert(now*1000>=start);++epochs;
}
bool PerformanceTimerEnabled(const std::uint32_t*){return true;}
PerformanceTick& PerformanceCurrent(){return current;}
bool PerformanceWrite(const char*){
    assert(audioPaused && !gameClock.Running());
    const auto clock=gameClock.Read(now);const auto course=gCourseTimer;
    ++exports;now+=60000;
    assert(gameClock.Read(now)==clock && gCourseTimer==course);
    return writeSuccess;
}
}
struct Mk64DiagnosticsInput3DS {unsigned downMask=0;};
extern "C" {
std::uint64_t osGetTime(){return now;}
std::uint64_t svcGetSystemTick(){return 0x12345678;}
std::int32_t svcSleepThread(std::int64_t){return 0;}
void TrackBrowser_SetTrackFromCup(){assert(world.cup && world.cup->mTracks.size()==4);}
void Mk64Auto3DSLabEnable(){++autoEnables;}
void Mk64GameAudio3DSSetPaused(bool paused){audioPaused=paused;if(paused)++pauses;}
void Mk64RaceClock3DSFreeze(){gameClock.Sync(now,false);}
void Mk64Graphics3DSResumeAfterDiagnosticPause(){assert(audioPaused);++graphicsResets;}
void Mk64BottomUI3DSResetFps(){assert(audioPaused);++fpsResets;}
void Mk64BottomUI3DSShowProgress(const char*t,const char*,unsigned){title=t;}
bool Mk64Diagnostics3DSConsumeInput(Mk64DiagnosticsInput3DS*i){i->downMask=exitRequested?8:0;return true;}
const char* Mk64Diagnostics3DSLabDirectory(){return "lab";}
bool Mk64Improved3DSWriteHistory(const char*){assert(audioPaused&&!gameClock.Running());return historySuccess;}
bool Mk64Diagnostics3DSLabFlush(){assert(audioPaused&&!gameClock.Running());return flushSuccess;}
bool Mk64Diagnostics3DSRequestLabDump(){if(!requestAllowed)return false;++requests;return true;}
int Mk64Settings3DSGetUiTextures(){return 2;}
bool Mk64Settings3DSGetStaticMeshes(){return true;}
}
'''
suffix = r'''
int main(int argc,char**argv){
    const auto* bytes=reinterpret_cast<const unsigned char*>("123456789");
    assert(mk64_3ds::DumpCrc32(0,bytes,9)==0xcbf43926U);
    auto part=mk64_3ds::DumpCrc32(0,bytes,3);
    assert(mk64_3ds::DumpCrc32(part,bytes+3,6)==0xcbf43926U);
    for(unsigned seed=0;seed<4096;++seed){
        mk64_3ds::AutoDumpSchedule p;p.Shuffle(seed);
        bool seen[16]={};for(unsigned i=0;i<6;++i){assert(p.courses[i]<16&&!seen[p.courses[i]]);seen[p.courses[i]]=true;}
        assert(p.HasStartCapture() && p.TargetMs()==0 && p.Due(0));
        p.race=2;assert(!p.HasStartCapture() && !p.Due(14999) && p.Due(15000));
        assert(!p.NextRace()&&!p.Complete(true));
    }
    const std::string mode=argc>1?argv[1]:"success";
    if(mode=="no-pack")packAvailable=false;
    if(mode=="no-space")freeBytes=1;
    if(mode=="no-trace")storageAvailable=false;
    if(mode=="no-pack"||mode=="no-space"||mode=="no-trace"){
        assert(!Mk64Lab3DSBoot());assert(Mk64Lab3DSTick()==1&&title=="LAB ERROR");return 0;
    }
    assert(Mk64Lab3DSBoot());
    if(mode=="timeout"){
        now+=900001;assert(Mk64Lab3DSTick()==1);assert(Mk64Lab3DSTick()==1&&title=="LAB ERROR");return 0;
    }
    std::vector<std::string> tracks;
    for(unsigned race=0;race<6;++race){
        assert(gGamestateNext==RACING && autoEnables==race+1);
        tracks.push_back(world.cup->mTracks[0]);
        gGamestate=RACING;gRaceState=0;gLapCountByPlayerId[0]=0;gCourseTimer=0;
        gameClock.Reset();gameClock.Sync(now,true);
        assert(Mk64Lab3DSTick()==0&&requests==race*3); // Never capture an uninitialized scene.
        const bool early=race<2;
        gRaceState=RACE_SETUP;
        if(!early){
            assert(Mk64Lab3DSTick()==0&&requests==race*3); // Only two tracks get a grid dump.
            gRaceState=RACE_IN_PROGRESS;
            gCourseTimer=14.999f;assert(Mk64Lab3DSTick()==0&&requests==race*3);
        }
        for(unsigned capture=0;capture<3;++capture){
            const float target=float((capture+(early?0:1))*15);
            if(capture>0){
                gRaceState=RACE_IN_PROGRESS;gCourseTimer=target-0.001f;
                assert(Mk64Lab3DSTick()==0&&requests==race*3+capture);
            }
            gCourseTimer=target;
            requestAllowed=false;
            assert(Mk64Lab3DSTick()==0&&requests==race*3+capture&&!sPlan.pending);
            requestAllowed=true;
            assert(Mk64Lab3DSTick()==0&&requests==race*3+capture+1);
            assert(Mk64Lab3DSTick()==0&&requests==race*3+capture+1); // pending cannot duplicate
            assert(gGamestateNext==RACING);
            assert(Mk64Lab3DSWriteContext("capture"));
            if(mode=="write-failure"){
                Mk64Lab3DSDumpComplete("capture",false);
                assert(Mk64Lab3DSTick()==1&&title=="LAB ERROR");return 0;
            }
            const unsigned epochBefore=epochs;
            { mk64_3ds::DiagnosticExportPause pause;
            now+=60000; // Long SD writes do not affect game-time scheduling.
            // Acknowledge a slightly delayed third request: the five seconds
            // must start here, not at its nominal 30/45-second deadline.
            if(capture==2)gCourseTimer=target+0.25f;
            Mk64Lab3DSDumpComplete("capture",true);
            }
            assert(epochs==epochBefore+1&&!audioPaused);
            gameClock.Sync(now,true);
            if(mode=="duplicate-ack"){
                Mk64Lab3DSDumpComplete("capture",true);assert(Mk64Lab3DSTick()==1&&title=="LAB ERROR");return 0;
            }
            assert(Mk64Lab3DSTick()==0&&requests==race*3+capture+1);
        }
        const float third=early?30.25f:45.25f;
        now+=60000;assert(Mk64Lab3DSTick()==0&&gGamestateNext==RACING);
        gIsGamePaused=1;gCourseTimer=third+5;
        assert(Mk64Lab3DSTick()==0&&gGamestateNext==RACING);
        gIsGamePaused=0;gCourseTimer=third+4.999f;
        assert(Mk64Lab3DSTick()==0&&gGamestateNext==RACING);
        if(mode=="summary-failure")writeSuccess=false;
        if(mode=="history-failure")historySuccess=false;
        if(mode=="flush-failure")flushSuccess=false;
        if(mode=="summary-dir-failure"){
            FILE* blocked=std::fopen("lab/race-01-final","wb");assert(blocked);std::fclose(blocked);
        }
        const unsigned epochBefore=epochs;
        const unsigned exportedBefore=exports;
        gCourseTimer=third+5;
        const int result=Mk64Lab3DSTick();
        // This is the action consumed before thread5_iteration in the real loop.
        // Both intermediate export and final completion must skip that iteration.
        assert(result!=MK64_LAB_ITERATE);
        if(mode=="summary-failure"||mode=="history-failure"||mode=="summary-dir-failure"|| (mode=="flush-failure"&&race==5)){
            assert(result==MK64_LAB_TERMINAL&&audioPaused&&sPlan.failed);
            assert(Mk64Lab3DSTick()==MK64_LAB_TERMINAL&&title=="LAB ERROR");return 0;
        }
        assert(result==(race==5?MK64_LAB_TERMINAL:MK64_LAB_EXPORTED));
        assert(epochs==epochBefore+1&&exports==exportedBefore+1);
        assert(audioPaused==(race==5));
        assert(gLapCountByPlayerId[0]==0); // Advancing never depends on finishing a lap.
        if(race<5){assert(gGamestateNext==MAIN_MENU_FROM_QUIT);gGamestate=MAIN_MENU_FROM_QUIT;assert(Mk64Lab3DSTick()==0);}
    }
    assert(requests==18&&epochs==24&&exports==6&&sTerminal);
    assert(fpsResets==epochs&&graphicsResets==epochs&&abandonments==epochs);
    assert(Mk64Lab3DSTick()==1&&title=="FINISH");
    const auto finalEpoch=epochs, finalExports=exports;
    for(unsigned i=0;i<20;++i)assert(Mk64Lab3DSTick()==MK64_LAB_TERMINAL&&audioPaused);
    assert(epochs==finalEpoch&&exports==finalExports);
    exitRequested=true;assert(Mk64Lab3DSTick()==2);
    for(unsigned a=0;a<6;++a)for(unsigned b=0;b<a;++b)assert(tracks[a]!=tracks[b]);
    std::puts("PASS: six unique tracks, two grid captures, 18 acknowledged dumps, five-second early exits with zero completed laps, and FINISH verified.");
}
'''
(out / 'controller.cpp').write_text(prefix + runtime + suffix)
binary = out / 'controller'
subprocess.run(['clang++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                '-DMK64_3DS_AUTODUMP_LAB=1', '-DMK64_3DS_VERSION="2.0-E3-test"',
                '-I' + str(platform / 'include'), str(out / 'controller.cpp'), '-o', str(binary)], check=True)
for case in ('success', 'write-failure', 'summary-failure', 'history-failure', 'summary-dir-failure', 'flush-failure', 'duplicate-ack', 'timeout', 'no-pack', 'no-space', 'no-trace'):
    cwd = Path(tempfile.mkdtemp(prefix=case+'-', dir=out))
    (cwd / 'lab').mkdir(parents=True, exist_ok=True)
    (cwd / 'capture').mkdir(exist_ok=True)
    subprocess.run([str(binary), case], cwd=cwd, check=True)
print('PASS: capture/summary/directory/final-flush failures, duplicate acknowledgements, timeout, missing artwork/space/trace never report FINISH; exports freeze clocks/audio and reset epochs; terminal never iterates; all 16 track IDs and CRC32 verified.')
