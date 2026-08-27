#include "bottom_ui_3ds.h"
#include "game_runtime_3ds.h"

#include "diagnostics_3ds.h"
#include "game_state_3ds.h"
#include "input_policy_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <malloc.h>

extern "C" uint32_t Mk64Audio3DSBufferedFrames(void) __attribute__((weak));
extern "C" uint32_t Mk64Audio3DSQueuedCount(void) __attribute__((weak));
extern "C" uint32_t Mk64Audio3DSDroppedCount(void) __attribute__((weak));
extern "C" void Mk64Graphics3DSGetDebugStats(size_t*, size_t*, size_t*, size_t*, size_t*)
    __attribute__((weak));
extern "C" uint32_t Mk64Graphics3DSResolvedOutputWidth(void) __attribute__((weak));
extern "C" void Mk64FrameInterpolation3DSGetStats(uint32_t*, uint32_t*, uint32_t*, uint32_t*,
                                                   uint32_t*) __attribute__((weak));

namespace {

constexpr float kBottomWidth = 320.0f;
constexpr float kBottomHeight = 240.0f;
constexpr uint32_t kMaxUiTextureDimension = 512;
constexpr int kCStickTurboDeadzone = 30;
constexpr uint64_t kFpsWindowMilliseconds = 2000;
constexpr size_t kFpsHistoryCount = 5;
constexpr size_t kC2DObjectCapacity = 1024;
constexpr size_t kRetiredTextureCapacity = 8;
constexpr float kBinaryAngleToRadians = 6.28318530717958647692f / 65536.0f;
constexpr uint16_t kFontAtlasWidth = 512;
constexpr uint16_t kFontAtlasHeight = 64;
constexpr uint16_t kFontGlyphCellWidth = 32;
constexpr uint16_t kFontGlyphCellHeight = 16;
constexpr size_t kFontGlyphsPerRow = kFontAtlasWidth / kFontGlyphCellWidth;
constexpr size_t kFontGlyphCount = 42;
constexpr float kOptionsTabY = 36.0f;
constexpr float kOptionsRowY = 70.0f;
constexpr float kOptionsRowStep = 39.0f;
constexpr const char* kGameSelectOptionResource = "__OTR__textures/texture_tkmk00/texture_l_option";
constexpr const char* kGameSelectDataResource = "__OTR__textures/texture_tkmk00/texture_r_data";
constexpr const char* kSelectionTriangleResource =
    "__OTR__textures/player_selection/texture_small_green_triangle";
constexpr const char* kHudLapResource = "__OTR__textures/common_data/common_texture_hud_lap";
constexpr const char* kHudTimeResource = "__OTR__textures/common_data/common_texture_hud_time";
constexpr const char* kHudTimerDigitsResource =
    "__OTR__textures/common_data/common_texture_hud_normal_digit";
constexpr const char* kPortraitBorderResource =
    "__OTR__textures/common_data/common_texture_character_portrait_border";
constexpr const char* kStandingRankPaletteResource =
    "__OTR__textures/common_data/common_tlut_hud_type_C_rank_font";
constexpr const char* kMinimapFinishLineResource =
    "__OTR__textures/common_data/common_texture_minimap_finish_line";

constexpr uint32_t kTransferFlags =
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

#if defined(SPAGHETTI_VERSION)
constexpr const char* kBuildVersion = SPAGHETTI_VERSION;
#elif defined(MK64_3DS_VERSION)
constexpr const char* kBuildVersion = "3DS-v" MK64_3DS_VERSION;
#else
constexpr const char* kBuildVersion = "3DS development";
#endif

enum class BaseView : uint8_t {
    Background,
    GameSelect,
    RaceHud,
    Paused,
};

enum class OptionsTab : uint8_t {
    Game,
    Screen,
    Gameplay,
    Developer,
    Count,
};

enum TextureType : uint32_t {
    TextureError = 0,
    TextureRgba32 = 1,
    TextureRgba16 = 2,
    TexturePalette4 = 3,
    TexturePalette8 = 4,
    TextureI4 = 5,
    TextureI8 = 6,
    TextureIa4 = 7,
    TextureIa8 = 8,
    TextureIa16 = 9,
};

struct UiTexture {
    C3D_Tex texture = {};
    Tex3DS_SubTexture subTexture = {};
    bool initialized = false;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t backingWidth = 0;
    uint16_t backingHeight = 0;
    char resourceName[192] = {};
    char paletteName[192] = {};
};

struct FontGlyph {
    Tex3DS_SubTexture subTexture = {};
    uint8_t advance = 7;
    bool loaded = false;
};

struct UiFontAtlas {
    C3D_Tex texture = {};
    bool initialized = false;
    std::array<FontGlyph, kFontGlyphCount> glyphs = {};
};

struct RaceHudTextures {
    bool loadAttempted = false;
    UiTexture lapLabel;
    std::array<UiTexture, 3> lapCounts = {};
    UiTexture timeLabel;
    UiTexture timerDigits;
    std::array<UiTexture, 8> places = {};
    std::array<UiTexture, 8> portraits = {};
    UiTexture questionPortrait;
    UiTexture portraitBorder;
    std::array<UiTexture, 8> standingRanks = {};
    std::array<UiTexture, 16> items = {};
    UiTexture finishLine;
    std::array<UiTexture, 8> minimapKarts = {};
};

struct FpsSample {
    uint32_t frames = 0;
    uint64_t milliseconds = 0;
};

struct BottomUiState {
    bool initialized = false;
    C3D_RenderTarget* bottomTarget = nullptr;
    UiFontAtlas font;
    UiTexture menuBackground;
    UiTexture coursePreview;
    UiTexture minimap;
    UiTexture gameSelectOption;
    UiTexture gameSelectData;
    UiTexture selectionTriangle;
    RaceHudTextures raceHud;
    std::array<UiTexture, kRetiredTextureCapacity> retiredTextures = {};
    size_t retiredTextureCount = 0;
    Mk64BottomUIGameState3DS game = {};
    BaseView view = BaseView::Background;
    bool modalOpen = false;
    bool modalOpenedFromPause = false;
    bool consumesCStick = false;
    bool bottomDirty = true;
    uint8_t raceHudRefreshPhase = 0;
    OptionsTab tab = OptionsTab::Game;
    uint8_t selectedRow = 0;
    uint32_t injectedGameKeys = 0;
    uint32_t blockedGameKeys = 0;
    uint32_t previousHeldKeys = 0;
    bool captureGameInputThisFrame = false;
    uint64_t statusExpiresAt = 0;
    char status[64] = {};

