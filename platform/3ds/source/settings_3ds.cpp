#include "settings_3ds.h"
#include "render_policy_3ds.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr const char* kSettingsDirectory = "sdmc:/3ds/MK64";
constexpr const char* kSettingsPath = "sdmc:/3ds/MK64/mk64-3ds.cfg";
constexpr const char* kSettingsTemporaryPath = "sdmc:/3ds/MK64/mk64-3ds.cfg.tmp";

struct Settings {
    Mk64AspectRatio3DS aspectRatio;
    bool topHudEnabled;
    Mk64HudLayout3DS hudLayout;
    uint16_t resolutionWidth;
    uint8_t renderScalePercent;
    Mk64DisplayFilter3DS displayFilter;
    Mk64RenderDistance3DS renderDistance;
    uint8_t turboMultiplier;
    uint16_t masterVolumePercent;
    bool showFpsEnabled;
    bool overlayEnabled;
    bool showLoadingScreens;
};

constexpr Settings kDefaults = {
    MK64_ASPECT_RATIO_3DS_WIDE,
    false,
    MK64_HUD_LAYOUT_3DS_MKDS_2,
    400,
    100,
    MK64_DISPLAY_FILTER_3DS_BILINEAR,
    MK64_RENDER_DISTANCE_3DS_LOW,
    1,
    100,
    false,
    false,
    false,
};

bool sNewModelDefaults = false;
Settings ModelDefaults() {
    Settings defaults = kDefaults;
    if (sNewModelDefaults) {
        defaults.renderScalePercent = 100;
        defaults.renderDistance = MK64_RENDER_DISTANCE_3DS_NORMAL;
    }
    return defaults;
}
Settings sSettings = kDefaults;
bool sLoaded = false;
bool sLayoutRead = false;
bool sLegacyHudRead = false;
constexpr const char* kLayoutNames[] = { "clean", "mk7", "mkds", "mkds2", "classic" };
constexpr const char* kLegacyLayoutNames[] = { "clean", "focus", "race", "hybrid", "classic" };

bool IsAsciiSpace(char character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n' ||
           character == '\f' || character == '\v';
}

