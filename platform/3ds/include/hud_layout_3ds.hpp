#pragma once
#include <cstddef>

namespace mk64_3ds {
struct HudRect { float x, y, width, height; };
// Standings have a screen-owned layout: vanilla animation coordinates assume
// smaller portraits and move to a horizontal strip at the finish line.
constexpr HudRect BottomStandingRect(std::size_t rank, bool finished) {
    return finished ? HudRect{18.0f + (rank % 2) * 60.0f, 64.0f + (rank / 2) * 60.0f, 48, 48}
                    : HudRect{18, 48.0f + rank * 46.0f, 40, 40};
}
constexpr float TopHudFpsY(int layout) {
    return layout == 1 ? 54.0f : (layout == 2 || layout == 4 ? 38.0f : 6.0f);
}
constexpr HudRect TopItemRect(int layout, bool original) {
    return {layout == 4 ? 176.0f : (original ? 48.0f : 8.0f), 8, 48, 38.4f};
}
constexpr HudRect TopPlaceRect(int layout, bool original) {
    const float left = original ? 48.0f : 8.0f;
    return {layout == 1 ? 400.0f - left - 80.0f : left, layout == 1 ? 8.0f : 190.0f, 80, 40};
}
}
