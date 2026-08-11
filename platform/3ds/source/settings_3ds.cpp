#include "settings_3ds.h"

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
    uint16_t resolutionWidth;
    uint8_t turboMultiplier;
    uint16_t masterVolumePercent;
    bool showFpsEnabled;
    bool overlayEnabled;
};

constexpr Settings kDefaults = {
    MK64_ASPECT_RATIO_3DS_WIDE,
    true,
    400,
    1,
    100,
    false,
    false,
};

Settings sSettings = kDefaults;
bool sLoaded = false;

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

    bool booleanValue = false;
    if (std::strcmp(key, "top_hud") == 0) {
        if (ParseBoolean(value, &booleanValue)) {
            sSettings.topHudEnabled = booleanValue;
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

extern "C" void Mk64Settings3DSLoad(void) {
    if (sLoaded) {
        return;
    }
    sSettings = kDefaults;
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
        "resolution=%u\n"
        "turbo_speed=%u\n"
        "master_volume=%u\n"
        "show_fps=%s\n"
        "overlay=%s\n",
        sSettings.aspectRatio == MK64_ASPECT_RATIO_3DS_ORIGINAL ? "original" : "wide",
        sSettings.topHudEnabled ? "on" : "off",
        static_cast<unsigned int>(sSettings.resolutionWidth),
        static_cast<unsigned int>(sSettings.turboMultiplier),
        static_cast<unsigned int>(sSettings.masterVolumePercent),
        sSettings.showFpsEnabled ? "on" : "off", sSettings.overlayEnabled ? "on" : "off");
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
    sSettings = kDefaults;
    sLoaded = true;
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
    sSettings.topHudEnabled = enabled;
}

extern "C" uint16_t Mk64Settings3DSGetResolutionWidth(void) {
    EnsureLoaded();
    return sSettings.resolutionWidth;
}

extern "C" void Mk64Settings3DSSetResolutionWidth(uint16_t width) {
    EnsureLoaded();
    sSettings.resolutionWidth = SanitizeResolution(width);
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
