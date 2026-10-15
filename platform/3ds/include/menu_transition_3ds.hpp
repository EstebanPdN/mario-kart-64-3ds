#pragma once

#include <cstdint>
#include <algorithm>

namespace mk64_3ds {
// Credits reload each course through the native sentinel 255 with next=9.
// UI snapshots precede dispatch, so neither the live state nor the snapshot
// alone covers every load/first-image boundary. Do not blank ordinary loads.
inline bool CreditsBottomBlank(int liveState, int nextState, int snapshotState) {
    return liveState == 9 || snapshotState == 9 || (liveState == 255 && nextState == 9);
}

// Native menu items enter by sliding from column 350 and leave/return using
// param1 (0..32 horizontal collapse). Logical selection changes before that
// animation ends; it must not be used as a binary drawing switch.
inline float GameSelectReveal(bool visible, int collapse, int column, int destination) {
    if (!visible) return 0.0f;
    const float width = 1.0f-std::clamp(collapse, 0, 32)/32.0f;
    const float slide = 1.0f-std::clamp((column-destination)/float(350-destination), 0.0f, 1.0f);
    return width*slide;
}

struct MenuTransition3DS {
    bool blankBackdrop = true;
    uint8_t shade = 0;
    uint8_t alpha = 0;
};

struct RaceTransition3DS {
    float opening = 0.0f;
    uint8_t exitAlpha = 0;
    void SetOpening(float reveal) {
        opening = std::clamp(reveal, 0.0f, 1.0f);
        exitAlpha = 0; // New race/retry; never inherit the previous black hold.
    }
    void SetExitSlide(int pixels) {
        opening = 1.0f;
        exitAlpha = static_cast<uint8_t>(std::clamp(pixels, 0, 240) * 255 / 240);
    }
    void SetExitFade(int type, uint32_t time, uint32_t duration) {
        // Failed-GP Retry / pause Quit use the native whole-screen fade,
        // not the successful-results two-window slide. Keep its black hold.
        if (type != 2 && type != 5) return;
        if (type == 2 && !duration) return;
        opening = 1.0f;
        exitAlpha = type == 5 ? 255 : static_cast<uint8_t>(
            std::min<uint64_t>(static_cast<uint64_t>(time) * 255 / duration, 255));
    }
    float BarHeight() const { return 120.0f * (1.0f - opening); }
    float TopHudOpacity(float base) const {
        return std::clamp(base, 0.0f, 1.0f) * (255-exitAlpha) / 255.0f;
    }
};

// Observe the stock timer BEFORE its drawing function advances it. All menu
// pages participate, including Options/Data and both ends of a back transition.
inline MenuTransition3DS ResolveMenuTransition(bool logo, bool menu, int type,
                                               uint32_t time, uint32_t duration) {
    MenuTransition3DS result{logo, 0, 0};
    if (!menu) return result;
    if (type == 5) { result.alpha = 255; return result; }
    if (!duration) return result;
    const bool entering = type == 1 || type == 8;
    const bool leaving = type == 2 || type == 3 || type == 4 || type == 7;
    if (!entering && !leaving) return result;
    // 3/4 advance before drawing, unlike 1/2/7/8. Match their shade ramp;
    // the upper screen alone retains the native type-3 mosaic overlay.
    const uint64_t drawTime = static_cast<uint64_t>(time) + (type == 3 || type == 4);
    const uint64_t progress = drawTime * 255 / duration;
    const unsigned ramp = progress > 255 ? 255 : static_cast<unsigned>(progress);
    result.shade = type == 7 || type == 8 ? 255 : 0;
    result.alpha = static_cast<uint8_t>(entering ? 255 - ramp : ramp);
    return result;
}
} // namespace mk64_3ds
