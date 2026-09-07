#include "hud_layout_3ds.hpp"
#include <cassert>
#include <initializer_list>
#include <cstdio>
using mk64_3ds::HudRect;
static bool overlap(HudRect a, HudRect b) {
    return a.x < b.x+b.width && b.x < a.x+a.width && a.y < b.y+b.height && b.y < a.y+a.height;
}
static bool inside(HudRect a, float width) {
    return a.x>=0 && a.y>=0 && a.x+a.width<=width && a.y+a.height<=240;
}
int main() {
    for (bool finished : {false,true}) {
        for (size_t i=0;i<4;++i) {
            auto a=mk64_3ds::BottomStandingRect(i,finished);
            assert(inside(a,320));
            assert(!overlap(a,{165,48,149,184}));
            assert(!overlap(a,{0,0,320,40}));
            for(size_t j=0;j<i;++j) assert(!overlap(a,mk64_3ds::BottomStandingRect(j,finished)));
        }
    }
    for(bool original : {false,true}) for(int layout=1;layout<=4;++layout) {
        auto item=mk64_3ds::TopItemRect(layout,original);
        auto place=mk64_3ds::TopPlaceRect(layout,original);
        assert(inside(item,400) && inside(place,400));
        assert(!overlap(item,place));
        assert(!overlap(place,{315,mk64_3ds::TopHudFpsY(layout),77,12}));
        assert(!overlap(item,{315,mk64_3ds::TopHudFpsY(layout),77,12}));
        if(layout==2) assert(!overlap(item,{(original?352.0f:392.0f)-94,8,94,21}));
    }
    assert(mk64_3ds::TopHudFpsY(3)==6);
    std::puts("HUD portrait spacing/results, top bounds, item/place/FPS separation: passed");
}
