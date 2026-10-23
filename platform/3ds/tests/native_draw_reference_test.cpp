#include "native_draw_reference_3ds.hpp"
#include "native_anchor_queue_3ds.hpp"
#include <array>
#include <cassert>
#include <vector>
using namespace mk64_3ds;
int main() {
    NativeAnchorQueue<NativeDrawReference,320,1024> queue;
    std::array<std::vector<NativeDrawReference>,3> expected;
    unsigned characters=0,objects=0;
    for(unsigned i=0;i<320;++i) {
        NativeDrawReference r=i%5==0?NativeDrawReference{NativeDrawFamily::Character,uint16_t(characters++)}:
            NativeDrawReference{NativeDrawFamily::Vegetation,uint16_t(objects++)};
        const unsigned group=i%3;assert(queue.Push(0x100+group*8,r));expected[group].push_back(r);
    }
    assert(characters==64 && objects==256 && !queue.Push(0x100,{NativeDrawFamily::Vegetation,0}));
    for(unsigned replay=0;replay<3;++replay) {
        queue.BeginReplay();assert(!queue.Consume(0x9000,[](auto){assert(false);}));
        for(unsigned group:{2u,0u,1u}) {
            unsigned at=0;assert(queue.Consume(0x100+group*8,[&](auto value){
                const auto& ref=expected[group][at++];assert(ref.family==value.family && ref.index==value.index);
            }));assert(at==expected[group].size());
        }
        assert(queue.Remaining()==0);
    }
    queue.Reset();assert(queue.Push(0x100,{NativeDrawFamily::Vegetation,11}));queue.BeginReplay();
    assert(queue.Consume(0x100,[](auto r){assert(r.family==NativeDrawFamily::Vegetation && r.index==11);}));
    NativeAnchorQueue<NativeDrawReference,832,2048> extended;
    std::array<std::vector<NativeDrawReference>,5> ordered;
    unsigned counts[3]{};
    for(unsigned i=0;i<832;++i){
        const unsigned family=i%13==0?0:(i%13<9?1:2);
        NativeDrawReference r{static_cast<NativeDrawFamily>(family),uint16_t(counts[family]++)};
        assert(extended.Push(0x800+i%5*8,r));ordered[i%5].push_back(r);
    }
    assert(counts[0]==64 && counts[1]==512 && counts[2]==256);
    assert(!extended.Push(0x800,{NativeDrawFamily::ObjectState,0}));
    for(unsigned replay=0;replay<3;++replay){extended.BeginReplay();
        for(unsigned group:{4u,1u,3u,0u,2u}){unsigned at=0;
            assert(extended.Consume(0x800+group*8,[&](auto r){const auto e=ordered[group][at++];assert(r.family==e.family && r.index==e.index);}));
            assert(at==ordered[group].size());
        }
        assert(!extended.Remaining());
    }
    NativeAnchorQueue<NativeDrawReference,1344,4096> particles;
    std::array<std::vector<NativeDrawReference>,7> mixed;
    unsigned familyCounts[4]{};
    for(unsigned i=0;i<1344;++i) {
        const unsigned at=i%21,family=at==0?0:(at<9?1:(at<13?2:3));
        const NativeDrawReference r{static_cast<NativeDrawFamily>(family),uint16_t(familyCounts[family]++)};
        assert(particles.Push(0x1000+i%7*8,r));mixed[i%7].push_back(r);
    }
    assert(familyCounts[0]==64 && familyCounts[1]==512 && familyCounts[2]==256 && familyCounts[3]==512);
    assert(!particles.Push(0x1000,{NativeDrawFamily::Particle,0}));
    for(unsigned replay=0;replay<3;++replay){particles.BeginReplay();
        for(unsigned group:{6u,2u,0u,3u,1u,5u,4u}){unsigned at=0;
            assert(particles.Consume(0x1000+group*8,[&](auto r){const auto e=mixed[group][at++];assert(r.family==e.family && r.index==e.index);}));
            assert(at==mixed[group].size());
            assert(!particles.Consume(0x1000+group*8,[](auto){assert(false);}));
        }
        assert(!particles.Remaining());
    }
}
