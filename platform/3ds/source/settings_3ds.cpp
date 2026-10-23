#include "settings_3ds.h"
#include "render_policy_3ds.hpp"
#include "file_replace_3ds.hpp"

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
constexpr const char* kSettingsRecoveryPath = "sdmc:/3ds/MK64/mk64-3ds.cfg.bak";

struct SettingsFileOps {
    bool Rename(const char* from,const char* to) { return std::rename(from,to)==0; }
    void Remove(const char* path) { std::remove(path); }
    bool Exists(const char* path) { struct stat info{}; return ::stat(path,&info)==0; }
};

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
    bool newItems;
    Mk64FrameLimit3DS frameLimiter;
    Mk64HudMeters3DS hudMeters;
    bool zrDebug;
    bool logging;
    Mk64DisplayProfile3DS profile;
    Mk64SelectAction3DS selectAction;
    uint8_t menuBlur;
    bool menuMaps;
    bool threatWarnings;
    Mk64UiTextures3DS uiTextures;
};

constexpr Settings kDefaults = {
    MK64_ASPECT_RATIO_3DS_WIDE,
    false,
    MK64_HUD_LAYOUT_3DS_MKDS_2,
    400,
    100,
    MK64_DISPLAY_FILTER_3DS_BILINEAR,
    MK64_RENDER_DISTANCE_3DS_LOW,
    2,
    100,
    false,
    false,
    false,
    true,
    MK64_FRAME_LIMIT_3DS_30,
    MK64_HUD_METERS_3DS_OFF,
    false,
    true,
    MK64_DISPLAY_PROFILE_3DS_CUSTOM,
    MK64_SELECT_ACTION_3DS_CAMERA,
    1,
    true,
    true,
    MK64_UI_TEXTURES_3DS_MODIFIED,
};

bool sNewModelDefaults = false;
Settings ModelDefaults() {
    Settings defaults = kDefaults;
    // Only the initial/reset profile depends on hardware. Saved values are
    // applied afterwards and must keep precedence, even on another model.
    defaults.frameLimiter = sNewModelDefaults ? MK64_FRAME_LIMIT_3DS_60 : MK64_FRAME_LIMIT_3DS_30;
    defaults.uiTextures = sNewModelDefaults ? MK64_UI_TEXTURES_3DS_IMPROVED : MK64_UI_TEXTURES_3DS_MENU;
    if (sNewModelDefaults) defaults.renderDistance = MK64_RENDER_DISTANCE_3DS_NORMAL;
    return defaults;
}
Settings sSettings = kDefaults;
// Renderer experiment switch, independent of display profiles: recorded
// static geometry runs drawn by PICA (on) or the CPU interpreter path (off).
bool sGpuStaticMeshes = true;
bool sLoaded = false;
bool sLayoutRead = false;
bool sLegacyHudRead = false;
bool sProfileRead = false;
bool sFrameLimitRead = false;
constexpr const char* kLayoutNames[] = { "clean", "mk7", "mkds", "mkds2", "classic" };
constexpr const char* kLegacyLayoutNames[] = { "clean", "focus", "race", "hybrid", "classic" };
constexpr const char* kMeterNames[] = { "off", "distance", "speed", "position_lap" };

constexpr const char* kProfileNames[] = { "performance", "quality", "custom" };

Settings ProfileValues(Mk64DisplayProfile3DS profile) {
    Settings result = ModelDefaults();
    result.profile = profile;
    result.renderScalePercent = 100;
    result.displayFilter = MK64_DISPLAY_FILTER_3DS_BILINEAR;
    result.menuBlur = 1;
    result.menuMaps = profile == MK64_DISPLAY_PROFILE_3DS_QUALITY;
    result.frameLimiter = profile == MK64_DISPLAY_PROFILE_3DS_PERFORMANCE || !sNewModelDefaults
        ? MK64_FRAME_LIMIT_3DS_30 : MK64_FRAME_LIMIT_3DS_60;
    result.renderDistance = profile == MK64_DISPLAY_PROFILE_3DS_PERFORMANCE
        ? MK64_RENDER_DISTANCE_3DS_LOW
        : (sNewModelDefaults ? MK64_RENDER_DISTANCE_3DS_HIGH : MK64_RENDER_DISTANCE_3DS_NORMAL);
    return result;
}

