#include "presentation_fps_3ds.hpp"
#include "native_texture_storage_3ds.hpp"
#include "improved_texture_pack_3ds.hpp"
#include "hud_layout_3ds.hpp"
#include "standings_presentation_3ds.hpp"
#include "frame_begin_3ds.hpp"
#include "menu_transition_3ds.hpp"
#include "menu_backdrop_3ds.hpp"
#include "menu_hint_3ds.hpp"
#include "art/menu_art_3ds.hpp"
#include "menu_hint_native_3ds.hpp"
#include "options_layout_3ds.hpp"
#include "texture_mode_3ds.hpp"
#include "art/option_icons_art_3ds.hpp"
#include "render_policy_3ds.hpp"
#include "bottom_ui_3ds.h"
#include "game_runtime_3ds.h"

#include "diagnostics_3ds.h"
#include "game_state_3ds.h"
#include "auto_mode_3ds.h"
#include "solo_modes_3ds.h"
#include "input_policy_3ds.hpp"
#include "turbo_policy_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "settings_3ds.h"
#include "race_clock_3ds.hpp"
#include "threat_warning_ui_3ds.hpp"
#include "system_3ds.h"
#include "updater.h"
#include "select_policy_3ds.hpp"
#include "title_credit_native_3ds.hpp"
#include "native_sprite_3ds.hpp"
#include "texture_lease_3ds.hpp"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <new>

extern "C" uint32_t Mk64Audio3DSBufferedFrames(void) __attribute__((weak));
extern "C" uint32_t Mk64Audio3DSQueuedCount(void) __attribute__((weak));
extern "C" uint32_t Mk64Audio3DSDroppedCount(void) __attribute__((weak));
extern "C" void Mk64Graphics3DSGetDebugStats(size_t*, size_t*, size_t*, size_t*, size_t*)
    __attribute__((weak));
extern "C" uint32_t Mk64Graphics3DSResolvedOutputWidth(void) __attribute__((weak));
extern "C" int32_t gGamestate;
extern "C" int32_t gGamestateNext;
extern "C" void Mk64FrameInterpolation3DSGetStats(uint32_t*, uint32_t*, uint32_t*, uint32_t*,
                                                   uint32_t*) __attribute__((weak));

namespace {

constexpr float kBottomWidth = 320.0f;
constexpr float kBottomHeight = 240.0f;
constexpr uint32_t kMaxUiTextureDimension = 512;
constexpr int kCStickTurboDeadzone = 30;
static_assert(mk64_3ds::SelectPolicy3DS::kChord == (KEY_L | KEY_R | KEY_SELECT));
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
constexpr const char* kGameSelectOptionResource = "__OTR__textures/texture_tkmk00/texture_l_option";
constexpr const char* kGameSelectDataResource = "__OTR__textures/texture_tkmk00/texture_r_data";
constexpr const char* kSelectionTriangleResource =
    "__OTR__textures/player_selection/texture_small_green_triangle";
constexpr const char* kHudLapResource = "__OTR__textures/common_data/common_texture_hud_lap";
constexpr const char* kHudTimeResource = "__OTR__textures/common_data/common_texture_hud_time";
constexpr const char* kHudTotalTimeResource = "__OTR__textures/common_data/common_texture_hud_total_time";
constexpr const char* kHudTimerDigitsResource =
    "__OTR__textures/common_data/common_texture_hud_normal_digit";
constexpr const char* kPortraitBorderResource =
    "__OTR__textures/common_data/common_texture_character_portrait_border";
constexpr const char* kStandingRankPaletteResource =
    "__OTR__textures/common_data/common_tlut_hud_type_C_rank_font";
constexpr const char* kMinimapFinishLineResource =
    "__OTR__textures/common_data/common_texture_minimap_finish_line";
constexpr const char* kMinimapDotResource =
    "__OTR__textures/common_data/common_texture_minimap_mario";

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

struct NativeUiVariant {
    Mk64UiTextures3DS style=MK64_UI_TEXTURES_3DS_MODIFIED;
    C3D_Tex texture = {};
    Tex3DS_SubTexture subTexture = {};
    uint16_t width=0, height=0, logicalWidth=0, logicalHeight=0;
};

struct UiTexture {
    std::array<NativeUiVariant,8> native{};
    uint8_t nativeCount=0;
    bool redBackground = false;
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

struct NativeUiFont {
    C3D_Tex texture{};
    bool initialized=false;
    std::array<std::array<Tex3DS_SubTexture,kFontGlyphCount>,9> glyphs{};
};

struct UiFontAtlas {
    C3D_Tex texture = {};
    // Persistent sampler view of the same allocation, not another atlas.
    // Citro2D retains texture pointers until its batch is flushed.
    C3D_Tex smoothTexture = {};
    bool initialized = false;
    std::array<FontGlyph, kFontGlyphCount> glyphs = {};
};

struct RaceHudTextures {
    bool loadAttempted = false;
    UiTexture lapLabel;
    std::array<UiTexture, 3> lapCounts = {};
    UiTexture timeLabel;
    UiTexture totalTimeLabel;
    UiTexture timerDigits;
    std::array<UiTexture, 8> places = {};
    std::array<UiTexture, 8> portraits = {};
    UiTexture questionPortrait;
    UiTexture portraitBorder;
    std::array<UiTexture, 8> standingRanks = {};
    std::array<UiTexture, 16> items = {};
    UiTexture finishLine;
    std::array<UiTexture, 8> minimapKarts = {};
    std::array<UiTexture, 8> minimapKartsWhite = {};
    UiTexture minimapDot;
    UiTexture minimapDotWhite;
};

struct BottomUiState {
    bool initialized = false;
    C3D_RenderTarget* bottomTarget = nullptr;
    UiFontAtlas font;
    UiFontAtlas fpsFont;
    NativeUiFont nativeFont;
    std::array<UiFontAtlas, 6> dataFonts;
    C3D_RenderTarget* lastTopTarget = nullptr;
    UiTexture menuBackground;
    UiTexture softMenuBackground;
    UiTexture optionIcons;
    // The same standalone native textures/subtextures as the race HUD, not
    // cropped atlas regions with a separate sampling/opacity path.
    std::array<UiTexture, mk64_3ds::kMenuCourseOrder.size()> menuMaps{};
    bool menuMapLoadAttempted = false;
    uint64_t conveyorElapsed = 0;
    uint64_t conveyorLastTick = 0;
    UiTexture coursePreview;
    UiTexture minimap;
    UiTexture gameSelectOption;
    UiTexture gameSelectData;
    UiTexture randomCupHint;
    UiTexture autoHint;
    mk64_3ds::AutoHintTween autoHintTween;
    std::array<UiTexture, 16> dataPreviews;
    std::array<UiTexture, 16> dataTitles;
    UiTexture dataHeading;
    char dataMapResource[192] = {};
    float dataMapX=0,dataMapY=0,dataMapWidth=0,dataMapHeight=0;
    int dataAction = 0;
    float dataScroll = 0;
    uint32_t dataDirection = 0;
    uint64_t dataRepeatAt = 0;
    UiTexture selectionTriangle;
    UiTexture titleCredit;
    RaceHudTextures raceHud;
    std::array<UiTexture, kRetiredTextureCapacity> retiredTextures = {};
    size_t retiredTextureCount = 0;
    Mk64BottomUIGameState3DS game = {};
    BaseView view = BaseView::Background;
    bool modalOpen = false;
    bool displayAdvanced = false;
    mk64_3ds::SelectPolicy3DS selectPolicy;
    int cachedMenuBlur = -1;
    bool modalOpenedFromPause = false;
    bool consumesCStick = false;
    mk64_3ds::TurboPolicy3DS turbo;
    bool bottomDirty = true;
    uint8_t raceHudRefreshPhase = 0;
    mk64_3ds::ThreatWarningTween3DS threatWarning;
    bool warningWasDrawn = false;
    OptionsTab tab = OptionsTab::Screen;
    uint8_t selectedRow = 0;
    uint32_t injectedGameKeys = 0;
    uint32_t blockedGameKeys = 0;
    bool resumeThrottleThisFrame = false;
    uint32_t previousHeldKeys = 0;
    bool captureGameInputThisFrame = false;
    uint64_t statusExpiresAt = 0;
    char status[64] = {};

    mk64_3ds::PresentationFps fps;
};

BottomUiState sUi;
bool sRenderSliderDragging = false;
bool sVolumeSliderDragging = false;
bool sLoadingBlanked = false;
bool sTurboSliderDragging = false;
void OpenUpdate();
void CloseUpdate();
void HandleUpdateInput(const Mk64DiagnosticsInput3DS& input);
bool UpdateIsOpen();
mk64_3ds::MenuTransition3DS sMenuTransition;
mk64_3ds::RaceTransition3DS sRaceTransition;

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
    for (unsigned i=0;i<texture.nativeCount;++i) C3D_TexDelete(&texture.native[i].texture);
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
                            UiTexture& destination, bool redBackground) {
    UiTexture missing;
    missing.redBackground = redBackground;
    std::snprintf(missing.resourceName, sizeof(missing.resourceName), "%s",
                  resourceName == nullptr ? "" : resourceName);
    std::snprintf(missing.paletteName, sizeof(missing.paletteName), "%s",
                  paletteName == nullptr ? "" : paletteName);
    ReplaceTexture(destination, missing);
}

#include "native_ui_3ds.inc.cpp"

bool LoadTexture(const char* resourceName, UiTexture& destination, const char* paletteName = nullptr,
                 bool redBackground = false) {
    if (resourceName == nullptr || resourceName[0] == '\0') return false;
    const char* normalizedPaletteName = paletteName == nullptr ? "" : paletteName;
    if (std::strcmp(destination.resourceName, resourceName) == 0 &&
        std::strcmp(destination.paletteName, normalizedPaletteName) == 0 &&
        destination.redBackground == redBackground) {
        return destination.initialized;
    }

    mk64_3ds::TextureLease3DS resourceLease(resourceName);
    const auto& resource=resourceLease.Get();
    if (!resourceLease.Valid() ||
        resource.width == 0 || resource.height == 0 || resource.width > kMaxUiTextureDimension ||
        resource.height > kMaxUiTextureDimension) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination, redBackground);
        return false;
    }
    const size_t required = RequiredTextureBytes(resource);
    if (required == 0 || resource.size < required) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination, redBackground);
        return false;
    }

    mk64_3ds::TextureLease3DS paletteLease;
    const auto& palette=paletteLease.Get();
    const bool paletteRequired = resource.type == TexturePalette4 || resource.type == TexturePalette8;
    const size_t paletteEntries = resource.type == TexturePalette4 ? 16U : 256U;
    if (paletteRequired &&
        (normalizedPaletteName[0] == '\0' ||
         !paletteLease.Acquire(normalizedPaletteName) || !paletteLease.Valid() ||
         palette.type != TextureRgba16 || palette.size < paletteEntries * 2U)) {
        RememberMissingTexture(resourceName, normalizedPaletteName, destination, redBackground);
        return false;
    }

    UiTexture decoded;
    decoded.redBackground = redBackground;
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
        RememberMissingTexture(resourceName, normalizedPaletteName, destination, redBackground);
        return false;
    }
    decoded.initialized = true;

    auto* texturePixels = static_cast<uint32_t*>(decoded.texture.data);
    const uint32_t tilesPerRow = backingWidth / 8U;
    // Scope hidden-color cleanup to these native hints, not general game art.
    const bool cleanMenuHint = !std::strcmp(resourceName,kGameSelectOptionResource) ||
                              !std::strcmp(resourceName,kGameSelectDataResource);
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
                    uint32_t rgba = DecodeTexturePixel(
                        resource, sourceIndex, paletteRequired ? &palette : nullptr);
                    if (cleanMenuHint) rgba = mk64_3ds::CleanMenuHintPixel(rgba);
                    if (redBackground) {
                        // Bake the original Data grayscale/red filter once per
                        // texture load, independent of Citro2D tint state.
                        const uint32_t luma = ((rgba & 255U) * 299U +
                            ((rgba >> 8U) & 255U) * 587U +
                            ((rgba >> 16U) & 255U) * 114U + 500U) / 1000U;
                        const uint32_t tinted = (luma * 175U + 127U) / 255U;
                        rgba = (rgba & 0xff000000U) | luma | (tinted << 8U) | (tinted << 16U);
                    }
                    tile[MortonOffset8x8(column, row)] = __builtin_bswap32(rgba);
                }
            }
        }
    }
    if (!Mk64System3DSCleanDataCache(
            decoded.texture.data,
            static_cast<size_t>(backingWidth) * backingHeight * sizeof(uint32_t))) {
        DeleteTexture(decoded);
        return false;
    }
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

    try {
        LoadNativeVariants(decoded, &destination == &sUi.minimap);
    } catch (const std::bad_alloc&) {
        // These variants are optional. Completed variants remain owned by
        // decoded; the original upload is valid and must still be published.
    }
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

bool InitBackdropTexture(UiTexture& decoded, uint16_t width, uint16_t height) {
    if (!C3D_TexInit(&decoded.texture, width, height, GPU_RGBA8)) return false;
    decoded.initialized = true;
    decoded.width = decoded.backingWidth = width;
    decoded.height = decoded.backingHeight = height;
    decoded.subTexture = {width, height, 0.0f, 1.0f, 1.0f, 0.0f};
    C3D_TexSetFilter(&decoded.texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&decoded.texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    std::memset(decoded.texture.data, 0, static_cast<size_t>(width) * height * 4);
    return true;
}

bool PublishBackdropTexture(UiTexture& destination, UiTexture& decoded) {
    if (!Mk64System3DSCleanDataCache(decoded.texture.data,
            static_cast<size_t>(decoded.backingWidth) * decoded.backingHeight * 4) ||
        !ReplaceTexture(destination, decoded)) {
        DeleteTexture(decoded); // Never submitted; old live image is preserved.
        return false;
    }
    return true;
}

void LoadWhiteMinimapVariant(const UiTexture& original, UiTexture& destination) {
    if (!original.initialized || destination.initialized) return;
    // Preserve native alpha and black contour. Cache this tiny variant once;
    // leader flashes do not allocate, read resources or rewrite GPU textures.
    UiTexture white{};
    if (!InitBackdropTexture(white, original.backingWidth, original.backingHeight)) return;
    const auto* source = static_cast<const uint32_t*>(original.texture.data);
    auto* target = static_cast<uint32_t*>(white.texture.data);
    const size_t count = size_t(white.backingWidth) * white.backingHeight;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t pixel = __builtin_bswap32(source[i]);
        target[i] = __builtin_bswap32(mk64_3ds::WhiteMinimapPixel(pixel));
    }
    white.width = original.width;
    white.height = original.height;
    white.subTexture = original.subTexture;
    for(unsigned i=0;i<original.nativeCount;++i)
        AddPackVariant(white,original.resourceName,original.native[i].width,original.native[i].height,true);
    PublishBackdropTexture(destination, white);
}

