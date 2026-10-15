#pragma once

#include <array>
#include <cstdint>

namespace mk64_3ds {

// One ordering for drawing, activation and touch. Settings retain their saved
// identities when moved between the main display page and its advanced page.
enum class DisplayOption : uint8_t {
    Profile, Aspect, Hud, Render, Distance, FrameLimiter, Advanced,
    Filter, Loading, Meters, Blur, Maps, Warnings, Back, Textures
};
inline constexpr std::array<DisplayOption, 7> kDisplayOptions = {
    DisplayOption::Profile, DisplayOption::Aspect, DisplayOption::Hud,
    DisplayOption::Render, DisplayOption::Distance, DisplayOption::FrameLimiter,
    DisplayOption::Advanced
};
inline constexpr std::array<DisplayOption, 8> kAdvancedDisplayOptions = {
    DisplayOption::Filter, DisplayOption::Loading, DisplayOption::Meters,
    DisplayOption::Blur, DisplayOption::Maps, DisplayOption::Warnings,
    DisplayOption::Textures, DisplayOption::Back
};
inline constexpr uint8_t kAdvancedDisplayRow = 6;
// Same triangular greyscale ramp as the native slow selection (64 ticks).
constexpr uint8_t OptionSelectionShade(uint32_t nativeFlashTick) {
    unsigned shade = (nativeFlashTick % 64U) * 512U / 64U;
    if (shade > 256U) shade = 512U - shade;
    return static_cast<uint8_t>(shade > 255U ? 255U : shade);
}
constexpr DisplayOption DisplayOptionAt(bool advanced, uint8_t row) {
    if (advanced) return row < kAdvancedDisplayOptions.size() ? kAdvancedDisplayOptions[row] : DisplayOption::Back;
    return row < kDisplayOptions.size() ? kDisplayOptions[row] : DisplayOption::Back;
}

// Native 320x240 bottom-screen coordinates. Keep text, cursor and touch rows
// coupled; a submenu must never inherit another page's touch spacing.
constexpr float OptionRowY(bool display, uint8_t row) {
    return (display ? 66.0f : 72.0f) + row * 18.0f;
}
constexpr int OptionRowHeight(bool display) { return 18; }
constexpr float OptionRowCenterY(bool display, uint8_t row) {
    return OptionRowY(display, row) - 3.0f + OptionRowHeight(display) * 0.5f;
}

} // namespace mk64_3ds