bool SameDisplay(const Settings& a, const Settings& b) {
    return a.aspectRatio == b.aspectRatio && a.hudLayout == b.hudLayout &&
        a.resolutionWidth == b.resolutionWidth && a.renderScalePercent == b.renderScalePercent &&
        a.displayFilter == b.displayFilter && a.renderDistance == b.renderDistance &&
        a.frameLimiter == b.frameLimiter && a.showLoadingScreens == b.showLoadingScreens &&
        a.hudMeters == b.hudMeters && a.menuBlur == b.menuBlur &&
        a.menuMaps == b.menuMaps && a.threatWarnings == b.threatWarnings;
}

void MarkCustom() { sSettings.profile = MK64_DISPLAY_PROFILE_3DS_CUSTOM; }

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
    if (multiplier < 2) {
        return 2;
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
    bool parsed = false;
    if (std::strcmp(key, "profile") == 0) {
        for (int i = 0; i < 3; ++i) if (EqualsIgnoreCase(value, kProfileNames[i])) {
            sSettings.profile = static_cast<Mk64DisplayProfile3DS>(i); sProfileRead = true;
        }
        return;
    }
    if (std::strcmp(key, "select_for_dump") == 0 && ParseBoolean(value, &parsed)) {
        if (parsed) sSettings.selectAction = MK64_SELECT_ACTION_3DS_DUMP;
        else if (sSettings.selectAction == MK64_SELECT_ACTION_3DS_DUMP) sSettings.selectAction = MK64_SELECT_ACTION_3DS_CAMERA;
        return;
    }
    if (std::strcmp(key, "select_for_overlay") == 0 && ParseBoolean(value, &parsed)) {
        if (parsed) sSettings.selectAction = MK64_SELECT_ACTION_3DS_OVERLAY;
        else if (sSettings.selectAction == MK64_SELECT_ACTION_3DS_OVERLAY) sSettings.selectAction = MK64_SELECT_ACTION_3DS_CAMERA;
        return;
    }
    if (std::strcmp(key, "ui_textures") == 0) {
        if (EqualsIgnoreCase(value, "original")) sSettings.uiTextures = MK64_UI_TEXTURES_3DS_ORIGINAL;
        else if (EqualsIgnoreCase(value, "menu") || EqualsIgnoreCase(value, "menu_only"))
            sSettings.uiTextures = MK64_UI_TEXTURES_3DS_MENU;
        else if (EqualsIgnoreCase(value, "improved") || EqualsIgnoreCase(value, "scaled") ||
                 EqualsIgnoreCase(value, "modified")) sSettings.uiTextures = MK64_UI_TEXTURES_3DS_IMPROVED;
        return;
    }
    if (std::strcmp(key, "menu_maps") == 0 && ParseBoolean(value, &parsed)) { sSettings.menuMaps = parsed; return; }
    if (std::strcmp(key, "threat_warnings") == 0 && ParseBoolean(value, &parsed)) { sSettings.threatWarnings = parsed; return; }
    if (std::strcmp(key, "gpu_static_meshes") == 0 && ParseBoolean(value, &parsed)) { sGpuStaticMeshes = parsed; return; }
    if (std::strcmp(key, "hud_meters") == 0) {
        for (int i = 0; i < 4; ++i)
            if (EqualsIgnoreCase(value, kMeterNames[i]))
                sSettings.hudMeters = static_cast<Mk64HudMeters3DS>(i);
        return;
    }
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
    if (std::strcmp(key, "logging") == 0) {
        // Preserve the diagnostic infrastructure; legacy UI logging keys are ignored.
        return;
    }
    if (std::strcmp(key, "zr_debug") == 0) {
        // ZR remains unassigned, including legacy saved bindings.
        return;
    }
    if (std::strcmp(key, "frame_limit") == 0) {
        const char* names[] = {"30", "60", "off"};
        for (unsigned i=0; i<3; ++i) if (std::strcmp(value,names[i])==0) {
            sSettings.frameLimiter=static_cast<Mk64FrameLimit3DS>(i); sFrameLimitRead=true;
        }
        return;
    }
    if (std::strcmp(key, "frame_limiter") == 0) {
        // E1 and earlier used off to mean 60. Never migrate it to uncapped.
        if (!sFrameLimitRead && ParseBoolean(value, &booleanValue))
            sSettings.frameLimiter = booleanValue ? MK64_FRAME_LIMIT_3DS_30 : MK64_FRAME_LIMIT_3DS_60;
        return;
    }
    if (std::strcmp(key, "new_items") == 0) {
        if (ParseBoolean(value, &booleanValue)) sSettings.newItems = booleanValue;
        return;
    }
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
        // Live overlay state is transient; use the new explicit SELECT binding.
        return;
    }

    long integerValue = 0;
    if (!ParseInteger(value, &integerValue)) {
        return;
    }
    if (std::strcmp(key, "menu_blur") == 0) {
        sSettings.menuBlur = static_cast<uint8_t>(integerValue < 0 ? 0 : integerValue > 3 ? 3 : integerValue);
    } else if (std::strcmp(key, "resolution") == 0) {
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
    sLayoutRead = sLegacyHudRead = sProfileRead = sFrameLimitRead = false;
    sLoaded = true;

    FILE* file = std::fopen(kSettingsPath, "rb");
    if (file == nullptr) file = std::fopen(kSettingsRecoveryPath, "rb");
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
    if (sProfileRead && sSettings.profile != MK64_DISPLAY_PROFILE_3DS_CUSTOM) {
        if (!SameDisplay(sSettings, ProfileValues(sSettings.profile))) MarkCustom();
    } else if (!sProfileRead) {
        if (SameDisplay(sSettings, ProfileValues(MK64_DISPLAY_PROFILE_3DS_PERFORMANCE))) sSettings.profile = MK64_DISPLAY_PROFILE_3DS_PERFORMANCE;
        else if (SameDisplay(sSettings, ProfileValues(MK64_DISPLAY_PROFILE_3DS_QUALITY))) sSettings.profile = MK64_DISPLAY_PROFILE_3DS_QUALITY;
        else MarkCustom();
    }
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
        "show_loading_screens=%s\n"
        "new_items=%s\n"
        "frame_limit=%s\n"
        "hud_meters=%s\n"
        "profile=%s\n"
        "select_for_dump=%s\n"
        "select_for_overlay=%s\n"
        "menu_blur=%u\n"
        "menu_maps=%s\n"
        "threat_warnings=%s\n"
        "ui_textures=%s\n"
        "gpu_static_meshes=%s\n",
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
        sSettings.showFpsEnabled ? "on" : "off",
        sSettings.showLoadingScreens ? "on" : "off", sSettings.newItems ? "on" : "off",
        sSettings.frameLimiter == MK64_FRAME_LIMIT_3DS_30 ? "30" :
            (sSettings.frameLimiter == MK64_FRAME_LIMIT_3DS_60 ? "60" : "off"), kMeterNames[sSettings.hudMeters],
        kProfileNames[sSettings.profile],
        sSettings.selectAction == MK64_SELECT_ACTION_3DS_DUMP ? "on" : "off",
        sSettings.selectAction == MK64_SELECT_ACTION_3DS_OVERLAY ? "on" : "off",
        static_cast<unsigned>(sSettings.menuBlur), sSettings.menuMaps ? "on" : "off",
        sSettings.threatWarnings ? "on" : "off",
        sSettings.uiTextures == MK64_UI_TEXTURES_3DS_ORIGINAL ? "original" :
            sSettings.uiTextures == MK64_UI_TEXTURES_3DS_MENU ? "menu" : "improved",
        sGpuStaticMeshes ? "on" : "off");
    const bool flushSucceeded = std::fflush(file) == 0;
    const bool closeSucceeded = std::fclose(file) == 0;
    const bool writeSucceeded = written >= 0 && flushSucceeded && closeSucceeded;
    if (!writeSucceeded) {
        std::remove(kSettingsTemporaryPath);
        return false;
    }

    SettingsFileOps ops;
    const bool replaced=mk64_3ds::ReplacePreparedFile(
        kSettingsTemporaryPath,kSettingsPath,kSettingsRecoveryPath,ops);
    if (!replaced) std::remove(kSettingsTemporaryPath);
    return replaced;
}

