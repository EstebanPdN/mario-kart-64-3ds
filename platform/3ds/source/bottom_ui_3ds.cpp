#include "bottom_ui_3ds.h"

#include "diagnostics_3ds.h"
#include "game_state_3ds.h"
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
constexpr size_t kTextGlyphCapacity = 2048;
constexpr size_t kC2DObjectCapacity = 1024;
constexpr size_t kRetiredTextureCapacity = 8;
constexpr float kOptionsTabY = 36.0f;
constexpr float kOptionsRowY = 70.0f;
constexpr float kOptionsRowStep = 39.0f;
constexpr const char* kOptionLogoResource = "__OTR__textures/texture_tkmk00/texture_option";
constexpr const char* kGameSelectOptionResource = "__OTR__textures/texture_tkmk00/texture_l_option";
constexpr const char* kGameSelectDataResource = "__OTR__textures/texture_tkmk00/texture_r_data";

constexpr uint32_t kTransferFlags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
                                    GX_TRANSFER_RAW_COPY(0) |
                                    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
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
    char resourceName[192] = {};
};

struct FpsSample {
    uint32_t frames = 0;
    uint64_t milliseconds = 0;
};

struct BottomUiState {
    bool initialized = false;
    C3D_RenderTarget* bottomTarget = nullptr;
    C2D_TextBuf textBuffer = nullptr;
    UiTexture menuBackground;
    UiTexture coursePreview;
    UiTexture minimap;
    UiTexture optionLogo;
    UiTexture gameSelectOption;
    UiTexture gameSelectData;
    std::array<UiTexture, kRetiredTextureCapacity> retiredTextures = {};
    size_t retiredTextureCount = 0;
    Mk64BottomUIGameState3DS game = {};
    BaseView view = BaseView::Background;
    bool modalOpen = false;
    bool modalOpenedFromPause = false;
    bool consumesCStick = false;
    bool bottomDirty = true;
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

constexpr std::array<const char*, 8> kCharacterNames = {
    "MARIO", "LUIGI", "YOSHI", "TOAD", "D.K.", "WARIO", "PEACH", "BOWSER",
};

constexpr std::array<uint32_t, 8> kCharacterColors = {
    C2D_Color32(245, 60, 55, 255), C2D_Color32(65, 220, 75, 255),
    C2D_Color32(80, 210, 95, 255), C2D_Color32(90, 165, 255, 255),
    C2D_Color32(176, 112, 64, 255), C2D_Color32(245, 205, 45, 255),
    C2D_Color32(255, 125, 190, 255), C2D_Color32(235, 125, 45, 255),
};

constexpr std::array<const char*, 16> kItemNames = {
    "NONE", "BANANA", "BANANA BUNCH", "GREEN SHELL", "3 GREEN SHELLS", "RED SHELL",
    "3 RED SHELLS", "SPINY SHELL", "THUNDERBOLT", "FAKE ITEM BOX", "STAR", "BOO",
    "MUSHROOM", "2 MUSHROOMS", "3 MUSHROOMS", "SUPER MUSHROOM",
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
        case TextureI4:
        case TextureIa4: return (pixels + 1U) / 2U;
        case TextureI8:
        case TextureIa8: return pixels;
        default: return 0;
    }
}

