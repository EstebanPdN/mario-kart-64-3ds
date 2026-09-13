#pragma once

#include <cstdint>

namespace mk64_3ds {
struct MenuTransition3DS {
    bool blankBackdrop = true;
    uint8_t shade = 0;
    uint8_t alpha = 0;
};

// Stock full-screen transitions: 1/2 are from/to black, 8/7 from/to white.
// Keep Nintendo's lower backdrop black underneath its outgoing white fade.
inline MenuTransition3DS ResolveMenuTransition(bool logo, bool title, int type,
                                               uint32_t time, uint32_t duration) {
    MenuTransition3DS result{logo, 0, 0};
    if ((!logo && !title) || !duration) return result;
    const bool entering = type == 1 || type == 8;
    const bool leaving = type == 2 || type == 7;
    if (!entering && !leaving) return result;
    const uint64_t progress = static_cast<uint64_t>(time) * 255 / duration;
    const unsigned ramp = progress > 255 ? 255 : static_cast<unsigned>(progress);
    result.shade = type == 7 || type == 8 ? 255 : 0;
    result.alpha = static_cast<uint8_t>(entering ? 255 - ramp : ramp);
    return result;
}
} // namespace mk64_3ds