extern "C" void Mk64Settings3DSResetDefaults(void) {
    sSettings = ModelDefaults();
    sLayoutRead = sLegacyHudRead = sProfileRead = sFrameLimitRead = false;
    sLoaded = true;
}

extern "C" bool Mk64Settings3DSGetShowLoadingScreens(void) {
    EnsureLoaded();
    return sSettings.showLoadingScreens;
}

extern "C" Mk64FrameLimit3DS Mk64Settings3DSGetFrameLimit(void) { EnsureLoaded(); return sSettings.frameLimiter; }
extern "C" void Mk64Settings3DSSetFrameLimit(Mk64FrameLimit3DS limit) {
    EnsureLoaded();
    if (limit<MK64_FRAME_LIMIT_3DS_30 || limit>MK64_FRAME_LIMIT_3DS_OFF) return;
    MarkCustom(); sSettings.frameLimiter=limit;
}
extern "C" bool Mk64Settings3DSGetFrameLimiter(void) { return Mk64Settings3DSGetFrameLimit()==MK64_FRAME_LIMIT_3DS_30; }
extern "C" void Mk64Settings3DSSetFrameLimiter(bool enabled) {
    Mk64Settings3DSSetFrameLimit(enabled ? MK64_FRAME_LIMIT_3DS_30 : MK64_FRAME_LIMIT_3DS_60);
}
extern "C" bool Mk64Settings3DSGetZrDebug(void) { EnsureLoaded(); return sSettings.zrDebug; }
extern "C" void Mk64Settings3DSSetZrDebug(bool enabled) { EnsureLoaded(); sSettings.zrDebug = enabled; }
extern "C" bool Mk64Settings3DSGetLogging(void) { EnsureLoaded(); return sSettings.logging; }
extern "C" void Mk64Settings3DSSetLogging(bool enabled) { EnsureLoaded(); sSettings.logging = enabled; }

