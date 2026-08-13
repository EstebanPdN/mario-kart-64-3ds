#pragma once

#include "game_state_3ds.h"
#include "input_3ds.h"

#include <cstdint>

namespace mk64_3ds {

constexpr int kCStickDigitalButtonThreshold = 70;

struct PrimaryInputPolicy3DS {
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool start = false;
    bool l = false;
    bool r = false;
    bool dUp = false;
    bool dDown = false;
    bool dLeft = false;
    bool dRight = false;
    bool raceControls = false;
    int topHudRenderMode = MK64_TOP_HUD_RENDER_FULL;
};

struct LegacyCInputPolicy3DS {
    bool zl = false;
    bool zr = false;
    int cstickX = 0;
    int cstickY = 0;
    bool raceControls = false;
};

enum class ModalDismissAction3DS : std::uint8_t {
    None,
    Close,
    Continue,
};

constexpr ModalDismissAction3DS ResolveModalDismissAction(bool dismissPressed,
                                                           bool openedFromPause) noexcept {
    if (!dismissPressed) return ModalDismissAction3DS::None;
    return openedFromPause ? ModalDismissAction3DS::Continue
                           : ModalDismissAction3DS::Close;
}

constexpr bool ShouldCycleHiddenTopHud(bool raceControls, bool yHeld,
                                       bool yPressed) noexcept {
    return raceControls && yHeld && yPressed;
}

constexpr std::uint16_t MapPrimaryInput(const PrimaryInputPolicy3DS& input) noexcept {
    std::uint16_t buttons = 0;
    if (input.a) buttons |= MK64_N64_A;
    if (input.b) buttons |= MK64_N64_B;
    if (input.x) buttons |= MK64_N64_CUP;
    if (input.y) {
        if (!input.raceControls) {
            buttons |= MK64_N64_CLEFT;
        } else if (input.topHudRenderMode == MK64_TOP_HUD_RENDER_FULL) {
            buttons |= MK64_N64_CRIGHT;
        }
    }
    if (input.start) buttons |= MK64_N64_START;
    if (input.l) buttons |= input.raceControls ? MK64_N64_CDOWN : MK64_N64_L;
    if (input.r) buttons |= MK64_N64_R;
    if (input.dUp) buttons |= MK64_N64_DUP;
    if (input.dDown) buttons |= MK64_N64_DDOWN;
    if (input.dLeft) buttons |= MK64_N64_DLEFT;
    if (input.dRight) buttons |= MK64_N64_DRIGHT;
    return buttons;
}

constexpr std::uint16_t MapLegacyCInput(const LegacyCInputPolicy3DS& input) noexcept {
    if (input.raceControls) return 0;

    std::uint16_t buttons = 0;
    if (input.zl || input.cstickY < -kCStickDigitalButtonThreshold) {
        buttons |= MK64_N64_CDOWN;
    }
    if (input.zr || input.cstickX > kCStickDigitalButtonThreshold) {
        buttons |= MK64_N64_CRIGHT;
    }
    if (input.cstickY > kCStickDigitalButtonThreshold) {
        buttons |= MK64_N64_CUP;
    }
    if (input.cstickX < -kCStickDigitalButtonThreshold) {
        buttons |= MK64_N64_CLEFT;
    }
    return buttons;
}

constexpr Mk64TopHudRenderMode3DS NextHiddenTopHudMode(
    Mk64TopHudRenderMode3DS mode) noexcept {
    return static_cast<Mk64TopHudRenderMode3DS>((static_cast<int>(mode) + 1) % 4);
}

} // namespace mk64_3ds
