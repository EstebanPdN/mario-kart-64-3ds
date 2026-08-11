#include "input_policy_3ds.hpp"

#include <cassert>
#include <cstdio>

namespace {

using mk64_3ds::LegacyCInputPolicy3DS;
using mk64_3ds::MapLegacyCInput;
using mk64_3ds::MapPrimaryInput;
using mk64_3ds::NextHiddenTopHudMode;
using mk64_3ds::PrimaryInputPolicy3DS;
using mk64_3ds::ShouldCycleHiddenTopHud;

void CheckMenuAndFilteredInput() {
    PrimaryInputPolicy3DS menu = {};
    menu.a = true;
    menu.b = true;
    menu.x = true;
    menu.y = true;
    menu.start = true;
    menu.l = true;
    menu.r = true;
    menu.dUp = true;
    menu.dDown = true;
    menu.dLeft = true;
    menu.dRight = true;

    constexpr std::uint16_t kExpected =
        MK64_N64_A | MK64_N64_B | MK64_N64_CUP | MK64_N64_CLEFT |
        MK64_N64_START | MK64_N64_L | MK64_N64_R | MK64_N64_DUP |
        MK64_N64_DDOWN | MK64_N64_DLEFT | MK64_N64_DRIGHT;
    assert(MapPrimaryInput(menu) == kExpected);
    assert(!ShouldCycleHiddenTopHud(false, true, true));

    // A modal bottom-screen panel filters every game key before this policy.
    const PrimaryInputPolicy3DS filtered = {};
    const LegacyCInputPolicy3DS filteredC = {};
    assert(MapPrimaryInput(filtered) == 0);
    assert(MapLegacyCInput(filteredC) == 0);
    assert(!ShouldCycleHiddenTopHud(true, false, false));
}

void CheckRaceControls() {
    PrimaryInputPolicy3DS race = {};
    race.a = true;
    race.b = true;
    race.x = true;
    race.l = true;
    race.r = true;
    race.raceControls = true;
    race.topHudRenderMode = MK64_TOP_HUD_RENDER_NONE;

    assert(MapPrimaryInput(race) ==
           (MK64_N64_A | MK64_N64_B | MK64_N64_CUP |
            MK64_N64_CDOWN | MK64_N64_R));

    LegacyCInputPolicy3DS suppressed = {};
    suppressed.zl = true;
    suppressed.zr = true;
    suppressed.cstickX = 127;
    suppressed.cstickY = -127;
    suppressed.raceControls = true;
    assert(MapLegacyCInput(suppressed) == 0);

    // ZL, ZR, and all four C-Stick directions stay non-digital in a race.
    suppressed.zr = false;
    suppressed.cstickX = 0;
    suppressed.cstickY = 0;
    assert(MapLegacyCInput(suppressed) == 0);
    suppressed.zl = false;
    suppressed.zr = true;
    assert(MapLegacyCInput(suppressed) == 0);
    suppressed.zr = false;
    suppressed.cstickX = -127;
    assert(MapLegacyCInput(suppressed) == 0);
    suppressed.cstickX = 0;
    suppressed.cstickY = 127;
    assert(MapLegacyCInput(suppressed) == 0);
}

void CheckHudPolicy() {
    PrimaryInputPolicy3DS fullHud = {};
    fullHud.y = true;
    fullHud.raceControls = true;
    fullHud.topHudRenderMode = MK64_TOP_HUD_RENDER_FULL;
    assert(MapPrimaryInput(fullHud) == MK64_N64_CRIGHT);
    assert(ShouldCycleHiddenTopHud(true, true, true));
    assert(!ShouldCycleHiddenTopHud(true, true, false));

    PrimaryInputPolicy3DS hiddenHud = fullHud;
    hiddenHud.topHudRenderMode = MK64_TOP_HUD_RENDER_NONE;
    assert(MapPrimaryInput(hiddenHud) == 0);

    auto mode = MK64_TOP_HUD_RENDER_NONE;
    mode = NextHiddenTopHudMode(mode);
    assert(mode == MK64_TOP_HUD_RENDER_LAP_PROGRESS);
    mode = NextHiddenTopHudMode(mode);
    assert(mode == MK64_TOP_HUD_RENDER_SPEEDOMETER);
    mode = NextHiddenTopHudMode(mode);
    assert(mode == MK64_TOP_HUD_RENDER_NONE);
}

void CheckLegacyCButtons() {
    LegacyCInputPolicy3DS legacy = {};
    legacy.zl = true;
    assert(MapLegacyCInput(legacy) == MK64_N64_CDOWN);
    legacy = {};
    legacy.zr = true;
    assert(MapLegacyCInput(legacy) == MK64_N64_CRIGHT);
    legacy = {};
    legacy.cstickY = -71;
    assert(MapLegacyCInput(legacy) == MK64_N64_CDOWN);
    legacy.cstickY = 71;
    assert(MapLegacyCInput(legacy) == MK64_N64_CUP);
    legacy = {};
    legacy.cstickX = -71;
    assert(MapLegacyCInput(legacy) == MK64_N64_CLEFT);
    legacy.cstickX = 71;
    assert(MapLegacyCInput(legacy) == MK64_N64_CRIGHT);

    legacy.cstickX = 70;
    assert(MapLegacyCInput(legacy) == 0);
    legacy.cstickX = -70;
    assert(MapLegacyCInput(legacy) == 0);
}

} // namespace

int main() {
    CheckMenuAndFilteredInput();
    CheckRaceControls();
    CheckHudPolicy();
    CheckLegacyCButtons();
    std::puts("3DS input policy: ok");
    return 0;
}
