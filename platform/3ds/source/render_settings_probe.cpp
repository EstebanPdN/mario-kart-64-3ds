#include "settings_3ds.h"
#include "render_policy_3ds.hpp"
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

// Run in an isolated directory containing sdmc:/3ds/MK64. Optional arguments
// check legacy or malformed input before exercising the public settings API.
int main(int argc, char** argv) {
    const bool newModel = std::getenv("MK64_TEST_NEW_MODEL") != nullptr;
    Mk64Settings3DSSetHardwareModel(newModel);
    if (argc == 4) {
        assert(Mk64Settings3DSGetRenderScalePercent() == std::atoi(argv[1]));
        assert(Mk64Settings3DSGetRenderDistance() == std::atoi(argv[2]));
        assert(Mk64Settings3DSGetHudLayout() == std::atoi(argv[3]));
    }
    using namespace mk64_3ds;
    assert(NormalizeRenderScale(LONG_MIN) == 50);
    assert(NormalizeRenderScale(LONG_MAX) == 100);
    for (int n = 0; n <= 255; ++n) {
        Mk64Settings3DSSetRenderScalePercent(n);
        int result = Mk64Settings3DSGetRenderScalePercent();
        assert(result >= 50 && result <= 100 && result % 5 == 0);
    }
    for (int step = 0; step <= 10; ++step) {
        assert(RenderScaleFromTouch(146 + step * 10) == 50 + step * 5);
        Mk64Settings3DSSetRenderScalePercent(50 + step * 5);
        assert(Mk64Settings3DSGetRenderScalePercent() == 50 + step * 5);
        for (int layout = 0; layout < 5; ++layout) {
            Mk64Settings3DSSetHudLayout(static_cast<Mk64HudLayout3DS>(layout));
            assert(Mk64Settings3DSGetTopHudEnabled() == (layout == 4));
            for (int distance = 0; distance < 3; ++distance) {
                Mk64Settings3DSSetRenderDistance(static_cast<Mk64RenderDistance3DS>(distance));
                assert(Mk64Settings3DSSave());
                std::ifstream f("sdmc:/3ds/MK64/mk64-3ds.cfg");
                std::string content((std::istreambuf_iterator<char>(f)), {});
                assert(content.find("render_scale=" + std::to_string(50 + step * 5) + "\n") != std::string::npos);
                constexpr const char* names[] = { "low", "normal", "high" };
                assert(content.find(std::string("render_distance=") + names[distance] + "\n") != std::string::npos);
                constexpr const char* layouts[] = { "clean", "mk7", "mkds", "mkds2", "classic" };
                assert(content.find(std::string("hud_layout=") + layouts[layout] + "\n") != std::string::npos);
            }
        }
    }
    for (float far : { 1500.0f, 2700.0f, 4500.0f, 4800.0f, 5000.0f, 7000.0f }) {
        assert(RenderDistanceEnd(far, 2) == 0);
        assert(RenderDistanceEnd(far, 0) < RenderDistanceEnd(far, 1));
        const float end = RenderDistanceEnd(far, 1);
        assert(CullDistantTriangle(end+1,end+1,end+1,end,false,true));
        assert(!CullDistantTriangle(end-1,end+1,end+1,end,false,true));
        assert(!CullDistantTriangle(end+1,end+1,end+1,end,true,true));
        assert(!CullDistantTriangle(end+1,end+1,end+1,end,false,false));
        assert(!CullDistantTriangle(end+1,end+1,end+1,0,false,true));
    }
    assert(RenderDistanceEnd(NAN, 0) == 0);
    assert(RenderDistanceEnd(INFINITY, 0) == 0);
    assert(!CullDistantTriangle(NAN,3000,3000,2000,false,true));
    Mk64Settings3DSResetDefaults();
    assert(Mk64Settings3DSGetRenderScalePercent() == (newModel ? 100 : 75));
    assert(Mk64Settings3DSGetRenderDistance() == (newModel ? MK64_RENDER_DISTANCE_3DS_NORMAL : MK64_RENDER_DISTANCE_3DS_LOW));
    assert(Mk64Settings3DSGetHudLayout() == MK64_HUD_LAYOUT_3DS_CLEAN);
    std::puts("render settings, migration, persistence and conservative distance culling: OK");
}
