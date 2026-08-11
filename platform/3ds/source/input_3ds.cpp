#include "input_3ds.h"
#include "diagnostics_3ds.h"
#include "input_policy_3ds.hpp"

#include <3ds.h>

#include <algorithm>

extern "C" bool Mk64Diagnostics3DSOwnsHid(void) __attribute__((weak));
extern "C" bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS*) __attribute__((weak));
extern "C" uint32_t Mk64BottomUI3DSFilterGameKeys(uint32_t) __attribute__((weak));
extern "C" bool Mk64BottomUI3DSConsumesCStick(void) __attribute__((weak));
extern "C" bool Mk64BottomUI3DSIsModalOpen(void) __attribute__((weak));
extern "C" bool Mk64GameState3DSRaceControlsActive(void) __attribute__((weak));
extern "C" bool Mk64GameState3DSCycleHiddenTopHud(void) __attribute__((weak));
extern "C" int Mk64GameState3DSGetTopHudRenderMode(void) __attribute__((weak));

namespace {

constexpr int kCirclePadRange = 156;
constexpr int kN64StickRange = 80;
constexpr int kCircleDeadzone = 12;
u32 sPreviousFilteredKeys = 0;

int8_t ScaleStickAxis(int value) {
    value = std::clamp(value, -kCirclePadRange, kCirclePadRange);
    if (value > -kCircleDeadzone && value < kCircleDeadzone) {
        return 0;
    }
    return static_cast<int8_t>(value * kN64StickRange / kCirclePadRange);
}

} // namespace

extern "C" void Mk64Input3DSInit(void) {
    sPreviousFilteredKeys = 0;
    if (Mk64Diagnostics3DSOwnsHid == nullptr || !Mk64Diagnostics3DSOwnsHid()) {
        hidScanInput();
    }
}

extern "C" void Mk64Input3DSPoll(Mk64Pad3DS* pad) {
    if (pad == nullptr) {
        return;
    }

    *pad = {};
    Mk64DiagnosticsInput3DS diagnosticInput = {};
    const bool diagnosticReady = Mk64Diagnostics3DSReadInput != nullptr &&
                                 Mk64Diagnostics3DSReadInput(&diagnosticInput);
    if (!diagnosticReady) hidScanInput();
    u32 keys = diagnosticReady ? diagnosticInput.heldMask : hidKeysHeld();
    if (Mk64BottomUI3DSFilterGameKeys != nullptr) {
        keys = Mk64BottomUI3DSFilterGameKeys(keys);
    }
    const u32 pressedKeys = keys & ~sPreviousFilteredKeys;
    sPreviousFilteredKeys = keys;
    const bool raceControls = Mk64GameState3DSRaceControlsActive != nullptr &&
                              Mk64GameState3DSRaceControlsActive();

    const bool yHeld = (keys & KEY_Y) != 0;
    const bool yPressed = (pressedKeys & KEY_Y) != 0;
    if (mk64_3ds::ShouldCycleHiddenTopHud(raceControls, yHeld, yPressed) &&
        Mk64GameState3DSCycleHiddenTopHud != nullptr) {
        Mk64GameState3DSCycleHiddenTopHud();
    }
    const int topHudMode = raceControls && yHeld &&
                                   Mk64GameState3DSGetTopHudRenderMode != nullptr
                               ? Mk64GameState3DSGetTopHudRenderMode()
                               : MK64_TOP_HUD_RENDER_FULL;
    mk64_3ds::PrimaryInputPolicy3DS primaryInput;
    primaryInput.a = (keys & KEY_A) != 0;
    primaryInput.b = (keys & KEY_B) != 0;
    primaryInput.x = (keys & KEY_X) != 0;
    primaryInput.y = yHeld;
    primaryInput.start = (keys & KEY_START) != 0;
    primaryInput.l = (keys & KEY_L) != 0;
    primaryInput.r = (keys & KEY_R) != 0;
    primaryInput.dUp = (keys & KEY_DUP) != 0;
    primaryInput.dDown = (keys & KEY_DDOWN) != 0;
    primaryInput.dLeft = (keys & KEY_DLEFT) != 0;
    primaryInput.dRight = (keys & KEY_DRIGHT) != 0;
    primaryInput.raceControls = raceControls;
    primaryInput.topHudRenderMode = topHudMode;
    pad->buttons |= mk64_3ds::MapPrimaryInput(primaryInput);

    circlePosition circle = {};
    if (diagnosticReady) {
        circle.dx = diagnosticInput.circleX;
        circle.dy = diagnosticInput.circleY;
    } else {
        hidCircleRead(&circle);
    }
    const bool modalInputCapture = Mk64BottomUI3DSIsModalOpen != nullptr &&
                                   Mk64BottomUI3DSIsModalOpen();
    if (modalInputCapture) {
        circle = {};
    }
    pad->stickX = ScaleStickAxis(circle.dx);
    pad->stickY = ScaleStickAxis(circle.dy);

    circlePosition cstick = {};
    if (diagnosticReady) {
        cstick.dx = diagnosticInput.cstickX;
        cstick.dy = diagnosticInput.cstickY;
    } else {
        hidCstickRead(&cstick);
    }
    const bool consumeCStick = modalInputCapture ||
        (Mk64BottomUI3DSConsumesCStick != nullptr && Mk64BottomUI3DSConsumesCStick());
    if (consumeCStick) {
        cstick = {};
    }
    pad->rightStickX = ScaleStickAxis(cstick.dx);
    pad->rightStickY = ScaleStickAxis(cstick.dy);

    mk64_3ds::LegacyCInputPolicy3DS legacyCInput;
    legacyCInput.zl = (keys & KEY_ZL) != 0;
    legacyCInput.zr = (keys & KEY_ZR) != 0;
    legacyCInput.cstickX = cstick.dx;
    legacyCInput.cstickY = cstick.dy;
    legacyCInput.raceControls = raceControls;
    pad->buttons |= mk64_3ds::MapLegacyCInput(legacyCInput);
}
