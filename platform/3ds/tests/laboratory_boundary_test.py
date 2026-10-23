#!/usr/bin/env python3
"""Compile the actual main-loop lab branch and texture-mode transaction on host."""
from pathlib import Path
import subprocess
import sys

platform = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (platform / 'source/game_main_3ds.cpp').read_text()
start = source.index('bool ApplyArtworkModeChange(')
end = source.index('\n[[noreturn]]', start)
function = source[start:end]
start = source.index('#ifdef MK64_3DS_AUTODUMP_LAB\n        int labAction;')
end = source.index('#endif', start) + len('#endif')
lab_branch = source[start:end]
# Both UI/event changes and a change made inside PrepareFrame must be handled
# before a game iteration. Startup compaction/preparation has separate budgets.
assert source.index('Mk64Improved3DSConfigure(Mk64Settings3DSGetUiTextures())') < source.index('if (!Mk64BottomUI3DSInit())')
assert source.index('Mk64Improved3DSConfigure(Mk64Settings3DSGetUiTextures())') < source.index('mk64_3ds::improved::LocalPack()')
assert source.index('const bool resident = Mk64Resource3DSMakeResident') < source.index('const std::size_t artworkBudget')
assert source.index('const std::size_t artworkBudget') < source.index('    Mk64Loading3DSStop();', source.index('const std::size_t artworkBudget'))
loop = source[source.index('    while (WindowIsRunning()'):]
assert loop.count('ApplyArtworkModeChange(observedTextureMode)') == 2
assert loop.index('ApplyArtworkModeChange(observedTextureMode)') < loop.index('Mk64BottomUI3DSPrepareFrame();')
assert loop.rindex('ApplyArtworkModeChange(observedTextureMode)') < loop.index('thread5_iteration();')
# The existing graphics reset fences before releasing Fast3D owners; changing
# artwork must use that boundary, not merely the mode-keyed lookup cache.
runtime = (platform / 'source/game_runtime_3ds.cpp').read_text()
reset = runtime[runtime.index('extern "C" void Mk64Graphics3DSResetForTrackLoad('):]
reset = reset[:reset.index('\nextern "C"', 1)]
assert reset.index('ReleaseTextureAllocations()') < reset.index('TextureCacheClear()')
for released in ('loaded_texture', 'texture_to_load', 'palette_dram_addr', 'Mk64FrameInterpolation3DSResetHistory()'):
    assert released in reset
