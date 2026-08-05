#include "input_3ds.h"

#include <3ds.h>

#include <algorithm>

namespace {

constexpr int kCirclePadRange = 156;
constexpr int kN64StickRange = 80;
constexpr int kCircleDeadzone = 12;
constexpr int kCStickButtonThreshold = 70;

int8_t ScaleStickAxis(int value) {
    value = std::clamp(value, -kCirclePadRange, kCirclePadRange);
    if (value > -kCircleDeadzone && value < kCircleDeadzone) {
        return 0;
    }
    return static_cast<int8_t>(value * kN64StickRange / kCirclePadRange);
}

} // namespace

extern "C" void Mk64Input3DSInit(void) {
    hidScanInput();
}

extern "C" void Mk64Input3DSPoll(Mk64Pad3DS* pad) {
    if (pad == nullptr) {
        return;
    }

    *pad = {};
    hidScanInput();
    const u32 keys = hidKeysHeld();

    if ((keys & KEY_A) != 0) {
        pad->buttons |= MK64_N64_A;
    }
    if ((keys & KEY_B) != 0) {
        pad->buttons |= MK64_N64_B;
    }
    if ((keys & KEY_X) != 0) {
        pad->buttons |= MK64_N64_CUP;
    }
    if ((keys & KEY_Y) != 0) {
        pad->buttons |= MK64_N64_CLEFT;
    }
    if ((keys & KEY_START) != 0) {
        pad->buttons |= MK64_N64_START;
    }
    if ((keys & KEY_SELECT) != 0) {
        pad->buttons |= MK64_N64_Z;
    }
    if ((keys & KEY_L) != 0) {
        pad->buttons |= MK64_N64_L;
    }
    if ((keys & KEY_R) != 0) {
        pad->buttons |= MK64_N64_R;
    }
    if ((keys & KEY_DUP) != 0) {
        pad->buttons |= MK64_N64_DUP;
    }
    if ((keys & KEY_DDOWN) != 0) {
        pad->buttons |= MK64_N64_DDOWN;
    }
    if ((keys & KEY_DLEFT) != 0) {
        pad->buttons |= MK64_N64_DLEFT;
    }
    if ((keys & KEY_DRIGHT) != 0) {
        pad->buttons |= MK64_N64_DRIGHT;
    }

    circlePosition circle = {};
    hidCircleRead(&circle);
    pad->stickX = ScaleStickAxis(circle.dx);
    pad->stickY = ScaleStickAxis(circle.dy);

    circlePosition cstick = {};
    hidCstickRead(&cstick);
    pad->rightStickX = ScaleStickAxis(cstick.dx);
    pad->rightStickY = ScaleStickAxis(cstick.dy);

    if ((keys & KEY_ZL) != 0 || cstick.dy < -kCStickButtonThreshold) {
        pad->buttons |= MK64_N64_CDOWN;
    }
    if ((keys & KEY_ZR) != 0 || cstick.dx > kCStickButtonThreshold) {
        pad->buttons |= MK64_N64_CRIGHT;
    }
    if (cstick.dy > kCStickButtonThreshold) {
        pad->buttons |= MK64_N64_CUP;
    }
    if (cstick.dx < -kCStickButtonThreshold) {
        pad->buttons |= MK64_N64_CLEFT;
    }
}
