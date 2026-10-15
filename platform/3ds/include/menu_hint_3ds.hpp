#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>

namespace mk64_3ds {
// Texture dimensions match the generated original artwork. GPU storage is
// owned by bottom_ui_3ds.cpp; this module contains only presentation policy.
constexpr unsigned kRandomHintWidth = 127, kRandomHintHeight = 19;
constexpr unsigned kAutoHintWidth = 81, kAutoHintHeight = 19;
inline uint32_t CleanMenuHintPixel(uint32_t pixel) {
    // Legacy TKMK00 alpha-zero RGB must not bleed into filtered edges.
    return pixel >> 24 ? pixel : 0;
}
struct AutoHintTween {
    float reveal=0, from=0;
    bool target=false;
    uint64_t started=0;
    bool Update(bool available,uint64_t now) {
        const float old=reveal;
        if (available!=target) {from=reveal;target=available;started=now;}
        const float elapsed=now>=started ? (now-started)/200.0f : 0.0f;
        reveal=from+((target ? 1.0f:0.0f)-from)*std::clamp(elapsed,0.0f,1.0f);
        return old!=reveal;
    }
};
inline std::array<char,64> RandomCupSubtitle(bool fullRandom,int character,int cc) {
    constexpr const char* drivers[]={"Mario","Luigi","Yoshi","Toad","Donkey Kong","Wario","Peach","Bowser"};
    constexpr const char* levels[]={"50cc","100cc","150cc","extra"};
    std::array<char,64> text{};
    if (!fullRandom) std::snprintf(text.data(),text.size(),"driver %s   level %s",
        character>=0&&character<8 ? drivers[character]:"?", cc>=0&&cc<4 ? levels[cc]:"?");
    return text;
}
} // namespace mk64_3ds
