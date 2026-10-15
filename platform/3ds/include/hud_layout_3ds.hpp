#pragma once
#include <cstddef>
#include <cstdint>
#include "settings_3ds.h"

namespace mk64_3ds {
struct HudRect { float x, y, width, height; };
constexpr bool HudMetersAvailable(Mk64HudLayout3DS layout, Mk64HudMeters3DS meters) {
    return meters == MK64_HUD_METERS_3DS_OFF || layout == MK64_HUD_LAYOUT_3DS_CLEAN ||
        (meters == MK64_HUD_METERS_3DS_SPEED && layout != MK64_HUD_LAYOUT_3DS_CLASSIC);
}
// TIME (32), label/digit gap (2), eight native 8-pixel glyphs. Both ends
// of the lower-screen top row retain the LAP label's 13-pixel safe margin.
constexpr float BottomTimerX() { return 197.0f; }
constexpr float kBottomMinimapAreaWidth = 149.0f;
constexpr float kBottomMinimapAreaHeight = 184.0f;
constexpr float BottomMinimapScale(float width, float height,
                                   float areaWidth = kBottomMinimapAreaWidth,
                                   float areaHeight = kBottomMinimapAreaHeight) {
    if (width <= 0 || height <= 0) return 0;
    const float xScale = areaWidth / width, yScale = areaHeight / height;
    return xScale < yScale ? xScale : yScale;
}
// Retain the owner's original four-slot race column and 2x2 finish grid.
constexpr HudRect BottomStandingRect(std::size_t rank, bool finished = false) {
    return finished ? HudRect{18.0f+(rank%2)*60.0f,64.0f+(rank/2)*60.0f,48,48}
                    : HudRect{18.0f,48.0f+rank*46.0f,40,40};
}
// Same 16-visible/8-hidden logical-tick cadence as PUSH START BUTTON.
constexpr bool AutoRaceLabelVisible(uint32_t tick) { return (tick / 8) % 3 != 0; }
constexpr bool MinimapLeaderWhite(uint32_t tick) { return tick % 7 < 3; }
constexpr bool MinimapLeaderFlashing(int rank, int mode, uint32_t tick) {
    return rank == 0 && mode != 1 && MinimapLeaderWhite(tick); // No TT ranking.
}
constexpr uint32_t WhiteMinimapPixel(uint32_t pixel) {
    return (pixel & 0x00ffffffU) && (pixel >> 24)
        ? (pixel & 0xff000000U) | 0x00ffffffU : pixel;
}
constexpr float kMinimapDotSize = 8.0f * 1.2f;
// Presentation-only, one turn per 15 simulation ticks (half a second).
// After damage ends, complete the current turn rather than snapping upright.
// Keep this state per driver, not per rank or selected camera.
struct PortraitSpin3DS {
    unsigned phase = 0;
    bool active = false;
    void Tick(bool hit) {
        if (!hit && phase == 0) { active = false; return; }
        if (!hit && !active) return;
        active = true;
        phase = (phase + 1) % 15;
        if (!hit && phase == 0) active = false;
    }
    uint16_t Angle() const { return static_cast<uint16_t>(phase * 65536U / 15); }
};
constexpr float TopHudFpsY(int layout, int meters = 0) {
    if (layout == 0 && meters == 3) return 38.0f;
    return layout == 1 ? 54.0f : (layout == 2 || layout == 4 ? 38.0f : 6.0f);
}
constexpr HudRect TopItemRect(int layout, bool original) {
    return {layout == 4 ? 176.0f : (original ? 48.0f : 8.0f), 8, 48, 38.4f};
}
constexpr HudRect TopPlaceRect(int layout, bool original) {
    const float left = original ? 48.0f : 8.0f;
    return {layout == 1 ? 400.0f - left - 80.0f : left, layout == 1 ? 8.0f : 190.0f, 80, 40};
}
// Native single-player item motion is slideItemBoxY 0 -> 64 -> 0 in
// four-pixel steps; the texture is 40x32. Keep each layout's settled rectangle.
constexpr HudRect ItemSlideRect(HudRect box, float nativeSlideY) {
    box.y += (nativeSlideY - 64.0f) * (box.height / 32.0f);
    return box;
}
// Native place scale is .5 while racing and grows to 1 after finishing.
// Grow inward from the layout's screen corner, without moving its resting box
// or letting the enlarged result extend past the bottom/right screen edge.
constexpr HudRect PlaceScaleRect(HudRect box, float nativeScale) {
    const float scale = nativeScale > 0 ? nativeScale / .5f : 1.0f;
    const float width = box.width * scale, height = box.height * scale;
    if (box.x + box.width * .5f > 200.0f) box.x += box.width - width;
    if (box.y + box.height * .5f > 120.0f) box.y += box.height - height;
    box.width = width;
    box.height = height;
    return box;
}

// CPUs have no native item-window/viewport HUD. Mirror only the native
// presentation timings for AUTO, never its inventory or gameplay state.
struct AutoHudMotion {
    float itemSlideY = 0;
    float placeScale = .5f;
    unsigned emptyTicks = 0;
    bool itemVisible = false, wasFinished = false;
    void Tick(bool hasItem, bool finished) {
        if (finished && wasFinished && placeScale < 1.0f) placeScale += .125f;
        if (!finished) placeScale = .5f;
        wasFinished = finished;
        // Lower HUD remains live after finishing. Only the upper draw path
        // suppresses the item window; do not clear a CPU's presentation here.
        if (hasItem) {
            itemVisible = true;
            emptyTicks = 20;
            if (itemSlideY < 64) itemSlideY += 4;
        } else if (itemVisible) {
            if (emptyTicks) --emptyTicks;
            else {
                if (itemSlideY > 0) itemSlideY -= 4;
                if (itemSlideY == 0) itemVisible = false;
            }
        }
    }
};
}