    uint64_t fpsWindowStartedAt = 0;
    uint32_t fpsWindowFrames = 0;
    std::array<FpsSample, kFpsHistoryCount> fpsHistory = {};
    size_t fpsHistoryNext = 0;
    size_t fpsHistorySize = 0;
    float currentFps = 0.0f;
    float averageFps = 0.0f;
};

BottomUiState sUi;

constexpr std::array<const char*, kFontGlyphCount> kFontResources = {
    "__OTR__textures/texture_data_2/font_letter_A",
    "__OTR__textures/texture_data_2/font_letter_B",
    "__OTR__textures/texture_data_2/font_letter_C",
    "__OTR__textures/texture_data_2/font_letter_D",
    "__OTR__textures/texture_data_2/font_letter_E",
    "__OTR__textures/texture_data_2/font_letter_F",
    "__OTR__textures/texture_data_2/font_letter_G",
    "__OTR__textures/texture_data_2/font_letter_H",
    "__OTR__textures/texture_data_2/font_letter_I",
    "__OTR__textures/texture_data_2/font_letter_J",
    "__OTR__textures/texture_data_2/font_letter_K",
    "__OTR__textures/texture_data_2/font_letter_L",
    "__OTR__textures/texture_data_2/font_letter_M",
    "__OTR__textures/texture_data_2/font_letter_N",
    "__OTR__textures/texture_data_2/font_letter_O",
    "__OTR__textures/texture_data_2/font_letter_P",
    "__OTR__textures/texture_data_2/font_letter_Q",
    "__OTR__textures/texture_data_2/font_letter_R",
    "__OTR__textures/texture_data_2/font_letter_S",
    "__OTR__textures/texture_data_2/font_letter_T",
    "__OTR__textures/texture_data_2/font_letter_U",
    "__OTR__textures/texture_data_2/font_letter_V",
    "__OTR__textures/texture_data_2/font_letter_W",
    "__OTR__textures/texture_data_2/font_letter_X",
    "__OTR__textures/texture_data_2/font_letter_Y",
    "__OTR__textures/texture_data_2/font_letter_Z",
    "__OTR__textures/texture_data_2/font_number_zero",
    "__OTR__textures/texture_data_2/font_number_one",
    "__OTR__textures/texture_data_2/font_number_two",
    "__OTR__textures/texture_data_2/font_number_three",
    "__OTR__textures/texture_data_2/font_number_four",
    "__OTR__textures/texture_data_2/font_number_five",
    "__OTR__textures/texture_data_2/font_number_six",
    "__OTR__textures/texture_data_2/font_number_seven",
    "__OTR__textures/texture_data_2/font_number_eight",
    "__OTR__textures/texture_data_2/font_number_nine",
    "__OTR__textures/texture_data_2/7F1534",
    "__OTR__textures/texture_data_2/font_minus",
    "__OTR__textures/texture_data_2/7F16D4",
    "__OTR__textures/texture_data_2/font_exclamation_mark",
    "__OTR__textures/texture_data_2/font_plus",
    "__OTR__textures/texture_data_2/7F17A4",
};

// These are the original display advances used by print_text1 for the same
// glyphs. The 26x16 bitmap includes its own side bearings.
constexpr std::array<uint8_t, kFontGlyphCount> kFontAdvances = {
    12, 13, 11, 11, 10, 11, 11, 13, 7, 10, 12, 10, 18,
    13, 12, 12, 12, 12, 11, 13, 12, 12, 18, 13, 12, 12,
    10, 8, 11, 12, 12, 13, 10, 11, 10, 10,
    6, 10, 10, 10, 10, 6,
};

constexpr std::array<const char*, 3> kLapCountResources = {
    "__OTR__textures/common_data/common_texture_hud_lap_1_on_3",
    "__OTR__textures/common_data/common_texture_hud_lap_2_on_3",
    "__OTR__textures/common_data/common_texture_hud_lap_3_on_3",
};

constexpr std::array<const char*, 8> kPortraitResources = {
    "__OTR__textures/common_data/common_texture_portrait_mario",
    "__OTR__textures/common_data/common_texture_portrait_luigi",
    "__OTR__textures/common_data/common_texture_portrait_yoshi",
    "__OTR__textures/common_data/common_texture_portrait_toad",
    "__OTR__textures/common_data/common_texture_portrait_donkey_kong",
    "__OTR__textures/common_data/common_texture_portrait_wario",
    "__OTR__textures/common_data/common_texture_portrait_peach",
    "__OTR__textures/common_data/common_texture_portrait_bowser",
};

constexpr std::array<const char*, 8> kPortraitPaletteResources = {
    "__OTR__textures/common_data/common_tlut_portrait_mario",
    "__OTR__textures/common_data/common_tlut_portrait_luigi",
    "__OTR__textures/common_data/common_tlut_portrait_yoshi",
    "__OTR__textures/common_data/common_tlut_portrait_toad",
    "__OTR__textures/common_data/common_tlut_portrait_donkey_kong",
    "__OTR__textures/common_data/common_tlut_portrait_wario",
    "__OTR__textures/common_data/common_tlut_portrait_peach",
    "__OTR__textures/common_data/common_tlut_portrait_bowser",
};

constexpr const char* kQuestionPortraitResource =
    "__OTR__textures/common_data/common_texture_portrait_question_mark";
constexpr const char* kQuestionPortraitPaletteResource =
    "__OTR__textures/common_data/common_tlut_portrait_bomb_kart_and_question_mark";

constexpr std::array<const char*, 16> kItemResources = {
    "__OTR__textures/common_data/common_texture_item_window_none",
    "__OTR__textures/common_data/common_texture_item_window_banana",
    "__OTR__textures/common_data/common_texture_item_window_banana_bunch",
    "__OTR__textures/common_data/common_texture_item_window_green_shell",
    "__OTR__textures/common_data/common_texture_item_window_triple_green_shell",
    "__OTR__textures/common_data/common_texture_item_window_red_shell",
    "__OTR__textures/common_data/common_texture_item_window_triple_red_shell",
    "__OTR__textures/common_data/common_texture_item_window_blue_shell",
    "__OTR__textures/common_data/common_texture_item_window_thunder_bolt",
    "__OTR__textures/common_data/common_texture_item_window_fake_item_box",
    "__OTR__textures/common_data/common_texture_item_window_star",
    "__OTR__textures/common_data/common_texture_item_window_boo",
    "__OTR__textures/common_data/common_texture_item_window_mushroom",
    "__OTR__textures/common_data/common_texture_item_window_double_mushroom",
    "__OTR__textures/common_data/common_texture_item_window_triple_mushroom",
    "__OTR__textures/common_data/common_texture_item_window_super_mushroom",
};

constexpr std::array<const char*, 16> kItemPaletteResources = {
    "__OTR__textures/common_data/common_tlut_item_window_none",
    "__OTR__textures/common_data/common_tlut_item_window_banana",
    "__OTR__textures/common_data/common_tlut_item_window_banana_bunch",
    "__OTR__textures/common_data/common_tlut_item_window_green_shell",
    "__OTR__textures/common_data/common_tlut_item_window_triple_green_shell",
    "__OTR__textures/common_data/common_tlut_item_window_red_shell",
    "__OTR__textures/common_data/common_tlut_item_window_triple_red_shell",
    "__OTR__textures/common_data/common_tlut_item_window_blue_shell",
    "__OTR__textures/common_data/common_tlut_item_window_thunder_bolt",
    "__OTR__textures/common_data/common_tlut_item_window_fake_item_box",
    "__OTR__textures/common_data/common_tlut_item_window_star",
    "__OTR__textures/common_data/common_tlut_item_window_boo",
    "__OTR__textures/common_data/common_tlut_item_window_mushroom",
    "__OTR__textures/common_data/common_tlut_item_window_double_mushroom",
    "__OTR__textures/common_data/common_tlut_item_window_triple_mushroom",
    "__OTR__textures/common_data/common_tlut_item_window_super_mushroom",
};

constexpr std::array<const char*, 8> kPlaceResources = {
    "__OTR__textures/common_data/common_texture_hud_1st",
    "__OTR__textures/common_data/common_texture_hud_2nd",
    "__OTR__textures/common_data/common_texture_hud_3rd",
    "__OTR__textures/common_data/common_texture_hud_4th",
    "__OTR__textures/common_data/common_texture_hud_5th",
    "__OTR__textures/common_data/common_texture_hud_6th",
    "__OTR__textures/common_data/common_texture_hud_7th",
    "__OTR__textures/common_data/common_texture_hud_8th",
};

constexpr std::array<const char*, 8> kStandingRankResources = {
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_1",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_2",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_3",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_4",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_5",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_6",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_7",
    "__OTR__textures/common_data/common_texture_hud_type_C_rank_font_8",
};

constexpr std::array<const char*, 8> kMinimapKartResources = {
    "__OTR__textures/common_data/common_texture_minimap_kart_mario",
    "__OTR__textures/common_data/common_texture_minimap_kart_luigi",
    "__OTR__textures/common_data/common_texture_minimap_kart_yoshi",
    "__OTR__textures/common_data/common_texture_minimap_kart_toad",
    "__OTR__textures/common_data/common_texture_minimap_kart_donkey_kong",
    "__OTR__textures/common_data/common_texture_minimap_kart_wario",
    "__OTR__textures/common_data/common_texture_minimap_kart_peach",
    "__OTR__textures/common_data/common_texture_minimap_kart_bowser",
};

constexpr std::array<uint16_t, 6> kVolumeSteps = { 25, 50, 75, 100, 150, 200 };

uint16_t NextPowerOfTwo(uint32_t value) {
    uint32_t result = 8;
    while (result < value && result < kMaxUiTextureDimension) result <<= 1U;
    return static_cast<uint16_t>(result);
}

constexpr uint32_t MortonOffset8x8(uint32_t x, uint32_t y) {
    return (x & 1U) | ((y & 1U) << 1U) | ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

constexpr uint8_t Scale3To8(uint8_t value) {
    return static_cast<uint8_t>((value << 5U) | (value << 2U) | (value >> 1U));
}

constexpr uint8_t Scale4To8(uint8_t value) {
    return static_cast<uint8_t>((value << 4U) | value);
}

constexpr uint8_t Scale5To8(uint8_t value) {
    return static_cast<uint8_t>((value << 3U) | (value >> 2U));
}

void DeleteTexture(UiTexture& texture) {
    if (texture.initialized) C3D_TexDelete(&texture.texture);
    texture = {};
}

bool RetireTexture(UiTexture& texture) {
    if (!texture.initialized) {
        texture = {};
        return true;
    }
    if (sUi.retiredTextureCount >= sUi.retiredTextures.size()) return false;
    sUi.retiredTextures[sUi.retiredTextureCount++] = texture;
    texture = {};
    return true;
}

void DrainRetiredTextures() {
    for (size_t index = 0; index < sUi.retiredTextureCount; ++index) {
        DeleteTexture(sUi.retiredTextures[index]);
    }
    sUi.retiredTextureCount = 0;
}

bool ReplaceTexture(UiTexture& destination, UiTexture& replacement) {
    if (!RetireTexture(destination)) return false;
    destination = replacement;
    replacement = {};
    return true;
}

size_t RequiredTextureBytes(const Mk64TextureResource3DS& resource) {
    const size_t pixels = static_cast<size_t>(resource.width) * resource.height;
    switch (resource.type) {
        case TextureRgba32: return pixels * 4U;
        case TextureRgba16:
        case TextureIa16: return pixels * 2U;
        case TexturePalette4:
        case TextureI4:
        case TextureIa4: return (pixels + 1U) / 2U;
        case TexturePalette8:
        case TextureI8:
        case TextureIa8: return pixels;
        default: return 0;
    }
}

constexpr uint32_t UiSourceRowForBackingRow(uint32_t sourceHeight, uint32_t destinationRow) {
    return destinationRow < sourceHeight ? destinationRow : sourceHeight - 1U;
}

constexpr uint8_t Palette4Index(uint8_t packed, size_t pixelIndex) {
    return (pixelIndex & 1U) == 0 ? packed >> 4U : packed & 0x0FU;
}

static_assert(UiSourceRowForBackingRow(240, 0) == 0);
static_assert(UiSourceRowForBackingRow(240, 239) == 239);
static_assert(UiSourceRowForBackingRow(240, 255) == 239);
static_assert(UiSourceRowForBackingRow(19, 31) == 18);
static_assert(Palette4Index(0xAB, 0) == 0xA);
static_assert(Palette4Index(0xAB, 1) == 0xB);

uint32_t DecodeRgba16(const uint8_t* pixel) {
    const uint16_t packed = static_cast<uint16_t>((pixel[0] << 8U) | pixel[1]);
    const uint8_t red = Scale5To8(static_cast<uint8_t>(packed >> 11U));
    const uint8_t green = Scale5To8(static_cast<uint8_t>((packed >> 6U) & 0x1FU));
    const uint8_t blue = Scale5To8(static_cast<uint8_t>((packed >> 1U) & 0x1FU));
    const uint8_t alpha = (packed & 1U) != 0 ? 255 : 0;
    return static_cast<uint32_t>(red) | (static_cast<uint32_t>(green) << 8U) |
           (static_cast<uint32_t>(blue) << 16U) | (static_cast<uint32_t>(alpha) << 24U);
}

uint32_t DecodeTexturePixel(const Mk64TextureResource3DS& resource, size_t pixelIndex,
                            const Mk64TextureResource3DS* palette = nullptr) {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t alpha = 255;
    switch (resource.type) {
        case TextureRgba32: {
            const uint8_t* pixel = resource.data + pixelIndex * 4U;
            red = pixel[0];
            green = pixel[1];
            blue = pixel[2];
            alpha = pixel[3];
            break;
        }
        case TextureRgba16: {
            return DecodeRgba16(resource.data + pixelIndex * 2U);
        }
        case TexturePalette4:
        case TexturePalette8: {
            if (palette == nullptr || palette->data == nullptr) break;
            const uint8_t paletteIndex = resource.type == TexturePalette4
                ? Palette4Index(resource.data[pixelIndex / 2U], pixelIndex)
                : resource.data[pixelIndex];
            return DecodeRgba16(palette->data + static_cast<size_t>(paletteIndex) * 2U);
        }
        case TextureI4: {
            const uint8_t packed = resource.data[pixelIndex / 2U];
            const uint8_t intensity = Palette4Index(packed, pixelIndex);
            red = green = blue = alpha = Scale4To8(intensity);
            break;
        }
        case TextureI8:
            red = green = blue = alpha = resource.data[pixelIndex];
            break;
        case TextureIa4: {
            const uint8_t packed = resource.data[pixelIndex / 2U];
            const uint8_t value = (pixelIndex & 1U) == 0 ? packed >> 4U : packed & 0x0FU;
            red = green = blue = Scale3To8(value >> 1U);
            alpha = (value & 1U) != 0 ? 255 : 0;
            break;
        }
        case TextureIa8: {
            const uint8_t value = resource.data[pixelIndex];
            red = green = blue = Scale4To8(value >> 4U);
            alpha = Scale4To8(value & 0x0FU);
            break;
        }
        case TextureIa16:
            red = green = blue = resource.data[pixelIndex * 2U];
            alpha = resource.data[pixelIndex * 2U + 1U];
            break;
        default:
            break;
    }
    return static_cast<uint32_t>(red) | (static_cast<uint32_t>(green) << 8U) |
           (static_cast<uint32_t>(blue) << 16U) | (static_cast<uint32_t>(alpha) << 24U);
}

void RememberMissingTexture(const char* resourceName, const char* paletteName,
                            UiTexture& destination) {
    UiTexture missing;
    std::snprintf(missing.resourceName, sizeof(missing.resourceName), "%s",
                  resourceName == nullptr ? "" : resourceName);
    std::snprintf(missing.paletteName, sizeof(missing.paletteName), "%s",
                  paletteName == nullptr ? "" : paletteName);
    ReplaceTexture(destination, missing);
}

bool LoadTexture(const char* resourceName, UiTexture& destination, const char* paletteName = nullptr) {
    if (resourceName == nullptr || resourceName[0] == '\0') return false;
    const char* normalizedPaletteName = paletteName == nullptr ? "" : paletteName;
    if (std::strcmp(destination.resourceName, resourceName) == 0 &&
        std::strcmp(destination.paletteName, normalizedPaletteName) == 0) {
        return destination.initialized;
    }

    Mk64TextureResource3DS resource = {};
    if (!Mk64Resource3DSGetTexture(resourceName, &resource) || resource.data == nullptr ||
        resource.width == 0 || resource.height == 0 || resource.width > kMaxUiTextureDimension ||
        resource.height > kMaxUiTextureDimension) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination);
        return false;
    }
    const size_t required = RequiredTextureBytes(resource);
    if (required == 0 || resource.size < required) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination);
        return false;
    }

    Mk64TextureResource3DS palette = {};
    const bool paletteRequired = resource.type == TexturePalette4 || resource.type == TexturePalette8;
    const size_t paletteEntries = resource.type == TexturePalette4 ? 16U : 256U;
    if (paletteRequired &&
        (normalizedPaletteName[0] == '\0' ||
         !Mk64Resource3DSGetTexture(normalizedPaletteName, &palette) || palette.data == nullptr ||
         palette.type != TextureRgba16 || palette.size < paletteEntries * 2U)) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination);
        return false;
    }

    UiTexture decoded;
    decoded.width = resource.width;
    decoded.height = resource.height;
    std::snprintf(decoded.resourceName, sizeof(decoded.resourceName), "%s", resourceName);
    std::snprintf(decoded.paletteName, sizeof(decoded.paletteName), "%s", normalizedPaletteName);
    const uint16_t backingWidth = NextPowerOfTwo(resource.width);
    const uint16_t backingHeight = NextPowerOfTwo(resource.height);
    decoded.backingWidth = backingWidth;
    decoded.backingHeight = backingHeight;
    if (backingWidth < resource.width || backingHeight < resource.height ||
        !C3D_TexInit(&decoded.texture, backingWidth, backingHeight, GPU_RGBA8)) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination);
        return false;
    }
    decoded.initialized = true;

    auto* texturePixels = static_cast<uint32_t*>(decoded.texture.data);
    const uint32_t tilesPerRow = backingWidth / 8U;
    for (uint32_t tileY = 0; tileY < backingHeight; tileY += 8U) {
        for (uint32_t tileX = 0; tileX < backingWidth; tileX += 8U) {
            uint32_t* tile = texturePixels +
                (static_cast<size_t>(tileY / 8U) * tilesPerRow + tileX / 8U) * 64U;
            for (uint32_t row = 0; row < 8U; ++row) {
                const uint32_t destinationY = tileY + row;
                // Tex3DS/Citro2D subtextures use top=1 and address source rows
                // top-down. Fast3D's V=0 POT-backed row inversion must not be
                // reused here; doing so turns every lower-screen image over.
                const uint32_t sourceY = UiSourceRowForBackingRow(resource.height, destinationY);
                for (uint32_t column = 0; column < 8U; ++column) {
                    const uint32_t sourceX = std::min<uint32_t>(tileX + column, resource.width - 1U);
                    const size_t sourceIndex = static_cast<size_t>(sourceY) * resource.width + sourceX;
                    tile[MortonOffset8x8(column, row)] =
                        __builtin_bswap32(DecodeTexturePixel(
                            resource, sourceIndex, paletteRequired ? &palette : nullptr));
                }
            }
        }
    }
    C3D_TexFlush(&decoded.texture);
    C3D_TexSetFilter(&decoded.texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&decoded.texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    decoded.subTexture = {
        .width = resource.width,
        .height = resource.height,
        .left = 0.0f,
        .top = 1.0f,
        .right = static_cast<float>(resource.width) / backingWidth,
        .bottom = 1.0f - static_cast<float>(resource.height) / backingHeight,
    };

    if (!ReplaceTexture(destination, decoded)) {
        // The new texture has never been submitted to the GPU and is safe to
        // destroy immediately. Retain the old live texture and retry next tick.
        DeleteTexture(decoded);
        return false;
    }
    return true;
}