void LoadMinimapDots() {
    auto& hud = sUi.raceHud;
    if (LoadTexture(kMinimapDotResource, hud.minimapDot))
        LoadWhiteMinimapVariant(hud.minimapDot, hud.minimapDotWhite);
    for (size_t id=0; id<hud.minimapKarts.size(); ++id)
        LoadWhiteMinimapVariant(hud.minimapKarts[id], hud.minimapKartsWhite[id]);
}

// Original, generated artwork is uploaded once. CPU arrays live in read-only
// program storage; GPU textures follow the existing retirement/shutdown owner.
template <size_t N>
void LoadMenuHint(const std::array<uint32_t, N>& pixels, UiTexture& destination) {
    constexpr uint16_t height = 19, width = N / height;
    constexpr uint16_t backingWidth = width <= 64 ? 64 : 128;
    UiTexture decoded{};
    if (!InitBackdropTexture(decoded, backingWidth, 32)) return;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            WriteTiledPixel(decoded.texture, backingWidth, x, y, pixels[y * width + x]);
    decoded.width = decoded.subTexture.width = width;
    decoded.height = decoded.subTexture.height = height;
    decoded.subTexture.right = float(width) / backingWidth;
    decoded.subTexture.bottom = 1.0f - float(height) / 32;
    C3D_TexSetFilter(&decoded.texture,GPU_NEAREST,GPU_NEAREST);
    PublishBackdropTexture(destination, decoded);
}

void LoadRandomCupHint() {
    const auto read = [](const char* name) {
        char resourceName[128]{};
        std::snprintf(resourceName, sizeof(resourceName), "__OTR__textures/texture_tkmk00/%s", name);
        mk64_3ds::TextureLease3DS lease(resourceName);
        const auto& resource=lease.Get();
        mk64_3ds::MenuHintSource copy;
        if (!lease.Valid() ||
            resource.type != TextureRgba16 || resource.width > 64 || resource.height > 19 ||
            resource.size < RequiredTextureBytes(resource)) return copy;
        copy.width = resource.width; copy.height = resource.height;
        copy.pixels.resize(size_t(copy.width) * copy.height);
        for (size_t i = 0; i < copy.pixels.size(); ++i)
            copy.pixels[i] = DecodeTexturePixel(resource, i);
        return copy;
    };
    const auto option = read("texture_l_option"), data = read("texture_r_data");
    const auto nativeButton = [&](const mk64_3ds::MenuHintSource& source,
                                  const auto& fallback, UiTexture& destination) {
        auto pixels = fallback;
        if (source.Is(58, 19)) for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = source.pixels[i] >> 24 ? source.pixels[i] : 0xff000000U;
        LoadMenuHint(pixels, destination);
    };
    nativeButton(option, mk64_3ds::menu_art::kOption, sUi.gameSelectOption);
    nativeButton(data, mk64_3ds::menu_art::kData, sUi.gameSelectData);
    auto random = mk64_3ds::menu_art::kRandomCup;
    auto automatic = mk64_3ds::menu_art::kAuto;
    mk64_3ds::MakeRandomCupHint(random, read);
    mk64_3ds::MakeAutoHint(automatic, read);
    LoadMenuHint(random, sUi.randomCupHint);
    LoadMenuHint(automatic, sUi.autoHint);
    AddPackVariant(sUi.gameSelectOption,"@hint_option",87,29);
    AddPackVariant(sUi.gameSelectData,"@hint_data",87,29);
    AddPackVariant(sUi.randomCupHint,"@hint_random",192,29);
    AddPackVariant(sUi.autoHint,"@hint_auto",122,29);
}

void LoadOptionIcons() {
    UiTexture decoded{};
    if (!InitBackdropTexture(decoded, 128, 128)) return;
    for (unsigned y = 0; y < 128; ++y) for (unsigned x = 0; x < 128; ++x)
        WriteTiledPixel(decoded.texture, 128, x, y, mk64_3ds::option_art::kPixels[y * 128 + x]);
    C3D_TexSetFilter(&decoded.texture, GPU_NEAREST, GPU_NEAREST);
    PublishBackdropTexture(sUi.optionIcons, decoded);
}

void LoadSoftMenuBackground() {
    const char* name = sUi.game.mainBackgroundTexture;
    const int blur = Mk64Settings3DSGetMenuBlur();
    // OFF/LIGHT keep the original image. Only stronger blur needs a cache.
    if (blur < 2) return;
    if (!name || !*name || (!std::strcmp(name, sUi.softMenuBackground.resourceName) &&
        sUi.softMenuBackground.redBackground == sUi.game.dataCourse && sUi.cachedMenuBlur == blur)) return;
    mk64_3ds::TextureLease3DS lease(name);
    const auto& resource=lease.Get();
    if (!lease.Valid() ||
        !resource.width || !resource.height || resource.width > 512 || resource.height > 512 ||
        !RequiredTextureBytes(resource) || resource.size < RequiredTextureBytes(resource) ||
        resource.type == TexturePalette4 || resource.type == TexturePalette8) return;
    auto pixels = mk64_3ds::MakeSoftBackdrop(resource.width, resource.height,
        [&](unsigned x, unsigned y) { return DecodeTexturePixel(resource, y * resource.width + x); }, blur);
    if (sUi.game.dataCourse) {
        for (auto& rgba : pixels) {
            const uint32_t luma = ((rgba & 255U) * 299U + ((rgba >> 8U) & 255U) * 587U +
                                   ((rgba >> 16U) & 255U) * 114U + 500U) / 1000U;
            const uint32_t tinted = (luma * 175U + 127U) / 255U;
            rgba = (rgba & 0xff000000U) | luma | (tinted << 8U) | (tinted << 16U);
        }
    }
    UiTexture decoded{};
    // A bounded 128x128 texture retains much more of the source detail.
    if (!InitBackdropTexture(decoded, 128, 128)) return;
    for (unsigned y = 0; y < 128; ++y)
        for (unsigned x = 0; x < 128; ++x)
            WriteTiledPixel(decoded.texture, 128, x, y, pixels[std::min(y, 95U) * 128 + x]);
    decoded.height = 96;
    decoded.subTexture.height = 96;
    decoded.subTexture.bottom = 0.25f;
    std::snprintf(decoded.resourceName, sizeof(decoded.resourceName), "%s", name);
    decoded.redBackground = sUi.game.dataCourse;
    if (PublishBackdropTexture(sUi.softMenuBackground, decoded)) sUi.cachedMenuBlur = blur;
}

void LoadMenuMaps() {
    if (sUi.menuMapLoadAttempted) return;
    sUi.menuMapLoadAttempted = true;
    for (unsigned i = 0; i < mk64_3ds::kMenuCourseOrder.size(); ++i) {
        const char* name = TrackBrowser_GetMinimapTextureByIdx(mk64_3ds::kMenuCourseOrder[i]);
        LoadTexture(name, sUi.menuMaps[i]);
    }
}