uint32_t DecodeTexturePixel(const Mk64TextureResource3DS& resource, size_t pixelIndex) {
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
            const uint8_t* pixel = resource.data + pixelIndex * 2U;
            const uint16_t packed = static_cast<uint16_t>((pixel[0] << 8U) | pixel[1]);
            red = Scale5To8(static_cast<uint8_t>(packed >> 11U));
            green = Scale5To8(static_cast<uint8_t>((packed >> 6U) & 0x1FU));
            blue = Scale5To8(static_cast<uint8_t>((packed >> 1U) & 0x1FU));
            alpha = (packed & 1U) != 0 ? 255 : 0;
            break;
        }
        case TextureI4: {
            const uint8_t packed = resource.data[pixelIndex / 2U];
            const uint8_t intensity = (pixelIndex & 1U) == 0 ? packed >> 4U : packed & 0x0FU;
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

bool LoadTexture(const char* resourceName, UiTexture& destination) {
    if (resourceName == nullptr || resourceName[0] == '\0') return false;
    if (std::strcmp(destination.resourceName, resourceName) == 0) return destination.initialized;

    Mk64TextureResource3DS resource = {};
    if (!Mk64Resource3DSGetTexture(resourceName, &resource) || resource.data == nullptr ||
        resource.width == 0 || resource.height == 0 || resource.width > kMaxUiTextureDimension ||
        resource.height > kMaxUiTextureDimension) {
        UiTexture missing;
        std::snprintf(missing.resourceName, sizeof(missing.resourceName), "%s", resourceName);
        ReplaceTexture(destination, missing);
        return false;
    }
    const size_t required = RequiredTextureBytes(resource);
    if (required == 0 || resource.size < required) {
        UiTexture missing;
        std::snprintf(missing.resourceName, sizeof(missing.resourceName), "%s", resourceName);
        ReplaceTexture(destination, missing);
        return false;
    }

    UiTexture decoded;
    decoded.width = resource.width;
    decoded.height = resource.height;
    std::snprintf(decoded.resourceName, sizeof(decoded.resourceName), "%s", resourceName);
    const uint16_t backingWidth = NextPowerOfTwo(resource.width);
    const uint16_t backingHeight = NextPowerOfTwo(resource.height);
    if (backingWidth < resource.width || backingHeight < resource.height ||
        !C3D_TexInit(&decoded.texture, backingWidth, backingHeight, GPU_RGBA8)) {
        UiTexture missing;
        std::snprintf(missing.resourceName, sizeof(missing.resourceName), "%s", resourceName);
        ReplaceTexture(destination, missing);
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
                const uint32_t sourceY =
                    (backingHeight - 1U - destinationY) % resource.height;
                for (uint32_t column = 0; column < 8U; ++column) {
                    const uint32_t sourceX = std::min<uint32_t>(tileX + column, resource.width - 1U);
                    const size_t sourceIndex = static_cast<size_t>(sourceY) * resource.width + sourceX;
                    tile[MortonOffset8x8(column, row)] =
                        __builtin_bswap32(DecodeTexturePixel(resource, sourceIndex));
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

C2D_Image TextureImage(UiTexture& texture) {
    return { .tex = &texture.texture, .subtex = &texture.subTexture };
}

void DrawText(const char* value, float x, float y, float scale, uint32_t color,
              uint32_t alignment = C2D_AlignLeft, float depth = 0.8f) {
    if (value == nullptr || sUi.textBuffer == nullptr) return;
    C2D_Text text = {};
    if (C2D_TextParse(&text, sUi.textBuffer, value) == nullptr) return;
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor | alignment, x, y, depth, scale, scale, color);
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

void DrawTextureCover(UiTexture& texture, float x, float y, float width, float height, float depth) {
    if (!texture.initialized) return;
    const float scale = std::max(width / texture.width, height / texture.height);
    const float drawWidth = texture.width * scale;
    const float drawHeight = texture.height * scale;
    DrawTexture(texture, x + (width - drawWidth) * 0.5f, y + (height - drawHeight) * 0.5f,
                drawWidth, drawHeight, depth);
}

void DrawFallbackBackground() {
    C2D_DrawRectangle(0.0f, 0.0f, 0.0f, kBottomWidth, kBottomHeight,
                      C2D_Color32(15, 27, 52, 255), C2D_Color32(15, 27, 52, 255),
                      C2D_Color32(4, 7, 18, 255), C2D_Color32(4, 7, 18, 255));
}

void DrawDimMenuBackground() {
    DrawFallbackBackground();
    if (sUi.menuBackground.initialized) {
        DrawTexture(sUi.menuBackground, 0.0f, 0.0f, kBottomWidth, kBottomHeight, 0.05f);
        // The source image contributes roughly twenty percent to the result.
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, kBottomWidth, kBottomHeight,
                          C2D_Color32(0, 0, 0, 204));
    }
}

void DrawRaceBackground(float scrimAlpha = 150.0f) {
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

void DrawPanel(float x, float y, float width, float height, bool selected = false) {
    const uint32_t border = selected ? C2D_Color32(255, 210, 55, 245)
                                     : C2D_Color32(100, 155, 220, 215);
    C2D_DrawRectSolid(x, y, 0.35f, width, height, border);
    C2D_DrawRectSolid(x + 2.0f, y + 2.0f, 0.4f, width - 4.0f, height - 4.0f,
                      C2D_Color32(4, 9, 18, 218));
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
        case OptionsTab::Game: return 1;
        case OptionsTab::Screen: return 3;
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
    sUi.selectedRow = 0;
    sUi.bottomDirty = true;
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
                case 2: {
                    if (!Mk64Diagnostics3DSSupportsWideMode()) {
                        Mk64Settings3DSSetResolutionWidth(400);
                        SaveChangedSetting("800 PX UNAVAILABLE ON THIS MODEL");
                        break;
                    }
                    const uint16_t width = Mk64Settings3DSGetResolutionWidth() == 800 ? 400 : 800;
                    Mk64Settings3DSSetResolutionWidth(width);
                    SaveChangedSetting("RESOLUTION SAVED - RESTART");
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
    if (PointInside(x, y, 84, 207, 152, 30)) CloseOptions();
}

void HandleModalInput(const Mk64DiagnosticsInput3DS& input) {
    if (Mk64Settings3DSGetOverlayEnabled()) {
        if ((input.downMask & KEY_B) != 0) {
            Mk64Settings3DSSetOverlayEnabled(false);
            SaveChangedSetting("BACK TO DEVELOPER OPTIONS");
        } else if ((input.downMask & KEY_TOUCH) != 0) {
            HandleOptionsTouch(input.touchX, input.touchY);
        }
        return;
    }

    if ((input.downMask & KEY_B) != 0) {
        CloseOptions();
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
    if ((input.downMask & KEY_DLEFT) != 0) {
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
    C2D_DrawRectSolid(35.0f, 188.0f, 0.82f, 250.0f, 20.0f, C2D_Color32(0, 0, 0, 225));
    DrawText(sUi.status, 160.0f, 191.0f, 0.42f, C2D_Color32(255, 225, 90, 255),
             C2D_AlignCenter, 0.86f);
}

void DrawGameSelect() {
    DrawDimMenuBackground();
    DrawText("GAME SELECT", 160.0f, 54.0f, 0.72f, C2D_Color32(255, 225, 90, 255), C2D_AlignCenter);
    DrawPanel(22.0f, 100.0f, 132.0f, 58.0f);
    DrawPanel(166.0f, 100.0f, 132.0f, 58.0f);
    if (sUi.gameSelectOption.initialized) {
        DrawTexture(sUi.gameSelectOption, 30.0f, 110.0f, 116.0f, 38.0f, 0.7f);
    } else {
        DrawText("L  OPTION", 88.0f, 115.0f, 0.58f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter);
    }
    if (sUi.gameSelectData.initialized) {
        DrawTexture(sUi.gameSelectData, 174.0f, 110.0f, 116.0f, 38.0f, 0.7f);
    } else {
        DrawText("R  DATA", 232.0f, 115.0f, 0.58f, C2D_Color32(255, 255, 255, 255),
                 C2D_AlignCenter);
    }
    DrawText("BUTTONS OR TOUCH", 160.0f, 174.0f, 0.42f, C2D_Color32(155, 195, 235, 255),
             C2D_AlignCenter);
}

void FormatCourseTime(char* output, size_t outputSize, float seconds) {
    const int hundredthsTotal = std::clamp(static_cast<int>(seconds * 100.0f), 0, 599999);
    const int minutes = hundredthsTotal / 6000;
    const int secondsPart = (hundredthsTotal / 100) % 60;
    const int hundredths = hundredthsTotal % 100;
    std::snprintf(output, outputSize, "%d:%02d.%02d", minutes, secondsPart, hundredths);
}

void DrawMinimap() {
    constexpr float panelX = 124.0f;
    constexpr float panelY = 49.0f;
    constexpr float panelWidth = 190.0f;
    constexpr float panelHeight = 184.0f;
    DrawPanel(panelX, panelY, panelWidth, panelHeight);
    DrawText("MAP", panelX + panelWidth * 0.5f, panelY + 7.0f, 0.45f,
             C2D_Color32(150, 205, 255, 255), C2D_AlignCenter);
    if (!sUi.minimap.initialized) {
        DrawText("MAP UNAVAILABLE", panelX + panelWidth * 0.5f, panelY + 85.0f, 0.4f,
                 C2D_Color32(190, 190, 190, 255), C2D_AlignCenter);
        return;
    }

    constexpr float areaX = panelX + 9.0f;
    constexpr float areaY = panelY + 29.0f;
    constexpr float areaWidth = panelWidth - 18.0f;
    constexpr float areaHeight = panelHeight - 38.0f;
    const float scale = std::min(areaWidth / sUi.minimap.width, areaHeight / sUi.minimap.height);
    const float mapWidth = sUi.minimap.width * scale;
    const float mapHeight = sUi.minimap.height * scale;
    const float mapX = areaX + (areaWidth - mapWidth) * 0.5f;
    const float mapY = areaY + (areaHeight - mapHeight) * 0.5f;
    C2D_ImageTint tint = {};
    C2D_PlainImageTint(&tint,
                       C2D_Color32(sUi.game.minimapRed, sUi.game.minimapGreen,
                                   sUi.game.minimapBlue, 209),
                       1.0f);
    DrawTexture(sUi.minimap, mapX, mapY, mapWidth, mapHeight, 0.52f, &tint,
                sUi.game.mirrorMode);

    const float logicalWidth = sUi.game.minimapWidth > 0 ? sUi.game.minimapWidth : sUi.minimap.width;
    const float logicalHeight = sUi.game.minimapHeight > 0 ? sUi.game.minimapHeight : sUi.minimap.height;
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
        if (playerId == 0) {
            C2D_DrawCircleSolid(markerX, markerY, 0.69f, 4.3f, C2D_Color32(255, 255, 255, 255));
        }
        C2D_DrawCircleSolid(markerX, markerY, 0.72f, playerId == 0 ? 3.0f : 2.4f,
                            kCharacterColors[character]);
    }
}

void DrawRaceHud() {
    DrawRaceBackground();
    DrawPanel(6.0f, 5.0f, 308.0f, 38.0f);
    DrawPanel(6.0f, 49.0f, 112.0f, 184.0f);

    char time[24] = {};
    FormatCourseTime(time, sizeof(time), sUi.game.courseTimerSeconds);
    DrawText("TIME", 15.0f, 9.0f, 0.38f, C2D_Color32(135, 190, 245, 255));
    DrawText(time, 15.0f, 22.0f, 0.52f, C2D_Color32(255, 255, 255, 255));

    char lap[20] = {};
    if (sUi.game.gameMode == 3) {
        std::snprintf(lap, sizeof(lap), "BATTLE");
    } else {
        std::snprintf(lap, sizeof(lap), "LAP %d/%d", sUi.game.currentLap, sUi.game.totalLaps);
    }
    DrawText(lap, 160.0f, 15.0f, 0.52f, C2D_Color32(255, 225, 80, 255), C2D_AlignCenter);

    const char* item = sUi.game.currentItem >= 0 && sUi.game.currentItem < static_cast<int>(kItemNames.size())
                           ? kItemNames[sUi.game.currentItem]
                           : "UNKNOWN";
    DrawText("ITEM", 305.0f, 9.0f, 0.38f, C2D_Color32(135, 190, 245, 255), C2D_AlignRight);
    DrawText(item, 305.0f, 22.0f, 0.35f, C2D_Color32(255, 255, 255, 255), C2D_AlignRight);

    DrawText(sUi.game.gameMode == 3 ? "PLAYERS" : "TOP 5", 62.0f, 57.0f, 0.46f,
             C2D_Color32(150, 205, 255, 255), C2D_AlignCenter);
    for (size_t rank = 0; rank < MK64_BOTTOM_UI_STANDING_COUNT; ++rank) {
        const bool available = rank < sUi.game.standingCount;
        const int character = available ? sUi.game.standingCharacterIds[rank] : -1;
        const int playerId = available ? sUi.game.standingPlayerIds[rank] : -1;
        const float y = 83.0f + rank * 27.0f;
        if (playerId == 0) {
            C2D_DrawRectSolid(10.0f, y - 3.0f, 0.54f, 104.0f, 24.0f,
                              C2D_Color32(70, 115, 165, 170));
        }
        char position[8] = {};
        std::snprintf(position, sizeof(position), "%lu", static_cast<unsigned long>(rank + 1U));
        DrawText(position, 17.0f, y, 0.48f, C2D_Color32(255, 220, 70, 255));
        DrawText(character >= 0 && character < 8 ? kCharacterNames[character] : "---",
                 39.0f, y, 0.43f,
                 character >= 0 && character < 8 ? kCharacterColors[character]
                                                    : C2D_Color32(150, 150, 150, 255));
    }
    DrawMinimap();
}

void DrawPausedPrompt() {
    DrawRaceBackground(188.0f);
    DrawPanel(56.0f, 76.0f, 208.0f, 88.0f);
    DrawText("PAUSED", 160.0f, 87.0f, 0.68f, C2D_Color32(255, 225, 80, 255), C2D_AlignCenter);
    DrawText("L  OPTIONS", 160.0f, 119.0f, 0.58f, C2D_Color32(255, 255, 255, 255), C2D_AlignCenter);
    DrawText("BUTTON OR TOUCH", 160.0f, 145.0f, 0.36f, C2D_Color32(150, 195, 235, 255),
             C2D_AlignCenter);
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
            *label = sUi.modalOpenedFromPause ? "RETURN TO PAUSE" : "CLOSE OPTIONS";
            std::snprintf(value, valueSize, "A / TOUCH");
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
            } else {
                *label = "RESOLUTION";
                const bool supportsWide = Mk64Diagnostics3DSSupportsWideMode();
                const uint16_t configured = supportsWide ? Mk64Settings3DSGetResolutionWidth() : 400;
                const uint32_t active = Mk64Graphics3DSResolvedOutputWidth != nullptr
                                            ? Mk64Graphics3DSResolvedOutputWidth()
                                            : configured;
                if (!supportsWide) {
                    std::snprintf(value, valueSize, "400 PX  MODEL LIMIT");
                } else if (configured != active) {
                    std::snprintf(value, valueSize, "%u PX  RESTART", configured);
                } else {
                    std::snprintf(value, valueSize, "%u PX  ACTIVE", configured);
                }
            }
            break;
        case OptionsTab::Gameplay:
            if (row == 0) {
                *label = "TURBO SPEED";
                std::snprintf(value, valueSize, "X%u  C-STICK", Mk64Settings3DSGetTurboMultiplier());
            } else {
                *label = "MASTER VOLUME";
                std::snprintf(value, valueSize, "%u%%", Mk64Settings3DSGetMasterVolumePercent());
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
                      C2D_Color32(1, 4, 10, 100));
    if (sUi.optionLogo.initialized) {
        DrawTexture(sUi.optionLogo, 95.0f, 2.0f, 130.0f, 32.0f, 0.67f);
    } else {
        DrawText("OPTION", 160.0f, 7.0f, 0.62f, C2D_Color32(255, 225, 75, 255),
                 C2D_AlignCenter);
    }
    for (int index = 0; index < static_cast<int>(OptionsTab::Count); ++index) {
        const bool selected = index == static_cast<int>(sUi.tab);
        DrawPanel(index * 80.0f + 2.0f, kOptionsTabY, 76.0f, 28.0f, selected);
        DrawText(TabName(static_cast<OptionsTab>(index)), index * 80.0f + 40.0f,
                 kOptionsTabY + 7.0f,
                 index == 3 ? 0.34f : 0.4f,
                 selected ? C2D_Color32(255, 225, 75, 255) : C2D_Color32(180, 200, 220, 255),
                 C2D_AlignCenter);
    }

    const uint8_t rows = RowCount(sUi.tab);
    for (uint8_t row = 0; row < rows; ++row) {
        const float y = kOptionsRowY + row * kOptionsRowStep;
        DrawPanel(12.0f, y, 296.0f, 33.0f, row == sUi.selectedRow);
        const char* label = "";
        char value[48] = {};
        GetRowText(sUi.tab, row, &label, value, sizeof(value));
        DrawText(label, 22.0f, y + 8.0f, 0.43f, C2D_Color32(235, 240, 248, 255));
        DrawText(value, 298.0f, y + 8.0f, 0.39f, C2D_Color32(120, 215, 255, 255),
                 C2D_AlignRight);
    }
    DrawPanel(84.0f, 207.0f, 152.0f, 30.0f);
    DrawText("B  BACK", 160.0f, 214.0f, 0.43f, C2D_Color32(220, 230, 240, 255), C2D_AlignCenter);
    DrawStatus();
}

void DrawDeveloperOverlay() {
    if (sUi.game.racing) DrawRaceBackground(216.0f); else DrawDimMenuBackground();
    C2D_DrawRectSolid(7.0f, 5.0f, 0.4f, 306.0f, 197.0f, C2D_Color32(0, 0, 0, 230));
    DrawText("DEVELOPER OVERLAY", 160.0f, 10.0f, 0.56f, C2D_Color32(255, 215, 70, 255),
             C2D_AlignCenter);

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
                      "DISPLAY   %lu PX (%u PENDING)  %s",
                      static_cast<unsigned long>(activeWidth), configuredWidth, aspect);
    }
    std::snprintf(lines[5].data(), lines[5].size(), "MEM FREE  APP %luK  LIN %luK  VRAM %luK",
                  static_cast<unsigned long>(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024U),
                  static_cast<unsigned long>(linearSpaceFree() / 1024U),
                  static_cast<unsigned long>(vramSpaceFree() / 1024U));
    std::snprintf(lines[6].data(), lines[6].size(), "HEAP      USED %dK  FREE %dK", heap.uordblks / 1024,
                  heap.fordblks / 1024);
    std::snprintf(lines[7].data(), lines[7].size(), "RENDER    TEX %lu/%lu  %luK  SH %lu",
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
                      "INTERP    MID %lu RET %lu  LAST %lu/%lu",
                      static_cast<unsigned long>(interpolatedFrames),
                      static_cast<unsigned long>(retainedFrames),
                      static_cast<unsigned long>(matchedMatrices),
                      static_cast<unsigned long>(totalMatrices));
    }
    (void) clipBytes;
    for (size_t i = 0; i < lines.size(); ++i) {
        DrawText(lines[i].data(), 15.0f, 35.0f + i * 13.8f, 0.36f,
                 i == 2 ? C2D_Color32(120, 255, 145, 255) : C2D_Color32(210, 225, 240, 255));
    }
    DrawPanel(84.0f, 207.0f, 152.0f, 30.0f);
    DrawText("B  BACK", 160.0f, 214.0f, 0.43f, C2D_Color32(255, 225, 80, 255), C2D_AlignCenter);
}

void DrawTopFps(C3D_RenderTarget* topTarget) {
    if (topTarget == nullptr || !Mk64Settings3DSGetShowFpsEnabled()) return;
    // Fast3D leaves its reverse-depth contents intact. Clear only depth so
    // Citro2D's GEQUAL overlay is never hidden by scene geometry; preserve the
    // completed game color buffer underneath.
    C3D_FrameSplit(0);
    C3D_RenderTargetClear(topTarget, C3D_CLEAR_DEPTH, 0, 0);
    C2D_SceneBegin(topTarget);
    // Citro2D keeps a 400x240 logical projection for the top screen even when
    // Fast3D renders into the 800-wide high-density target.
    constexpr float topWidth = 400.0f;
    constexpr float boxWidth = 82.0f;
    C2D_DrawRectSolid(topWidth - boxWidth - 5.0f, 5.0f, 0.85f,
                      boxWidth, 23.0f, C2D_Color32(0, 0, 0, 205));
    char fps[32] = {};
    std::snprintf(fps, sizeof(fps), "FPS %.1f", sUi.currentFps);
    DrawText(fps, topWidth - 10.0f, 9.0f, 0.43f,
             C2D_Color32(125, 255, 145, 255), C2D_AlignRight, 0.9f);
}

void DrawBottom() {
    C2D_TargetClear(sUi.bottomTarget, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(sUi.bottomTarget);
    if (Mk64Settings3DSGetOverlayEnabled()) {
        DrawDeveloperOverlay();
    } else if (sUi.modalOpen) {
        DrawOptions();
    } else {
        switch (sUi.view) {
            case BaseView::GameSelect: DrawGameSelect(); break;
            case BaseView::RaceHud: DrawRaceHud(); break;
            case BaseView::Paused: DrawPausedPrompt(); break;
            case BaseView::Background:
            default: DrawDimMenuBackground(); break;
        }
        DrawStatus();
    }
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
    sUi.textBuffer = C2D_TextBufNew(kTextGlyphCapacity);
    if (sUi.textBuffer == nullptr) {
        C3D_RenderTargetDelete(sUi.bottomTarget);
        sUi.bottomTarget = nullptr;
        C2D_Fini();
        return false;
    }
    C2D_Prepare();
    sUi.initialized = true;
    sUi.bottomDirty = true;
    sUi.modalOpen = Mk64Settings3DSGetOverlayEnabled();
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    sUi.view = GetBaseView(sUi.game);
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground);
    LoadTexture(kOptionLogoResource, sUi.optionLogo);
    LoadTexture(kGameSelectOptionResource, sUi.gameSelectOption);
    LoadTexture(kGameSelectDataResource, sUi.gameSelectData);
    if (sUi.game.racing) {
        LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
        LoadTexture(sUi.game.minimapTexture, sUi.minimap);
    }
    Mk64GameState3DSSetTopHudEnabled(Mk64Settings3DSGetTopHudEnabled());
    return true;
}

extern "C" void Mk64BottomUI3DSShutdown() {
    if (!sUi.initialized) return;
    // The final C3D_FrameEnd is asynchronous. Detaching an output while no
    // frame is active drains Citro3D's GPU queue; FrameSync alone only waits
    // for VBlank and is not a resource-lifetime fence.
    if (sUi.bottomTarget != nullptr) C3D_RenderTargetDetachOutput(sUi.bottomTarget);
    C3D_FrameSync();
    Mk64GameState3DSApplyTurbo(false, 1);
    DrainRetiredTextures();
    DeleteTexture(sUi.minimap);
    DeleteTexture(sUi.coursePreview);
    DeleteTexture(sUi.menuBackground);
    DeleteTexture(sUi.gameSelectData);
    DeleteTexture(sUi.gameSelectOption);
    DeleteTexture(sUi.optionLogo);
    if (sUi.textBuffer != nullptr) C2D_TextBufDelete(sUi.textBuffer);
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
    const size_t oldTrack = sUi.game.trackIndex;
    const char* oldBackground = sUi.game.mainBackgroundTexture;
    Mk64GameState3DSGetBottomUISnapshot(&sUi.game);
    sUi.view = GetBaseView(sUi.game);
    if (oldView != sUi.view || oldTrack != sUi.game.trackIndex ||
        oldBackground != sUi.game.mainBackgroundTexture) {
        sUi.bottomDirty = true;
    }
    LoadTexture(sUi.game.mainBackgroundTexture, sUi.menuBackground);
    if (sUi.game.racing) {
        // The selected track usually changes while still in Map Select, before
        // the RACING edge. Compare the actual resource names every frame so a
        // second race can never retain the previous course art. LoadTexture's
        // strcmp fast path makes the steady-state call allocation-free.
        LoadTexture(sUi.game.coursePreviewTexture, sUi.coursePreview);
        LoadTexture(sUi.game.minimapTexture, sUi.minimap);
    }

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
            // panel used from Game Select. It opens once on the pause edge so
            // B can close it and leave START available to resume the race.
            OpenOptions(true);
            openedThisFrame = true;
        } else if (sUi.game.gameSelectVisible && (input.downMask & KEY_L) != 0) {
            OpenOptions(false);
            openedThisFrame = true;
        } else if (sUi.game.paused && (input.downMask & KEY_L) != 0) {
            OpenOptions(true);
            openedThisFrame = true;
        } else if ((input.downMask & KEY_TOUCH) != 0) {
            if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 22, 100, 132, 58)) {
                OpenOptions(false);
                openedThisFrame = true;
            } else if (sUi.game.gameSelectVisible && PointInside(input.touchX, input.touchY, 166, 100, 132, 58)) {
                sUi.injectedGameKeys |= KEY_R;
            } else if (sUi.game.paused && PointInside(input.touchX, input.touchY, 56, 76, 208, 88)) {
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

    // The game snapshot advances at 30 Hz. Redraw the race HUD once for each
    // new snapshot, not twice during New 3DS midpoint/key-frame presentation.
    if (sUi.view == BaseView::RaceHud) sUi.bottomDirty = true;
}

extern "C" void Mk64BottomUI3DSDraw(void* existingTopTarget) {
    if (!sUi.initialized || sUi.bottomTarget == nullptr || existingTopTarget == nullptr) return;
    // The renderer enters this function from a SYNCDRAW frame. Previous GPU
    // work is complete, so textures replaced during PrepareFrame are now safe
    // to release before issuing any new Citro2D commands.
    DrainRetiredTextures();
    UpdateFpsCounter();
    const bool drawTopFps = existingTopTarget != nullptr && Mk64Settings3DSGetShowFpsEnabled();
    if (!sUi.bottomDirty && !drawTopFps) return;
    C2D_Prepare();
    C2D_TextBufClear(sUi.textBuffer);
    DrawTopFps(static_cast<C3D_RenderTarget*>(existingTopTarget));
    if (sUi.bottomDirty) {
        DrawBottom();
        sUi.bottomDirty = false;
    }
    C2D_Flush();
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