void WriteTiledPixel(C3D_Tex& texture, uint16_t backingWidth, uint16_t x, uint16_t y,
                     uint32_t rgba) {
    auto* pixels = static_cast<uint32_t*>(texture.data);
    const uint32_t tilesPerRow = backingWidth / 8U;
    const size_t tile = (static_cast<size_t>(y / 8U) * tilesPerRow + x / 8U) * 64U;
    pixels[tile + MortonOffset8x8(x & 7U, y & 7U)] = __builtin_bswap32(rgba);
}

bool LoadFontAtlas() {
    UiFontAtlas& font = sUi.font;
    if (font.initialized) return true;
    if (!C3D_TexInit(&font.texture, kFontAtlasWidth, kFontAtlasHeight, GPU_RGBA8)) return false;
    std::memset(font.texture.data, 0,
                static_cast<size_t>(kFontAtlasWidth) * kFontAtlasHeight * sizeof(uint32_t));

    size_t loadedCount = 0;
    for (size_t index = 0; index < kFontResources.size(); ++index) {
        Mk64TextureResource3DS resource = {};
        if (!Mk64Resource3DSGetTexture(kFontResources[index], &resource) ||
            resource.data == nullptr || resource.width != 26 || resource.height != 16 ||
            resource.type != TextureI4 || resource.size < RequiredTextureBytes(resource)) {
            continue;
        }
        const uint16_t atlasX = static_cast<uint16_t>((index % kFontGlyphsPerRow) * kFontGlyphCellWidth);
        const uint16_t atlasY = static_cast<uint16_t>((index / kFontGlyphsPerRow) * kFontGlyphCellHeight);
        for (uint16_t y = 0; y < resource.height; ++y) {
            for (uint16_t x = 0; x < resource.width; ++x) {
                const size_t pixel = static_cast<size_t>(y) * resource.width + x;
                WriteTiledPixel(font.texture, kFontAtlasWidth, atlasX + x, atlasY + y,
                                DecodeTexturePixel(resource, pixel));
            }
        }
        FontGlyph& glyph = font.glyphs[index];
        glyph.subTexture = {
            .width = resource.width,
            .height = resource.height,
            .left = static_cast<float>(atlasX) / kFontAtlasWidth,
            .top = 1.0f - static_cast<float>(atlasY) / kFontAtlasHeight,
            .right = static_cast<float>(atlasX + resource.width) / kFontAtlasWidth,
            .bottom = 1.0f - static_cast<float>(atlasY + resource.height) / kFontAtlasHeight,
        };
        glyph.advance = kFontAdvances[index];
        glyph.loaded = true;
        ++loadedCount;
    }
    C3D_TexFlush(&font.texture);
    C3D_TexSetFilter(&font.texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&font.texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    font.initialized = loadedCount >= 36;
    if (!font.initialized) C3D_TexDelete(&font.texture);
    return font.initialized;
}

C2D_Image TextureImage(UiTexture& texture) {
    return { .tex = &texture.texture, .subtex = &texture.subTexture };
}

int GlyphIndexForCharacter(char character) {
    if (character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
    if (character >= 'A' && character <= 'Z') return character - 'A';
    if (character >= '0' && character <= '9') return 26 + character - '0';
    switch (character) {
        case '.': return 36;
        case '-': return 37;
        case '?': return 38;
        case '!': return 39;
        case '+': return 40;
        case '\'': return 41;
        default: return -1;
    }
}

float CharacterAdvance(char character) {
    if (character == ' ') return 7.0f;
    if (character == ':' || character == '/') return 8.0f;
    if (character == '%') return 14.0f;
    const int glyphIndex = GlyphIndexForCharacter(character);
    if (glyphIndex >= 0) return kFontAdvances[static_cast<size_t>(glyphIndex)];
    return kFontAdvances[38];
}

float MeasureText(const char* value, float scale) {
    if (value == nullptr) return 0.0f;
    float width = 0.0f;
    while (*value != '\0') width += CharacterAdvance(*value++) * scale;
    return width;
}

void DrawSpecialCharacter(char character, float x, float y, float scale, uint32_t color,
                          float depth) {
    const float thickness = std::max(1.0f, 1.35f * scale);
    if (character == ':') {
        const float size = std::max(1.0f, 2.0f * scale);
        C2D_DrawRectSolid(x + 2.0f * scale, y + 4.0f * scale, depth, size, size, color);
        C2D_DrawRectSolid(x + 2.0f * scale, y + 11.0f * scale, depth, size, size, color);
    } else if (character == '/') {
        C2D_DrawLine(x + 1.0f * scale, y + 14.0f * scale, color,
                     x + 7.0f * scale, y + 2.0f * scale, color, thickness, depth);
    } else if (character == '%') {
        C2D_DrawRectSolid(x + scale, y + 2.0f * scale, depth, 3.0f * scale, 3.0f * scale, color);
        C2D_DrawLine(x + 2.0f * scale, y + 14.0f * scale, color,
                     x + 11.0f * scale, y + 2.0f * scale, color, thickness, depth);
        C2D_DrawRectSolid(x + 9.0f * scale, y + 11.0f * scale, depth,
                          3.0f * scale, 3.0f * scale, color);
    }
}

void DrawText(const char* value, float x, float y, float scale, uint32_t color,
              uint32_t alignment = C2D_AlignLeft, float depth = 0.8f, bool shadow = false) {
    if (value == nullptr || !sUi.font.initialized) return;
    const float width = MeasureText(value, scale);
    float cursor = x;
    if ((alignment & C2D_AlignCenter) != 0) cursor -= width * 0.5f;
    if ((alignment & C2D_AlignRight) != 0) cursor -= width;

    const auto drawPass = [&](float offsetX, float offsetY, uint32_t passColor, float passDepth) {
        C2D_ImageTint tint = {};
        C2D_PlainImageTint(&tint, passColor, 1.0f);
        float glyphX = cursor;
        for (const char* character = value; *character != '\0'; ++character) {
            const float advance = CharacterAdvance(*character) * scale;
            const int glyphIndex = GlyphIndexForCharacter(*character);
            if (glyphIndex >= 0 && sUi.font.glyphs[static_cast<size_t>(glyphIndex)].loaded) {
                FontGlyph& glyph = sUi.font.glyphs[static_cast<size_t>(glyphIndex)];
                C2D_Image image = { .tex = &sUi.font.texture, .subtex = &glyph.subTexture };
                const C2D_DrawParams params = {
                    .pos = { .x = glyphX + offsetX, .y = y + offsetY,
                             .w = 26.0f * scale, .h = 16.0f * scale },
                    .center = { .x = 0.0f, .y = 0.0f },
                    .depth = passDepth,
                    .angle = 0.0f,
                };
                C2D_DrawImage(image, &params, &tint);
            } else if (*character != ' ') {
                DrawSpecialCharacter(*character, glyphX + offsetX, y + offsetY,
                                     scale, passColor, passDepth);
            }
            glyphX += advance;
        }
    };
    if (shadow) drawPass(std::max(1.0f, scale), std::max(1.0f, scale),
                         C2D_Color32(0, 0, 0, 210), depth - 0.01f);
    drawPass(0.0f, 0.0f, color, depth);
}

void DrawTexture(UiTexture& texture, float x, float y, float width, float height, float depth,
                 const C2D_ImageTint* tint = nullptr, bool flipHorizontal = false) {
    if (!texture.initialized) return;
    const C2D_DrawParams params = {
        .pos = { .x = x, .y = y, .w = width, .h = height },
        .center = { .x = 0.0f, .y = 0.0f },
        .depth = depth,
        .angle = 0.0f,
    };
    C2D_Image image = TextureImage(texture);
    Tex3DS_SubTexture flippedSubTexture = texture.subTexture;
    if (flipHorizontal) {
        std::swap(flippedSubTexture.left, flippedSubTexture.right);
        image.subtex = &flippedSubTexture;
    }
    C2D_DrawImage(image, &params, tint);
}

void DrawTextureRotated(UiTexture& texture, float centerX, float centerY, float width,
                        float height, float depth, float angle) {
    if (!texture.initialized || texture.width == 0 || texture.height == 0) return;
    const C2D_Image image = TextureImage(texture);
    C2D_DrawImageAtRotated(image, centerX, centerY, depth, angle, nullptr,
                           width / texture.width, height / texture.height);
}

void DrawTextureRegion(UiTexture& texture, float sourceX, float sourceY, float sourceWidth,
                       float sourceHeight, float x, float y, float width, float height,
                       float depth, const C2D_ImageTint* tint = nullptr) {
    if (!texture.initialized || texture.backingWidth == 0 || texture.backingHeight == 0) return;
    sourceX = std::clamp(sourceX, 0.0f, static_cast<float>(texture.width));
    sourceY = std::clamp(sourceY, 0.0f, static_cast<float>(texture.height));
    sourceWidth = std::clamp(sourceWidth, 0.0f, texture.width - sourceX);
    sourceHeight = std::clamp(sourceHeight, 0.0f, texture.height - sourceY);
    Tex3DS_SubTexture region = {
        .width = static_cast<uint16_t>(sourceWidth),
        .height = static_cast<uint16_t>(sourceHeight),
        .left = sourceX / texture.backingWidth,
        .top = 1.0f - sourceY / texture.backingHeight,
        .right = (sourceX + sourceWidth) / texture.backingWidth,
        .bottom = 1.0f - (sourceY + sourceHeight) / texture.backingHeight,
    };
    C2D_Image image = { .tex = &texture.texture, .subtex = &region };
    const C2D_DrawParams params = {
        .pos = { .x = x, .y = y, .w = width, .h = height },
        .center = { .x = 0.0f, .y = 0.0f },
        .depth = depth,
        .angle = 0.0f,
    };
    C2D_DrawImage(image, &params, tint);
}

void DrawTextureCover(UiTexture& texture, float x, float y, float width, float height, float depth,
                      const C2D_ImageTint* tint = nullptr) {
    if (!texture.initialized) return;
    const float scale = std::max(width / texture.width, height / texture.height);
    const float drawWidth = texture.width * scale;
    const float drawHeight = texture.height * scale;
    DrawTexture(texture, x + (width - drawWidth) * 0.5f, y + (height - drawHeight) * 0.5f,
                drawWidth, drawHeight, depth, tint);
}

template <size_t Count>
void DeleteTextures(std::array<UiTexture, Count>& textures) {
    for (UiTexture& texture : textures) DeleteTexture(texture);
}

void DeleteFontAtlas() {
    if (sUi.font.initialized) C3D_TexDelete(&sUi.font.texture);
    sUi.font = {};
}

void LoadRaceHudTextures() {
    RaceHudTextures& hud = sUi.raceHud;
    if (hud.loadAttempted) return;
    hud.loadAttempted = true;

    LoadTexture(kHudLapResource, hud.lapLabel);
    for (size_t index = 0; index < hud.lapCounts.size(); ++index) {
        LoadTexture(kLapCountResources[index], hud.lapCounts[index]);
    }
    LoadTexture(kHudTimeResource, hud.timeLabel);
    LoadTexture(kHudTimerDigitsResource, hud.timerDigits);
    for (size_t index = 0; index < hud.places.size(); ++index) {
        LoadTexture(kPlaceResources[index], hud.places[index]);
        LoadTexture(kPortraitResources[index], hud.portraits[index],
                    kPortraitPaletteResources[index]);
        LoadTexture(kStandingRankResources[index], hud.standingRanks[index],
                    kStandingRankPaletteResource);
        LoadTexture(kMinimapKartResources[index], hud.minimapKarts[index]);
    }
    LoadTexture(kQuestionPortraitResource, hud.questionPortrait,
                kQuestionPortraitPaletteResource);
    LoadTexture(kPortraitBorderResource, hud.portraitBorder);
    for (size_t index = 0; index < hud.items.size(); ++index) {
        LoadTexture(kItemResources[index], hud.items[index], kItemPaletteResources[index]);
    }
    LoadTexture(kMinimapFinishLineResource, hud.finishLine);
}

void DeleteRaceHudTextures() {
    RaceHudTextures& hud = sUi.raceHud;
    DeleteTexture(hud.lapLabel);
    DeleteTextures(hud.lapCounts);
    DeleteTexture(hud.timeLabel);
    DeleteTexture(hud.timerDigits);
    DeleteTextures(hud.places);
    DeleteTextures(hud.portraits);
    DeleteTexture(hud.questionPortrait);
    DeleteTexture(hud.portraitBorder);
    DeleteTextures(hud.standingRanks);
    DeleteTextures(hud.items);
    DeleteTexture(hud.finishLine);
    DeleteTextures(hud.minimapKarts);
    hud.loadAttempted = false;
}

void DrawFallbackBackground() {
    C2D_DrawRectangle(0.0f, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                      C2D_Color32(15, 27, 52, 255), C2D_Color32(15, 27, 52, 255),
                      C2D_Color32(4, 7, 18, 255), C2D_Color32(4, 7, 18, 255));
}

void DrawDimMenuBackground() {
    DrawFallbackBackground();
    if (sUi.menuBackground.initialized) {
        // Match the original grayscale menu filters: Game Select red, Player
        // Select green, and Course Select blue-purple.
        uint32_t tintColor = C2D_Color32(255, 255, 255, 255);
        bool filtered = true;
        if (sUi.game.menuSelection == 11) {
            tintColor = C2D_Color32(255, 175, 175, 255);
        } else if (sUi.game.menuSelection == 12) {
            tintColor = C2D_Color32(175, 255, 175, 255);
        } else if (sUi.game.menuSelection == 13) {
            tintColor = C2D_Color32(175, 175, 255, 255);
        } else {
            filtered = false;
        }
        if (filtered) {
            C2D_ImageTint tint = {};
            C2D_PlainImageTint(&tint, tintColor, 1.0f);
            // The N64 menu enables grayscale and applies gBackgroundColor;
            // Citro2D's luma tint is the equivalent operation. Solid tinting
            // would discard the background detail and produce a flat quad.
            C2D_SetTintMode(C2D_TintLuma);
            DrawTextureCover(sUi.menuBackground, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                             0.05f, &tint);
            C2D_SetTintMode(C2D_TintSolid);
        } else {
            DrawTextureCover(sUi.menuBackground, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                             0.05f);
        }
        // The source image contributes roughly twenty percent to the result.
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, kBottomWidth, kBottomHeight,
                          C2D_Color32(0, 0, 0, 204));
    }
}

void DrawRaceBackground(float scrimAlpha = 204.0f) {
    if (sUi.coursePreview.initialized) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                          C2D_Color32(0, 0, 0, 255));
        DrawTextureCover(sUi.coursePreview, 0.0f, 0.0f, kBottomWidth, kBottomHeight, 0.05f);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, kBottomWidth, kBottomHeight,
                          C2D_Color32(0, 0, 0, static_cast<uint8_t>(scrimAlpha)));
    } else {
        DrawDimMenuBackground();
    }
}