extern "C" Mk64HudMeters3DS Mk64Settings3DSGetHudMeters(void) { EnsureLoaded(); return sSettings.hudMeters; }
extern "C" void Mk64Settings3DSSetHudMeters(Mk64HudMeters3DS meters) {
    EnsureLoaded(); MarkCustom();
    sSettings.hudMeters = meters >= MK64_HUD_METERS_3DS_OFF && meters <= MK64_HUD_METERS_3DS_POSITION_LAP
        ? meters : MK64_HUD_METERS_3DS_OFF;
}

extern "C" void Mk64Settings3DSSetShowLoadingScreens(bool enabled) {
    EnsureLoaded(); MarkCustom();
    sSettings.showLoadingScreens = enabled;
}

extern "C" Mk64AspectRatio3DS Mk64Settings3DSGetAspectRatio(void) {
    EnsureLoaded();
    return sSettings.aspectRatio;
}

extern "C" void Mk64Settings3DSSetAspectRatio(Mk64AspectRatio3DS aspectRatio) {
    EnsureLoaded(); MarkCustom();
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
    EnsureLoaded(); MarkCustom();
    sSettings.resolutionWidth = SanitizeResolution(width);
}

extern "C" uint8_t Mk64Settings3DSGetRenderScalePercent(void) {
    EnsureLoaded();
    return sSettings.renderScalePercent;
}

extern "C" void Mk64Settings3DSSetRenderScalePercent(uint8_t percent) {
    EnsureLoaded(); MarkCustom();
    sSettings.renderScalePercent = SanitizeRenderScale(percent);
}

extern "C" Mk64DisplayFilter3DS Mk64Settings3DSGetDisplayFilter(void) {
    EnsureLoaded();
    return sSettings.displayFilter;
}

