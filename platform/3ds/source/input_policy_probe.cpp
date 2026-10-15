#include "input_policy_3ds.hpp"
#include "select_policy_3ds.hpp"
#include "turbo_policy_3ds.hpp"
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace mk64_3ds;
int main() {
    PrimaryInputPolicy3DS keys{};
    keys.a=keys.b=keys.x=keys.y=keys.select=keys.l=keys.r=keys.start=true;
    keys.raceControls=true;
    assert(MapPrimaryInput(keys)==(MK64_N64_A|MK64_N64_B|MK64_N64_CLEFT|MK64_N64_CUP|MK64_N64_CDOWN|MK64_N64_R|MK64_N64_START));
    keys.raceControls=false;
    assert(MapPrimaryInput(keys)==(MK64_N64_A|MK64_N64_B|MK64_N64_CLEFT|MK64_N64_L|MK64_N64_R|MK64_N64_START));
    keys={}; keys.raceControls=true; keys.y=true;
    assert(MapPrimaryInput(keys)==0); // Y belongs to the complete preset cycle.
    LegacyCInputPolicy3DS legacy{}; legacy.zl=legacy.zr=true;
    for(bool race:{false,true}) { legacy.raceControls=race; assert(MapLegacyCInput(legacy)==0); }
    legacy={}; legacy.cstickX=71; assert(MapLegacyCInput(legacy)==MK64_N64_CRIGHT);
    legacy.raceControls=true; assert(MapLegacyCInput(legacy)==0);
    assert(ResolveModalDismissAction(true,true)==ModalDismissAction3DS::Continue);
    assert(ResolveModalDismissAction(true,false)==ModalDismissAction3DS::Close);
    for(bool manual:{false,true}) for(auto binding:{MK64_SELECT_ACTION_3DS_CAMERA,MK64_SELECT_ACTION_3DS_DUMP,MK64_SELECT_ACTION_3DS_OVERLAY}) {
        SelectPolicy3DS policy;
        assert(policy.Update(SelectPolicy3DS::kChord,SelectPolicy3DS::kChord,manual,binding)==SelectEvent3DS::DumpChord);
        for(int frame=0;frame<120;++frame) assert(policy.Update(SelectPolicy3DS::kChord,0,manual,binding)==SelectEvent3DS::None);
        policy.Update(0,0,manual,binding);
        assert(policy.Update(SelectPolicy3DS::kChord,SelectPolicy3DS::kSelect,manual,binding)==SelectEvent3DS::DumpChord);
        policy.Update(0,0,manual,binding);
        const auto expected=!manual ? SelectEvent3DS::None : binding==MK64_SELECT_ACTION_3DS_CAMERA ? SelectEvent3DS::Camera : binding==MK64_SELECT_ACTION_3DS_DUMP ? SelectEvent3DS::Dump : SelectEvent3DS::Overlay;
        assert(policy.Update(SelectPolicy3DS::kSelect,SelectPolicy3DS::kSelect,manual,binding)==expected);
        assert(policy.Update(SelectPolicy3DS::kSelect,0,manual,binding)==SelectEvent3DS::None);
    }
    TurboPolicy3DS turbo;
    for(int speed=2;speed<=5;++speed) {
        assert(turbo.Update(speed,false,false,false,true)==speed);
        assert(turbo.Update(speed,false,false,true,false)==1); // Shoulder input cannot latch Turbo.
        assert(turbo.Update(speed,false,false,false,false)==1);
        assert(turbo.Update(speed,true,false,false,true)==1);
        assert(turbo.Update(speed,false,true,false,true)==1);
    }
    std::puts("owner controls: contexts, rear view, Y, unassigned shoulders, SELECT priority/exclusivity/hold and hold-only Turbo: passed");
}