BaseView GetBaseView(const Mk64BottomUIGameState3DS& game) {
    if (game.racing) return game.paused ? BaseView::Paused : BaseView::RaceHud;
    return game.gameSelectVisible ? BaseView::GameSelect : BaseView::Background;
}

void SetStatus(const char* text, uint64_t durationMilliseconds = 1800) {
    std::snprintf(sUi.status, sizeof(sUi.status), "%s", text == nullptr ? "" : text);
    sUi.statusExpiresAt = osGetTime() + durationMilliseconds;
    sUi.bottomDirty = true;
}

void SaveChangedSetting(const char* successText) {
    if (Mk64Settings3DSSave()) {
        SetStatus(successText);
    } else {
        SetStatus("SETTINGS SAVE FAILED", 2400);
    }
}

uint8_t RowCount(OptionsTab tab) {
    switch (tab) {
        case OptionsTab::Game: return sUi.modalOpenedFromPause ? 2 : 1;
        case OptionsTab::Screen: return 2;
        case OptionsTab::Gameplay: return 2;
        case OptionsTab::Developer: return 3;
        default: return 1;
    }
}

void OpenOptions(bool fromPause) {
    sUi.modalOpen = true;
    sUi.modalOpenedFromPause = fromPause;
    sUi.tab = OptionsTab::Game;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void CloseOptions() {
    if (Mk64Settings3DSGetOverlayEnabled()) {
        Mk64Settings3DSSetOverlayEnabled(false);
        Mk64Settings3DSSave();
    }
    sUi.modalOpen = false;
    sUi.modalOpenedFromPause = false;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void DismissOptions(mk64_3ds::ModalDismissAction3DS action) {
    switch (action) {
        case mk64_3ds::ModalDismissAction3DS::Continue:
            if (Mk64GameState3DSPerformPauseAction(MK64_PAUSE_ACTION_CONTINUE)) {
                CloseOptions();
            }
            break;
        case mk64_3ds::ModalDismissAction3DS::Close:
            CloseOptions();
            break;
        case mk64_3ds::ModalDismissAction3DS::None:
        default:
            break;
    }
}

void SetTab(OptionsTab tab) {
    sUi.tab = tab;
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
}

void ActivateSelectedRow(int direction) {
    const int step = direction < 0 ? -1 : 1;
    switch (sUi.tab) {
        case OptionsTab::Game:
            if (sUi.modalOpenedFromPause) {
                const Mk64PauseAction3DS action = sUi.selectedRow == 0
                                                       ? MK64_PAUSE_ACTION_CONTINUE
                                                       : MK64_PAUSE_ACTION_QUIT;
                if (Mk64GameState3DSPerformPauseAction(action)) CloseOptions();
                return;
            }
            CloseOptions();
            return;
        case OptionsTab::Screen:
            switch (sUi.selectedRow) {
                case 0: {
                    const Mk64AspectRatio3DS oldValue = Mk64Settings3DSGetAspectRatio();
                    const Mk64AspectRatio3DS newValue = oldValue == MK64_ASPECT_RATIO_3DS_WIDE
                                                            ? MK64_ASPECT_RATIO_3DS_ORIGINAL
                                                            : MK64_ASPECT_RATIO_3DS_WIDE;
                    Mk64Settings3DSSetAspectRatio(newValue);
                    SaveChangedSetting(newValue == MK64_ASPECT_RATIO_3DS_WIDE ? "WIDE ENABLED"
                                                                              : "ORIGINAL 4:3 ENABLED");
                    break;
                }
                case 1: {
                    const bool enabled = !Mk64Settings3DSGetTopHudEnabled();
                    Mk64Settings3DSSetTopHudEnabled(enabled);
                    Mk64GameState3DSSetTopHudEnabled(enabled);
                    SaveChangedSetting(enabled ? "TOP HUD ON" : "TOP HUD OFF");
                    break;
                }
            }
            break;
        case OptionsTab::Gameplay:
            switch (sUi.selectedRow) {
                case 0: {
                    int multiplier = Mk64Settings3DSGetTurboMultiplier() + step;
                    if (multiplier < 1) multiplier = 5;
                    if (multiplier > 5) multiplier = 1;
                    Mk64Settings3DSSetTurboMultiplier(static_cast<uint8_t>(multiplier));
                    SaveChangedSetting("TURBO SPEED SAVED");
                    break;
                }
                case 1: {
                    const uint16_t current = Mk64Settings3DSGetMasterVolumePercent();
                    size_t index = 0;
                    for (size_t i = 0; i < kVolumeSteps.size(); ++i) {
                        if (kVolumeSteps[i] == current) index = i;
                    }
                    index = direction < 0 ? (index + kVolumeSteps.size() - 1U) % kVolumeSteps.size()
                                          : (index + 1U) % kVolumeSteps.size();
                    Mk64Settings3DSSetMasterVolumePercent(kVolumeSteps[index]);
                    SaveChangedSetting("MASTER VOLUME SAVED");
                    break;
                }
            }
            break;
        case OptionsTab::Developer:
            switch (sUi.selectedRow) {
                case 0:
                    SetStatus(Mk64Diagnostics3DSRequestDump() ? "MEMORY DUMP REQUESTED"
                                                              : "DUMP ALREADY QUEUED", 2400);
                    break;
                case 1: {
                    const bool enabled = !Mk64Settings3DSGetShowFpsEnabled();
                    Mk64Settings3DSSetShowFpsEnabled(enabled);
                    SaveChangedSetting(enabled ? "FPS DISPLAY ON" : "FPS DISPLAY OFF");
                    break;
                }
                case 2: {
                    const bool enabled = !Mk64Settings3DSGetOverlayEnabled();
                    Mk64Settings3DSSetOverlayEnabled(enabled);
                    SaveChangedSetting(enabled ? "DIAGNOSTIC OVERLAY OPEN" : "DIAGNOSTIC OVERLAY CLOSED");
                    break;
                }
            }
            break;
        default:
            break;
    }
    sUi.bottomDirty = true;
}

bool PointInside(uint16_t x, uint16_t y, int left, int top, int width, int height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

void HandleOptionsTouch(uint16_t x, uint16_t y) {
    if (Mk64Settings3DSGetOverlayEnabled()) {
        if (PointInside(x, y, 84, 207, 152, 30)) {
            Mk64Settings3DSSetOverlayEnabled(false);
            SaveChangedSetting("BACK TO DEVELOPER OPTIONS");
        }
        return;
    }
    if (y >= static_cast<uint16_t>(kOptionsTabY) &&
        y < static_cast<uint16_t>(kOptionsTabY + 28.0f)) {
        const size_t tabIndex = std::min<size_t>(x / 80U, 3U);
        SetTab(static_cast<OptionsTab>(tabIndex));
        return;
    }
    const uint8_t rows = RowCount(sUi.tab);
    for (uint8_t row = 0; row < rows; ++row) {
        const int top = static_cast<int>(kOptionsRowY + row * kOptionsRowStep);
        if (PointInside(x, y, 12, top, 296, 33)) {
            sUi.selectedRow = row;
            ActivateSelectedRow(1);
            return;
        }
    }
    if (PointInside(x, y, 84, 207, 152, 30)) {
        DismissOptions(mk64_3ds::ResolveModalDismissAction(true,
                                                          sUi.modalOpenedFromPause));
    }
}

void HandleModalInput(const Mk64DiagnosticsInput3DS& input) {
    const mk64_3ds::ModalDismissAction3DS dismissAction =
        mk64_3ds::ResolveModalDismissAction(
            (input.downMask & (KEY_B | KEY_START)) != 0,
            sUi.modalOpenedFromPause);
    if (dismissAction != mk64_3ds::ModalDismissAction3DS::None) {
        // Back/Continue is independent of the selected tab and row. Routing it
        // through ActivateSelectedRow used to turn B into MEMORY DUMP whenever
        // Developer row zero happened to be selected.
        DismissOptions(dismissAction);
        return;
    }

    if (Mk64Settings3DSGetOverlayEnabled()) {
        if ((input.downMask & KEY_TOUCH) != 0) {
            HandleOptionsTouch(input.touchX, input.touchY);
        }
        return;
    }
    if ((input.downMask & KEY_L) != 0) {
        const int tab = static_cast<int>(sUi.tab);
        SetTab(static_cast<OptionsTab>((tab + static_cast<int>(OptionsTab::Count) - 1) %
                                       static_cast<int>(OptionsTab::Count)));
    } else if ((input.downMask & KEY_R) != 0) {
        const int tab = static_cast<int>(sUi.tab);
        SetTab(static_cast<OptionsTab>((tab + 1) % static_cast<int>(OptionsTab::Count)));
    }

    const uint8_t rows = RowCount(sUi.tab);
    if ((input.downMask & KEY_DUP) != 0) {
        sUi.selectedRow = static_cast<uint8_t>((sUi.selectedRow + rows - 1U) % rows);
        sUi.bottomDirty = true;
    } else if ((input.downMask & KEY_DDOWN) != 0) {
        sUi.selectedRow = static_cast<uint8_t>((sUi.selectedRow + 1U) % rows);
        sUi.bottomDirty = true;
    }
    if (sUi.tab == OptionsTab::Game) {
        // Continue and Quit are actions, not adjustable values. Requiring A
        // prevents an ordinary left/right navigation press on QUIT from
        // ending the race accidentally.
        if ((input.downMask & KEY_A) != 0) ActivateSelectedRow(1);
    } else if ((input.downMask & KEY_DLEFT) != 0) {
        ActivateSelectedRow(-1);
    } else if ((input.downMask & (KEY_DRIGHT | KEY_A)) != 0) {
        ActivateSelectedRow(1);
    }
    if ((input.downMask & KEY_TOUCH) != 0) HandleOptionsTouch(input.touchX, input.touchY);
}

void UpdateFpsCounter() {
    const uint64_t now = osGetTime();
    if (sUi.fpsWindowStartedAt == 0) sUi.fpsWindowStartedAt = now;
    ++sUi.fpsWindowFrames;
    const uint64_t elapsed = now - sUi.fpsWindowStartedAt;
    if (elapsed < kFpsWindowMilliseconds) return;

    sUi.currentFps = static_cast<float>(sUi.fpsWindowFrames) * 1000.0f /
                     static_cast<float>(std::max<uint64_t>(elapsed, 1));
    sUi.fpsHistory[sUi.fpsHistoryNext] = { sUi.fpsWindowFrames, elapsed };
    sUi.fpsHistoryNext = (sUi.fpsHistoryNext + 1U) % sUi.fpsHistory.size();
    sUi.fpsHistorySize = std::min(sUi.fpsHistorySize + 1U, sUi.fpsHistory.size());
    uint64_t totalMilliseconds = 0;
    uint64_t totalFrames = 0;
    for (size_t i = 0; i < sUi.fpsHistorySize; ++i) {
        totalMilliseconds += sUi.fpsHistory[i].milliseconds;
        totalFrames += sUi.fpsHistory[i].frames;
    }
    sUi.averageFps = totalMilliseconds == 0 ? 0.0f
        : static_cast<float>(totalFrames) * 1000.0f / static_cast<float>(totalMilliseconds);
    sUi.fpsWindowStartedAt = now;
    sUi.fpsWindowFrames = 0;
    if (Mk64Settings3DSGetOverlayEnabled()) sUi.bottomDirty = true;
}

void DrawStatus() {
    if (sUi.status[0] == '\0' || osGetTime() >= sUi.statusExpiresAt) return;
    DrawText(sUi.status, 160.0f, 191.0f, 0.62f, C2D_Color32(255, 230, 92, 255),
             C2D_AlignCenter, 0.86f, true);
}

void DrawGameSelect() {
    DrawDimMenuBackground();
    if (sUi.gameSelectOption.initialized) {
        DrawTexture(sUi.gameSelectOption, 44.5f, 106.0f, 87.0f, 28.5f, 0.7f);
    } else {
        DrawText("L OPTION", 88.0f, 111.0f, 0.8f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter, 0.7f, true);
    }
    if (sUi.gameSelectData.initialized) {
        DrawTexture(sUi.gameSelectData, 188.5f, 106.0f, 87.0f, 28.5f, 0.7f);
    } else {
        DrawText("R DATA", 232.0f, 111.0f, 0.8f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter, 0.7f, true);
    }
    DrawText(kBuildVersion, 314.0f, 222.0f, 0.46f,
             C2D_Color32(155, 255, 171, 255), C2D_AlignRight, 0.76f, true);
}

void DrawTimerDigits(float x, float y, float scale) {
    if (!sUi.raceHud.timerDigits.initialized) return;
    int hundredthsTotal = std::clamp<int>(sUi.game.courseTimerCentiseconds, 0, 599999);
    const int minutes = hundredthsTotal / 6000;
    hundredthsTotal %= 6000;
    const int seconds = hundredthsTotal / 100;
    const int hundredths = hundredthsTotal % 100;
    const std::array<uint8_t, 8> glyphs = {
        static_cast<uint8_t>((minutes / 10) % 10), static_cast<uint8_t>(minutes % 10), 10,
        static_cast<uint8_t>(seconds / 10), static_cast<uint8_t>(seconds % 10), 11,
        static_cast<uint8_t>(hundredths / 10), static_cast<uint8_t>(hundredths % 10),
    };
    for (size_t index = 0; index < glyphs.size(); ++index) {
        DrawTextureRegion(sUi.raceHud.timerDigits, glyphs[index] * 8.0f, 0.0f, 8.0f, 16.0f,
                          x + index * 8.0f * scale, y, 8.0f * scale, 16.0f * scale, 0.7f);
    }
}

void DrawMinimap() {
    if (!sUi.minimap.initialized) return;
    constexpr float areaX = 165.0f;
    constexpr float areaY = 48.0f;
    constexpr float areaWidth = 149.0f;
    constexpr float areaHeight = 184.0f;
    const float scale = std::min(areaWidth / sUi.minimap.width, areaHeight / sUi.minimap.height);
    const float mapWidth = sUi.minimap.width * scale;
    const float mapHeight = sUi.minimap.height * scale;
    const float mapX = areaX + (areaWidth - mapWidth) * 0.5f;
    const float mapY = areaY + (areaHeight - mapHeight) * 0.5f;
    C2D_ImageTint tint = {};
    C2D_PlainImageTint(&tint,
                       C2D_Color32(sUi.game.minimapRed, sUi.game.minimapGreen,
                                   sUi.game.minimapBlue, 230),
                       1.0f);
    DrawTexture(sUi.minimap, mapX, mapY, mapWidth, mapHeight, 0.55f, &tint,
                sUi.game.mirrorMode);

    const float logicalWidth = sUi.game.minimapWidth > 0 ? sUi.game.minimapWidth : sUi.minimap.width;
    const float logicalHeight = sUi.game.minimapHeight > 0 ? sUi.game.minimapHeight : sUi.minimap.height;

    const float finishX = (sUi.game.minimapPlayerX + sUi.game.minimapFinishlineX) /
                          logicalWidth;
    const float finishY = (sUi.game.minimapPlayerY + sUi.game.minimapFinishlineY) /
                          logicalHeight;
    if (sUi.raceHud.finishLine.initialized && finishX >= 0.0f && finishX <= 1.0f &&
        finishY >= 0.0f && finishY <= 1.0f) {
        DrawTexture(sUi.raceHud.finishLine, mapX + finishX * mapWidth - 4.0f,
                    mapY + finishY * mapHeight - 4.0f, 8.0f, 8.0f, 0.66f);
    }

    for (size_t playerId = 0; playerId < MK64_BOTTOM_UI_RACER_COUNT; ++playerId) {
        const Mk64BottomUIRacer3DS& racer = sUi.game.racers[playerId];
        if (!racer.active) continue;
        const float localX = sUi.game.minimapPlayerX + racer.worldX * sUi.game.minimapPlayerScale;
        const float localY = sUi.game.minimapPlayerY + racer.worldZ * sUi.game.minimapPlayerScale;
        const float markerX = mapX + (localX / logicalWidth) * mapWidth;
        const float markerY = mapY + (localY / logicalHeight) * mapHeight;
        if (markerX < areaX || markerX > areaX + areaWidth || markerY < areaY || markerY > areaY + areaHeight) {
            continue;
        }
        const int character = racer.characterId >= 0 && racer.characterId < 8 ? racer.characterId : 0;
        UiTexture& marker = sUi.raceHud.minimapKarts[static_cast<size_t>(character)];
        const uint16_t heading = static_cast<uint16_t>(
            static_cast<uint16_t>(racer.rotationY) + 0x8000U);
        DrawTextureRotated(marker, markerX, markerY, 8.0f, 8.0f,
                           racer.rank == 0 ? 0.72f : 0.69f,
                           heading * kBinaryAngleToRadians);
    }
}

void DrawRaceHud() {
    DrawRaceBackground(204.0f);
    const bool positionLapOnTop =
        sUi.game.topHudRenderMode == MK64_TOP_HUD_RENDER_POSITION_LAP;

    // Preserve the original 320x240 HUD scale: lap at upper left, the live
    // item-window/roulette state in the center, and time at upper right.
    if (sUi.game.gameMode != 3 && !positionLapOnTop) {
        DrawTexture(sUi.raceHud.lapLabel, 13.0f, 15.0f, 40.0f, 10.0f, 0.7f);
        const int lap = std::clamp<int>(sUi.game.currentLap, 1, 3) - 1;
        DrawTexture(sUi.raceHud.lapCounts[static_cast<size_t>(lap)],
                    56.0f, 12.0f, 34.0f, 17.0f, 0.7f);
    }
    if (sUi.game.itemWindowVisible) {
        const int item = std::clamp<int>(sUi.game.itemTextureIndex, 0,
                                         static_cast<int>(sUi.raceHud.items.size()) - 1);
        DrawTexture(sUi.raceHud.items[static_cast<size_t>(item)],
                    140.0f, 5.0f, 40.0f, 32.0f, 0.7f);
    }
    DrawTexture(sUi.raceHud.timeLabel, 197.0f, 13.0f, 32.0f, 16.0f, 0.7f);
    DrawTimerDigits(231.0f, 13.0f, 1.0f);

    for (size_t rank = 0; rank < sUi.game.standingCount; ++rank) {
        const int character = sUi.game.standingCharacterIds[rank];
        if (character < 0 || character >= 8 || sUi.game.standingNativeY[rank] < 0.0f) continue;
        // Native X/Y and direction are the same animated values consumed by
        // func_80050320. Only shift Y below the new top row.
        const bool expanded = positionLapOnTop && !sUi.game.raceFinished;
        const float portraitSize = expanded ? 40.0f : 32.0f;
        const float portraitHalf = portraitSize * 0.5f;
        const float centerX = sUi.game.standingNativeX[rank];
        const float centerY = sUi.game.standingNativeY[rank] +
                              (sUi.game.raceFinished ? 8.0f : (expanded ? 28.0f : 24.0f));
        const float x = centerX - portraitHalf;
        const float y = centerY - portraitHalf;
        const bool isPlayer = sUi.game.standingPlayerIds[rank] == 0;
        const bool unknown = sUi.game.standingUnknown[rank];
        const uint8_t portraitAlpha = unknown ? sUi.game.standingAlpha
                                              : (isPlayer ? 255 : sUi.game.standingAlpha);
        C2D_DrawRectSolid(x, y, 0.48f, portraitSize, portraitSize,
                          C2D_Color32(0, 0, 0, portraitAlpha));
        C2D_ImageTint portraitTint = {};
        // Preserve the native CI8/TLUT RGB and apply only the ranking fade.
        // A solid white tint at blend=1 replaces the complete image color and
        // was the reason these slots appeared as white/gray boxes in hardware
        // dumps even though the correct portrait textures were loaded.
        C2D_AlphaImageTint(&portraitTint, static_cast<float>(portraitAlpha) / 255.0f);
        UiTexture& portrait = unknown ? sUi.raceHud.questionPortrait
                                      : sUi.raceHud.portraits[static_cast<size_t>(character)];
        DrawTexture(portrait,
                    x, y, portraitSize, portraitSize, 0.6f, &portraitTint);
        if (unknown) continue;
        if (isPlayer) {
            C2D_ImageTint borderTint = {};
            C2D_PlainImageTint(&borderTint,
                               C2D_Color32(sUi.game.playerBorderRed,
                                           sUi.game.playerBorderGreen,
                                           sUi.game.playerBorderBlue, 255), 1.0f);
            DrawTexture(sUi.raceHud.portraitBorder, x, y, portraitSize, portraitSize, 0.64f,
                        &borderTint);
        }
        const float rankSize = expanded ? 20.0f : 16.0f;
        const float rankX = sUi.game.standingNativeDirection[rank] < 0.0f
                                ? centerX + portraitSize * 0.28f
                                : centerX - portraitSize * 0.28f;
        DrawTexture(sUi.raceHud.standingRanks[rank], rankX - rankSize * 0.5f,
                    centerY - rankSize * 0.0625f, rankSize, rankSize, 0.66f,
                    &portraitTint);
    }

    if (sUi.game.gameMode != 3 && !positionLapOnTop && sUi.game.currentPlaceVisible) {
        const size_t place = std::min<size_t>(sUi.game.currentPlaceIndex,
                                              sUi.raceHud.places.size() - 1U);
        const float scale = std::clamp(sUi.game.currentPlaceScale, 0.25f, 1.0f);
        const float width = 128.0f * scale;
        const float height = 64.0f * scale;
        C2D_ImageTint placeTint = {};
        C2D_PlainImageTint(&placeTint,
                           C2D_Color32(255, sUi.game.currentPlaceGreen, 0, 255), 1.0f);
        DrawTexture(sUi.raceHud.places[place],
                    sUi.game.currentPlaceNativeX - width * 0.5f,
                    sUi.game.currentPlaceNativeY - height * 0.5f,
                    width, height, 0.68f, &placeTint);
    }
    DrawMinimap();
}

const char* TabName(OptionsTab tab) {
    switch (tab) {
        case OptionsTab::Game: return "GAME";
        case OptionsTab::Screen: return "SCREEN";
        case OptionsTab::Gameplay: return "GAMEPLAY";
        case OptionsTab::Developer: return "DEVELOPER";
        default: return "";
    }
}

void GetRowText(OptionsTab tab, uint8_t row, const char** label, char* value, size_t valueSize) {
    *label = "";
    value[0] = '\0';
    switch (tab) {
        case OptionsTab::Game:
            if (sUi.modalOpenedFromPause) {
                *label = row == 0 ? "CONTINUE GAME" : "QUIT";
            } else {
                *label = "CLOSE OPTIONS";
                std::snprintf(value, valueSize, "A OR TOUCH");
            }
            break;
        case OptionsTab::Screen:
            if (row == 0) {
                *label = "ASPECT RATIO";
                std::snprintf(value, valueSize, "%s",
                              Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE
                                  ? "WIDE" : "ORIGINAL 4:3");
            } else if (row == 1) {
                *label = "TOP HUD";
                std::snprintf(value, valueSize, "%s", Mk64Settings3DSGetTopHudEnabled() ? "ON" : "OFF");
            }
            break;
        case OptionsTab::Gameplay:
            if (row == 0) {
                *label = "TURBO SPEED";
                std::snprintf(value, valueSize, "X%u  C-STICK", Mk64Settings3DSGetTurboMultiplier());
            } else {
                *label = "MASTER VOLUME";
                std::snprintf(value, valueSize, "%u PCT", Mk64Settings3DSGetMasterVolumePercent());
            }
            break;
        case OptionsTab::Developer:
            if (row == 0) {
                *label = "MEMORY DUMP";
                std::snprintf(value, valueSize, "CREATE");
            } else if (row == 1) {
                *label = "SHOW FPS";
                std::snprintf(value, valueSize, "%s", Mk64Settings3DSGetShowFpsEnabled() ? "ON" : "OFF");
            } else {
                *label = "OVERLAY";
                std::snprintf(value, valueSize, "%s", Mk64Settings3DSGetOverlayEnabled() ? "OPEN" : "CLOSED");
            }
            break;
        default:
            break;
    }
}

void DrawOptions() {
    if (sUi.game.racing) DrawRaceBackground(205.0f); else DrawDimMenuBackground();
    C2D_DrawRectSolid(0.0f, 0.0f, 0.25f, kBottomWidth, kBottomHeight,
                      C2D_Color32(0, 0, 0, 82));
    for (int index = 0; index < static_cast<int>(OptionsTab::Count); ++index) {
        const bool selected = index == static_cast<int>(sUi.tab);
        DrawText(TabName(static_cast<OptionsTab>(index)), index * 80.0f + 40.0f,
                 kOptionsTabY + 4.0f, index == 3 ? 0.5f : 0.58f,
                 selected ? C2D_Color32(255, 225, 75, 255)
                          : C2D_Color32(210, 220, 220, 230),
                 C2D_AlignCenter, 0.72f, selected);
        if (selected && sUi.selectionTriangle.initialized) {
            DrawTexture(sUi.selectionTriangle, index * 80.0f + 34.0f, 55.0f,
                        12.0f, 7.0f, 0.71f);
        }
    }
    C2D_DrawRectSolid(8.0f, 64.0f, 0.42f, 304.0f, 1.0f, C2D_Color32(255, 235, 150, 92));

    const uint8_t rows = RowCount(sUi.tab);
    for (uint8_t row = 0; row < rows; ++row) {
        const float y = kOptionsRowY + row * kOptionsRowStep;
        const char* label = "";
        char value[48] = {};
        GetRowText(sUi.tab, row, &label, value, sizeof(value));
        const bool selected = row == sUi.selectedRow;
        if (selected && sUi.selectionTriangle.initialized) {
            DrawTexture(sUi.selectionTriangle, 7.0f, y + 10.0f, 12.0f, 7.0f, 0.73f);
        }
        DrawText(label, 23.0f, y + 5.0f, 0.7f,
                 selected ? C2D_Color32(255, 229, 79, 255)
                          : C2D_Color32(238, 238, 230, 245),
                 C2D_AlignLeft, 0.74f, selected);
        DrawText(value, 304.0f, y + 6.0f, 0.62f,
                 selected ? C2D_Color32(167, 255, 151, 255)
                          : C2D_Color32(198, 222, 210, 240),
                 C2D_AlignRight, 0.74f);
        C2D_DrawRectSolid(22.0f, y + 29.0f, 0.44f, 282.0f, 1.0f,
                          C2D_Color32(255, 255, 255, selected ? 75 : 35));
    }
    DrawText(sUi.modalOpenedFromPause ? "START OR B  CONTINUE" : "B BACK",
             160.0f, 214.0f, 0.68f, C2D_Color32(235, 238, 225, 255),
             C2D_AlignCenter, 0.74f, true);
    DrawStatus();
}

void DrawDeveloperOverlay() {
    if (sUi.game.racing) DrawRaceBackground(216.0f); else DrawDimMenuBackground();
    C2D_DrawRectSolid(7.0f, 5.0f, 0.4f, 306.0f, 197.0f, C2D_Color32(0, 0, 0, 230));
    DrawText("DEVELOPER OVERLAY", 160.0f, 10.0f, 0.72f,
             C2D_Color32(255, 215, 70, 255), C2D_AlignCenter, 0.8f, true);

    const struct mallinfo heap = mallinfo();
    const uint32_t buffered = Mk64Audio3DSBufferedFrames != nullptr ? Mk64Audio3DSBufferedFrames() : 0;
    const uint32_t queued = Mk64Audio3DSQueuedCount != nullptr ? Mk64Audio3DSQueuedCount() : 0;
    const uint32_t dropped = Mk64Audio3DSDroppedCount != nullptr ? Mk64Audio3DSDroppedCount() : 0;
    size_t textureSlots = 0;
    size_t liveTextures = 0;
    size_t textureBytes = 0;
    size_t shaders = 0;
    size_t clipBytes = 0;
    if (Mk64Graphics3DSGetDebugStats != nullptr) {
        Mk64Graphics3DSGetDebugStats(&textureSlots, &liveTextures, &textureBytes, &shaders, &clipBytes);
    }

    uint32_t interpolationAttempts = 0;
    uint32_t interpolatedFrames = 0;
    uint32_t retainedFrames = 0;
    uint32_t matchedMatrices = 0;
    uint32_t totalMatrices = 0;
    if (Mk64FrameInterpolation3DSGetStats != nullptr) {
        Mk64FrameInterpolation3DSGetStats(&interpolationAttempts, &interpolatedFrames,
                                          &retainedFrames, &matchedMatrices, &totalMatrices);
    }

    std::array<std::array<char, 96>, 12> lines = {};
    std::snprintf(lines[0].data(), lines[0].size(), "VERSION   %s", kBuildVersion);
    std::snprintf(lines[1].data(), lines[1].size(), "MODEL     %s",
                  Mk64Diagnostics3DSGetSystemModelName());
    std::snprintf(lines[2].data(), lines[2].size(), "FPS 2S    %.1f", sUi.currentFps);
    std::snprintf(lines[3].data(), lines[3].size(), "FPS 10S   %.1f", sUi.averageFps);
    const uint16_t configuredWidth = Mk64Diagnostics3DSSupportsWideMode()
                                         ? Mk64Settings3DSGetResolutionWidth()
                                         : 400;
    const uint32_t activeWidth = Mk64Graphics3DSResolvedOutputWidth != nullptr
                                     ? Mk64Graphics3DSResolvedOutputWidth()
                                     : configuredWidth;
    const char* aspect = Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_WIDE
                             ? "WIDE"
                             : "4:3";
    if (configuredWidth == activeWidth) {
        std::snprintf(lines[4].data(), lines[4].size(), "DISPLAY   %lu PX ACTIVE  %s",
                      static_cast<unsigned long>(activeWidth), aspect);
    } else {
        std::snprintf(lines[4].data(), lines[4].size(),
                      "DISPLAY   %lu PX  PENDING %u  %s",
                      static_cast<unsigned long>(activeWidth), configuredWidth, aspect);
    }
    std::snprintf(lines[5].data(), lines[5].size(), "MEM FREE  APP %luK  LIN %luK  VRAM %luK",
                  static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024U),
                  static_cast<unsigned long>(linearSpaceFree() / 1024U),
                  static_cast<unsigned long>(vramSpaceFree() / 1024U));
    std::snprintf(lines[6].data(), lines[6].size(), "HEAP      USED %dK  FREE %dK", heap.uordblks / 1024,
                  heap.fordblks / 1024);
    std::snprintf(lines[7].data(), lines[7].size(), "RENDER    TEX %lu OF %lu  %luK  SH %lu",
                  static_cast<unsigned long>(liveTextures), static_cast<unsigned long>(textureSlots),
                  static_cast<unsigned long>(textureBytes / 1024U), static_cast<unsigned long>(shaders));
    std::snprintf(lines[8].data(), lines[8].size(), "AUDIO     BUF %lu  QUE %lu  DROP %lu",
                  static_cast<unsigned long>(buffered), static_cast<unsigned long>(queued),
                  static_cast<unsigned long>(dropped));
    std::snprintf(lines[9].data(), lines[9].size(), "STATE     %ld  %s",
                  static_cast<long>(sUi.game.gameState), sUi.game.paused ? "PAUSED" : "RUNNING");
    std::snprintf(lines[10].data(), lines[10].size(), "COURSE    %s",
                  sUi.game.trackName == nullptr ? "UNKNOWN" : sUi.game.trackName);
    if (interpolationAttempts == 0) {
        std::snprintf(lines[11].data(), lines[11].size(), "INTERP    DISABLED");
    } else {
        std::snprintf(lines[11].data(), lines[11].size(),
                      "INTERP    MID %lu RET %lu  LAST %lu OF %lu",
                      static_cast<unsigned long>(interpolatedFrames),
                      static_cast<unsigned long>(retainedFrames),
                      static_cast<unsigned long>(matchedMatrices),
                      static_cast<unsigned long>(totalMatrices));
    }
    (void) clipBytes;
    for (size_t i = 0; i < lines.size(); ++i) {
        DrawText(lines[i].data(), 15.0f, 35.0f + i * 13.8f, 0.52f,
                 i == 2 ? C2D_Color32(120, 255, 145, 255)
                        : C2D_Color32(218, 228, 225, 255));
    }
    DrawText("B BACK", 160.0f, 214.0f, 0.66f, C2D_Color32(255, 225, 80, 255),
             C2D_AlignCenter, 0.82f, true);
}

void DrawTopFps(C3D_RenderTarget* topTarget) {
    if (topTarget == nullptr || !Mk64Settings3DSGetShowFpsEnabled()) return;
    // Citro2D keeps a 400x240 logical projection for the top screen even when
    // Fast3D renders into the 800-wide high-density target.
    constexpr float topWidth = 400.0f;
    char fps[32] = {};
    std::snprintf(fps, sizeof(fps), "FPS %.1f", sUi.currentFps);
    DrawText(fps, topWidth - 8.0f, 6.0f, 0.62f,
             C2D_Color32(125, 255, 145, 255), C2D_AlignRight, 0.9f, true);
}

void DrawBottom() {
    if (Mk64Settings3DSGetOverlayEnabled()) {
        DrawDeveloperOverlay();
    } else if (sUi.modalOpen) {
        DrawOptions();
    } else {
        switch (sUi.view) {
            case BaseView::GameSelect: DrawGameSelect(); break;
            case BaseView::RaceHud: DrawRaceHud(); break;
            case BaseView::Paused: DrawRaceBackground(204.0f); break;
            case BaseView::Background:
            default: DrawDimMenuBackground(); break;
        }
        DrawStatus();
    }
}

void PrepareC2DBatch() {
    // Fast3D can leave all six TEV stages, the TEV buffer update mask and
    // alpha tests configured for an N64 combiner. Reset that inherited state
    // before Citro2D installs its own shader and batching state. In particular,
    // this keeps I4/IA4 alpha masks from becoming solid colored quads.
    C3D_TexEnvBufUpdate(C3D_Both, 0);
    C3D_TexEnvBufColor(0xFFFFFFFF);
    for (int stage = 0; stage < 6; ++stage) C3D_TexEnvInit(C3D_GetTexEnv(stage));
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    C2D_Prepare();
}

void DrawBottomBatch() {
    // Clear before any Citro2D objects are queued. C2D_TargetClear after a top
    // batch used to split that pending batch and could invalidate its texture
    // state on hardware.
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    C3D_RenderTargetClear(sUi.bottomTarget, C3D_CLEAR_ALL, C2D_Color32(0, 0, 0, 255), 0);
    PrepareC2DBatch();
    Mk64Graphics3DSMarkExternalLinearBuffersDirty();
    C2D_SceneBegin(sUi.bottomTarget);
    DrawBottom();
    C2D_Flush();
}

void DrawTopFpsBatch(C3D_RenderTarget* topTarget) {
    if (topTarget == nullptr || !Mk64Settings3DSGetShowFpsEnabled()) return;
    // Preserve the completed game color buffer and clear only reverse-depth so
    // the overlay cannot be hidden behind scene geometry.
    // A lower-screen C2D batch may already be pending in its private linear
    // buffer. Submit it with Citro3D's coherency pass before starting the top
    // overlay batch; this path is disabled unless the developer FPS display is
    // explicitly enabled.
    C3D_FrameSplit(0);
    C3D_RenderTargetClear(topTarget, C3D_CLEAR_DEPTH, 0, 0);
    PrepareC2DBatch();
    Mk64Graphics3DSMarkExternalLinearBuffersDirty();
    C2D_SceneBegin(topTarget);
    DrawTopFps(topTarget);
    C2D_Flush();
}

} // namespace

extern "C" bool Mk64BottomUI3DSInit() {
    if (sUi.initialized) return true;
    sUi = {};
    if (!C2D_Init(kC2DObjectCapacity)) return false;
    sUi.bottomTarget = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if (sUi.bottomTarget == nullptr) {
        C2D_Fini();
        return false;
    }
    C3D_RenderTargetSetOutput(sUi.bottomTarget, GFX_BOTTOM, GFX_LEFT, kTransferFlags);
    if (!LoadFontAtlas()) {
        C3D_RenderTargetDelete(sUi.bottomTarget);
        sUi.bottomTarget = nullptr;
        C2D_Fini();
        sUi = {};
        return false;
    }
    C2D_Prepare();
    sUi.initialized = true;
    sUi.bottomDirty = true;
    sUi.modalOpen = Mk64Settings3DSGetOverlayEnabled();
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    sUi.view = GetBaseView(sUi.game);
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground);
    LoadTexture(kGameSelectOptionResource, sUi.gameSelectOption);
    LoadTexture(kGameSelectDataResource, sUi.gameSelectData);
    LoadTexture(kSelectionTriangleResource, sUi.selectionTriangle);
    // Preload the complete, bounded lower HUD while menus are starting. No
    // archive reads or C3D allocations are then needed when a race begins,
    // an item changes, or the standings reorder.
    LoadRaceHudTextures();
    // Stage the selected course art in menus so entering a race never has to
    // allocate or decode these lower-screen textures on its first frame.
    LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
    LoadTexture(sUi.game.minimapTexture, sUi.minimap);
    Mk64GameState3DSSetTopHudEnabled(Mk64Settings3DSGetTopHudEnabled());
    return true;
}

