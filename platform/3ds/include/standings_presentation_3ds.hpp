#pragma once
#include <array>
#include <algorithm>
#include "game_state_3ds.h"

namespace mk64_3ds {
// A small cache key, not a second ranking system. Only changed presentation
// invalidates the retained lower image; the ordinary map refresh stays capped.
struct StandingsPresentation3DS {
    std::array<int8_t, MK64_BOTTOM_UI_STANDING_COUNT> ids{}, characters{};
    std::array<bool, MK64_BOTTOM_UI_STANDING_COUNT> unknown{};
    uint8_t count, alpha, red, green, blue;
    int8_t observed;
    explicit StandingsPresentation3DS(const Mk64BottomUIGameState3DS& state)
        : count(std::min<unsigned>(4,state.standingCount)), alpha(state.standingAlpha),
          red(state.playerBorderRed), green(state.playerBorderGreen),
          blue(state.playerBorderBlue), observed(state.observedPlayer) {
        for (unsigned i = 0; i < count; ++i) {
            ids[i] = state.standingPlayerIds[i];
            characters[i] = state.standingCharacterIds[i];
            unknown[i] = state.standingUnknown[i];
        }
    }
    bool Changed(const Mk64BottomUIGameState3DS& state) const {
        if (count != std::min<unsigned>(4,state.standingCount) || observed != state.observedPlayer ||
            alpha != state.standingAlpha || red != state.playerBorderRed ||
            green != state.playerBorderGreen || blue != state.playerBorderBlue) return true;
        for (unsigned i = 0; i < count; ++i)
            if (ids[i] != state.standingPlayerIds[i] || characters[i] != state.standingCharacterIds[i] ||
                unknown[i] != state.standingUnknown[i]) return true;
        return false;
    }
};
}
