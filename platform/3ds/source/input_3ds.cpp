#include "input_3ds.h"
#include "diagnostics_3ds.h"

#include <3ds.h>

#include <algorithm>

extern "C" bool Mk64Diagnostics3DSOwnsHid(void) __attribute__((weak));
extern "C" bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS*) __attribute__((weak));
extern "C" uint32_t Mk64BottomUI3DSFilterGameKeys(uint32_t) __attribute__((weak));
extern "C" bool Mk64BottomUI3DSConsumesCStick(void) __attribute__((weak));
extern "C" bool Mk64BottomUI3DSIsModalOpen(void) __attribute__((weak));

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