extern "C" void Mk64BottomUI3DSShutdown() {
    if (!sUi.initialized) return;
    // The final C3D_FrameEnd is asynchronous. Detaching an output while no
    // frame is active drains Citro3D's GPU queue; FrameSync alone only waits
    // for VBlank and is not a resource-lifetime fence.
    if (sUi.bottomTarget != nullptr) C3D_RenderTargetDetachOutput(sUi.bottomTarget);
    Mk64GameState3DSApplyTurbo(false, 1);
    DrainRetiredTextures();
    DeleteRaceHudTextures();
    DeleteTexture(sUi.minimap);
    DeleteTexture(sUi.coursePreview);
    DeleteTexture(sUi.menuBackground);
    DeleteTexture(sUi.gameSelectData);
    DeleteTexture(sUi.gameSelectOption);
    DeleteTexture(sUi.selectionTriangle);
    DeleteFontAtlas();
    if (sUi.bottomTarget != nullptr) C3D_RenderTargetDelete(sUi.bottomTarget);
    C2D_Fini();
    sUi = {};
}

extern "C" void Mk64BottomUI3DSPrepareFrame() {
    if (!sUi.initialized) return;
    if (sUi.status[0] != '\0' && osGetTime() >= sUi.statusExpiresAt) {
        sUi.status[0] = '\0';
        sUi.statusExpiresAt = 0;
        sUi.bottomDirty = true;
    }
    const BaseView oldView = sUi.view;
    const bool wasPaused = sUi.game.paused;
    const int32_t oldMenuSelection = sUi.game.menuSelection;
    const size_t oldTrack = sUi.game.trackIndex;
    const char* oldBackground = sUi.game.mainBackgroundTexture;
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    sUi.view = GetBaseView(sUi.game);
    if (oldView != sUi.view || oldMenuSelection != sUi.game.menuSelection ||
        oldTrack != sUi.game.trackIndex ||
        oldBackground != sUi.game.mainBackgroundTexture) {
        sUi.bottomDirty = true;
    }
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground);
    // The selected track usually changes while still in Map Select, before
    // the RACING edge. Compare the actual resource names every frame so a
    // second race can never retain the previous course art. LoadTexture's
    // strcmp fast path makes the steady state allocation- and I/O-free.
    LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
    LoadTexture(sUi.game.minimapTexture, sUi.minimap);

    Mk64DiagnosticsInput3DS input = {};
    if (!Mk64Diagnostics3DSConsumeInput(&input)) {
        if (Mk64Diagnostics3DSReadInput(&input)) {
            input.downMask = input.heldMask & ~sUi.previousHeldKeys;
            input.upMask = sUi.previousHeldKeys & ~input.heldMask;
        } else {
            // If the diagnostic worker could not be created, retain the full
            // lower-screen interface instead of leaving it without controls.
            hidScanInput();
            input.heldMask = hidKeysHeld();
            input.downMask = hidKeysDown();
            input.upMask = hidKeysUp();
            circlePosition circle = {};
            circlePosition cstick = {};
            hidCircleRead(&circle);
            if (Mk64Diagnostics3DSIsNewModel()) hidCstickRead(&cstick);
            input.circleX = circle.dx;
            input.circleY = circle.dy;
            input.cstickX = cstick.dx;
            input.cstickY = cstick.dy;
            if ((input.heldMask & KEY_TOUCH) != 0) {
                touchPosition touch = {};
                hidTouchRead(&touch);
                input.touchX = touch.px;
                input.touchY = touch.py;
                input.touchHeld = true;
            }
        }
    }
    sUi.previousHeldKeys = input.heldMask;
    sUi.injectedGameKeys = 0;
    sUi.blockedGameKeys &= input.heldMask;

    const bool capturedAtFrameStart = sUi.modalOpen || Mk64Settings3DSGetOverlayEnabled();
    bool openedThisFrame = false;
    if (!sUi.modalOpen && !Mk64Settings3DSGetOverlayEnabled()) {
        if (!wasPaused && sUi.game.paused) {
            // Pause replaces the race HUD with the same lower-screen Options
            // panel used from Game Select. It opens once on the pause edge;
            // B or START now invokes the single native Continue action.
            OpenOptions(true);
            openedThisFrame = true;
        } else if (sUi.game.gameSelectVisible && (input.downMask & KEY_L) != 0) {
            OpenOptions(false);
            openedThisFrame = true;
        } else if (sUi.game.paused && (input.downMask & KEY_L) != 0) {
            OpenOptions(true);
            openedThisFrame = true;
        } else if ((input.downMask & KEY_TOUCH) != 0) {
            if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 38, 96, 100, 49)) {
                OpenOptions(false);
                openedThisFrame = true;
            } else if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 182, 96, 100, 49)) {
                sUi.injectedGameKeys |= KEY_R;
            } else if (sUi.game.paused && PointInside(input.touchX, input.touchY, 108, 96, 104, 48)) {
                OpenOptions(true);
                openedThisFrame = true;
            }
        }
    }
    if ((sUi.modalOpen || Mk64Settings3DSGetOverlayEnabled()) && !openedThisFrame) {
        HandleModalInput(input);
    }

    // A button that opened or closed the modal must not fall through to the
    // vanilla menu later in this same simulation tick. Keep every captured
    // digital key blocked until the HID worker observes its release.
    sUi.captureGameInputThisFrame = capturedAtFrameStart || openedThisFrame;
    if (sUi.captureGameInputThisFrame || sUi.modalOpen || Mk64Settings3DSGetOverlayEnabled()) {
        sUi.blockedGameKeys |= input.heldMask | input.downMask;
    }

    if (sUi.modalOpen && !Mk64Settings3DSGetOverlayEnabled() &&
        !sUi.game.gameSelectVisible && !sUi.game.paused) {
        CloseOptions();
    }

    Mk64GameState3DSSetTopHudEnabled(Mk64Settings3DSGetTopHudEnabled());
    const uint8_t turbo = Mk64Settings3DSGetTurboMultiplier();
    const bool cstickHeld = std::abs(static_cast<int>(input.cstickX)) > kCStickTurboDeadzone ||
                            std::abs(static_cast<int>(input.cstickY)) > kCStickTurboDeadzone;
    const bool turboAvailable = sUi.game.racing && !sUi.game.paused && !sUi.modalOpen && turbo > 1;
    sUi.consumesCStick = turboAvailable;
    Mk64GameState3DSApplyTurbo(turboAvailable && cstickHeld, turbo);

    // The game snapshot remains 30 Hz on both profiles. New 3DS redraws every
    // snapshot; Old 3DS redraws the secondary race HUD at 10 Hz to reserve CPU
    // and GPU bandwidth for the top-screen race. Menu and modal changes still
    // mark the target dirty immediately above.
    if (sUi.view == BaseView::RaceHud) {
        const uint32_t divisor = Mk64Graphics3DSBottomHudRefreshDivisor();
        if (sUi.raceHudRefreshPhase == 0) sUi.bottomDirty = true;
        sUi.raceHudRefreshPhase = static_cast<uint8_t>((sUi.raceHudRefreshPhase + 1U) % divisor);
    } else {
        sUi.raceHudRefreshPhase = 0;
    }
}

