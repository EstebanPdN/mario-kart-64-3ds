#include "native_sprite_batch_3ds.hpp"
#include "native_anchor_queue_3ds.hpp"
#include <array>
#include <cassert>
#include <vector>
using namespace mk64_3ds::native_scene;
int main() {
    std::array<uint16_t,18> indices{};
    assert(SpriteIndices(0,1,indices.data(),indices.size())==6);
    assert(SpriteIndices(4,2,indices.data()+6,12)==12);
    assert((indices==std::array<uint16_t,18>{0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11}));
    const auto saved=indices;
    assert(!SpriteIndices(65533,1,indices.data(),18));
    assert(!SpriteIndices(0,3,indices.data(),18));
    assert(!SpriteIndices(0,2,indices.data(),11));
    assert(!SpriteIndices(0,0,indices.data(),18));
    assert(indices==saved);
    assert(SpriteIndices(65528,2,indices.data(),18)==12 && indices[11]==65535);
    SpriteStyle kart{},other{};
    assert(SameSpriteMaterial(kart,other));
    other.quads=1;assert(SameSpriteMaterial(kart,other)); // Topology is not a material state.
    other.primitive[3]=128;assert(!SameSpriteMaterial(kart,other));
    other=kart;other.environment[1]=64;assert(!SameSpriteMaterial(kart,other));
    for(unsigned field=0;field<4;++field) {
        other=kart;
        if(field==0)other.blend=true;
        if(field==1)other.alphaTest=false;
        if(field==2)other.cutout=false;
        if(field==3)other.cull=2;
        assert(!SameSpriteMaterial(kart,other));
    }
    SpriteStyle shadow=kart;shadow.technique=SpriteStyle::ConstantRgb;
    assert(!SameSpriteMaterial(kart,shadow));other=shadow;other.environment[0]=255;
    assert(SameSpriteMaterial(shadow,other));
    for(auto technique:{SpriteStyle::TextureTint,SpriteStyle::ColorLerp}) {
        SpriteStyle effect;effect.technique=technique;other=effect;
        other.primitive[0]=91;other.primitive[3]=33;
        assert(SameSpriteMaterial(effect,other)); // Per-vertex primitive input.
        other.environment[0]=17;
        assert(SameSpriteMaterial(effect,other)==(technique==SpriteStyle::TextureTint));
    }
    struct Packet {unsigned player;bool shadow;};
    mk64_3ds::NativeAnchorQueue<Packet> queue;
    // Same-anchor batching must preserve each shadow/body pair, including
    // colliding foreign anchors and repeated interpolation replays.
    for(unsigned p=0;p<8;++p) {
        assert(queue.Push(0x100,{p,true}));assert(queue.Push(0x100,{p,false}));
    }
    assert(queue.Push(0x900,{99,false}));
    for(unsigned replay=0;replay<3;++replay) {
        queue.BeginReplay();std::vector<Packet> actual;
        assert(queue.Consume(0x100,[&](auto p){actual.push_back(p);}));
        assert(actual.size()==16 && queue.Remaining()==1);
        for(unsigned i=0;i<16;++i)assert(actual[i].player==i/2 && actual[i].shadow==(i%2==0));
    }
}
