#include "options_layout_3ds.hpp"
#include "menu_hint_3ds.hpp"
#include "art/menu_art_3ds.hpp"
#include "menu_hint_native_3ds.hpp"
#include "art/option_icons_art_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <set>
#include <string>

int main() {
    using namespace mk64_3ds;
    std::set<DisplayOption> options;
    for (bool advanced : {false, true}) {
        for (uint8_t row = 0; row < (advanced ? kAdvancedDisplayOptions.size() : kDisplayOptions.size()); ++row) {
            assert(options.insert(DisplayOptionAt(advanced, row)).second);
            const int top = int(OptionRowY(true, row)) - 3;
            assert(top >= 60 && top + OptionRowHeight(true) <= 213);
            if (row + 1 < (advanced ? kAdvancedDisplayOptions.size() : kDisplayOptions.size())) assert(top + OptionRowHeight(true) == int(OptionRowY(true, row + 1)) - 3);
        }
    }
    assert(options.size() == 15);
    assert(OptionSelectionShade(0)==0 && OptionSelectionShade(32)==255);
    assert(OptionSelectionShade(16)==128 && OptionSelectionShade(48)==128);
    assert(OptionSelectionShade(64)==0);

    assert(DisplayOptionAt(false, 0) == DisplayOption::Profile);
    assert(DisplayOptionAt(false, kAdvancedDisplayRow) == DisplayOption::Advanced);
    assert(DisplayOptionAt(true, 6) == DisplayOption::Textures && DisplayOptionAt(true, 7) == DisplayOption::Back);
    assert(RandomCupSubtitle(true, 0, 0)[0] == '\0');
    assert(RandomCupSubtitle(false, 0, 0)[0] != '\0');
    // Every black backing pixel participates in the same image as its glyphs.
    auto opaque = [](const auto& pixels) {
        for (auto pixel : pixels) assert((pixel >> 24) == 255);
    };
    opaque(menu_art::kOption); opaque(menu_art::kData);
    opaque(menu_art::kRandomCup); opaque(menu_art::kAuto);
    const auto source = [](const char* name) {
        MenuHintSource result;
        const bool button = std::string(name) == "texture_l_option" || std::string(name) == "texture_r_data";
        result.width = button ? 58 : 64; result.height = button ? 19 : 18;
        result.pixels.assign(result.width * result.height, 0xffffffffU);
        return result;
    };
    RandomHintPixels nativeRandom{}; AutoHintPixels nativeAuto{};
    assert(MakeRandomCupHint(nativeRandom, source) && MakeAutoHint(nativeAuto, source));
    opaque(nativeRandom); opaque(nativeAuto);
    for (size_t icon = 0; icon <= static_cast<size_t>(OptionIcon::Update); ++icon) {
        size_t visible = 0;
        for (size_t y = 0; y < 18; ++y) for (size_t x = 0; x < 18; ++x)
            visible += (option_art::kPixels[((icon / 7) * 18 + y) * 128 + (icon % 7) * 18 + x] >> 24) != 0;
        assert(visible == 18*18);
        const auto& bounds=option_art::kBounds[icon];
        assert(bounds[0]>=3 && bounds[1]>=3);
        // Odd pixel widths cannot have identical integer margins: allow one pixel.
        assert(bounds[0]*2+bounds[2]>=17 && bounds[0]*2+bounds[2]<=18);
        assert(bounds[1]*2+bounds[3]>=17 && bounds[1]*2+bounds[3]<=18);

    }
    AutoHintTween tween;
    tween.Update(true, 1000); tween.Update(true, 1100);
    assert(tween.reveal == 0.5f);
    tween.Update(false, 1100); tween.Update(false, 1200);
    assert(tween.reveal == 0.25f);
    tween.Update(false, 1300); assert(tween.reveal == 0);
    std::puts("PASS: display pages cover every setting exactly once, touch rows are disjoint, hints are opaque and reveal reverses continuously");
}