extern "C" void Mk64BottomUI3DSDraw(void* existingTopTarget) {
    if (!sUi.initialized || sUi.bottomTarget == nullptr || existingTopTarget == nullptr) return;
    // The renderer enters this function from a SYNCDRAW frame. Previous GPU
    // work is complete, so textures replaced during PrepareFrame are now safe
    // to release before issuing any new Citro2D commands.
    DrainRetiredTextures();
    Mk64BottomUI3DSRecordPresentation();
    const bool drawTopFps = existingTopTarget != nullptr && Mk64Settings3DSGetShowFpsEnabled();
    if (!sUi.bottomDirty && !drawTopFps) return;
    if (sUi.bottomDirty) {
        DrawBottomBatch();
        sUi.bottomDirty = false;
    }
    DrawTopFpsBatch(static_cast<C3D_RenderTarget*>(existingTopTarget));
}

extern "C" void Mk64BottomUI3DSRecordPresentation() {
    if (!sUi.initialized) return;
    UpdateFpsCounter();
}

extern "C" void Mk64BottomUI3DSDrawTopFps(void* existingTopTarget) {
    if (!sUi.initialized || existingTopTarget == nullptr ||
        !Mk64Settings3DSGetShowFpsEnabled()) {
        return;
    }
    DrawTopFpsBatch(static_cast<C3D_RenderTarget*>(existingTopTarget));
}

extern "C" uint32_t Mk64BottomUI3DSFilterGameKeys(uint32_t heldKeys) {
    if (!sUi.initialized) return heldKeys;
    if (sUi.modalOpen || Mk64Settings3DSGetOverlayEnabled() ||
        sUi.captureGameInputThisFrame) return 0;
    heldKeys &= ~sUi.blockedGameKeys;
    if (sUi.game.gameSelectVisible) heldKeys &= ~static_cast<uint32_t>(KEY_L);
    if (sUi.game.paused) heldKeys &= ~static_cast<uint32_t>(KEY_L);
    return heldKeys | sUi.injectedGameKeys;
}

extern "C" bool Mk64BottomUI3DSConsumesCStick() {
    return sUi.initialized && sUi.consumesCStick;
}

extern "C" bool Mk64BottomUI3DSIsModalOpen() {
    return sUi.initialized && (sUi.modalOpen || Mk64Settings3DSGetOverlayEnabled() ||
                               sUi.captureGameInputThisFrame);
}

extern "C" float Mk64BottomUI3DSGetCurrentFps() {
    return sUi.currentFps;
}

extern "C" float Mk64BottomUI3DSGetAverageFps() {
    return sUi.averageFps;
}