prefix = r'''
#include "diagnostic_export_3ds.hpp"
#include "autodump_lab_3ds.h"
#include "settings_3ds.h"
#include "heap_policy_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
Mk64UiTextures3DS mode=MK64_UI_TEXTURES_3DS_MENU, persisted=mode;
bool audioPaused=false, clockRunning=true, configureSucceeds=true, prepareSucceeds=true, saveSucceeds=true;
unsigned configureCalls=0, prepareCalls=0, saveCalls=0, epochs=0, resets=0, graphicsResets=0, logicCalls=0;
std::size_t heapUsed=10*1024*1024, lastBudget=0;
std::uint64_t now=1000;
int nextAction=0;
std::vector<std::string> logLines;
struct HeapInfo {std::size_t uordblks;};
HeapInfo mallinfo(){return {heapUsed};}
std::size_t envGetHeapSize(){return 48*1024*1024;}
std::uint64_t svcGetSystemTick(){return now;}
namespace mk64_3ds::improved {
struct Pack {bool available=true,aliases=false;bool IsOpen()const{return available;}} pack;
Pack& GlobalPack(){return pack;}
}
namespace mk64_3ds {
PerformanceTick row;
std::uint64_t PerformanceNow(){return now;}
void PerformanceAbandon(){}
void PerformanceResume(PerformancePause reason,std::uint64_t start){
    assert(reason==PerformancePause::ResourceLoad && audioPaused && !clockRunning && now>=start);++epochs;
}
bool PerformanceTimerEnabled(const std::uint32_t*){return false;}
PerformanceTick& PerformanceCurrent(){return row;}
}
extern "C" {
Mk64UiTextures3DS Mk64Settings3DSGetUiTextures(){return mode;}
void Mk64Settings3DSSetUiTextures(Mk64UiTextures3DS value){mode=value;}
bool Mk64Settings3DSSave(){++saveCalls;if(saveSucceeds)persisted=mode;return saveSucceeds;}
void Mk64RaceClock3DSFreeze(){clockRunning=false;}
void Mk64GameAudio3DSSetPaused(bool value){audioPaused=value;}
void Mk64Graphics3DSResumeAfterDiagnosticPause(){++resets;}
void Mk64Graphics3DSResetForTrackLoad(){
    assert(audioPaused&&!clockRunning&&graphicsResets==epochs);++graphicsResets;
}
void Mk64BottomUI3DSResetFps(){}
void Mk64Diagnostics3DSCheckpoint(const char* line){logLines.emplace_back(line);}
bool Mk64Graphics3DSResolvedNewModel(){return true;}
bool Mk64Improved3DSConfigure(int value){
    assert(audioPaused&&!clockRunning&&graphicsResets==epochs+1);++configureCalls;now+=100;
    if(!mk64_3ds::improved::pack.available)return false;
    if(!configureSucceeds){configureSucceeds=true;return false;}
    mk64_3ds::improved::pack.aliases=value==1;heapUsed+=1024;return true;
}
bool Mk64Improved3DSPrepare(std::size_t budget,bool newModel){
    assert(audioPaused&&!clockRunning&&newModel);++prepareCalls;lastBudget=budget;
    assert(budget==mk64_3ds::ArchiveResidencyBudget(envGetHeapSize(),heapUsed));now+=1000;return prepareSucceeds;
}
std::size_t Mk64Improved3DSMetadataBytes(){return 30000;}
std::size_t Mk64Improved3DSResidentBytes(){return prepareSucceeds?18000000:0;}
std::size_t Mk64Improved3DSPageBytes(){return 262208;}
int Mk64Lab3DSTick(){return nextAction;}
}
'''
suffix = r'''
void CheckLabBranch(int action){
    nextAction=action;audioPaused=false;clockRunning=true;
    std::uint64_t nextSimulationDeadline=1,deadlineRemainder=7;
    bool suppressNextPresentation=true;
    const auto before=logicCalls;
    for(unsigned once=0;once<1;++once){
        auto& perf=mk64_3ds::PerformanceCurrent();
LAB_BRANCH
        ++logicCalls;
    }
    assert(logicCalls==before+(action==MK64_LAB_ITERATE));
    if(action!=MK64_LAB_ITERATE){
        assert(nextSimulationDeadline==now&&deadlineRemainder==0&&!suppressNextPresentation&&!clockRunning);
        assert(audioPaused==(action!=MK64_LAB_EXPORTED));
    }
}
int main(){
    auto observed=mode;
    assert(!ApplyArtworkModeChange(observed)&&configureCalls==0&&prepareCalls==0&&graphicsResets==0);
    mode=MK64_UI_TEXTURES_3DS_IMPROVED;persisted=mode;
    assert(ApplyArtworkModeChange(observed)&&observed==mode&&mk64_3ds::improved::pack.aliases);
    assert(configureCalls==1&&prepareCalls==1&&epochs==1&&!audioPaused&&!clockRunning);
    assert(lastBudget+heapUsed+mk64_3ds::kArchiveReserveBytes==envGetHeapSize());
    for(unsigned i=0;i<100;++i)assert(!ApplyArtworkModeChange(observed));
    assert(configureCalls==1&&prepareCalls==1&&graphicsResets==1);
    mode=MK64_UI_TEXTURES_3DS_MENU;persisted=mode;configureSucceeds=false;clockRunning=true;
    assert(ApplyArtworkModeChange(observed));
    assert(mode==MK64_UI_TEXTURES_3DS_IMPROVED&&persisted==mode&&observed==mode);
    assert(mk64_3ds::improved::pack.aliases&&configureCalls==3&&prepareCalls==1&&saveCalls==1);
    assert(logLines.back()=="improved-mode-change-rejected-catalog-retained");
    for(unsigned i=0;i<100;++i)assert(!ApplyArtworkModeChange(observed));
    assert(configureCalls==3);
    mode=MK64_UI_TEXTURES_3DS_MENU;prepareSucceeds=false;
    assert(ApplyArtworkModeChange(observed)&&!mk64_3ds::improved::pack.aliases&&observed==mode);
    assert(logLines.back().find("catalog=ready resident=0")!=std::string::npos);
    mode=MK64_UI_TEXTURES_3DS_IMPROVED;configureSucceeds=false;saveSucceeds=false;
    assert(ApplyArtworkModeChange(observed)&&mode==MK64_UI_TEXTURES_3DS_MENU);
    assert(logLines.back()=="improved-mode-rollback-save-failed");
    assert(!ApplyArtworkModeChange(observed));
    mk64_3ds::improved::pack.available=false;mode=MK64_UI_TEXTURES_3DS_IMPROVED;
    const auto preparations=prepareCalls;
    assert(ApplyArtworkModeChange(observed)&&observed==mode&&prepareCalls==preparations);
    assert(logLines.back().find("absent-original-fallback")!=std::string::npos);
    assert(!ApplyArtworkModeChange(observed));
    assert(epochs==5&&resets==5&&graphicsResets==5);
    for(int action=0;action<=3;++action)CheckLabBranch(action);
    std::puts("PASS: actual mode transaction fences/releases graphics before catalog mutation, preserves 8 MiB reserve, rollback/settings, missing-pack fallback and once-only work; actual main-loop branch skips every nonzero lab action and resets pacing.");
}
'''.replace('LAB_BRANCH',lab_branch)
(out/'boundary.cpp').write_text(prefix+function+suffix)
binary=out/'boundary'
subprocess.run(['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-fsanitize=address,undefined',
                '-fno-sanitize-recover=all','-DMK64_3DS_AUTODUMP_LAB=1','-I'+str(platform/'include'),
                str(out/'boundary.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
