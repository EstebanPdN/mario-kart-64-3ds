#pragma once
#include <array>

namespace mk64_3ds {
struct BattleStandings3DS {
    std::array<int, 4> order{0, 1, 2, 3};
    void Update(const std::array<int, 4>& balloons) {
        // Four-entry stable insertion sort: ties retain the previous ordering.
        // Native -1 means eliminated; zero still has one balloon remaining.
        for (int i = 1; i < 4; ++i) {
            const int id = order[i];
            int j = i;
            while (j && balloons[id] > balloons[order[j - 1]]) {
                order[j] = order[j - 1]; --j;
            }
            order[j] = id;
        }
    }
};
}
