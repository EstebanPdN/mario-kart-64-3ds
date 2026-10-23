#include "frame_begin_3ds.hpp"
#include "recording_time_3ds.hpp"
#include "adaptive_presentation_3ds.hpp"
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace {
std::uint64_t nowUs=0, queueReady=0, presentReady=0, sdkStarted=0;
bool acquired=false;
}
extern "C" void Mk64Startup3DSStage(const char*){}
extern "C" void Mk64Startup3DSFailure(const char*){}
extern "C" void Mk64Startup3DSDisplayTimeout(){}
extern "C" void Mk64Gpu3DSFenceFailure(const char*){}
std::uint64_t osGetTime(){return nowUs/1000;}
std::uint64_t svcGetSystemTick(){return nowUs*SYSCLOCK_ARM11/1000000;}
void svcSleepThread(std::int64_t ns){nowUs+=ns/1000;}
[[noreturn]] void svcExitProcess(){throw std::runtime_error("timeout");}
bool gspIsPresentPending(int){assert(acquired);return nowUs<presentReady;}
unsigned C3D_FrameCounter(int){return nowUs/16667;}
bool C3D_FrameBegin(unsigned flags){
    assert(flags==C3D_FRAME_NONBLOCK);
    if(nowUs<queueReady)return false;
    acquired=true;sdkStarted=nowUs;return true;
}
int main(){
    using namespace mk64_3ds;
    // Execute the production fence: 7 ms GPU queue wait occurs before the SDK
    // timer; 16 ms pending swap occurs after it. Both ownership waits remain.
    queueReady=7000;presentReady=23000;FrameBeginWait3DS measured;
    assert(BeginFrame3DS(0,"test",&measured));
    assert(nowUs==23000 && sdkStarted==7000);
    assert(measured.pendingPresentUs>=15999 && measured.pendingPresentUs<=16000);
    nowUs+=9000;const float sdk=(nowUs-sdkStarted)/1000.f;
    assert(sdk==25.f);
    const float active=ActiveRecordingMilliseconds(sdk,measured.pendingPresentUs);
    assert(std::abs(active-9.f)<.002f);
    // The raw timer would reject healthy work. The corrected timer still
    // rejects actual CPU/GPU overload and low audio without weakening policy.
    AdaptivePresentationState state;state.midpointEnabled=true;
    AdaptivePresentationInputs input;
    input.hasPriorTopFrame=true;input.audioBufferedFrames=1800;
    input.audioLowWaterFrames=896;input.audioRecoveryFrames=1344;
    input.keyframeHeadroom=true;input.citro3DBusy=active>16.f;
    assert(UpdateAdaptivePresentation(&state,input).renderMidpoint);
    input.citro3DBusy=ActiveRecordingMilliseconds(37.f,16000)>16.f;
    assert(UpdateAdaptivePresentation(&state,input).pressureMask & AdaptivePressureCitro3DBusy);
    input.citro3DBusy=false;input.audioBufferedFrames=800;
    assert(UpdateAdaptivePresentation(&state,input).pressureMask & AdaptivePressureAudioLow);
    assert(ActiveRecordingMilliseconds(0.f,1000)==0.f);
    assert(ActiveRecordingMilliseconds(12.f,0)==12.f);
    nowUs=queueReady=presentReady=0;acquired=false;
    measured.pendingPresentUs=999;
    assert(BeginFrame3DS(0,"test",&measured) && measured.pendingPresentUs==0);
    // SYNCDRAW remains outside the SDK CPU timer and is not subtracted again.
    nowUs=0;acquired=false;measured={};
    assert(BeginFrame3DS(C3D_FRAME_SYNCDRAW,"test",&measured));
    assert(sdkStarted==17000 && measured.pendingPresentUs==0);
}
