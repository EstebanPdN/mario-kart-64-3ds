#include "arena_navigation_3ds.hpp"
#include "auto_confirm_3ds.hpp"
#include "battle_standings_3ds.hpp"
#include "menu_backdrop_3ds.hpp"
#include "menu_transition_3ds.hpp"
#include "race_clock_3ds.hpp"
#include "random_cup_3ds.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>

int main() {
    using namespace mk64_3ds;
    RaceClock clock;
    clock.Sync(100, true); assert(clock.Read(150) == 50);
    clock.Sync(200, false); assert(clock.Read(9999) == 100);
    clock.Sync(10000, true); assert(clock.Read(10050) == 150);
    clock.SetRate(10100, 5); assert(clock.Read(10200) == 700);
    clock.SetRate(10200, 1); assert(clock.Read(10300) == 800);
    clock.Reset(); clock.Sync(50000, true); assert(clock.Read(50000) == 0);
    AutoConfirmPulse3DS pulse;
    assert(!pulse.Tick(true, 0) && !pulse.Tick(true, 499));
    assert(pulse.Tick(true, 500) && !pulse.Tick(true, 501));
    assert(!pulse.Tick(false, 5000));
    assert(!pulse.Tick(true, 10000) && pulse.Tick(true, 20000));
    assert(!pulse.Tick(true, 20001)); // Loading cannot cause a catch-up burst.
    for (unsigned seed=0; seed<10000; ++seed) {
        RandomCupPlan cup(seed);
        assert(cup.character < 8);
        for (unsigned i=0;i<4;++i) {
            assert(cup.tracks[i] < 19 && cup.tracks[i] != 15 && cup.tracks[i] != 16 && cup.tracks[i] != 17);
            for (unsigned j=0;j<i;++j) assert(cup.tracks[i] != cup.tracks[j]);
        }
    }
    BattleStandings3DS standings;
    standings.Update({-1,0,2,1}); assert((standings.order == std::array<int,4>{2,3,1,0}));
    standings.Update({-1,0,0,0}); assert((standings.order == std::array<int,4>{2,3,1,0}));
    assert(CreditsBottomBlank(255,9,4) && CreditsBottomBlank(4,4,9));
    assert(!CreditsBottomBlank(255,4,3));
    RaceTransition3DS fade; fade.SetExitSlide(240); assert(fade.TopHudOpacity(1)==0);
    fade.SetOpening(0); assert(fade.exitAlpha==0 && fade.BarHeight()==120);
    const auto image=MakeSoftBackdrop(128,96,[](unsigned x,unsigned) { return x < 64 ? 0xff000000U : 0xffffffffU; });
    assert(image[48*128+16] == 0xff000000U && image[48*128+112] == 0xffffffffU);
    assert(image[48*128+63] != image[48*128+16]); // Local smoothing retains distant detail.

    using namespace ArenaNavigation3DS;
    auto graph=std::make_unique<Graph>();
    for (float y : {0.f,80.f}) {
        assert(graph->Add({0,y,0},{200,y,0},{0,y,200}));
        assert(graph->Add({200,y,0},{200,y,200},{0,y,200}));
    }
    graph->Connect();
    assert(graph->count == 4);
    Point floor{};
    const int lower=graph->ProjectFloor({50,3,50},floor); assert(lower>=0 && floor.y==0);
    const int upper=graph->ProjectFloor({50,83,50},floor); assert(upper>=0 && floor.y==80);
    assert(!graph->Reachable(lower,upper)); // Overlapping floors never merge.
    assert(graph->Supported({50,3,50}) && !graph->Supported({50,40,50}));
    assert(graph->SafeSegment({50,0,50},{80,0,70}));
    assert(!graph->SafeSegment({12,0,20},{12,0,40})); // Full body width crosses an edge.
    assert(!graph->SafeSegment({150,0,150},{220,0,150}));
    graph->AddWall({100,0,0},{100,60,0},{100,0,200});
    graph->AddWall({100,60,0},{100,60,200},{100,0,200});
    graph->Connect();
    assert(!graph->ClearWalls({90,0,50},{110,0,50}));
    assert(graph->ClearWalls({90,80,50},{110,80,50})); // Another floor's wall is not a barrier.
    graph->Reset(); assert(graph->count==0 && graph->wallCount==0);
    assert(!graph->Supported({50,0,50}));
    std::puts("race clock, AUTO pulse guards, 10,000 random cups, Battle standings, fades, blur detail and arena floor/body/wall/reset policies: passed");
}