extern "C" void Mk64Settings3DSSetDisplayFilter(Mk64DisplayFilter3DS filter) {
    EnsureLoaded(); MarkCustom();
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

extern "C" bool Mk64Settings3DSGetNewItems(void) {
    EnsureLoaded();
    return sSettings.newItems;
}
extern "C" void Mk64Settings3DSSetNewItems(bool enabled) {
    EnsureLoaded();
    sSettings.newItems = enabled;
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
    EnsureLoaded(); MarkCustom();
    sSettings.renderDistance = distance >= MK64_RENDER_DISTANCE_3DS_LOW && distance <= MK64_RENDER_DISTANCE_3DS_HIGH
                                  ? distance : MK64_RENDER_DISTANCE_3DS_HIGH;
}

extern "C" Mk64HudLayout3DS Mk64Settings3DSGetHudLayout(void) {
    EnsureLoaded();
    return sSettings.hudLayout;
}
extern "C" void Mk64Settings3DSSetHudLayout(Mk64HudLayout3DS layout) {
    EnsureLoaded(); MarkCustom();
    sSettings.hudLayout = layout >= MK64_HUD_LAYOUT_3DS_CLEAN && layout <= MK64_HUD_LAYOUT_3DS_CLASSIC
                             ? layout : MK64_HUD_LAYOUT_3DS_CLEAN;
    sSettings.topHudEnabled = sSettings.hudLayout == MK64_HUD_LAYOUT_3DS_CLASSIC;
}

extern "C" Mk64DisplayProfile3DS Mk64Settings3DSGetDisplayProfile(void) { EnsureLoaded(); return sSettings.profile; }
extern "C" void Mk64Settings3DSApplyDisplayProfile(Mk64DisplayProfile3DS profile) {
    EnsureLoaded();
    if (profile != MK64_DISPLAY_PROFILE_3DS_PERFORMANCE && profile != MK64_DISPLAY_PROFILE_3DS_QUALITY) { MarkCustom(); return; }
    const auto values = ProfileValues(profile);
    sSettings.aspectRatio = values.aspectRatio; sSettings.hudLayout = values.hudLayout;
    sSettings.topHudEnabled = values.topHudEnabled; sSettings.resolutionWidth = values.resolutionWidth;
    sSettings.renderScalePercent = values.renderScalePercent; sSettings.displayFilter = values.displayFilter;
    sSettings.renderDistance = values.renderDistance; sSettings.frameLimiter = values.frameLimiter;
    sSettings.showLoadingScreens = values.showLoadingScreens; sSettings.hudMeters = values.hudMeters;
    sSettings.menuBlur = values.menuBlur; sSettings.menuMaps = values.menuMaps;
    sSettings.threatWarnings = values.threatWarnings; sSettings.profile = profile;
}
extern "C" Mk64SelectAction3DS Mk64Settings3DSGetSelectAction(void) { EnsureLoaded(); return sSettings.selectAction; }
extern "C" void Mk64Settings3DSSetSelectAction(Mk64SelectAction3DS action) {
    EnsureLoaded(); sSettings.selectAction = action >= MK64_SELECT_ACTION_3DS_CAMERA && action <= MK64_SELECT_ACTION_3DS_OVERLAY ? action : MK64_SELECT_ACTION_3DS_CAMERA;
}
extern "C" uint8_t Mk64Settings3DSGetMenuBlur(void) { EnsureLoaded(); return sSettings.menuBlur; }
extern "C" void Mk64Settings3DSSetMenuBlur(uint8_t strength) { EnsureLoaded(); MarkCustom(); sSettings.menuBlur = strength > 3 ? 3 : strength; }
extern "C" bool Mk64Settings3DSGetMenuMaps(void) { EnsureLoaded(); return sSettings.menuMaps; }
extern "C" void Mk64Settings3DSSetMenuMaps(bool enabled) { EnsureLoaded(); MarkCustom(); sSettings.menuMaps = enabled; }
extern "C" bool Mk64Settings3DSGetThreatWarnings(void) { EnsureLoaded(); return sSettings.threatWarnings; }
extern "C" bool Mk64Settings3DSGetStaticMeshes(void) { EnsureLoaded(); return sGpuStaticMeshes; }
extern "C" void Mk64Settings3DSSetThreatWarnings(bool enabled) { EnsureLoaded(); MarkCustom(); sSettings.threatWarnings = enabled; }

extern "C" Mk64UiTextures3DS Mk64Settings3DSGetUiTextures(void) { EnsureLoaded(); return sSettings.uiTextures; }
extern "C" void Mk64Settings3DSSetUiTextures(Mk64UiTextures3DS textures) {
    EnsureLoaded();
    sSettings.uiTextures = textures >= MK64_UI_TEXTURES_3DS_ORIGINAL && textures <= MK64_UI_TEXTURES_3DS_MENU
        ? textures : MK64_UI_TEXTURES_3DS_MODIFIED;
}