void LoadNativeFont() try {
    auto& font=sUi.nativeFont;
    if (font.initialized || !C3D_TexInit(&font.texture,512,256,GPU_LA4)) return;
    std::memset(font.texture.data,0,512*256);
    unsigned atlasX=0,atlasY=0,rowHeight=0;
    for (unsigned h=8;h<=16;++h) for (size_t index=0;index<kFontResources.size();++index) {
        mk64_3ds::TextureLease3DS lease(kFontResources[index]);
        const auto& resource=lease.Get();
        if (!lease.Valid() ||
            resource.width!=26 || resource.height!=16 || resource.size<RequiredTextureBytes(resource)) continue;
        mk64_3ds::native_sprite::Image source{26,16,{}};
        source.pixels.resize(26*16);
        for (size_t p=0;p<source.pixels.size();++p) source.pixels[p]=DecodeTexturePixel(resource,p);
        const auto packed=mk64_3ds::improved::LocalPack().Load(kFontResources[index],26,16);
        if (packed && packed.width==26 && packed.height==16) {
            const auto* bytes=reinterpret_cast<const uint8_t*>(packed.pixels->data());
            for(size_t p=0;p<source.pixels.size();++p)
                source.pixels[p]=uint32_t(bytes[p*4])|uint32_t(bytes[p*4+1])<<8|
                    uint32_t(bytes[p*4+2])<<16|uint32_t(bytes[p*4+3])<<24;
        }
        const auto image=mk64_3ds::native_sprite::ScaleBilinear(source,std::lround(26*h/16.f),h);
        if (atlasX+image.width>512) {atlasX=0;atlasY+=rowHeight;rowHeight=0;}
        if (atlasY+h>256) {C3D_TexDelete(&font.texture);font={};return;}
        for (unsigned y=0;y<h;++y) for (unsigned x=0;x<image.width;++x) {
            const unsigned tx=atlasX+x,ty=atlasY+y;
            const size_t offset=(size_t(ty/8)*64+tx/8)*64+MortonOffset8x8(tx%8,ty%8);
            static_cast<uint8_t*>(font.texture.data)[offset]=0xf0U | (image.pixels[size_t(y)*image.width+x]>>28);
        }
        font.glyphs[h-8][index]={uint16_t(image.width),uint16_t(h),atlasX/512.f,1-atlasY/256.f,
            (atlasX+image.width)/512.f,1-(atlasY+h)/256.f};
        atlasX+=image.width+1;rowHeight=std::max(rowHeight,h+1);
    }
    if (!Mk64System3DSCleanDataCache(font.texture.data,512*256)) {C3D_TexDelete(&font.texture);font={};return;}
    C3D_TexSetFilter(&font.texture,GPU_NEAREST,GPU_NEAREST);
    C3D_TexSetWrap(&font.texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
    font.initialized=true;
} catch (const std::bad_alloc&) {
    // Keep the original glyph atlas available when an optional scaled atlas
    // cannot be prepared. No partially initialized GPU allocation may escape.
    if (sUi.nativeFont.texture.data) C3D_TexDelete(&sUi.nativeFont.texture);
    sUi.nativeFont={};
}

bool LoadFontAtlas(UiFontAtlas& font, int palette = -1) {
    if (font.initialized) return true;
    if (!C3D_TexInit(&font.texture, kFontAtlasWidth, kFontAtlasHeight, GPU_RGBA8)) return false;
    std::memset(font.texture.data, 0,
                static_cast<size_t>(kFontAtlasWidth) * kFontAtlasHeight * sizeof(uint32_t));

    size_t loadedCount = 0;
    for (size_t index = 0; index < kFontResources.size(); ++index) {
        mk64_3ds::TextureLease3DS lease(kFontResources[index]);
        const auto& resource=lease.Get();
        if (!lease.Valid() || resource.width != 26 || resource.height != 16 ||
            resource.type != TextureI4 || resource.size < RequiredTextureBytes(resource)) {
            continue;
        }
        const uint16_t atlasX = static_cast<uint16_t>((index % kFontGlyphsPerRow) * kFontGlyphCellWidth);
        const uint16_t atlasY = static_cast<uint16_t>((index / kFontGlyphsPerRow) * kFontGlyphCellHeight);
        for (uint16_t y = 0; y < resource.height; ++y) {
            for (uint16_t x = 0; x < resource.width; ++x) {
                const size_t pixel = static_cast<size_t>(y) * resource.width + x;
                uint32_t rgba = DecodeTexturePixel(resource, pixel);
                if (palette >= 0) {
                    // Native menu lettering uses a vertical colored gradient.
                    // Store it with the glyph mask so the same colors survive
                    // the Fast3D/Citro2D transition on both graphics backends.
                    constexpr uint8_t colors[6][2][3] = {
                        {{255,255,120},{255,130,0}}, {{190,255,140},{0,175,40}},
                        {{255,185,185},{235,30,45}}, {{150,185,255},{45,65,180}},
                        {{0,0,0},{0,0,0}}, {{0,255,255},{255,0,255}}
                    };
                    uint32_t rgb = 0;
                    for (int channel = 0; channel < 3; ++channel) {
                        const int top = colors[palette][0][channel], bottom = colors[palette][1][channel];
                        rgb |= uint32_t(top + (bottom - top) * y / 15) << (channel * 8);
                    }
                    rgba = (rgba & 0xff000000u) | rgb;
                }
                WriteTiledPixel(font.texture, kFontAtlasWidth, atlasX + x, atlasY + y, rgba);
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
    if (!Mk64System3DSCleanDataCache(
            font.texture.data,
            static_cast<size_t>(kFontAtlasWidth) * kFontAtlasHeight * sizeof(uint32_t))) {
        C3D_TexDelete(&font.texture);
        return false;
    }
    C3D_TexSetFilter(&font.texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&font.texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    font.smoothTexture = font.texture;
    C3D_TexSetFilter(&font.smoothTexture, GPU_LINEAR, GPU_LINEAR);
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
                          float depth, float horizontalScale = 1.0f) {
    const float thickness = std::max(1.0f, 1.35f * scale);
    if (character == ':') {
        const float size = std::max(1.0f, 2.0f * scale);
        C2D_DrawRectSolid(x + 2.0f * scale * horizontalScale, y + 4.0f * scale, depth, size * horizontalScale, size, color);
        C2D_DrawRectSolid(x + 2.0f * scale * horizontalScale, y + 11.0f * scale, depth, size * horizontalScale, size, color);
    } else if (character == '/') {
        C2D_DrawLine(x + 1.0f * scale * horizontalScale, y + 14.0f * scale, color,
                     x + 7.0f * scale * horizontalScale, y + 2.0f * scale, color, thickness, depth);
    } else if (character == '%') {
        C2D_DrawRectSolid(x + scale * horizontalScale, y + 2.0f * scale, depth, 3.0f * scale * horizontalScale, 3.0f * scale, color);
        C2D_DrawLine(x + 2.0f * scale * horizontalScale, y + 14.0f * scale, color,
                     x + 11.0f * scale * horizontalScale, y + 2.0f * scale, color, thickness, depth);
        C2D_DrawRectSolid(x + 9.0f * scale * horizontalScale, y + 11.0f * scale, depth,
                          3.0f * scale * horizontalScale, 3.0f * scale, color);
    }
}

float sDrawOpacity = 1.0f;
int sDataFontPalette = -1;
void DrawImageWithOpacity(C2D_Image image, const C2D_DrawParams* params, const C2D_ImageTint* original) {
    if (sDrawOpacity >= 1.0f) { C2D_DrawImage(image, params, original); return; }
    C2D_ImageTint tint{};
    if (original) tint = *original;
    else C2D_AlphaImageTint(&tint, 1.0f);
    for (auto& corner : tint.corners) {
        const uint32_t alpha = static_cast<uint32_t>((corner.color >> 24) * sDrawOpacity);
        corner.color = (corner.color & 0xffffffu) | (alpha << 24);
    }
    C2D_DrawImage(image, params, &tint);
}

void DrawText(const char* value, float x, float y, float scale, uint32_t color,
              uint32_t alignment = C2D_AlignLeft, float depth = 0.8f, bool shadow = false,
              float horizontalScale = 1.0f) {
    if (value == nullptr || !sUi.font.initialized || horizontalScale <= 0.0f) return;
    const float width = MeasureText(value, scale) * horizontalScale;
    float cursor = x;
    if ((alignment & C2D_AlignCenter) != 0) cursor -= width * 0.5f;
    if ((alignment & C2D_AlignRight) != 0) cursor -= width;

    const auto drawPass = [&](float offsetX, float offsetY, uint32_t passColor, float passDepth) {
        C2D_ImageTint tint = {};
        C2D_PlainImageTint(&tint, passColor, 1.0f);
        float glyphX = cursor;
        for (const char* character = value; *character != '\0'; ++character) {
            const float advance = CharacterAdvance(*character) * scale * horizontalScale;
            const int glyphIndex = GlyphIndexForCharacter(*character);
            if (glyphIndex >= 0 && sUi.font.glyphs[static_cast<size_t>(glyphIndex)].loaded) {
                const int nativeHeight=std::lround(16*scale);
                if (!sUi.modalOpen && mk64_3ds::ImprovedUiTextures(Mk64Settings3DSGetUiTextures()) &&
                    sUi.nativeFont.initialized && nativeHeight>=8 && nativeHeight<=16) {
                    auto& sub=sUi.nativeFont.glyphs[nativeHeight-8][glyphIndex];
                    C2D_Image image{&sUi.nativeFont.texture,&sub};
                    C2D_ImageTint nativeTint=tint;
                    if (sDataFontPalette>=0 && (passColor&0xffffffU)!=0) {
                        constexpr uint8_t colors[6][2][3]={
                            {{255,255,120},{255,130,0}},{{190,255,140},{0,175,40}},
                            {{255,185,185},{235,30,45}},{{150,185,255},{45,65,180}},
                            {{0,0,0},{0,0,0}},{{0,255,255},{255,0,255}}};
                        for (unsigned c=0;c<4;++c) {
                            const auto* rgb=colors[sDataFontPalette][c/2];
                            nativeTint.corners[c].color=C2D_Color32(rgb[0],rgb[1],rgb[2],passColor>>24);
                        }
                    }
                    const C2D_DrawParams params={
                        .pos={std::round(glyphX+offsetX*horizontalScale),std::round(y+offsetY),
                            std::round(sub.width*horizontalScale),float(sub.height)},
                        .center={0,0},.depth=passDepth,.angle=0};
                    DrawImageWithOpacity(image,&params,&nativeTint);
                    glyphX+=advance;
                    continue;
                }
                UiFontAtlas& atlas = sDataFontPalette < 0 ? sUi.font :
                    sUi.dataFonts[(passColor & 0xffffffu) == 0 ? 4 : sDataFontPalette];
                FontGlyph& glyph = atlas.glyphs[static_cast<size_t>(glyphIndex)];
                C2D_Image image = { .tex = &atlas.texture, .subtex = &glyph.subTexture };
                const C2D_DrawParams params = {
                    .pos = { .x = glyphX + offsetX * horizontalScale, .y = y + offsetY,
                             .w = 26.0f * scale * horizontalScale, .h = 16.0f * scale },
                    .center = { .x = 0.0f, .y = 0.0f },
                    .depth = passDepth,
                    .angle = 0.0f,
                };
                DrawImageWithOpacity(image, &params, sDataFontPalette >= 0 ? nullptr : &tint);
            } else if (*character != ' ') {
                const uint32_t specialColor = (passColor & 0xffffffu) |
                    (static_cast<uint32_t>((passColor >> 24) * sDrawOpacity) << 24);
                DrawSpecialCharacter(*character, glyphX + offsetX * horizontalScale, y + offsetY,
                                     scale, specialColor, passDepth, horizontalScale);
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
    auto* native = NativeVariantFor(texture,width,height);
    C2D_DrawParams drawParams=params;
    if (native) {
        drawParams.pos.x=std::round(x); drawParams.pos.y=std::round(y);
        drawParams.pos.w=std::round(width); drawParams.pos.h=std::round(height);
    }
    C2D_Image image = native ? C2D_Image{&native->texture,&native->subTexture} : TextureImage(texture);
    Tex3DS_SubTexture flippedSubTexture = *image.subtex;
    if (flipHorizontal) {
        std::swap(flippedSubTexture.left, flippedSubTexture.right);
        image.subtex = &flippedSubTexture;
    }
    DrawImageWithOpacity(image, &drawParams, tint);
}

void DrawTextureRotated(UiTexture& texture, float centerX, float centerY, float width,
                        float height, float depth, float angle, const C2D_ImageTint* tint = nullptr) {
    if (!texture.initialized || texture.width == 0 || texture.height == 0) return;
    auto* native=NativeVariantFor(texture,width,height);
    const C2D_Image image=native ? C2D_Image{&native->texture,&native->subTexture} : TextureImage(texture);
    if (native) {width=std::round(width);height=std::round(height);centerX=std::round(centerX);centerY=std::round(centerY);}
    const C2D_DrawParams params = {
        .pos = { .x = centerX, .y = centerY, .w = width, .h = height },
        .center = { .x = width * .5f, .y = height * .5f },
        .depth = depth, .angle = angle,
    };
    DrawImageWithOpacity(image, &params, tint);
}

void DrawStandingLayer(UiTexture& texture, const mk64_3ds::HudRect& box,
                       float offsetX, float offsetY, float width, float height,
                       float depth, float angle, const C2D_ImageTint* tint) {
    // Rotate every layer around the SAME portrait center. The number's own
    // center follows that rotation too; no fixed backing, fitting or clipping.
    const float dx = offsetX + width * .5f - box.width * .5f;
    const float dy = offsetY + height * .5f - box.height * .5f;
    const float cosine = std::cos(angle), sine = std::sin(angle);
    DrawTextureRotated(texture, box.x + box.width * .5f + dx*cosine - dy*sine,
                       box.y + box.height * .5f + dx*sine + dy*cosine,
                       width, height, depth, angle, tint);
}

void DrawStandingBacking(UiTexture& portrait, const mk64_3ds::HudRect& box,
                         float angle, uint8_t alpha) {
    float width = box.width, height = box.height;
    float centerX = box.x + width * .5f, centerY = box.y + height * .5f;
    // Match DrawTextureRotated's native-pixel alignment, including half pixels
    // in the original mode. The backing belongs to the rotating portrait.
    if (NativeVariantFor(portrait, width, height)) {
        width = std::round(width); height = std::round(height);
        centerX = std::round(centerX); centerY = std::round(centerY);
    }
    const uint32_t color = C2D_Color32(0, 0, 0, alpha);
    constexpr float depth = .48f;
    if (angle == 0.0f) {
        C2D_DrawRectSolid(centerX - width * .5f, centerY - height * .5f,
                         depth, width, height, color);
        return;
    }
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const float wx = width * .5f * cosine, wy = width * .5f * sine;
    const float hx = -height * .5f * sine, hy = height * .5f * cosine;
    C2D_DrawTriangle(centerX-wx-hx, centerY-wy-hy, color,
                     centerX+wx-hx, centerY+wy-hy, color,
                     centerX-wx+hx, centerY-wy+hy, color, depth);
    C2D_DrawTriangle(centerX+wx-hx, centerY+wy-hy, color,
                     centerX+wx+hx, centerY+wy+hy, color,
                     centerX-wx+hx, centerY-wy+hy, color, depth);
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
    auto* native=NativeVariantFor(texture,width*texture.width/std::max(1.f,sourceWidth),
        height*texture.height/std::max(1.f,sourceHeight));
    C3D_Tex* sampler=&texture.texture;
    if (native) {
        sampler=&native->texture;
        const float rw=(native->subTexture.right-native->subTexture.left)/native->logicalWidth;
        const float rh=(native->subTexture.top-native->subTexture.bottom)/native->logicalHeight;
        region.left=sourceX*rw; region.right=(sourceX+sourceWidth)*rw;
        region.top=1-sourceY*rh; region.bottom=1-(sourceY+sourceHeight)*rh;
        x=std::round(x);y=std::round(y);width=std::round(width);height=std::round(height);
    }
    C2D_Image image = { .tex = sampler, .subtex = &region };
    const C2D_DrawParams params = {
        .pos = { .x = x, .y = y, .w = width, .h = height },
        .center = { .x = 0.0f, .y = 0.0f },
        .depth = depth,
        .angle = 0.0f,
    };
    DrawImageWithOpacity(image, &params, tint);
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
    if (sUi.fpsFont.initialized) C3D_TexDelete(&sUi.fpsFont.texture);
    sUi.fpsFont={};
    if (sUi.nativeFont.initialized) C3D_TexDelete(&sUi.nativeFont.texture);
    sUi.nativeFont={};
    if (sUi.font.initialized) C3D_TexDelete(&sUi.font.texture);
    sUi.font = {};
    for (auto& font : sUi.dataFonts) { if (font.initialized) C3D_TexDelete(&font.texture); font = {}; }
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
    LoadTexture(kHudTotalTimeResource, hud.totalTimeLabel);
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
    LoadMinimapDots();
}

void DeleteRaceHudTextures() {
    RaceHudTextures& hud = sUi.raceHud;
    DeleteTexture(hud.lapLabel);
    DeleteTextures(hud.lapCounts);
    DeleteTexture(hud.timeLabel);
    DeleteTexture(hud.totalTimeLabel);
    DeleteTexture(hud.timerDigits);
    DeleteTextures(hud.places);
    DeleteTextures(hud.portraits);
    DeleteTexture(hud.questionPortrait);
    DeleteTexture(hud.portraitBorder);
    DeleteTextures(hud.standingRanks);
    DeleteTextures(hud.items);
    DeleteTexture(hud.finishLine);
    DeleteTextures(hud.minimapKarts);
    DeleteTextures(hud.minimapKartsWhite);
    DeleteTexture(hud.minimapDot);
    DeleteTexture(hud.minimapDotWhite);
    hud.loadAttempted = false;
}

void DrawFallbackBackground() {
    C2D_DrawRectangle(0.0f, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                      C2D_Color32(15, 27, 52, 255), C2D_Color32(15, 27, 52, 255),
                      C2D_Color32(4, 7, 18, 255), C2D_Color32(4, 7, 18, 255));
}

void DrawDimMenuBackground() {
    DrawFallbackBackground();
    UiTexture& background = Mk64Settings3DSGetMenuBlur() >= 2 && sUi.softMenuBackground.initialized &&
        !std::strcmp(sUi.softMenuBackground.resourceName, sUi.menuBackground.resourceName) &&
        sUi.softMenuBackground.redBackground == sUi.menuBackground.redBackground
        ? sUi.softMenuBackground : sUi.menuBackground;
    if (background.initialized) {
        // Match the original grayscale menu filters: Game Select red, Player
        // Select green, and Course Select blue-purple.
        uint32_t tintColor = C2D_Color32(255, 255, 255, 255);
        bool filtered = true;
        if (sUi.menuBackground.redBackground) {
            filtered = false;
        } else if (sUi.game.menuSelection == 11) {
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
            DrawTextureCover(background, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                             0.05f, &tint);
            C2D_SetTintMode(C2D_TintSolid);
        } else {
            DrawTextureCover(background, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                             0.05f);
        }
        // Shared ~40% brighter backdrop on every idle/occupied menu, including DATA.
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, kBottomWidth, kBottomHeight,
                          C2D_Color32(0, 0, 0, mk64_3ds::kMenuBackgroundScrimAlpha));
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
    return game.gameSelectVisible || game.gameSelectOptionReveal > 0.0f || game.gameSelectDataReveal > 0.0f
        ? BaseView::GameSelect : BaseView::Background;
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

#include "options_actions_3ds.inc.cpp"

void UpdateFpsCounter() {
    if (sUi.fps.Record(svcGetSystemTick(), SYSCLOCK_ARM11) && Mk64Settings3DSGetOverlayEnabled())
        sUi.bottomDirty = true;
}

bool StatusActive() {
    return sUi.status[0] != '\0' && osGetTime() < sUi.statusExpiresAt;
}

void DrawStatus() {
    if (!StatusActive()) return;
    // Submitted on the live top target, never baked into the cached bottom UI.
    C2D_DrawRectSolid(8, 214, 0.85f, 384, 24, C2D_Color32(0, 0, 0, 190));
    DrawText(sUi.status, 200.0f, 220.0f, 0.62f, C2D_Color32(255, 230, 92, 255),
             C2D_AlignCenter, 0.86f, true);
}

bool IdleMenuBackdropActive();
void DrawMenuConveyor();
void DrawRandomCupHint(bool fullRandom, float reveal = 1.0f) {
    reveal = std::clamp(reveal,0.0f,1.0f);
    if (reveal <= 0.0f) return;
    const float y = fullRandom ? 142.0f : 185.0f;
    if (sUi.randomCupHint.initialized) {
        const float width=mk64_3ds::kRandomHintWidth*1.5f*reveal;
        DrawTexture(sUi.randomCupHint,160-width/2,y,width,29.f,0.76f);
    } else {
        DrawText("SELECT RANDOM CUP",160,y+11,0.62f,C2D_Color32(255,255,255,255),C2D_AlignCenter,0.76f,true,reveal);
    }
    if (fullRandom) return; // The random-all action needs no redundant subtitle.
    const auto text=mk64_3ds::RandomCupSubtitle(fullRandom,sUi.game.selectedDriver,sUi.game.selectedLevel);
    // Reuse the native I4 glyph atlas/advances, including the same local
    // horizontal reveal and opacity as the label. No system-font allocation.
    DrawText(text.data(),160,y+33,0.625f,C2D_Color32(235,235,235,255),C2D_AlignCenter,0.76f,true,reveal);
}
void DrawGameSelect() {
    DrawDimMenuBackground();
    const float option = sUi.game.gameSelectOptionReveal, data = sUi.game.gameSelectDataReveal;
    if (IdleMenuBackdropActive()) {
        sDrawOpacity = 1.0f-std::max(option, data);
        DrawMenuConveyor();
        sDrawOpacity = 1.0f;
    }
    sDrawOpacity = option;
    if (option > 0.0f && sUi.gameSelectOption.initialized) {
        DrawTexture(sUi.gameSelectOption, 88.0f-43.5f*option, 76.0f, 87.0f*option, 29.0f, 0.7f);
    } else if (option > 0.0f) {
        DrawText("L OPTION", 88.0f, 81.0f, 0.8f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter, 0.7f, true);
    }
    sDrawOpacity = data;
    if (data > 0.0f && sUi.gameSelectData.initialized) {
        DrawTexture(sUi.gameSelectData, 232.0f-43.5f*data, 76.0f, 87.0f*data, 29.0f, 0.7f);
    } else if (data > 0.0f) {
        DrawText("R DATA", 232.0f, 81.0f, 0.8f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter, 0.7f, true);
    }
    sDrawOpacity = std::max(option, data);
    DrawRandomCupHint(true, sDrawOpacity);
    sDrawOpacity = 1.0f;
}

void DrawAutoHint() {
    const float reveal=sUi.autoHintTween.reveal;
    if (reveal<=0) return;
    const float savedOpacity=sDrawOpacity;
    sDrawOpacity*=reveal;
    if (sUi.autoHint.initialized) {
        const float width=mk64_3ds::kAutoHintWidth*1.5f*reveal;
        DrawTexture(sUi.autoHint,160-width/2,185,width,29.f,0.76f);
    } else {
        DrawText("SELECT AUTO",160,196,0.62f,C2D_Color32(255,255,255,255),C2D_AlignCenter,0.76f,true,reveal);
    }
    sDrawOpacity=savedOpacity;
}

void DrawTimerValue(uint32_t value, float x, float y, float scale) {
    if (!sUi.raceHud.timerDigits.initialized) return;
    int hundredthsTotal = std::min(value, uint32_t{599999});
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

void DrawTimerDigits(float x, float y, float scale) {
    DrawTimerValue(Mk64RaceClock3DSDisplay(sUi.game.courseTimerCentiseconds,
        !sUi.game.racing || sUi.game.paused || sUi.game.raceFinished),x,y,scale);
}

bool DrawsLapBreakdown() {
    // Time Trial has no standings; use its free left column for native lap times.
    // GP/VS retain the owner's original portrait grid and live total header.
    return sUi.game.raceFinished && !sUi.game.demo && !sUi.game.autoCinematic &&
        !sUi.game.multiplayer && sUi.game.gameMode == 1;
}

void DrawLapBreakdown() {
    // Relocate the native three lapDurations and frozen someTimer, preserving
    // its 16-pixel row spacing and TOTAL texture. No second race timer.
    for (unsigned row=0;row<4;++row) {
        const float y=64+row*16.f;
        if (row==3) DrawTexture(sUi.raceHud.totalTimeLabel,18,y,32,16,.7f);
        DrawTimerValue(row<3?sUi.game.lapDurationCentiseconds[row]:sUi.game.courseTimerCentiseconds,
                       58,y,1);
    }
}

mk64_3ds::HudRect DrawMinimapOutline(UiTexture& texture, float areaX, float areaY,
                                    float areaWidth, float areaHeight, uint32_t color,
                                    bool mirror, float depth) {
    const float scale = mk64_3ds::BottomMinimapScale(texture.width, texture.height,
                                                   areaWidth, areaHeight);
    const float width = texture.width * scale, height = texture.height * scale;
    const mk64_3ds::HudRect rect{areaX + (areaWidth - width) * 0.5f,
                                areaY + (areaHeight - height) * 0.5f, width, height};
    C2D_ImageTint tint{};
    C2D_PlainImageTint(&tint, color, 1.0f);
    DrawTexture(texture, rect.x, rect.y, width, height, depth, &tint, mirror);
    return rect;
}

void DrawMinimap(float areaX = 165.0f, float areaY = 48.0f,
                 float areaWidth = 149.0f, float areaHeight = 184.0f) {
    if (!sUi.minimap.initialized) return;
    const float scale = std::min(areaWidth / sUi.minimap.width, areaHeight / sUi.minimap.height);
    const bool modified=Mk64Settings3DSGetUiTextures()!=MK64_UI_TEXTURES_3DS_ORIGINAL;
    const float mapWidth = modified ? std::round(sUi.minimap.width*scale) : sUi.minimap.width*scale;
    const float mapHeight = modified ? std::round(sUi.minimap.height*scale) : sUi.minimap.height*scale;
    const float mapX = modified ? std::round(areaX+(areaWidth-mapWidth)*.5f) : areaX+(areaWidth-mapWidth)*.5f;
    const float mapY = modified ? std::round(areaY+(areaHeight-mapHeight)*.5f) : areaY+(areaHeight-mapHeight)*.5f;
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

    const size_t observedId = std::clamp<int>(sUi.game.observedPlayer,0,MK64_BOTTOM_UI_RACER_COUNT-1);
    for (size_t index=0;index<MK64_BOTTOM_UI_RACER_COUNT;++index) {
        const size_t playerId=index==MK64_BOTTOM_UI_RACER_COUNT-1 ? observedId : index+(index>=observedId ? 1:0);
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
        UiTexture& white=sUi.raceHud.minimapKartsWhite[static_cast<size_t>(character)];
        UiTexture& marker=mk64_3ds::MinimapLeaderFlashing(racer.rank,sUi.game.gameMode,sUi.game.hudAnimationTick) && white.initialized
            ? white : sUi.raceHud.minimapKarts[static_cast<size_t>(character)];
        const uint16_t heading = static_cast<uint16_t>(
            static_cast<uint16_t>(racer.rotationY) + 0x8000U);
        DrawTextureRotated(marker, markerX, markerY, 8.0f, 8.0f,
                           playerId == observedId ? 0.74f : racer.rank == 0 ? 0.72f : 0.69f,
                           heading * kBinaryAngleToRadians);
    }
}

bool UsesSplitHud() {
    const auto layout = Mk64Settings3DSGetHudLayout();
    return layout >= MK64_HUD_LAYOUT_3DS_MK7 && layout <= MK64_HUD_LAYOUT_3DS_MKDS_2;
}

bool DrawsTopRaceHud() {
    return sUi.game.racing && !sUi.game.paused && !sUi.game.multiplayer && !sUi.game.demo &&
        !sUi.game.autoCinematic && !sUi.game.raceFinished && Mk64Settings3DSGetHudLayout() != MK64_HUD_LAYOUT_3DS_CLEAN;
}

void DrawStandings(bool top = false, float topLeft = 8.0f) {
    if (sUi.game.gameMode == 1 || (top && sUi.game.raceFinished)) return;
    for (size_t rank = 0; rank < std::min<size_t>(4,sUi.game.standingCount); ++rank) {
        const int character = sUi.game.standingCharacterIds[rank];
        if (character < 0 || character >= 8) continue;
        const auto box = top ? mk64_3ds::HudRect{topLeft, 50.0f + rank * 34.0f, 30, 30}
                             : mk64_3ds::BottomStandingRect(rank, sUi.game.raceFinished);
        const float portraitSize = box.width;
        const bool isPlayer = sUi.game.standingPlayerIds[rank] == sUi.game.observedPlayer;
        const bool unknown = sUi.game.standingUnknown[rank];
        const uint8_t portraitAlpha = unknown ? sUi.game.standingAlpha
                                              : (isPlayer ? 255 : sUi.game.standingAlpha);
        C2D_ImageTint portraitTint = {};
        // Preserve the native CI8/TLUT RGB and apply only the ranking fade.
        // A solid white tint at blend=1 replaces the complete image color and
        // was the reason these slots appeared as white/gray boxes in hardware
        // dumps even though the correct portrait textures were loaded.
        C2D_AlphaImageTint(&portraitTint, static_cast<float>(portraitAlpha) / 255.0f);
        UiTexture& portrait = unknown ? sUi.raceHud.questionPortrait
                                      : sUi.raceHud.portraits[static_cast<size_t>(character)];
        const int playerId=sUi.game.standingPlayerIds[rank];
        const float angle=!sUi.game.raceFinished && playerId>=0 && playerId<MK64_BOTTOM_UI_RACER_COUNT
            ? sUi.game.racers[playerId].portraitHitAngle*kBinaryAngleToRadians : 0;
        DrawStandingBacking(portrait, box, angle, portraitAlpha);
        DrawStandingLayer(portrait,box,0,0,portraitSize,portraitSize,.6f,angle,&portraitTint);
        if (unknown) continue;
        if (isPlayer) {
            C2D_ImageTint borderTint = {};
            C2D_PlainImageTint(&borderTint,
                               C2D_Color32(sUi.game.playerBorderRed,
                                           sUi.game.playerBorderGreen,
                                           sUi.game.playerBorderBlue, 255), 1.0f);
            DrawStandingLayer(sUi.raceHud.portraitBorder,box,0,0,portraitSize,portraitSize,.64f,angle,&borderTint);
        }
        const float rankSize = portraitSize * 0.5f;
        DrawStandingLayer(sUi.raceHud.standingRanks[rank],box,1,portraitSize-rankSize,rankSize,rankSize,.66f,angle,&portraitTint);
    }

}

void DrawItemFrame(const mk64_3ds::HudRect& box, bool top = false) {
    if ((top && sUi.game.raceFinished) || sUi.game.demo) return;
    const size_t item = sUi.game.itemWindowVisible ? std::clamp<int>(sUi.game.itemTextureIndex,0,sUi.raceHud.items.size()-1) : 0;
    DrawTexture(sUi.raceHud.items[item],box.x,box.y,box.width,box.height,.8f);
}

void DrawPlace(const mk64_3ds::HudRect& box) {
    if (sUi.game.gameMode == 1 || sUi.game.gameMode == 3 || !sUi.game.currentPlaceVisible) return;
    const size_t place = std::min<size_t>(sUi.game.currentPlaceIndex, sUi.raceHud.places.size() - 1);
    C2D_ImageTint shadow = {}, color = {};
    C2D_PlainImageTint(&shadow, C2D_Color32(0, 0, 0, 220), 1.0f);
    DrawTexture(sUi.raceHud.places[place], box.x + 2, box.y + 2, box.width, box.height, 0.77f, &shadow);
    C2D_PlainImageTint(&color, C2D_Color32(255, sUi.game.currentPlaceGreen, 0, 255), 1.0f);
    DrawTexture(sUi.raceHud.places[place], box.x, box.y, box.width, box.height, 0.8f, &color);
}

void DrawLap(float x, float y, float scale) {
    if (sUi.game.gameMode == 3) return;
    DrawTexture(sUi.raceHud.lapLabel, x, y, 40 * scale, 17 * scale, 0.8f);
    DrawTexture(sUi.raceHud.lapCounts[std::clamp<int>(sUi.game.currentLap, 1, 3) - 1],
                x + 44 * scale, y, 34 * scale, 17 * scale, 0.8f);
}

void DrawRaceHudHeader() {
    const bool positionLapOnTop =
        sUi.game.topHudRenderMode == MK64_TOP_HUD_RENDER_POSITION_LAP ||
        (sUi.game.raceFinished && !sUi.game.autoMode &&
         mk64_3ds::HudMetersAvailable(Mk64Settings3DSGetHudLayout(), Mk64Settings3DSGetHudMeters()) &&
         Mk64Settings3DSGetHudMeters() == MK64_HUD_METERS_3DS_POSITION_LAP);
    const bool split = UsesSplitHud();
    const bool lapOnTop = positionLapOnTop || Mk64Settings3DSGetHudLayout() == MK64_HUD_LAYOUT_3DS_MKDS;

    // Preserve the original 320x240 HUD scale: lap at upper left, the live
    // item-window/roulette state in the center, and time at upper right.
    if (sUi.game.gameMode != 3 && !lapOnTop) {
        DrawTexture(sUi.raceHud.lapLabel, 13.0f, 12.0f, 40.0f, 17.0f, 0.7f);
        const int lap = std::clamp<int>(sUi.game.currentLap, 1, 3) - 1;
        DrawTexture(sUi.raceHud.lapCounts[static_cast<size_t>(lap)],
                    56.0f, 12.0f, 34.0f, 17.0f, 0.7f);
    }
    if (!split)
        DrawItemFrame({140, 5, 40, 32});

    const float timerX = mk64_3ds::BottomTimerX();
    DrawTexture(sUi.raceHud.timeLabel, timerX, 13.0f, 32.0f, 16.0f, 0.7f);
    DrawTimerDigits(timerX + 34.0f, 13.0f, 1.0f);
}

void DrawRaceHud() {
    DrawRaceBackground(204.0f);
    DrawRaceHudHeader();
    DrawStandings();
    if (DrawsLapBreakdown()) DrawLapBreakdown();

    if (sUi.game.topHudRenderMode != MK64_TOP_HUD_RENDER_POSITION_LAP && !UsesSplitHud() && !sUi.game.raceFinished)
        DrawPlace({76,188,80,40});
    DrawMinimap();
}

// Restore only the timer's background and eight digits. The standings/minimap
// remain cached at their old refresh rate; clock digits are sampled per image.
void DrawLiveBottomTimer() {
    constexpr float y = 11, w = 102, h = 20;
    const float x = mk64_3ds::BottomTimerX() - 2;
    auto& image = sUi.coursePreview;
    const float scale = std::max(kBottomWidth / image.width, kBottomHeight / image.height);
    const float left = (kBottomWidth - image.width * scale) * .5f;
    const float top = (kBottomHeight - image.height * scale) * .5f;
    C2D_DrawRectSolid(x, y, 0, w, h, C2D_Color32(0,0,0,255));
    DrawTextureRegion(image, (x-left)/scale, (y-top)/scale, w/scale, h/scale,
                      x, y, w, h, .05f);
    C2D_DrawRectSolid(x, y, .1f, w, h, C2D_Color32(0,0,0,204));
    DrawTexture(sUi.raceHud.timeLabel, x+2, 13, 32, 16, .7f);
    DrawTimerDigits(x+36, 13, 1);
}

// Only the top 42 pixels need restoration while a threat moves. Never redraw
// the minimap/standings at presentation rate or allocate a warning texture.
void DrawLiveBottomHeader() {
    auto& image = sUi.coursePreview;
    const float scale = std::max(kBottomWidth/image.width, kBottomHeight/image.height);
    const float left = (kBottomWidth-image.width*scale)*.5f;
    const float top = (kBottomHeight-image.height*scale)*.5f;
    C2D_DrawRectSolid(0, 0, 0, 320, 42, C2D_Color32(0,0,0,255));
    DrawTextureRegion(image, -left/scale, -top/scale, 320/scale, 42/scale,
                      0, 0, 320, 42, .05f);
    C2D_DrawRectSolid(0, 0, .1f, 320, 42, C2D_Color32(0,0,0,204));
    DrawRaceHudHeader();
    // Restore the warning's bounded empty gap, leaving all original HUD
    // rectangles, portrait textures and cached minimap untouched.
    constexpr float x=64, y=90, width=100, height=49;
    C2D_DrawRectSolid(x,y,0,width,height,C2D_Color32(0,0,0,255));
    DrawTextureRegion(image,(x-left)/scale,(y-top)/scale,width/scale,height/scale,
                      x,y,width,height,.05f);
    C2D_DrawRectSolid(x,y,.1f,width,height,C2D_Color32(0,0,0,204));
}

void DrawWarningRoundedRect(float x, float y, float w, float h, int radius,
                            float depth, uint32_t color) {
    // Pixel-rounded bands grouped by equal inset: a few rectangles, not a mesh
    // upload or one quad per pixel. Matches the approved 48x39 closed frame.
    static constexpr int corners[3][8] = {{5,3,2,1,1,0,0,0}, {4,3,2,1,0,0,0,0}, {3,1,1,0,0,0,0,0}};
    const int* inset = corners[radius == 8 ? 0 : radius == 7 ? 1 : 2];
    C2D_DrawRectSolid(x, y+radius, depth, w, h-2*radius, color);
    for (int row = 0; row < radius;) {
        int end = row+1;
        while (end < radius && inset[end] == inset[row]) ++end;
        C2D_DrawRectSolid(x+inset[row], y+row, depth, w-2*inset[row], end-row, color);
        C2D_DrawRectSolid(x+inset[row], y+h-end, depth, w-2*inset[row], end-row, color);
        row = end;
    }
}

void DrawThreatWarning() {
    const auto& motion = sUi.threatWarning;
    sUi.warningWasDrawn = motion.Visible();
    if (!sUi.warningWasDrawn) return;
    const float x = std::round(motion.x), y = std::round(motion.Y());
    const uint8_t alpha=static_cast<uint8_t>(255*motion.reveal);
    DrawWarningRoundedRect(x, y, 48, 39, 8, .90f, C2D_Color32(0,0,0,alpha));
    const uint32_t edge = motion.Pink() ? C2D_Color32(255,96,128,alpha) : C2D_Color32(255,255,255,alpha);
    DrawWarningRoundedRect(x+1, y+1, 46, 37, 7, .91f, edge);
    DrawWarningRoundedRect(x+3, y+3, 42, 33, 5, .92f, C2D_Color32(0,0,0,static_cast<uint8_t>(220*motion.reveal)));
    const float opacity=sDrawOpacity;
    sDrawOpacity*=motion.reveal;
    DrawTextureRegion(sUi.raceHud.items[motion.item], 3, 3, 34, 26,
                      x+5, y+4, 38, 30, .93f);
    sDrawOpacity=opacity;
}

#include "options_view_3ds.inc.cpp"

#include "update_ui_3ds.inc.cpp"

bool UpdateIsOpen() { return sUpdate.open; }

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
    std::snprintf(lines[2].data(), lines[2].size(), "FPS AVG   %.1f  LAST SECOND", sUi.fps.OneSecond());
    std::snprintf(lines[3].data(), lines[3].size(), "FPS NOW   %.1f  PRESENTED", sUi.fps.Instant());
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
    DrawText("SELECT CLOSE", 160.0f, 214.0f, 0.66f, C2D_Color32(255, 225, 80, 255),
             C2D_AlignCenter, 0.82f, true);
}

void DrawTopRaceHud() {
    if (!DrawsTopRaceHud()) return;
    const auto layout = Mk64Settings3DSGetHudLayout();
    const bool original = Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_ORIGINAL;
    const float left = original ? 48.0f : 8.0f;
    const float right = 400.0f - left;
    DrawItemFrame(mk64_3ds::TopItemRect(layout, original));
    DrawPlace(mk64_3ds::TopPlaceRect(layout, original));
    if (layout == MK64_HUD_LAYOUT_3DS_MKDS) DrawLap(right - 94, 8, 1.2f);
    if (layout == MK64_HUD_LAYOUT_3DS_CLASSIC) {
        DrawLap(left, 8, 1.2f);
        DrawTexture(sUi.raceHud.timeLabel, right - 102, 12, 32, 16, 0.8f);
        DrawTimerDigits(right - 68, 12, 1.0f);
        DrawStandings(true, left);
        DrawMinimap(right - 116, 88, 110, 140);
    }
}

void LoadFpsFont() {
    auto& font=sUi.fpsFont;
    if(font.initialized || !sUi.nativeFont.initialized ||
       !C3D_TexInit(&font.texture,256,32,GPU_RGBA8))return;
    std::memset(font.texture.data,0,256*32*4);
    unsigned column=0;
    // Prebake native-size lime glyphs and their black shadows. This avoids a
    // procedural tint dependency when the native 3D pass changes texture state.
    for(char c: std::string_view("FPS0123456789.-")) {
        const int i=GlyphIndexForCharacter(c);if(i<0)continue;
        const auto& sub=sUi.nativeFont.glyphs[2][i]; // Exactly ten logical pixels high.
        const unsigned sx=std::lround(sub.left*512),sy=std::lround((1-sub.top)*256);
        if(sub.width>16 || sub.height!=10)continue;
        for(unsigned y=0;y<10;++y)for(unsigned x=0;x<sub.width;++x) {
            const unsigned tx=sx+x,ty=sy+y;
            const size_t at=(size_t(ty/8)*64+tx/8)*64+MortonOffset8x8(tx%8,ty%8);
            const unsigned alpha=(static_cast<const uint8_t*>(sUi.nativeFont.texture.data)[at]&15)*17;
            WriteTiledPixel(font.texture,256,column*16+x,y,C2D_Color32(210,255,0,alpha));
            WriteTiledPixel(font.texture,256,column*16+x,y+16,C2D_Color32(0,0,0,alpha));
        }
        auto& glyph=font.glyphs[i];glyph.loaded=true;
        glyph.subTexture={sub.width,10,column*16/256.f,1.f,(column*16+sub.width)/256.f,1-10/32.f};
        ++column;
    }
    if(!Mk64System3DSCleanDataCache(font.texture.data,256*32*4)) {C3D_TexDelete(&font.texture);font={};return;}
    C3D_TexSetFilter(&font.texture,GPU_NEAREST,GPU_NEAREST);
    C3D_TexSetWrap(&font.texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
    font.initialized=true;
}
void DrawFpsGlyphs(const char* value,float right,float y) {
    const float left=right-MeasureText(value,.62f);
    for(unsigned shadow=0;shadow<2;++shadow) {
        float x=left;
        for(const char* c=value;*c;++c) {
            const int i=GlyphIndexForCharacter(*c);
            if(i>=0 && sUi.fpsFont.glyphs[i].loaded) {
                auto sub=sUi.fpsFont.glyphs[i].subTexture;
                if(!shadow){sub.top-=.5f;sub.bottom-=.5f;}
                const C2D_Image image{&sUi.fpsFont.texture,&sub};
                const C2D_DrawParams params={.pos={std::round(x)+(shadow?0:1),std::round(y)+(shadow?0:1),float(sub.width),float(sub.height)},
                    .center={0,0},.depth=shadow?.9f:.89f,.angle=0};
                C2D_DrawImage(image,&params,nullptr);
            }
            x+=CharacterAdvance(*c)*.62f;
        }
    }
}
void DrawTopFps(C3D_RenderTarget* topTarget) {
    if (topTarget == nullptr || !Mk64Settings3DSGetShowFpsEnabled()) return;
    if (sUi.game.demo || sUi.game.autoCinematic || (sUi.game.racing && sUi.game.raceFinished)) return;
    // Citro2D keeps a 400x240 logical projection for the top screen even when
    // Fast3D renders into the 800-wide high-density target.
    constexpr float topWidth = 400.0f;
    char fps[32] = {};
    if (sUi.fps.OneSecond() <= 0) std::snprintf(fps, sizeof(fps), "FPS --");
    else std::snprintf(fps, sizeof(fps), "FPS %.1f", sUi.fps.OneSecond());
    const float y=DrawsTopRaceHud() ? mk64_3ds::TopHudFpsY(Mk64Settings3DSGetHudLayout(),sUi.game.topHudRenderMode) : 6.f;
    if(sUi.fpsFont.initialized)DrawFpsGlyphs(fps,topWidth-8,y);
    else DrawText(fps,topWidth-8,y,.62f,C2D_Color32(210,255,0,255),C2D_AlignRight,.9f,true);
}


void DrawFittedText(const char* text, float x, float y, float width, float scale, uint32_t color,
                    uint32_t alignment = C2D_AlignLeft) {
    const float measured = MeasureText(text, scale);
    if (measured > width) scale *= width / measured;
    DrawText(text, x, y, scale, color, alignment, 0.8f, true);
}

bool DataActive() { return sUi.game.dataGrid || sUi.game.dataCourse; }
bool DataActionEnabled(int row) {
    return row == 0 || (row == 1 ? sUi.game.dataHasRecords : sUi.game.dataHasGhost);
}

void HandleDataInput(const Mk64DiagnosticsInput3DS& input) {
    uint32_t directions = input.heldMask & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
    if (input.circleY > 40) directions |= KEY_UP;
    if (input.circleY < -40) directions |= KEY_DOWN;
    if (input.circleX > 40) directions |= KEY_RIGHT;
    if (input.circleX < -40) directions |= KEY_LEFT;
    uint32_t pressed = input.downMask;
    const uint64_t now = osGetTime();
    if (directions != sUi.dataDirection) { pressed |= directions; sUi.dataRepeatAt = now + 320; }
    else if (directions && now >= sUi.dataRepeatAt) { pressed |= directions; sUi.dataRepeatAt = now + 130; }
    sUi.dataDirection = directions;
    if (sUi.game.dataGrid) {
        int delta = pressed & KEY_UP ? -2 : pressed & KEY_DOWN ? 2 :
                    pressed & KEY_LEFT ? -1 : pressed & KEY_RIGHT ? 1 : 0;
        if (delta) Mk64GameState3DSDataMove(delta);
        if (pressed & KEY_B) Mk64GameState3DSDataAction(-1);
        else if (pressed & (KEY_A | KEY_START)) { sUi.dataAction = 0; Mk64GameState3DSDataAction(0); }
    } else if (sUi.game.dataSubmenu != 11) {
        Mk64GameState3DSDataConfirm(pressed & KEY_UP ? -1 : pressed & KEY_DOWN ? 1 : 0,
            (pressed & (KEY_A | KEY_START)) != 0, (pressed & KEY_B) != 0);
    } else {
        if (pressed & KEY_LEFT) Mk64GameState3DSDataMove(-1);
        if (pressed & KEY_RIGHT) Mk64GameState3DSDataMove(1);
        int direction = pressed & KEY_UP ? -1 : pressed & KEY_DOWN ? 1 : 0;
        if (direction) {
            int row = sUi.dataAction;
            do { row += direction; } while (row >= 0 && row < 4 && !DataActionEnabled(row));
            if (row >= 0 && row < 4) sUi.dataAction = row;
        }
        if (!DataActionEnabled(sUi.dataAction)) sUi.dataAction = 0;
        if (pressed & KEY_TOUCH) {
            for (int row = 0; row < 4; ++row)
                if (PointInside(input.touchX, input.touchY, 18, 79 + row * 30, 284, 28) && DataActionEnabled(row)) {
                    sUi.dataAction = row; pressed |= KEY_A; break;
                }
        }
        if (pressed & KEY_B) Mk64GameState3DSDataAction(-1);
        else if (pressed & (KEY_A | KEY_START)) Mk64GameState3DSDataAction(sUi.dataAction);
    }
    sUi.bottomDirty = true;
}

uint32_t DataSelectionColor() {
    constexpr uint8_t colors[3][3] = {{0,243,243},{255,168,195},{255,255,0}};
    const uint64_t now = osGetTime();
    const int first = (now / 600) % 3, next = (first + 1) % 3;
    const int phase = now % 600;
    return C2D_Color32((colors[first][0]*(600-phase)+colors[next][0]*phase)/600,
        (colors[first][1]*(600-phase)+colors[next][1]*phase)/600,
        (colors[first][2]*(600-phase)+colors[next][2]*phase)/600,255);
}

void DrawDataText(const char* text, float x, float y, float scale, uint32_t color,
                  uint32_t alignment = C2D_AlignLeft, float depth = 0.8f, bool shadow = true) {
    const int r = color & 255, g = (color >> 8) & 255, b = (color >> 16) & 255;
    const int palette = r > 200 && g > 180 ? 0 : g > r ? 1 : r > g ? 2 : 3;
    sDataFontPalette = sUi.dataFonts[palette].initialized && sUi.dataFonts[4].initialized ? palette : -1;
    DrawText(text,x,y,scale,color,alignment,depth,shadow);
    sDataFontPalette = -1;
}

void DrawDataFittedText(const char* text, float x, float y, float width, float scale, uint32_t color,
                        uint32_t alignment = C2D_AlignLeft) {
    const float measured = MeasureText(text,scale);
    if (measured > width) scale *= width / measured;
    DrawDataText(text,x,y,scale,color,alignment);
}

void LoadDataTextures() {
    if (!DataActive()) return;
    for (int palette=0;palette<5;++palette) LoadFontAtlas(sUi.dataFonts[palette],palette);
    LoadTexture("__OTR__textures/texture_tkmk00/texture_data",sUi.dataHeading);
    if (sUi.game.dataCourse && std::strcmp(sUi.dataMapResource,sUi.minimap.resourceName)!=0) {
        std::snprintf(sUi.dataMapResource,sizeof(sUi.dataMapResource),"%s",sUi.minimap.resourceName);
        sUi.dataMapX=sUi.dataMapY=0;
        sUi.dataMapWidth=sUi.minimap.width;sUi.dataMapHeight=sUi.minimap.height;
        mk64_3ds::TextureLease3DS lease(sUi.dataMapResource);
        const auto& resource=lease.Get();
        if (lease.Valid() &&
            resource.size>=RequiredTextureBytes(resource)) {
            int left=resource.width,top=resource.height,right=-1,bottom=-1;
            for (uint32_t y=0;y<resource.height;++y) for (uint32_t x=0;x<resource.width;++x) {
                if ((DecodeTexturePixel(resource,y*resource.width+x)>>24)>16) {
                    left=std::min(left,int(x));right=std::max(right,int(x));
                    top=std::min(top,int(y));bottom=std::max(bottom,int(y));
                }
            }
            if (right>=left && bottom>=top) {
                sUi.dataMapX=left;sUi.dataMapY=top;
                sUi.dataMapWidth=right-left+1;sUi.dataMapHeight=bottom-top+1;
            }
        }
    }
    for (int i=0;i<16;++i) {
        if (sUi.game.dataGrid) LoadTexture(Mk64GameState3DSDataPreview(i),sUi.dataPreviews[i]);
        const char* suffix = std::strstr(Mk64GameState3DSDataPreview(i),"gTextureCoursePreview");
        if (suffix) {
            char resource[192];
            std::snprintf(resource,sizeof(resource),"__OTR__textures/texture_tkmk00/gTextureTitle%s",
                          suffix+std::strlen("gTextureCoursePreview"));
            LoadTexture(resource,sUi.dataTitles[i]);
        }
    }
    if (sUi.game.dataGrid) {
        const float target = (sUi.game.dataIndex/2)*140.0f;
        sUi.dataScroll += (target-sUi.dataScroll)*0.35f;
    }
}

// Crop the scrolling grid at the title strip without changing the renderer's
// rotated framebuffer/scissor state or allowing one row to cover the heading.
void DrawDataGridTexture(UiTexture& texture,float x,float y,float width,float height,float depth) {
    const float top=std::max(32.0f,y), bottom=std::min(240.0f,y+height);
    if (bottom<=top) return;
    DrawTextureRegion(texture,0,(top-y)/height*texture.height,texture.width,
        (bottom-top)/height*texture.height,x,top,width,bottom-top,depth);
}

void DrawDataBottom() {
    DrawDimMenuBackground();
    const auto white = C2D_Color32(255,255,255,255), yellow = C2D_Color32(255,225,80,255);
    DrawDataFittedText(sUi.game.trackName,160,22,292,1.0f,DataSelectionColor(),C2D_AlignCenter);
    char distance[48]; std::snprintf(distance,sizeof(distance),"DISTANCE  %s",sUi.game.trackLength);
    DrawDataText(distance,160,48,0.72f,C2D_Color32(255,80,80,255),C2D_AlignCenter,0.8f,true);
    if (sUi.game.dataGrid) {
        DrawDataText("A  VIEW COURSE DATA",160,106,0.9f,white,C2D_AlignCenter,0.8f,true);
        DrawDataText("B  EXIT",160,144,0.9f,white,C2D_AlignCenter,0.8f,true);
        DrawDataText("D-PAD OR STICK  SCROLL",160,202,0.68f,yellow,C2D_AlignCenter,0.8f,true);
    } else if (sUi.game.dataSubmenu == 12) {
        DrawDataText(sUi.game.dataEraseOption == 1 ? "ERASE COURSE RECORDS?" : "ERASE SAVED GHOST?",
                 160,88,0.78f,yellow,C2D_AlignCenter,0.8f,true);
        DrawDataText("THIS CANNOT BE UNDONE",160,111,0.65f,white,C2D_AlignCenter,0.8f,true);
        for (int row=0;row<2;++row) {
            if (row==sUi.game.dataConfirm) DrawTexture(sUi.selectionTriangle,50,149+row*30,12,8,0.8f);
            DrawDataText(row==0 ? "KEEP DATA" : "ERASE DATA",160,144+row*30,0.9f,
                row==sUi.game.dataConfirm ? C2D_Color32(100,255,100,255) : yellow,C2D_AlignCenter,0.8f,true);
        }
        DrawDataText("A  CONFIRM     B  CANCEL",160,214,0.64f,white,C2D_AlignCenter,0.8f,true);
    } else if (sUi.game.dataSubmenu != 11) {
        DrawDataText("COULD NOT ERASE DATA",160,108,0.82f,yellow,C2D_AlignCenter,0.8f,true);
        DrawDataText("A OR B  BACK",160,164,0.8f,white,C2D_AlignCenter,0.8f,true);
    } else {
        const char* labels[]={"RETURN TO MENU","ERASE RECORDS","ERASE GHOST","RACE GHOST"};
        for (int row=0;row<4;++row) {
            const auto color=!DataActionEnabled(row) ? C2D_Color32(100,100,180,255) : row==sUi.dataAction ? DataSelectionColor() : C2D_Color32(100,255,100,255);
            if (row==sUi.dataAction) DrawTexture(sUi.selectionTriangle,20,89+row*30,12,8,0.8f);
            DrawDataText(labels[row],43,83+row*30,0.85f,color,C2D_AlignLeft,0.8f,true);
        }
        DrawDataText("A  SELECT     B  BACK",160,214,0.68f,white,C2D_AlignCenter,0.8f,true);
    }
}

void DrawDataTop() {
    // This custom layer is submitted after Fast3D's native fade. Repaint its
    // own base, then apply the shared fade after all custom content, once.
    C2D_DrawRectSolid(0, 0, 0.02f, 400, 240, C2D_Color32(0, 0, 0, 255));
    if (!sUi.game.dataGrid)
        DrawTextureCover(sUi.menuBackground, 0, 0, 400, 240, 0.05f);
    const auto yellow=C2D_Color32(255,225,80,255), white=C2D_Color32(255,255,255,255);
    if (sUi.game.dataGrid) {
        for (int index=0;index<16;++index) {
            const bool selected=index==sUi.game.dataIndex;
            const float zoom=selected ? 1.035f+0.012f*std::sin(osGetTime()*0.006f) : 1.0f;
            const float w=176*zoom,h=132*zoom;
            const float x=12+(index%2)*194-(w-176)/2;
            const float y=39+(index/2)*140-sUi.dataScroll-(h-132)/2;
            if (y+h<32 || y>=240) continue;
            if (selected) {
                const float top=std::max(32.0f,y-3),bottom=std::min(240.0f,y+h+3);
                C2D_DrawRectSolid(x-3,top,0.3f,w+6,bottom-top,DataSelectionColor());
            }
            const float top=std::max(32.0f,y),bottom=std::min(240.0f,y+h);
            C2D_DrawRectSolid(x,top,0.35f,w,bottom-top,C2D_Color32(0,0,0,255));
            DrawDataGridTexture(sUi.dataPreviews[index],x,y,w,107.25f*zoom,0.4f);
            DrawDataGridTexture(sUi.dataTitles[index],x,y+107.25f*zoom,w,24.75f*zoom,0.5f);
        }
        C2D_DrawRectSolid(12,4,0.85f,96,27,C2D_Color32(0,0,0,255));
        DrawTexture(sUi.dataHeading,12,4,96,27,0.9f);
        if (sUi.game.dataIndex>=2) C2D_DrawTriangle(374,8,white,366,18,white,382,18,white,0.9f);
        if (sUi.game.dataIndex<14) C2D_DrawTriangle(374,233,white,366,223,white,382,223,white,0.9f);
    } else {
        // Preserve the original red-filtered menu background rendered below.
        C2D_DrawRectSolid(0,0,0.1f,400,240,C2D_Color32(0,0,0,90));
        DrawTexture(sUi.dataHeading,12,7,96,27,0.9f);
        DrawTexture(sUi.coursePreview,12,49,156,95,0.4f);
        DrawDataText("BEST LAP",22,169,0.86f,yellow);
        DrawDataText("BEST RECORDS",190,46,0.76f,yellow);
        // A dedicated full-height column gives tall circuits useful scale.
        // Wide circuits still preserve their shape within the same bounds.
        if (sUi.dataMapWidth>0 && sUi.dataMapHeight>0) {
            // Fit the visible course outline, excluding transparent padding.
            const float scale=std::min(68.0f/sUi.dataMapWidth,176.0f/sUi.dataMapHeight);
            const float w=sUi.dataMapWidth*scale,h=sUi.dataMapHeight*scale;
            C2D_ImageTint mapTint{};
            C2D_PlainImageTint(&mapTint,C2D_Color32(sUi.game.minimapRed,sUi.game.minimapGreen,sUi.game.minimapBlue,255),1.0f);
            DrawTextureRegion(sUi.minimap,sUi.dataMapX,sUi.dataMapY,sUi.dataMapWidth,sUi.dataMapHeight,
                356-w/2,136-h/2,w,h,0.6f,&mapTint);
        }
        for (int row=0;row<6;++row) {
            const uint32_t record=sUi.game.dataTimes[row],centis=record&0xfffff;
            char time[32];
            if (centis>=600000) std::snprintf(time,sizeof(time),"--'--''--");
            else std::snprintf(time,sizeof(time),"%02lu'%02lu''%02lu",(unsigned long)(centis/6000),
                (unsigned long)(centis/100%60),(unsigned long)(centis%100));
            const float y=row<5 ? 76+row*29 : 195;
            const float x=row<5 ? 204 : 22;
            if (row<5) {
                char rank[4];std::snprintf(rank,sizeof(rank),"%d",row+1);
                DrawDataText(rank,187,y,0.75f,C2D_Color32(100,255,100,255));
            }
            DrawDataText(time,x,y,row<5 ? 0.83f : 0.9f,yellow);
            const auto character=record>>20;
            if (centis<600000 && character<8)
                DrawTexture(sUi.raceHud.portraits[character],row<5 ? 304 : 145,y-2,18,18,0.8f);
        }
    }
}

bool IdleMenuBackdropActive() {
    const bool transitioningSelect = sUi.view == BaseView::GameSelect &&
        std::max(sUi.game.gameSelectOptionReveal, sUi.game.gameSelectDataReveal) < 1.0f;
    return mk64_3ds::IdleMenuBackdrop(sUi.game.gameState, sUi.game.menuSelection,
        (sUi.view != BaseView::Background && !transitioningSelect) || sUi.game.racing || sUi.modalOpen ||
        DataActive() || Mk64Settings3DSGetOverlayEnabled() ||
        sMenuTransition.blankBackdrop);
}

void DrawMenuConveyor() {
    if (!Mk64Settings3DSGetMenuMaps()) return;
    for (unsigned slot = 0; slot < 5; ++slot) {
        const auto tile = mk64_3ds::MenuConveyorSlot(sUi.conveyorElapsed, slot);
        auto& texture = sUi.menuMaps[tile.map];
        if (!texture.initialized || tile.x + mk64_3ds::kMenuConveyorSpacing <= 0 ||
            tile.x >= kBottomWidth) continue;
        // Keep the conveyor cadence and horizontal centers; give the hint
        // below it more room with a 20% smaller map, raised by 10% of the screen.
        constexpr float mapScale = 0.8f;
        constexpr float centerY = kBottomHeight * (0.5f - 0.10f);
        const float width = mk64_3ds::kBottomMinimapAreaWidth * mapScale;
        const float height = mk64_3ds::kBottomMinimapAreaHeight * mapScale;
        const uint32_t color = C2D_Color32(255, 255, 255, 230);
        DrawMinimapOutline(texture, tile.x + (mk64_3ds::kMenuConveyorSpacing - width) * 0.5f,
                           centerY - height * 0.5f, width, height, color, false, 0.3f);
    }
}

void DrawBottom() {
    if (UpdateIsOpen()) { DrawUpdateBottom(); return; }
    if (mk64_3ds::CreditsBottomBlank(gGamestate, gGamestateNext, sUi.game.gameState)) return;
    if (sMenuTransition.blankBackdrop) return;
    if (DataActive() && !sUi.modalOpen) { DrawDataBottom(); return; }
    if (sUi.modalOpen) {
        DrawOptions();
    } else if (Mk64Settings3DSGetOverlayEnabled()) {
        DrawDeveloperOverlay();
    } else {
        if (sUi.game.racing && sUi.game.autoCinematic && !sUi.game.paused) return;
        switch (sUi.view) {
            case BaseView::GameSelect: DrawGameSelect(); break;
            case BaseView::RaceHud: DrawRaceHud(); break;
            case BaseView::Paused: DrawRaceBackground(204.0f); break;
            case BaseView::Background:
            default:
                DrawDimMenuBackground();
                if (IdleMenuBackdropActive()) DrawMenuConveyor();
                // MAIN_MENU can remain logical-active after its L/R items
                // finish collapsing. Do not pop the full hint back into view.
                if (Mk64RandomCup3DSMenuKind() == 2)
                    DrawRandomCupHint(false);
                DrawAutoHint();
                if (sUi.game.gameState == 5) // Native ENDING.
                    DrawText("A OR START SKIP", 160, 212, 0.65f,
                             C2D_Color32(255,255,255,255), C2D_AlignCenter, 0.76f, true);
                break;
        }
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

void DrawMenuTransitionOverlay(float width) {
    if (!sMenuTransition.alpha) return;
    const auto shade = sMenuTransition.shade;
    C2D_DrawRectSolid(0, 0, 0.99f, width, 240,
                      C2D_Color32(shade, shade, shade, sMenuTransition.alpha));
}

void DrawRaceTransitionOverlay() {
    if (gGamestate != 4) {
        // Scene dispatch can leave a race snapshot on the first menu image.
        // Retain the black exit hold instead of exposing its live timer before
        // the next PrepareFrame replaces that snapshot with the menu.
        if (sUi.game.racing)
            C2D_DrawRectSolid(0, 0, .99f, 320, 240, C2D_Color32(0,0,0,255));
        return;
    }
    // Follow the native opening fraction, not wall time or courseTimer. Retain
    // the full-sized lower layout behind opaque letterbox shutters.
    // The input snapshot is taken before native scene dispatch. Cover its old
    // menu contents completely on the first race image, rather than exposing
    // them through already-opening shutters for one frame.
    const float bars = sUi.game.racing ? std::ceil(sRaceTransition.BarHeight()) : 120.0f;
    if (bars > 0) {
        C2D_DrawRectSolid(0, 0, .99f, 320, bars, C2D_Color32(0,0,0,255));
        C2D_DrawRectSolid(0, 240-bars, .99f, 320, bars, C2D_Color32(0,0,0,255));
    }
    if (sRaceTransition.exitAlpha)
        C2D_DrawRectSolid(0, 0, .99f, 320, 240, C2D_Color32(0,0,0,sRaceTransition.exitAlpha));
}

bool RaceExitBlackHold() {
    return (gGamestate == 4 || sUi.game.racing) && sRaceTransition.exitAlpha == 255;
}

void DrawBottomBatch() {
    // Clear before any Citro2D objects are queued. C2D_TargetClear after a top
    // batch used to split that pending batch and could invalidate its texture
    // state on hardware.
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    // Raw Citro3D clears use 0xRRGGBBAA; C2D_Color32 uses 0xAABBGGRR.
    C3D_RenderTargetClear(sUi.bottomTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
    PrepareC2DBatch();
    Mk64Graphics3DSMarkExternalLinearBuffersDirty();
    C2D_SceneBegin(sUi.bottomTarget);
    DrawBottom();
    DrawThreatWarning();
    DrawMenuTransitionOverlay(320);
    DrawRaceTransitionOverlay();
    C2D_Flush();
}

bool TitleCreditVisible() {
    return sUi.game.gameState >= 0 && sUi.game.gameState <= 3 &&
        sUi.game.menuSelection == 10 && !sMenuTransition.blankBackdrop;
}

void DrawTopFpsBatch(C3D_RenderTarget* topTarget) {
    if (topTarget == nullptr || !Mk64BottomUI3DSNeedsTopOverlay()) return;
    // Preserve the completed game color buffer and clear only reverse-depth so
    // the overlay cannot be hidden behind scene geometry.
    // Split the queued lower-screen commands before clearing top depth.
    // EndFrame cleans C2D geometry, but its narrow ranges do not include this
    // command-list fragment. Publish each list independently before GSP reads it.
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    C3D_RenderTargetClear(topTarget, C3D_CLEAR_DEPTH, 0, 0);
    PrepareC2DBatch();
    Mk64Graphics3DSMarkExternalLinearBuffersDirty();
    C2D_SceneBegin(topTarget);
    if (UpdateIsOpen()) { DrawUpdateTop(); C2D_Flush(); return; }
    if (TitleCreditVisible()) {
        C2D_ImageTint tint = {};
        C2D_AlphaImageTint(&tint, (255-sMenuTransition.alpha)/255.0f);
        const float width=sUi.titleCredit.width;
        const float height=sUi.titleCredit.height;
        DrawTexture(sUi.titleCredit,std::floor((400-width)*.5f),202,width,height,.80f,&tint);
    }
    // Native Fast3D fades were drawn before this custom overlay. Fade the HUD
    // with that same retained exit state; never redraw rank over held black.
    sDrawOpacity = sRaceTransition.TopHudOpacity(sUi.game.topHudOpacity);
    if (sDrawOpacity > 0) DrawTopRaceHud();
    sDrawOpacity = 1.0f;
    if (DataActive()) {
        DrawDataTop();
        DrawMenuTransitionOverlay(400);
    }
    DrawTopFps(topTarget);
    DrawStatus();
    // This is the final cover, after both native HUD and custom live text.
    // Retain it across the one-image pre-dispatch snapshot lag on exit too.
    if (RaceExitBlackHold())
        C2D_DrawRectSolid(0, 0, .99f, 400, 240, C2D_Color32(0,0,0,255));
    C2D_Flush();
}

} // namespace

extern "C" void Mk64BottomUI3DSSetMenuTransition(int logo, int menu, int type,
                                                    unsigned time, unsigned duration) {
    const auto next = mk64_3ds::ResolveMenuTransition(logo != 0, menu != 0, type, time, duration);
    if (sMenuTransition.blankBackdrop != next.blankBackdrop || sMenuTransition.shade != next.shade ||
        sMenuTransition.alpha != next.alpha) sUi.bottomDirty = true;
    sMenuTransition = next;
}

extern "C" void Mk64BottomUI3DSSetRaceOpening(float reveal) {
    const auto before = sRaceTransition;
    sRaceTransition.SetOpening(reveal);
    if (before.opening != sRaceTransition.opening || before.exitAlpha != sRaceTransition.exitAlpha)
        sUi.bottomDirty = true;
}

extern "C" void Mk64BottomUI3DSSetRaceExitSlide(int pixels) {
    const auto before = sRaceTransition;
    sRaceTransition.SetExitSlide(pixels);
    if (before.opening != sRaceTransition.opening || before.exitAlpha != sRaceTransition.exitAlpha)
        sUi.bottomDirty = true;
}

extern "C" void Mk64BottomUI3DSSetRaceExitFade(int type, unsigned time, unsigned duration) {
    const auto before = sRaceTransition;
    sRaceTransition.SetExitFade(type, time, duration);
    if (before.opening != sRaceTransition.opening || before.exitAlpha != sRaceTransition.exitAlpha)
        sUi.bottomDirty = true;
}

extern "C" bool Mk64BottomUI3DSInit() {
    if (sUi.initialized) return true;
    sUi = {};
    Mk64System3DSBeginLinearAllocationCapture();
    const bool c2dInitialized = C2D_Init(kC2DObjectCapacity);
    const void* c2dLinearBase = nullptr;
    size_t c2dLinearBytes = 0;
    const bool c2dRangeCaptured =
        Mk64System3DSEndLinearAllocationCapture(&c2dLinearBase,
                                                &c2dLinearBytes);
    if (!c2dInitialized) return false;
    if (c2dRangeCaptured) {
        Mk64Graphics3DSSetExternalLinearBufferRange(c2dLinearBase,
                                                    c2dLinearBytes);
    }
    sUi.bottomTarget = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if (sUi.bottomTarget == nullptr) {
        C2D_Fini();
        return false;
    }
    C3D_RenderTargetSetOutput(sUi.bottomTarget, GFX_BOTTOM, GFX_LEFT, kTransferFlags);
    if (!LoadFontAtlas(sUi.font)) {
        C3D_RenderTargetDelete(sUi.bottomTarget);
        sUi.bottomTarget = nullptr;
        C2D_Fini();
        sUi = {};
        return false;
    }
    C2D_Prepare();
    sUi.initialized = true;
    sUi.bottomDirty = true;
    sUi.modalOpen = false;
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    sUi.view = GetBaseView(sUi.game);
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground, nullptr, sUi.game.dataCourse);
    LoadNativeFont();
    LoadFpsFont();
    LoadRandomCupHint();
    LoadOptionIcons();
    LoadFontAtlas(sUi.dataFonts[5], 5); // Native cyan/magenta lettering survives the selection flash.
    LoadTexture(kSelectionTriangleResource, sUi.selectionTriangle);
    UiTexture copyright{};
    if (LoadTexture("__OTR__textures/player_selection/copyright_1996",copyright)) {
        const auto image=mk64_3ds::MakeNativeTitleCredits(ReadUiPixels(copyright));
        DeleteTexture(copyright); // Owned staging image, never submitted to a draw.
        UiTexture credit{};
        const auto bw=NextPowerOfTwo(image.width),bh=NextPowerOfTwo(image.height);
        if (image.Valid() && InitBackdropTexture(credit,bw,bh)) {
            for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x)
                WriteTiledPixel(credit.texture,bw,x,y,image.pixels[size_t(y)*image.width+x]);
            credit.width=credit.subTexture.width=image.width;
            credit.height=credit.subTexture.height=image.height;
            credit.subTexture.right=float(image.width)/bw;
            credit.subTexture.bottom=1-float(image.height)/bh;
            C3D_TexSetFilter(&credit.texture,GPU_NEAREST,GPU_NEAREST);
            // This image already contains both lines at their final size.
            // The artwork pack's older port-only variant must not replace it.
            PublishBackdropTexture(sUi.titleCredit,credit);
        }
    }
    // Preload the complete, bounded lower HUD while menus are starting. No
    // archive reads or C3D allocations are then needed when a race begins,
    // an item changes, or the standings reorder.
    LoadRaceHudTextures();
    // Stage the selected course art in menus so entering a race never has to
    // allocate or decode these lower-screen textures on its first frame.
    LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
    LoadTexture(sUi.game.minimapTexture, sUi.minimap);
    LoadDataTextures();
    if (!DataActive()) { sUi.dataDirection=0; sUi.dataAction=0; }
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
    DeleteTexture(sUi.softMenuBackground);
    DeleteTexture(sUi.optionIcons);
    DeleteTextures(sUi.menuMaps);
    DeleteTexture(sUi.gameSelectData);
    DeleteTexture(sUi.randomCupHint);
    DeleteTexture(sUi.autoHint);
    for (auto& preview : sUi.dataPreviews) DeleteTexture(preview);
    for (auto& title : sUi.dataTitles) DeleteTexture(title);
    DeleteTexture(sUi.dataHeading);
    DeleteTexture(sUi.gameSelectOption);
    DeleteTexture(sUi.selectionTriangle);
    DeleteTexture(sUi.titleCredit);
    DeleteFontAtlas();
    if (sUi.bottomTarget != nullptr) C3D_RenderTargetDelete(sUi.bottomTarget);
    C2D_Fini();
    sUi = {};
}

extern "C" void Mk64BottomUI3DSResetTurbo() {
    sUi.turbo.Reset();
    sUi.consumesCStick = false;
    Mk64GameState3DSApplyTurbo(false, 1);
}

extern "C" void Mk64BottomUI3DSPrepareFrame() {
    if (!sUi.initialized) return;
    sUi.resumeThrottleThisFrame = false;
    if (sUi.status[0] != '\0' && osGetTime() >= sUi.statusExpiresAt) {
        sUi.status[0] = '\0';
        sUi.statusExpiresAt = 0;
        sUi.bottomDirty = true;
    }
    const BaseView oldView = sUi.view;
    const auto oldGameState = sUi.game.gameState;
    const bool wasPaused = sUi.game.paused;
    const int32_t oldMenuSelection = sUi.game.menuSelection;
    const size_t oldTrack = sUi.game.trackIndex;
    const char* oldBackground = sUi.game.mainBackgroundTexture;
    const float oldOptionReveal = sUi.game.gameSelectOptionReveal, oldDataReveal = sUi.game.gameSelectDataReveal;
    const auto oldDriver=sUi.game.selectedDriver, oldLevel=sUi.game.selectedLevel;
    const float oldItemSlideY = sUi.game.itemWindowSlideY;
    const bool oldItemVisible = sUi.game.itemWindowVisible;
    const int oldObservedPlayer = sUi.game.observedPlayer;
    const auto oldRaceTime = sUi.game.courseTimerCentiseconds;
    const auto oldHudTick = sUi.game.hudAnimationTick;
    const mk64_3ds::StandingsPresentation3DS oldStandings(sUi.game);
    std::array<uint16_t, MK64_BOTTOM_UI_RACER_COUNT> oldHitAngles{};
    for (size_t id=0; id<oldHitAngles.size(); ++id)
        oldHitAngles[id] = sUi.game.racers[id].portraitHitAngle;
    Mk64GameState3DSUpdateHudAnimations();
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    if (sUi.game.racing && oldStandings.Changed(sUi.game)) sUi.bottomDirty = true;
    sUi.view = GetBaseView(sUi.game);
    if (oldMenuSelection != sUi.game.menuSelection) sUi.autoHintTween = {};
    if (sUi.autoHintTween.Update(sUi.game.autoMenuAvailable,osGetTime())) sUi.bottomDirty=true;
    if (oldView != sUi.view || oldGameState != sUi.game.gameState ||
        oldMenuSelection != sUi.game.menuSelection ||
        oldTrack != sUi.game.trackIndex ||
        oldBackground != sUi.game.mainBackgroundTexture ||
        oldDriver != sUi.game.selectedDriver || oldLevel != sUi.game.selectedLevel ||
        oldOptionReveal != sUi.game.gameSelectOptionReveal || oldDataReveal != sUi.game.gameSelectDataReveal) {
        sUi.bottomDirty = true;
    }
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground, nullptr, sUi.game.dataCourse);
    // The selected track usually changes while still in Map Select, before
    // the RACING edge. Compare the actual resource names every frame so a
    // second race can never retain the previous course art. LoadTexture's
    // strcmp fast path makes the steady state allocation- and I/O-free.
    LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
    LoadTexture(sUi.game.minimapTexture, sUi.minimap);
    LoadDataTextures();
    if (!DataActive()) { sUi.dataDirection=0; sUi.dataAction=0; }

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

    const auto selectAction = sUi.selectPolicy.Update(input.heldMask,input.downMask,
        sUi.game.racing && !sUi.game.paused && !sUi.game.demo && !sUi.modalOpen && !DataActive(),
        Mk64Settings3DSGetSelectAction());
    if (selectAction == mk64_3ds::SelectEvent3DS::DumpChord) {
        sUi.blockedGameKeys |= KEY_L | KEY_R | KEY_SELECT;
        input.downMask &= ~(KEY_L | KEY_R | KEY_SELECT);
        SetStatus(Mk64Diagnostics3DSRequestFullDump() ? "MEMORY DUMP REQUESTED" : "DUMP ALREADY QUEUED",2400);
    } else if (selectAction == mk64_3ds::SelectEvent3DS::Dump || selectAction == mk64_3ds::SelectEvent3DS::Overlay) {
        sUi.blockedGameKeys |= KEY_SELECT; input.downMask &= ~KEY_SELECT;
        if (selectAction == mk64_3ds::SelectEvent3DS::Dump)
            SetStatus(Mk64Diagnostics3DSRequestDump() ? "MEMORY DUMP REQUESTED" : "DUMP ALREADY QUEUED",2400);
        else { Mk64Settings3DSSetOverlayEnabled(!Mk64Settings3DSGetOverlayEnabled()); sUi.bottomDirty=true; }
    }
    if (!sUi.game.racing && Mk64Settings3DSGetOverlayEnabled()) { Mk64Settings3DSSetOverlayEnabled(false); sUi.bottomDirty=true; }
    if (sUi.game.racing && !sUi.game.paused && !sUi.game.demo && !sUi.modalOpen &&
        !Mk64Auto3DSRaceInputLocked() && !Mk64SoloBattle3DSSpectating() && (input.downMask & KEY_Y)) {
        Mk64Settings3DSSetHudLayout(static_cast<Mk64HudLayout3DS>((Mk64Settings3DSGetHudLayout()+1)%5));
        SaveChangedSetting("HUD LAYOUT SAVED"); sUi.blockedGameKeys |= KEY_Y;
    }
    Mk64Auto3DSMenuInput((input.downMask & KEY_A) && !(input.downMask & KEY_SELECT),
                       (input.downMask & KEY_B) != 0);
    static int autoStickDirection = 0;
    const int stickDirection = input.circleX < -60 ? -1 : (input.circleX > 60 ? 1 : 0);
    if ((Mk64Auto3DSRaceInputLocked() || Mk64SoloBattle3DSSpectating()) && !sUi.game.paused && !sUi.modalOpen) {
        int direction = (input.downMask & KEY_DLEFT) ? -1 : ((input.downMask & KEY_DRIGHT) ? 1 : 0);
        if (!direction && stickDirection != autoStickDirection) direction = stickDirection;
        const bool toggle = (input.downMask & (KEY_A | KEY_B | KEY_X | KEY_Y)) != 0;
        if(Mk64Auto3DSRaceInputLocked())Mk64Auto3DSObserveInput(direction, toggle);
        else Mk64SoloBattle3DSObserveInput(direction, toggle);
        if (direction || toggle) { Mk64GameState3DSGetBottomUISnapshot(&sUi.game); sUi.bottomDirty = true; }
    }
    autoStickDirection = stickDirection;

    if (!sUi.modalOpen && !DataActive()) {
        const int randomMenu = Mk64RandomCup3DSMenuKind();
        if ((input.downMask & KEY_SELECT) && Mk64Auto3DSSelect()) {
            sUi.blockedGameKeys |= input.heldMask;
            sUi.injectedGameKeys |= KEY_A;
            sUi.bottomDirty = true;
        } else if ((input.downMask & KEY_SELECT) && randomMenu != 0) {
            if (Mk64RandomCup3DSStart(randomMenu == 1, osGetTime() ^ svcGetSystemTick())) {
                sUi.blockedGameKeys |= input.heldMask;
                sUi.bottomDirty = true;
            }
        } else if (sUi.game.gameState == 5 && (input.downMask & (KEY_A | KEY_START))) {
            if (Mk64GameState3DSSkipCeremony()) {
                sUi.blockedGameKeys |= input.heldMask;
                sUi.bottomDirty = true;
            }
        }
    }

    if (DataActive()) HandleDataInput(input);
    const bool capturedAtFrameStart = DataActive() || sUi.modalOpen;
    bool openedThisFrame = false;
    if (!sUi.modalOpen) {
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
            if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 38, 72, 100, 36)) {
                OpenOptions(false);
                openedThisFrame = true;
            } else if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 182, 72, 100, 36)) {
                sUi.injectedGameKeys |= KEY_R;
            } else if (sUi.game.paused && PointInside(input.touchX, input.touchY, 108, 96, 104, 48)) {
                OpenOptions(true);
                openedThisFrame = true;
            }
        }
    }
    if ((sUi.modalOpen) && !openedThisFrame) {
        HandleModalInput(input);
    }

    // A button that opened or closed the modal must not fall through to the
    // vanilla menu later in this same simulation tick. Keep every captured
    // digital key blocked until the HID worker observes its release.
    sUi.captureGameInputThisFrame = capturedAtFrameStart || openedThisFrame;
    if (sUi.captureGameInputThisFrame || sUi.modalOpen) {
        sUi.blockedGameKeys |= input.heldMask | input.downMask;
    }
    if (sUi.resumeThrottleThisFrame) sUi.blockedGameKeys &= ~static_cast<uint32_t>(KEY_A);

    if (sUi.modalOpen && !UpdateIsOpen() &&
        !sUi.game.gameSelectVisible && !sUi.game.paused) {
        CloseOptions();
    }

    Mk64GameState3DSSetTopHudEnabled(Mk64Settings3DSGetTopHudEnabled());
    const uint8_t turbo = Mk64Settings3DSGetTurboMultiplier();
    const bool cstickHeld = std::abs(static_cast<int>(input.cstickX)) > kCStickTurboDeadzone ||
                            std::abs(static_cast<int>(input.cstickY)) > kCStickTurboDeadzone;
    const bool turboCaptured = sUi.modalOpen || sUi.captureGameInputThisFrame;
    sUi.consumesCStick = sUi.game.racing && !sUi.game.paused && !turboCaptured && turbo > 1;
    const int speed = sUi.turbo.Update(turbo, sUi.game.paused, turboCaptured,
        false, sUi.game.racing && cstickHeld);
    Mk64GameState3DSApplyTurbo(speed > 1, static_cast<uint8_t>(speed));

    // Includes Options, DATA grid and per-course DATA, not only idle menus.
    // Rebuild on the native red-filter edge too, so no page falls back to sharp art.
    if (sUi.game.gameState >= 0 && sUi.game.gameState <= 3)
        LoadSoftMenuBackground();
    if (IdleMenuBackdropActive() && Mk64Settings3DSGetMenuMaps()) {
        LoadMenuMaps();
        const uint64_t now = osGetTime();
        if (sUi.conveyorLastTick) sUi.conveyorElapsed += now - sUi.conveyorLastTick;
        sUi.conveyorLastTick = now;
        sUi.bottomDirty = true;
    } else {
        // Do not advance behind races or interactive lower-screen panels.
        sUi.conveyorLastTick = 0;
    }

    // Snapshots follow the 30 Hz simulation. The resolved profile redraws the
    // lower race HUD at 10 Hz, or 15 Hz with sustained New-400 headroom. Menu
    // and modal changes mark the target dirty immediately above.
    if (sUi.view == BaseView::RaceHud) {
        const uint32_t divisor = Mk64Graphics3DSBottomHudRefreshDivisor();
        if (sUi.raceHudRefreshPhase == 0) sUi.bottomDirty = true;
        // Native item motion and finish flashing must not be reduced to the
        // cached map/standings rate. Refresh only these short animation spans.
        if (sUi.game.raceFinished || (Mk64Settings3DSGetHudLayout() == MK64_HUD_LAYOUT_3DS_CLASSIC &&
            (oldItemSlideY != sUi.game.itemWindowSlideY || oldItemVisible != sUi.game.itemWindowVisible)))
            sUi.bottomDirty = true;
        // Keep cache-rate rendering for a steady race, but never drop the
        // native blink edges or the brief rigid hit-rotation frames.
        if (mk64_3ds::MinimapLeaderWhite(oldHudTick) != mk64_3ds::MinimapLeaderWhite(sUi.game.hudAnimationTick))
            sUi.bottomDirty = true;
        for (size_t rank=0;rank<std::min<size_t>(4,sUi.game.standingCount);++rank) {
            const int id=sUi.game.standingPlayerIds[rank];
            if (id>=0 && id<static_cast<int>(oldHitAngles.size()) && oldHitAngles[id]!=sUi.game.racers[id].portraitHitAngle) sUi.bottomDirty=true;
        }
        sUi.raceHudRefreshPhase = static_cast<uint8_t>((sUi.raceHudRefreshPhase + 1U) % divisor);
    } else {
        sUi.raceHudRefreshPhase = 0;
    }
    if (sUi.modalOpen) sUi.bottomDirty = true; // Animate native cursor while paused.
    const bool warningAllowed = Mk64Settings3DSGetThreatWarnings() && !Mk64Settings3DSGetOverlayEnabled() && sUi.view == BaseView::RaceHud && !sUi.modalOpen &&
        !Mk64Settings3DSGetOverlayEnabled() && !sUi.game.paused && !sUi.game.demo &&
        !sUi.game.autoCinematic && !sUi.game.raceFinished;
    if (!warningAllowed || oldTrack != sUi.game.trackIndex ||
        oldObservedPlayer != sUi.game.observedPlayer ||
        sUi.game.courseTimerCentiseconds < oldRaceTime) {
        sUi.threatWarning.Reset();
        if (sUi.warningWasDrawn) sUi.bottomDirty = true;
    }
    Mk64ThreatWarning3DS threat{};
    sUi.threatWarning.Target(warningAllowed && Mk64ThreatWarning3DSRead(&threat) ? &threat : nullptr);
}

extern "C" void Mk64BottomUI3DSDraw(void* existingTopTarget) {
    sUi.lastTopTarget = static_cast<C3D_RenderTarget*>(existingTopTarget);
    if (!sUi.initialized || sUi.bottomTarget == nullptr || existingTopTarget == nullptr) return;
    // The renderer enters this function from a SYNCDRAW frame. Previous GPU
    // work is complete, so textures replaced during PrepareFrame are now safe
    // to release before issuing any new Citro2D commands.
    DrainRetiredTextures();
    sUi.threatWarning.Advance(osGetTime());
    if (sUi.view == BaseView::RaceHud && sUi.game.racing && !sUi.game.raceFinished) {
        const mk64_3ds::StandingsPresentation3DS oldStandings(sUi.game);
        Mk64GameState3DSRefreshStandings(&sUi.game);
        if (oldStandings.Changed(sUi.game)) sUi.bottomDirty = true;
    }
    const bool drawTopFps = Mk64BottomUI3DSNeedsTopOverlay();
    const bool liveTimer = sUi.view == BaseView::RaceHud && !sUi.modalOpen &&
        !Mk64Settings3DSGetOverlayEnabled() &&
        !sUi.game.paused && !sUi.game.autoCinematic && !sUi.game.raceFinished && sUi.coursePreview.initialized;
    if (mk64_3ds::CreditsBottomBlank(gGamestate, gGamestateNext, sUi.game.gameState)) {
        // Covers native 255->9 reloads and the pre-dispatch snapshot lag.
        // Bypass ALL lower overlays/fades, not only the backdrop draw. Even
        // a clean cached target can still contain ceremony/menu artwork.
        C3D_FrameSplit(GX_CMDLIST_FLUSH);
        C3D_RenderTargetClear(sUi.bottomTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
        sUi.bottomDirty = false;
    } else if (sUi.bottomDirty) {
        DrawBottomBatch();
        sUi.bottomDirty = false;
    } else if (liveTimer) {
        C3D_FrameSplit(GX_CMDLIST_FLUSH);
        // Retain color, but do not let the previous digits' depth reject the
        // background restoration or equally placed digits on the next image.
        C3D_RenderTargetClear(sUi.bottomTarget, C3D_CLEAR_DEPTH, 0, 0);
        PrepareC2DBatch();
        Mk64Graphics3DSMarkExternalLinearBuffersDirty();
        C2D_SceneBegin(sUi.bottomTarget);
        if (sUi.threatWarning.Visible() || sUi.warningWasDrawn) DrawLiveBottomHeader();
        else DrawLiveBottomTimer();
        DrawThreatWarning();
        // Midpoint timer restoration must not punch holes through the shutters.
        DrawRaceTransitionOverlay();
        C2D_Flush();
    }
    // Re-transfer the retained target on EVERY image, including midpoints.
    // With double buffering, omitting this can leave the paired screen stale.
    C3D_FrameDrawOn(sUi.bottomTarget);
    if (!drawTopFps) return;
    DrawTopFpsBatch(static_cast<C3D_RenderTarget*>(existingTopTarget));
}

static void ShowProgress(const char* title, const char* detail, unsigned percent, bool showArtwork) {
    if (mk64_3ds::CreditsBottomBlank(gGamestate, gGamestateNext, sUi.game.gameState))
        showArtwork = false; // Includes sentinel reloads BEFORE the UI snapshot updates.
    if (!sUi.initialized || !mk64_3ds::BeginFrame3DS(C3D_FRAME_SYNCDRAW, "loading-progress-previous-frame-wait")) return;
    percent = std::min(percent, 100U);
    const bool firstTopImage = sUi.lastTopTarget == nullptr;
    if (firstTopImage) {
        sUi.lastTopTarget = static_cast<C3D_RenderTarget*>(Mk64Graphics3DSGetTopRenderTarget());
    }
    // Re-present the unchanged upper image as well. Some display consumers
    // refresh the pair only on a top-screen swap during a paused operation.
    if (sUi.lastTopTarget != nullptr) {
        C3D_FrameDrawOn(sUi.lastTopTarget);
        if (!showArtwork || firstTopImage) C3D_RenderTargetClear(sUi.lastTopTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
    }
    C3D_RenderTargetClear(sUi.bottomTarget, C3D_CLEAR_ALL, 0x000000FF, 0);
    C3D_FrameDrawOn(sUi.bottomTarget);
    if (showArtwork) {
        PrepareC2DBatch();
        C2D_SceneBegin(sUi.bottomTarget);
        if (sUi.game.racing) DrawRaceBackground(); else DrawDimMenuBackground();
        DrawText(title, 160, 66, 0.9f, C2D_Color32(255, 225, 80, 255), C2D_AlignCenter, 0.8f, true);
        DrawText(detail, 160, 105, 0.65f, C2D_Color32(235, 245, 255, 255), C2D_AlignCenter, 0.8f, true);
        C2D_DrawRectSolid(32, 140, 0.7f, 256, 14, C2D_Color32(35, 45, 58, 255));
        C2D_DrawRectSolid(34, 142, 0.8f, 252.0f * percent / 100, 10, C2D_Color32(125, 255, 145, 255));
        char value[24];
        std::snprintf(value, sizeof(value), "%u PCT", percent);
        DrawText(value, 160, 171, 0.75f, C2D_Color32(125, 255, 145, 255), C2D_AlignCenter, 0.9f, true);
        C2D_Flush();
    }
    // Paused diagnostic frames may use Citro3D's full cache-clean path.
    C3D_FrameEnd(0);
    if (mk64_3ds::BeginFrame3DS(C3D_FRAME_SYNCDRAW, "loading-progress-submit-wait"))
        C3D_FrameEnd(GX_CMDLIST_FLUSH);
    sUi.bottomDirty = true;
}

extern "C" void Mk64BottomUI3DSShowProgress(const char* title, const char* detail, unsigned percent) {
    ShowProgress(title, detail, percent, true);
}

extern "C" void Mk64BottomUI3DSShowLoadingProgress(const char* title, const char* detail, unsigned percent) {
    sUi.fps.Reset();
    const bool artwork = Mk64Settings3DSGetShowLoadingScreens();
    // With loading artwork disabled, one black presentation lasts until the
    // next game image. Do not perform two GPU/VBlank waits per progress step.
    if (!artwork && sLoadingBlanked) return;
    ShowProgress(title, detail, percent, artwork);
    sLoadingBlanked = !artwork;
}

extern "C" void Mk64BottomUI3DSResetFps() {
    sUi.fps.Reset();
}

extern "C" void Mk64BottomUI3DSRecordPresentation() {
    if (!sUi.initialized) return;
    sLoadingBlanked = false;
    UpdateFpsCounter();
}

extern "C" bool Mk64BottomUI3DSNeedsTopOverlay() {
    const bool showFps = Mk64Settings3DSGetShowFpsEnabled() && !sUi.game.demo && !sUi.game.autoCinematic &&
        !(sUi.game.racing && sUi.game.raceFinished);
    return sUi.initialized && (showFps || DrawsTopRaceHud() ||
                              TitleCreditVisible() ||
                              UpdateIsOpen() || DataActive() || StatusActive() || RaceExitBlackHold());
}

extern "C" bool Mk64BottomUI3DSHasTitleCopyright() {
    return sUi.initialized && sUi.titleCredit.initialized;
}

extern "C" void Mk64BottomUI3DSDrawTopFps(void* existingTopTarget) {
    if (!Mk64BottomUI3DSNeedsTopOverlay() || existingTopTarget == nullptr) return;
    DrawTopFpsBatch(static_cast<C3D_RenderTarget*>(existingTopTarget));
}

extern "C" uint32_t Mk64BottomUI3DSFilterGameKeys(uint32_t heldKeys) {
    if (!sUi.initialized) return heldKeys;
    if (sUi.modalOpen) return 0;
    if (sUi.captureGameInputThisFrame)
        return sUi.resumeThrottleThisFrame ? heldKeys & KEY_A : 0;
    heldKeys &= ~sUi.blockedGameKeys;
    if (Mk64Auto3DSRaceInputLocked() || Mk64SoloBattle3DSSpectating()) return heldKeys & KEY_START;
    if (sUi.game.gameSelectVisible) heldKeys &= ~static_cast<uint32_t>(KEY_L);
    if (sUi.game.paused) heldKeys &= ~static_cast<uint32_t>(KEY_L);
    return heldKeys | sUi.injectedGameKeys;
}

extern "C" bool Mk64BottomUI3DSConsumesCStick() {
    return sUi.initialized && sUi.consumesCStick;
}

extern "C" bool Mk64BottomUI3DSIsModalOpen() {
    return sUi.initialized && (sUi.modalOpen ||
                               (sUi.captureGameInputThisFrame && !sUi.resumeThrottleThisFrame));
}

extern "C" float Mk64BottomUI3DSGetCurrentFps() {
    return sUi.fps.OneSecond();
}

extern "C" float Mk64BottomUI3DSGetAverageFps() {
    return sUi.fps.Average();
}

extern "C" bool Mk64BottomUI3DSIsUpdateOpen() { return sUi.initialized && UpdateIsOpen(); }