char* Trim(char* text) {
    while (*text != '\0' && IsAsciiSpace(*text)) {
        ++text;
    }

    char* end = text + std::strlen(text);
    while (end > text && IsAsciiSpace(end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

char AsciiLower(char character) {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character - 'A' + 'a');
    }
    return character;
}

bool EqualsIgnoreCase(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        if (AsciiLower(*left) != AsciiLower(*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

bool ParseInteger(const char* text, long* result) {
    if (text == nullptr || result == nullptr || *text == '\0') {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') {
        return false;
    }
    *result = value;
    return true;
}

bool ParseBoolean(const char* text, bool* result) {
    if (EqualsIgnoreCase(text, "on") || EqualsIgnoreCase(text, "true") ||
        std::strcmp(text, "1") == 0) {
        *result = true;
        return true;
    }
    if (EqualsIgnoreCase(text, "off") || EqualsIgnoreCase(text, "false") ||
        std::strcmp(text, "0") == 0) {
        *result = false;
        return true;
    }
    return false;
}

uint16_t SanitizeResolution(long width) {
    if (width <= 400) {
        return 400;
    }
    if (width >= 800) {
        return 800;
    }
    return width < 600 ? 400 : 800;
}

uint8_t SanitizeRenderScale(long percent) {
    return mk64_3ds::NormalizeRenderScale(percent);
}

Mk64DisplayFilter3DS SanitizeDisplayFilter(Mk64DisplayFilter3DS filter) {
    switch (filter) {
        case MK64_DISPLAY_FILTER_3DS_BLUR:
        case MK64_DISPLAY_FILTER_3DS_CRT:
            return filter;
        case MK64_DISPLAY_FILTER_3DS_BILINEAR:
        default:
            return MK64_DISPLAY_FILTER_3DS_BILINEAR;
    }
}

uint8_t SanitizeTurboMultiplier(long multiplier) {
    if (multiplier < 1) {
        return 1;
    }
    if (multiplier > 5) {
        return 5;
    }
    return static_cast<uint8_t>(multiplier);
}

uint16_t SanitizeMasterVolume(long percent) {
    constexpr uint16_t kAcceptedVolumes[] = { 25, 50, 75, 100, 150, 200 };
    if (percent <= kAcceptedVolumes[0]) {
        return kAcceptedVolumes[0];
    }

    for (size_t i = 1; i < sizeof(kAcceptedVolumes) / sizeof(kAcceptedVolumes[0]); ++i) {
        const long midpoint =
            (static_cast<long>(kAcceptedVolumes[i - 1]) + kAcceptedVolumes[i]) / 2;
        if (percent <= midpoint) {
            return kAcceptedVolumes[i - 1];
        }
    }
    return kAcceptedVolumes[sizeof(kAcceptedVolumes) / sizeof(kAcceptedVolumes[0]) - 1];
}

void ApplySetting(const char* key, const char* value) {
    if (std::strcmp(key, "aspect_ratio") == 0) {
        if (EqualsIgnoreCase(value, "wide")) {
            sSettings.aspectRatio = MK64_ASPECT_RATIO_3DS_WIDE;
        } else if (EqualsIgnoreCase(value, "original")) {
            sSettings.aspectRatio = MK64_ASPECT_RATIO_3DS_ORIGINAL;
        }
        return;
    }

    if (std::strcmp(key, "hud_layout") == 0) {
        for (int i = 0; i < 5; ++i) {
            if (EqualsIgnoreCase(value, kLayoutNames[i]) || EqualsIgnoreCase(value, kLegacyLayoutNames[i])) {
                sSettings.hudLayout = static_cast<Mk64HudLayout3DS>(i);
                sLayoutRead = true;
            }
        }
        return;
    }
    bool booleanValue = false;
    if (std::strcmp(key, "show_loading_screens") == 0) {
        if (ParseBoolean(value, &booleanValue)) sSettings.showLoadingScreens = booleanValue;
        return;
    }
    if (std::strcmp(key, "top_hud") == 0) {
        if (ParseBoolean(value, &booleanValue)) {
            sSettings.topHudEnabled = booleanValue;
            sLegacyHudRead = true;
        }
        return;
    }
    if (std::strcmp(key, "render_distance") == 0) {
        if (EqualsIgnoreCase(value, "low")) sSettings.renderDistance = MK64_RENDER_DISTANCE_3DS_LOW;
        else if (EqualsIgnoreCase(value, "normal")) sSettings.renderDistance = MK64_RENDER_DISTANCE_3DS_NORMAL;
        else if (EqualsIgnoreCase(value, "high")) sSettings.renderDistance = MK64_RENDER_DISTANCE_3DS_HIGH;
        return;
    }
    if (std::strcmp(key, "display_filter") == 0) {
        if (EqualsIgnoreCase(value, "blur")) {
            sSettings.displayFilter = MK64_DISPLAY_FILTER_3DS_BLUR;
        } else if (EqualsIgnoreCase(value, "crt")) {
            sSettings.displayFilter = MK64_DISPLAY_FILTER_3DS_CRT;
        } else if (EqualsIgnoreCase(value, "bilinear")) {
            sSettings.displayFilter = MK64_DISPLAY_FILTER_3DS_BILINEAR;
        }
        return;
    }
    if (std::strcmp(key, "show_fps") == 0) {
        if (ParseBoolean(value, &booleanValue)) {
            sSettings.showFpsEnabled = booleanValue;
        }
        return;
    }
    if (std::strcmp(key, "overlay") == 0) {
        if (ParseBoolean(value, &booleanValue)) {
            sSettings.overlayEnabled = booleanValue;
        }
        return;
    }

    long integerValue = 0;
    if (!ParseInteger(value, &integerValue)) {
        return;
    }
    if (std::strcmp(key, "resolution") == 0) {
        sSettings.resolutionWidth = SanitizeResolution(integerValue);
    } else if (std::strcmp(key, "render_scale") == 0) {
        sSettings.renderScalePercent = SanitizeRenderScale(integerValue);
    } else if (std::strcmp(key, "turbo_speed") == 0) {
        sSettings.turboMultiplier = SanitizeTurboMultiplier(integerValue);
    } else if (std::strcmp(key, "master_volume") == 0) {
        sSettings.masterVolumePercent = SanitizeMasterVolume(integerValue);
    }
}

bool EnsureSettingsDirectory() {
    if (mkdir("sdmc:/3ds", 0777) != 0 && errno != EEXIST) {
        return false;
    }
    return mkdir(kSettingsDirectory, 0777) == 0 || errno == EEXIST;
}

void EnsureLoaded() {
    if (!sLoaded) {
        Mk64Settings3DSLoad();
    }
}
}

extern "C" void Mk64Settings3DSSetHardwareModel(bool isNewModel) {
    sNewModelDefaults = isNewModel;
}

extern "C" void Mk64Settings3DSLoad(void) {
    if (sLoaded) {
        return;
    }
    sSettings = ModelDefaults();
    sLoaded = true;

    FILE* file = std::fopen(kSettingsPath, "rb");
    if (file == nullptr) {
        return;
    }

    char line[192] = {};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char* key = Trim(line);
        if (*key == '\0' || *key == '#' || *key == ';') {
            continue;
        }

        char* separator = std::strchr(key, '=');
        if (separator == nullptr) {
            continue;
        }
        *separator = '\0';
        key = Trim(key);
        char* value = Trim(separator + 1);
        if (*key != '\0' && *value != '\0') {
            ApplySetting(key, value);
        }
    }
    std::fclose(file);
    if (!sLayoutRead && sLegacyHudRead) sSettings.hudLayout = sSettings.topHudEnabled ? MK64_HUD_LAYOUT_3DS_CLASSIC : MK64_HUD_LAYOUT_3DS_CLEAN;
    sSettings.topHudEnabled = sSettings.hudLayout == MK64_HUD_LAYOUT_3DS_CLASSIC;
}

extern "C" bool Mk64Settings3DSSave(void) {
    EnsureLoaded();
    if (!EnsureSettingsDirectory()) {
        return false;
    }

    FILE* file = std::fopen(kSettingsTemporaryPath, "wb");
    if (file == nullptr) {
        return false;
    }

    const int written = std::fprintf(
        file,
        "# Mario Kart 64 3DS settings\n"
        "aspect_ratio=%s\n"
        "top_hud=%s\n"
        "hud_layout=%s\n"
        "resolution=%u\n"
        "render_scale=%u\n"
        "display_filter=%s\n"
        "render_distance=%s\n"
        "turbo_speed=%u\n"
        "master_volume=%u\n"
        "show_fps=%s\n"
        "overlay=%s\n"
        "show_loading_screens=%s\n",
        sSettings.aspectRatio == MK64_ASPECT_RATIO_3DS_ORIGINAL ? "original" : "wide",
        sSettings.topHudEnabled ? "on" : "off",
        kLayoutNames[static_cast<int>(sSettings.hudLayout)],
        static_cast<unsigned int>(sSettings.resolutionWidth),
        static_cast<unsigned int>(sSettings.renderScalePercent),
        sSettings.displayFilter == MK64_DISPLAY_FILTER_3DS_BLUR
            ? "blur"
            : (sSettings.displayFilter == MK64_DISPLAY_FILTER_3DS_CRT ? "crt" : "bilinear"),
        sSettings.renderDistance == MK64_RENDER_DISTANCE_3DS_LOW ? "low" :
            (sSettings.renderDistance == MK64_RENDER_DISTANCE_3DS_NORMAL ? "normal" : "high"),
        static_cast<unsigned int>(sSettings.turboMultiplier),
        static_cast<unsigned int>(sSettings.masterVolumePercent),
        sSettings.showFpsEnabled ? "on" : "off", sSettings.overlayEnabled ? "on" : "off",
        sSettings.showLoadingScreens ? "on" : "off");
    const bool flushSucceeded = std::fflush(file) == 0;
    const bool closeSucceeded = std::fclose(file) == 0;
    const bool writeSucceeded = written >= 0 && flushSucceeded && closeSucceeded;
    if (!writeSucceeded) {
        std::remove(kSettingsTemporaryPath);
        return false;
    }

    if (std::rename(kSettingsTemporaryPath, kSettingsPath) == 0) {
        return true;
    }

    // Some FAT implementations do not replace an existing destination. Keep
    // the old file until the complete temporary file is safely closed first.
    if (std::remove(kSettingsPath) == 0 &&
        std::rename(kSettingsTemporaryPath, kSettingsPath) == 0) {
        return true;
    }
    std::remove(kSettingsTemporaryPath);
    return false;
}

extern "C" void Mk64Settings3DSResetDefaults(void) {
    sSettings = ModelDefaults();
    sLoaded = true;
}

extern "C" bool Mk64Settings3DSGetShowLoadingScreens(void) {
    EnsureLoaded();
    return sSettings.showLoadingScreens;
}

extern "C" void Mk64Settings3DSSetShowLoadingScreens(bool enabled) {
    EnsureLoaded();
    sSettings.showLoadingScreens = enabled;
}

extern "C" Mk64AspectRatio3DS Mk64Settings3DSGetAspectRatio(void) {
    EnsureLoaded();
    return sSettings.aspectRatio;
}

extern "C" void Mk64Settings3DSSetAspectRatio(Mk64AspectRatio3DS aspectRatio) {
    EnsureLoaded();
    sSettings.aspectRatio = aspectRatio == MK64_ASPECT_RATIO_3DS_ORIGINAL
                                ? MK64_ASPECT_RATIO_3DS_ORIGINAL
                                : MK64_ASPECT_RATIO_3DS_WIDE;
}

extern "C" bool Mk64Settings3DSGetTopHudEnabled(void) {
    EnsureLoaded();
    return sSettings.topHudEnabled;
}

extern "C" void Mk64Settings3DSSetTopHudEnabled(bool enabled) {
    EnsureLoaded();
    Mk64Settings3DSSetHudLayout(enabled ? MK64_HUD_LAYOUT_3DS_CLASSIC : MK64_HUD_LAYOUT_3DS_CLEAN);
}

extern "C" uint16_t Mk64Settings3DSGetResolutionWidth(void) {
    EnsureLoaded();
    return sSettings.resolutionWidth;
}

extern "C" void Mk64Settings3DSSetResolutionWidth(uint16_t width) {
    EnsureLoaded();
    sSettings.resolutionWidth = SanitizeResolution(width);
}

extern "C" uint8_t Mk64Settings3DSGetRenderScalePercent(void) {
    EnsureLoaded();
    return sSettings.renderScalePercent;
}

extern "C" void Mk64Settings3DSSetRenderScalePercent(uint8_t percent) {
    EnsureLoaded();
    sSettings.renderScalePercent = SanitizeRenderScale(percent);
}

extern "C" Mk64DisplayFilter3DS Mk64Settings3DSGetDisplayFilter(void) {
    EnsureLoaded();
    return sSettings.displayFilter;
}

extern "C" void Mk64Settings3DSSetDisplayFilter(Mk64DisplayFilter3DS filter) {
    EnsureLoaded();
    sSettings.displayFilter = SanitizeDisplayFilter(filter);
}

extern "C" uint8_t Mk64Settings3DSGetTurboMultiplier(void) {
    EnsureLoaded();
    return sSettings.turboMultiplier;
}

extern "C" void Mk64Settings3DSSetTurboMultiplier(uint8_t multiplier) {
    EnsureLoaded();
    sSettings.turboMultiplier = SanitizeTurboMultiplier(multiplier);
}

extern "C" uint16_t Mk64Settings3DSGetMasterVolumePercent(void) {
    EnsureLoaded();
    return sSettings.masterVolumePercent;
}

extern "C" void Mk64Settings3DSSetMasterVolumePercent(uint16_t percent) {
    EnsureLoaded();
    sSettings.masterVolumePercent = SanitizeMasterVolume(percent);
}

extern "C" bool Mk64Settings3DSGetShowFpsEnabled(void) {
    EnsureLoaded();
    return sSettings.showFpsEnabled;
}

extern "C" void Mk64Settings3DSSetShowFpsEnabled(bool enabled) {
    EnsureLoaded();
    sSettings.showFpsEnabled = enabled;
}

extern "C" bool Mk64Settings3DSGetOverlayEnabled(void) {
    EnsureLoaded();
    return sSettings.overlayEnabled;
}

extern "C" void Mk64Settings3DSSetOverlayEnabled(bool enabled) {
    EnsureLoaded();
    sSettings.overlayEnabled = enabled;
}

extern "C" Mk64RenderDistance3DS Mk64Settings3DSGetRenderDistance(void) {
    EnsureLoaded();
    return sSettings.renderDistance;
}

extern "C" void Mk64Settings3DSSetRenderDistance(Mk64RenderDistance3DS distance) {
    EnsureLoaded();
    sSettings.renderDistance = distance >= MK64_RENDER_DISTANCE_3DS_LOW && distance <= MK64_RENDER_DISTANCE_3DS_HIGH
                                  ? distance : MK64_RENDER_DISTANCE_3DS_HIGH;
}

extern "C" Mk64HudLayout3DS Mk64Settings3DSGetHudLayout(void) {
    EnsureLoaded();
    return sSettings.hudLayout;
}
extern "C" void Mk64Settings3DSSetHudLayout(Mk64HudLayout3DS layout) {
    EnsureLoaded();
    sSettings.hudLayout = layout >= MK64_HUD_LAYOUT_3DS_CLEAN && layout <= MK64_HUD_LAYOUT_3DS_CLASSIC
                             ? layout : MK64_HUD_LAYOUT_3DS_CLEAN;
    sSettings.topHudEnabled = sSettings.hudLayout == MK64_HUD_LAYOUT_3DS_CLASSIC;
}
