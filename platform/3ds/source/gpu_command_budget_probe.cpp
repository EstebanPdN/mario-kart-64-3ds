#include "gpu_command_budget_3ds.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <vector>
using namespace mk64_3ds;
int main() {
    // The photographed libctru check: two-word command at the old full buffer.
    assert(!GpuCommandRoom(0x10000, 0x10000, 2));
    assert(GpuCommandRoom(kGpuCommandBufferBytes/4, 0x10000, 2));
    // A complete 512 KiB scene exceeds the old capacity but fits with room for UI.
    const auto required=kGpuCommandTailWords+kGpuCommandDrawWords;
    std::vector<unsigned> buffer(kGpuCommandBufferBytes/4+1, 0xa55aa55a);
    unsigned offset=0;
    for(unsigned i=0;i<65536;++i) {
        assert(GpuCommandRoom(buffer.size()-1,offset,required));
        buffer[offset++]=i;buffer[offset++]=0x40080;
    }
    assert(GpuCommandRoom(buffer.size()-1,offset,kGpuCommandTailWords));
    // At sustained pressure, the guard rejects a batch while closure still fits.
    while(GpuCommandRoom(buffer.size()-1,offset,required)) {
        for(unsigned i=0;i<kGpuCommandDrawWords;++i) buffer[offset++]=0;
    }
    assert(GpuCommandRoom(buffer.size()-1,offset,kGpuCommandTailWords));
    for(unsigned i=0;i<kGpuCommandTailWords;++i) buffer[offset++]=0;
    assert(offset<=buffer.size()-1 && buffer.back()==0xa55aa55a);
    // Splitting changes the base and capacity; it does not replenish storage.
    const unsigned used=12345, capacity=kGpuCommandBufferBytes/4;
    for(unsigned next=0;next<capacity-used;next+=137)
        assert(GpuCommandRoom(capacity,used+next,required)==
               GpuCommandRoom(capacity-used,next,required));
    assert(GpuCommandRoom(100,70,30));
    assert(!GpuCommandRoom(100,70,31));
    assert(!GpuCommandRoom(100,101,0));
    assert(!GpuCommandRoom(UINT_MAX,UINT_MAX-1,2));
    assert(GpuCommandRoom(UINT_MAX,UINT_MAX-1,1));
    bool caught=false;
    try { throw GpuCommandPressure(); }
    catch(const std::length_error& e) { caught=dynamic_cast<const GpuCommandPressure*>(&e)!=nullptr; }
    assert(caught);
    puts("PASS: photographed boundary, dense scene, reserved closure, split accounting, overflow and recovery exception");
}
