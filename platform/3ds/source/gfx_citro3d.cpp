#include "gfx_citro3d.h"

#include <3ds.h>
#include <citro3d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

#include "fast3d_passthrough_shbin.h"

namespace Fast {
namespace {

constexpr uint32_t kTopWidth = 400;
constexpr uint32_t kTopHeight = 240;
constexpr uint32_t kNativeWidth = 320;
constexpr uint32_t kNativeHeight = 240;
constexpr uint32_t kMaxBackingTextureSize = 512;
constexpr uint32_t kMaxSourceVertices = 256 * 3;
constexpr uint32_t kMaxDrawVertices = 256 * 6;
constexpr uint32_t kVertexBufferCapacity = 32 * 1024;
constexpr uint32_t kPackedVertexFloats = 12;
// The largest dimension in the vanilla MK64 O2R texture set is 320 pixels.
// Advertising that real source limit reduces Fast3D's square RGBA conversion
// buffer from 1 MiB to 400 KiB; the Citro3D backing allocation can still round
// a 320-pixel source up to 512 internally.
constexpr uint32_t kMaxTextureSize = 320;
constexpr float kClipWEpsilon = 1.0e-4f;
constexpr float kFullscreenBoundsEpsilon = 0.03f;
constexpr float kFullscreenCoverScale = static_cast<float>(kTopWidth) / kNativeWidth;
constexpr size_t kMaxVertexStrideFloats = 64;

constexpr uint32_t kDisplayTransferFlags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
                                                   GX_TRANSFER_RAW_COPY(0) |
                                                   GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                                   GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
                                                   GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

enum CombinerSource : int {
    SourceZero = 0,
    SourceInput1 = 1,
    SourceInput7 = 7,
    SourceTexel0 = 8,
    SourceTexel0Alpha = 9,
    SourceTexel1 = 10,
    SourceTexel1Alpha = 11,
    SourceOne = 12,
    SourceCombined = 13,
    SourceNoise = 14,
};

enum ShaderOption : uint8_t {
    OptionAlpha = 0,
    OptionFog = 1,
    OptionTextureEdge = 2,
    OptionNoise = 3,
    OptionTwoCycle = 4,
    OptionAlphaThreshold = 5,
    OptionInvisible = 6,
    OptionGrayscale = 7,
    OptionTexel0ClampS = 8,
    OptionTexel0ClampT = 9,
    OptionTexel1ClampS = 10,
    OptionTexel1ClampT = 11,
};

constexpr bool HasOption(uint64_t options, ShaderOption option) {
    return (options & (uint64_t{1} << static_cast<uint8_t>(option))) != 0;
}

uint16_t NextPowerOfTwo(uint32_t value) {
    uint32_t result = 8;
    while (result < value && result < kMaxTextureSize) {
        result <<= 1;
    }
    return static_cast<uint16_t>(result);
}

uint8_t ToByte(float value) {
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

constexpr uint32_t MortonOffset8x8(uint32_t x, uint32_t y) {
    return (x & 1U) | ((y & 1U) << 1U) | ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

constexpr uint32_t SourceRowForBackingRow(uint32_t backingHeight, uint32_t sourceHeight,
                                           uint32_t destinationRow) {
    return (backingHeight - 1U - destinationRow) % sourceHeight;
}

static_assert(SourceRowForBackingRow(256, 240, 255) == 0);
static_assert(SourceRowForBackingRow(256, 240, 240) == 15);
static_assert(SourceRowForBackingRow(256, 240, 239) == 16);
static_assert(SourceRowForBackingRow(16, 12, 15) == 0);
static_assert(SourceRowForBackingRow(16, 12, 4) == 11);

uint32_t PackColor(const std::array<float, 4>& color) {
    return static_cast<uint32_t>(ToByte(color[0])) | (static_cast<uint32_t>(ToByte(color[1])) << 8U) |
           (static_cast<uint32_t>(ToByte(color[2])) << 16U) |
           (static_cast<uint32_t>(ToByte(color[3])) << 24U);
}

size_t ClipTriangleAgainstW(const float* vertices[3], size_t stride, float* output) {
    std::array<std::array<float, kMaxVertexStrideFloats>, 4> polygon = {};
    int polygonCount = 0;

    for (int index = 0; index < 3; ++index) {
        const float* current = vertices[index];
        const float* next = vertices[(index + 1) % 3];
        const bool currentInside = current[3] >= kClipWEpsilon;
        const bool nextInside = next[3] >= kClipWEpsilon;
        if (currentInside) {
            std::copy_n(current, stride, polygon[polygonCount++].begin());
        }
        if (currentInside != nextInside) {
            const float amount = (kClipWEpsilon - current[3]) / (next[3] - current[3]);
            auto& clipped = polygon[polygonCount++];
            for (size_t component = 0; component < stride; ++component) {
                clipped[component] = current[component] + (next[component] - current[component]) * amount;
            }
        }
    }

    size_t outputCount = 0;
    for (int index = 2; index < polygonCount; ++index) {
        std::copy_n(polygon[0].begin(), stride, output + outputCount++ * stride);
        std::copy_n(polygon[index - 1].begin(), stride, output + outputCount++ * stride);
        std::copy_n(polygon[index].begin(), stride, output + outputCount++ * stride);
    }
    return outputCount;
}

bool IsNativeFullscreenQuad(const float* vertices, size_t vertexCount, size_t stride) {
    if (vertices == nullptr || vertexCount != 6 || stride < 4) {
        return false;
    }

    float minX = vertices[0];
    float maxX = vertices[0];
    float minY = vertices[1];
    float maxY = vertices[1];
    for (size_t vertex = 0; vertex < vertexCount; ++vertex) {
        const float* source = vertices + vertex * stride;
        if (std::fabs(source[3] - 1.0f) > kFullscreenBoundsEpsilon) {
            return false;
        }
        minX = std::min(minX, source[0]);
        maxX = std::max(maxX, source[0]);
        minY = std::min(minY, source[1]);
        maxY = std::max(maxY, source[1]);
    }

    // Fast3D preserves the original 4:3 frame in the 5:3 top screen, so a
    // native full-screen rectangle reaches +/-0.8 horizontally and +/-1.0
    // vertically in clip space.
    const float nativeHalfWidth = static_cast<float>(kNativeWidth) / kTopWidth;
    return std::fabs(minX + nativeHalfWidth) <= kFullscreenBoundsEpsilon &&
           std::fabs(maxX - nativeHalfWidth) <= kFullscreenBoundsEpsilon &&
           std::fabs(minY + 1.0f) <= kFullscreenBoundsEpsilon &&
           std::fabs(maxY - 1.0f) <= kFullscreenBoundsEpsilon;
}

struct ChannelOperation {
    GPU_COMBINEFUNC function = GPU_REPLACE;
    std::array<int, 3> source = { SourceCombined, SourceZero, SourceZero };
};

struct ChannelPlan {
    std::array<ChannelOperation, 2> operations = {};
    uint8_t count = 0;

    size_t size() const {
        return count;
    }

    const ChannelOperation& operator[](size_t index) const {
        return operations[index];
    }
};

ChannelPlan SingleOperationPlan(const ChannelOperation& operation) {
    ChannelPlan plan;
    plan.operations[0] = operation;
    plan.count = 1;
    return plan;
}

ChannelPlan BuildChannelPlan(const int formula[4]) {
    const int a = formula[0];
    const int b = formula[1];
    const int c = formula[2];
    const int d = formula[3];

    if (c == SourceZero) {
        return SingleOperationPlan({ GPU_REPLACE, { d, SourceZero, SourceZero } });
    }
    if (b == SourceZero && d == SourceZero) {
        return SingleOperationPlan({ GPU_MODULATE, { a, c, SourceZero } });
    }
    if (b == d) {
        return SingleOperationPlan({ GPU_INTERPOLATE, { a, b, c } });
    }
    if (b == SourceZero) {
        return SingleOperationPlan({ GPU_MULTIPLY_ADD, { a, c, d } });
    }

    ChannelPlan plan;
    plan.operations[0] = { GPU_SUBTRACT, { a, b, SourceZero } };
    plan.operations[1] = { GPU_MULTIPLY_ADD, { SourceCombined, c, d } };
    plan.count = 2;
    return plan;
}

ChannelOperation PassPrevious() {
    return { GPU_REPLACE, { SourceCombined, SourceZero, SourceZero } };
}

GPU_TEVSRC SourceToTev(int source, int varyingInput, int cycle) {
    if (source >= SourceInput1 && source <= SourceInput7) {
        return source - SourceInput1 == varyingInput ? GPU_PRIMARY_COLOR : GPU_CONSTANT;
    }
    switch (source) {
        case SourceTexel0:
        case SourceTexel0Alpha:
            return cycle == 0 ? GPU_TEXTURE0 : GPU_TEXTURE1;
        case SourceTexel1:
        case SourceTexel1Alpha:
            return cycle == 0 ? GPU_TEXTURE1 : GPU_TEXTURE0;
        case SourceCombined:
            return GPU_PREVIOUS;
        default:
            return GPU_CONSTANT;
    }
}

GPU_TEVOP_RGB SourceToRgbOperand(int source) {
    return source == SourceTexel0Alpha || source == SourceTexel1Alpha ? GPU_TEVOP_RGB_SRC_ALPHA
                                                                      : GPU_TEVOP_RGB_SRC_COLOR;
}

std::array<float, 4> ConstantForSource(int source, const std::array<std::array<float, 4>, 7>& inputs) {
    if (source >= SourceInput1 && source <= SourceInput7) {
        return inputs[source - SourceInput1];
    }
    if (source == SourceOne) {
        return { 1.0f, 1.0f, 1.0f, 1.0f };
    }
    if (source == SourceNoise) {
        return { 0.5f, 0.5f, 0.5f, 0.5f };
    }
    return { 0.0f, 0.0f, 0.0f, 0.0f };
}

bool IsConstantSource(int source, int varyingInput) {
    if (source >= SourceInput1 && source <= SourceInput7) {
        return source - SourceInput1 != varyingInput;
    }
    return source == SourceZero || source == SourceOne || source == SourceNoise;
}

void ConfigureChannel(C3D_TexEnv* environment, C3D_TexEnvMode mode, const ChannelOperation& operation,
                      int varyingInput, int cycle) {
    C3D_TexEnvSrc(environment, mode, SourceToTev(operation.source[0], varyingInput, cycle),
                  SourceToTev(operation.source[1], varyingInput, cycle),
                  SourceToTev(operation.source[2], varyingInput, cycle));
    C3D_TexEnvFunc(environment, mode, operation.function);
}

void ConfigureOperands(C3D_TexEnv* environment, const ChannelOperation& rgbOperation,
                       const ChannelOperation& alphaOperation) {
    C3D_TexEnvOpRgb(environment, SourceToRgbOperand(rgbOperation.source[0]),
                    SourceToRgbOperand(rgbOperation.source[1]), SourceToRgbOperand(rgbOperation.source[2]));
    C3D_TexEnvOpAlpha(environment, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
    (void) alphaOperation;
}

} // namespace

struct GfxRenderingAPICitro3D::Impl {
    struct TextureSlot {
        C3D_Tex texture = {};
        bool initialized = false;
        bool hasTransparency = false;
        uint16_t sourceWidth = 0;
        uint16_t sourceHeight = 0;
        size_t allocatedBytes = 0;
    };

    struct FramebufferSlot {
        C3D_Tex texture = {};
        C3D_RenderTarget* target = nullptr;
        bool initialized = false;
        bool needsClear = false;
        uint16_t logicalWidth = 0;
        uint16_t logicalHeight = 0;
    };

    Impl() {
        framebuffers.emplace_back(); // ID 0 is the top-screen render target.
    }

    bool initialized = false;
    bool ready = false;
    bool frameActive = false;
    bool useAlpha = false;
    bool depthTest = false;
    bool depthWrite = true;
    bool decal = false;
    FilteringMode filteringMode = FILTER_THREE_POINT;
    DVLB_s* shaderBinary = nullptr;
    shaderProgram_s shaderProgram = {};
    int projectionUniform = -1;
    C3D_RenderTarget* topTarget = nullptr;
    C3D_RenderTarget* activeTarget = nullptr;
    float* packedVertices = nullptr;
    size_t packedVertexCount = 0;
    std::vector<float> clipScratch;
    ShaderProgram* currentProgram = nullptr;
    std::unordered_map<uint64_t, std::unique_ptr<ShaderProgram>> shaderPrograms;
    // Citro3D retains C3D_Tex pointers after C3D_TexBind. A vector can move
    // every slot when NewTexture grows it, leaving the GPU state pointing at
    // freed storage. deque keeps existing slot addresses stable while the
    // Fast3D cache creates textures incrementally.
    std::deque<TextureSlot> textures = { TextureSlot{} };
    std::vector<std::unique_ptr<FramebufferSlot>> framebuffers;
    std::array<uint32_t, 6> selectedTextures = {};
    std::array<int, 6> selectedFramebuffers = {};
    int selectedTextureUnit = 0;
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = static_cast<int>(kTopWidth);
    int viewportHeight = static_cast<int>(kTopHeight);
    int scissorX = 0;
    int scissorY = 0;
    int scissorWidth = static_cast<int>(kTopWidth);
    int scissorHeight = static_cast<int>(kTopHeight);
    bool scissorEnabled = false;

    ~Impl() {
        for (auto& slot : framebuffers) {
            if (slot == nullptr) {
                continue;
            }
            if (slot->target != nullptr) {
                C3D_RenderTargetDelete(slot->target);
            }
            if (slot->initialized) {
                C3D_TexDelete(&slot->texture);
            }
        }
        for (auto& slot : textures) {
            if (slot.initialized) {
                C3D_TexDelete(&slot.texture);
            }
        }
        if (packedVertices != nullptr) {
            linearFree(packedVertices);
        }
        if (shaderBinary != nullptr) {
            shaderProgramFree(&shaderProgram);
            DVLB_Free(shaderBinary);
        }
        if (initialized) {
            if (topTarget != nullptr) {
                C3D_RenderTargetDelete(topTarget);
            }
            C3D_Fini();
            gfxExit();
        }
    }
};

GfxRenderingAPICitro3D::GfxRenderingAPICitro3D() : mImpl(std::make_unique<Impl>()) {
}

GfxRenderingAPICitro3D::~GfxRenderingAPICitro3D() = default;

bool GfxRenderingAPICitro3D::IsInitialized() const {
    return mImpl->ready;
}

const char* GfxRenderingAPICitro3D::GetName() {
    return "Citro3D";
}

int GfxRenderingAPICitro3D::GetMaxTextureSize() {
    return kMaxTextureSize;
}

GfxClipParameters GfxRenderingAPICitro3D::GetClipParameters() {
    // Fast3D emits OpenGL-style clip Z in [-w, w]. StartFrame's projection
    // matrix performs the sole conversion to PICA's [-w, 0] range.
    return { false, false };
}

void GfxRenderingAPICitro3D::UnloadShader(ShaderProgram* oldPrg) {
    (void) oldPrg;
}

void GfxRenderingAPICitro3D::LoadShader(ShaderProgram* newPrg) {
    mImpl->currentProgram = newPrg;
}

void GfxRenderingAPICitro3D::ClearShaderCache() {
    mImpl->currentProgram = nullptr;
    mImpl->shaderPrograms.clear();
}

ShaderProgram* GfxRenderingAPICitro3D::CreateAndLoadNewShader(uint64_t shaderId0, uint64_t shaderId1) {
    const uint64_t key = shaderId0 ^ (shaderId1 + 0x9E3779B97F4A7C15ULL + (shaderId0 << 6U) + (shaderId0 >> 2U));
    auto program = std::make_unique<ShaderProgram>();
    program->shaderId0 = shaderId0;
    program->shaderId1 = shaderId1;
    program->alpha = HasOption(shaderId1, OptionAlpha);
    program->fog = HasOption(shaderId1, OptionFog);
    program->textureEdge = HasOption(shaderId1, OptionTextureEdge);
    program->alphaThreshold = HasOption(shaderId1, OptionAlphaThreshold);
    program->invisible = HasOption(shaderId1, OptionInvisible);
    program->twoCycle = HasOption(shaderId1, OptionTwoCycle);
    program->clamp[0][0] = HasOption(shaderId1, OptionTexel0ClampS);
    program->clamp[0][1] = HasOption(shaderId1, OptionTexel0ClampT);
    program->clamp[1][0] = HasOption(shaderId1, OptionTexel1ClampS);
    program->clamp[1][1] = HasOption(shaderId1, OptionTexel1ClampT);

    for (int cycle = 0; cycle < 2; ++cycle) {
        for (int channel = 0; channel < 2; ++channel) {
            for (int term = 0; term < 4; ++term) {
                program->combiner[cycle][channel][term] =
                    static_cast<int>((shaderId0 >> (cycle * 32 + channel * 16 + term * 4)) & 0xFULL);
                const int source = program->combiner[cycle][channel][term];
                if (source >= SourceInput1 && source <= SourceInput7) {
                    program->numInputs = std::max(program->numInputs, static_cast<uint8_t>(source));
                } else if (source == SourceTexel0 || source == SourceTexel0Alpha ||
                           source == SourceTexel1 || source == SourceTexel1Alpha) {
                    const int texture =
                        (source == SourceTexel0 || source == SourceTexel0Alpha) ? 0 : 1;
                    program->usedTextures[texture] = true;
                    // Fast3D supplies both coordinate sets for two-cycle
                    // programs because the RDP swaps TEXEL0/TEXEL1 between
                    // cycles. Keep ShaderGetInfo's vertex layout identical to
                    // the desktop backends even if only one token appears in
                    // the packed combiner.
                    if (program->twoCycle) {
                        program->usedTextures[texture ^ 1] = true;
                    }
                }
            }
        }
    }

    uint8_t offset = 4;
    for (int texture = 0; texture < 2; ++texture) {
        if (!program->usedTextures[texture]) {
            continue;
        }
        program->textureOffsets[texture] = offset;
        offset += 2;
        offset += program->clamp[texture][0] ? 1 : 0;
        offset += program->clamp[texture][1] ? 1 : 0;
    }
    if (program->fog) {
        program->fogOffset = offset;
        offset += 4;
    }
    if (HasOption(shaderId1, OptionGrayscale)) {
        offset += 4;
    }
    for (uint8_t input = 0; input < program->numInputs; ++input) {
        program->inputOffsets[input] = offset;
        offset += program->alpha ? 4 : 3;
    }
    program->strideFloats = offset;

    ShaderProgram* result = program.get();
    mImpl->shaderPrograms[key] = std::move(program);
    LoadShader(result);
    return result;
}

ShaderProgram* GfxRenderingAPICitro3D::LookupShader(uint64_t shaderId0, uint64_t shaderId1) {
    const uint64_t key = shaderId0 ^ (shaderId1 + 0x9E3779B97F4A7C15ULL + (shaderId0 << 6U) + (shaderId0 >> 2U));
    const auto iterator = mImpl->shaderPrograms.find(key);
    if (iterator == mImpl->shaderPrograms.end()) {
        return nullptr;
    }
    const ShaderProgram* candidate = iterator->second.get();
    return candidate->shaderId0 == shaderId0 && candidate->shaderId1 == shaderId1 ? iterator->second.get() : nullptr;
}

void GfxRenderingAPICitro3D::ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    *numInputs = prg->numInputs;
    usedTextures[0] = prg->usedTextures[0];
    usedTextures[1] = prg->usedTextures[1];
}

uint32_t GfxRenderingAPICitro3D::NewTexture() {
    mImpl->textures.emplace_back();
    return static_cast<uint32_t>(mImpl->textures.size() - 1);
}

void GfxRenderingAPICitro3D::SelectTexture(int tile, uint32_t textureId) {
    if (tile < 0 || tile >= static_cast<int>(mImpl->selectedTextures.size()) || textureId >= mImpl->textures.size()) {
        return;
    }
    mImpl->selectedTextureUnit = tile;
    mImpl->selectedTextures[tile] = textureId;
    mImpl->selectedFramebuffers[tile] = 0;
    auto& slot = mImpl->textures[textureId];
    if (slot.initialized && tile < 3) {
        C3D_TexBind(tile, &slot.texture);
    }
}

void GfxRenderingAPICitro3D::UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) {
    if (rgba32Buf == nullptr || width == 0 || height == 0 || width > kMaxTextureSize || height > kMaxTextureSize) {
        return;
    }

    const uint32_t textureId = mImpl->selectedTextures[mImpl->selectedTextureUnit];
    if (textureId == 0 || textureId >= mImpl->textures.size()) {
        return;
    }

    auto& slot = mImpl->textures[textureId];
    if (slot.initialized) {
        C3D_TexDelete(&slot.texture);
        slot.initialized = false;
        slot.allocatedBytes = 0;
    }

    const uint16_t textureWidth = NextPowerOfTwo(width);
    const uint16_t textureHeight = NextPowerOfTwo(height);
    if (!C3D_TexInit(&slot.texture, textureWidth, textureHeight, GPU_RGBA8)) {
        return;
    }
    slot.initialized = true;
    slot.hasTransparency = false;
    slot.sourceWidth = static_cast<uint16_t>(width);
    slot.sourceHeight = static_cast<uint16_t>(height);
    slot.allocatedBytes = static_cast<size_t>(textureWidth) * textureHeight * 4U;

    // Fast3D supplies RGBA bytes while PICA stores GPU_RGBA8 texels in tiled
    // A-B-G-R byte order. Swap each host word while building the Morton layout;
    // otherwise red is interpreted as alpha. Direct swizzling also avoids an
    // extra transfer and allocation.
    // Fill POT padding by wrapping so filtering cannot sample transparent
    // border texels. C3D_TexInit already provides writable linear memory, so
    // write its Morton layout directly and avoid a second allocation, cache
    // flush, transfer, and free for every texture upload.
    auto* texturePixels = static_cast<uint32_t*>(slot.texture.data);
    const uint32_t tilesPerRow = textureWidth / 8U;
    std::array<uint16_t, kMaxBackingTextureSize> sourceColumns;
    std::array<uint16_t, kMaxBackingTextureSize> sourceRows;
    uint32_t sourceColumn = 0;
    for (uint32_t destinationColumn = 0; destinationColumn < textureWidth; ++destinationColumn) {
        sourceColumns[destinationColumn] = static_cast<uint16_t>(sourceColumn);
        if (++sourceColumn == width) {
            sourceColumn = 0;
        }
    }
    uint32_t sourceRow = SourceRowForBackingRow(textureHeight, height, 0);
    for (uint32_t destinationRow = 0; destinationRow < textureHeight; ++destinationRow) {
        sourceRows[destinationRow] = static_cast<uint16_t>(sourceRow);
        sourceRow = sourceRow == 0 ? height - 1U : sourceRow - 1U;
    }
    uint8_t combinedAlpha = 0xFF;
    for (uint32_t tileY = 0; tileY < textureHeight; tileY += 8U) {
        for (uint32_t tileX = 0; tileX < textureWidth; tileX += 8U) {
            uint32_t* tile = texturePixels +
                (static_cast<size_t>(tileY / 8U) * tilesPerRow + tileX / 8U) * 64U;
            for (uint32_t row = 0; row < 8U; ++row) {
                // PICA samples V=0 from the final row of the POT backing while
                // Fast3D supplies rows top-down and scales V by logical/POT
                // height. Anchor the flip to the backing height: for a 240-row
                // image in a 256-row texture, backing row 255 must contain
                // source row 0. Flipping around 240 instead starts at source
                // row 224 and visibly wraps after the first 16 screen rows.
                const uint32_t sourceRow = sourceRows[tileY + row];
                for (uint32_t column = 0; column < 8U; ++column) {
                    const uint32_t sourceColumn = sourceColumns[tileX + column];
                    uint32_t pixel = 0;
                    const uint8_t* sourcePixel =
                        rgba32Buf + (static_cast<size_t>(sourceRow) * width + sourceColumn) * 4;
                    combinedAlpha &= sourcePixel[3];
                    std::memcpy(&pixel, sourcePixel, sizeof(pixel));
                    tile[MortonOffset8x8(column, row)] = __builtin_bswap32(pixel);
                }
            }
        }
    }
    slot.hasTransparency = combinedAlpha != 0xFF;

    C3D_TexFlush(&slot.texture);
    C3D_TexSetFilter(&slot.texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&slot.texture, GPU_REPEAT, GPU_REPEAT);
    C3D_TexBind(mImpl->selectedTextureUnit, &slot.texture);
}

void GfxRenderingAPICitro3D::SetSamplerParameters(int sampler, bool linearFilter, uint32_t cms, uint32_t cmt) {
    if (sampler < 0 || sampler >= static_cast<int>(mImpl->selectedTextures.size())) {
        return;
    }
    const int framebufferId = mImpl->selectedFramebuffers[sampler];
    if (framebufferId > 0 && framebufferId < static_cast<int>(mImpl->framebuffers.size()) &&
        mImpl->framebuffers[framebufferId] != nullptr && mImpl->framebuffers[framebufferId]->initialized) {
        C3D_Tex& texture = mImpl->framebuffers[framebufferId]->texture;
        const GPU_TEXTURE_FILTER_PARAM filter = linearFilter ? GPU_LINEAR : GPU_NEAREST;
        C3D_TexSetFilter(&texture, filter, filter);
        C3D_TexSetWrap(&texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        return;
    }
    const uint32_t textureId = mImpl->selectedTextures[sampler];
    if (textureId == 0 || textureId >= mImpl->textures.size() || !mImpl->textures[textureId].initialized) {
        return;
    }
    auto& texture = mImpl->textures[textureId].texture;
    const GPU_TEXTURE_FILTER_PARAM filter = linearFilter ? GPU_LINEAR : GPU_NEAREST;
    C3D_TexSetFilter(&texture, filter, filter);
    const auto wrapS = (cms & 2U) != 0 ? GPU_CLAMP_TO_EDGE : ((cms & 1U) != 0 ? GPU_MIRRORED_REPEAT : GPU_REPEAT);
    const auto wrapT = (cmt & 2U) != 0 ? GPU_CLAMP_TO_EDGE : ((cmt & 1U) != 0 ? GPU_MIRRORED_REPEAT : GPU_REPEAT);
    C3D_TexSetWrap(&texture, wrapS, wrapT);
}

void GfxRenderingAPICitro3D::SetDepthTestAndMask(bool depthTest, bool zUpdate) {
    mImpl->depthTest = depthTest;
    mImpl->depthWrite = zUpdate;
    C3D_DepthTest(depthTest, depthTest ? GPU_GREATER : GPU_ALWAYS,
                  static_cast<GPU_WRITEMASK>(GPU_WRITE_COLOR | (zUpdate ? GPU_WRITE_DEPTH : 0)));
}

void GfxRenderingAPICitro3D::SetZmodeDecal(bool decal) {
    mImpl->decal = decal;
    // The vertex shader maps OpenGL clip depth into PICA's [-w, 0] range.
    // Reverse that range onto [1, 0] for GPU_GREATER and the zero-cleared
    // depth buffer. An offset of +1 saturates nearly every fragment at 1 and
    // makes later geometry fail the depth test.
    C3D_DepthMap(true, -1.0f, decal ? -0.001f : 0.0f);
}

void GfxRenderingAPICitro3D::SetViewport(int x, int y, int width, int height) {
    mImpl->viewportX = x;
    mImpl->viewportY = y;
    mImpl->viewportWidth = width;
    mImpl->viewportHeight = height;
    C3D_SetViewport(static_cast<uint32_t>(std::max(0, y)), static_cast<uint32_t>(std::max(0, x)),
                    static_cast<uint32_t>(std::max(0, height)), static_cast<uint32_t>(std::max(0, width)));
}

void GfxRenderingAPICitro3D::SetScissor(int x, int y, int width, int height) {
    mImpl->scissorX = x;
    mImpl->scissorY = y;
    mImpl->scissorWidth = width;
    mImpl->scissorHeight = height;
    mImpl->scissorEnabled = width > 0 && height > 0;
    if (!mImpl->scissorEnabled) {
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
        return;
    }
    C3D_SetScissor(GPU_SCISSOR_NORMAL, static_cast<uint32_t>(std::max(0, y)),
                   static_cast<uint32_t>(std::max(0, x)), static_cast<uint32_t>(std::max(0, y + height)),
                   static_cast<uint32_t>(std::max(0, x + width)));
}

void GfxRenderingAPICitro3D::SetUseAlpha(bool useAlpha) {
    mImpl->useAlpha = useAlpha;
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, useAlpha ? GPU_SRC_ALPHA : GPU_ONE,
                   useAlpha ? GPU_ONE_MINUS_SRC_ALPHA : GPU_ZERO, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
}

void GfxRenderingAPICitro3D::DrawTriangles(float bufVbo[], size_t bufVboLen, size_t bufVboNumTris) {
    ShaderProgram* program = mImpl->currentProgram;
    if (!mImpl->frameActive || program == nullptr || program->invisible || bufVbo == nullptr ||
        program->strideFloats == 0 || bufVboNumTris == 0) {
        return;
    }

    const size_t sourceVertexCount = std::min(bufVboNumTris * 3, static_cast<size_t>(kMaxSourceVertices));
    const size_t sourceTriangleCount = sourceVertexCount / 3;
    if (program->strideFloats > kMaxVertexStrideFloats ||
        bufVboLen < sourceVertexCount * program->strideFloats) {
        return;
    }

    const float* drawVertices = bufVbo;
    size_t vertexCount = sourceVertexCount;
    bool needsClipping = false;
    for (size_t vertex = 0; vertex < sourceVertexCount; ++vertex) {
        if (bufVbo[vertex * program->strideFloats + 3] < kClipWEpsilon) {
            needsClipping = true;
            break;
        }
    }
    if (needsClipping) {
        mImpl->clipScratch.resize(sourceTriangleCount * 6 * program->strideFloats);
        size_t clippedVertexCount = 0;
        for (size_t triangle = 0; triangle < sourceTriangleCount; ++triangle) {
            const float* triangleVertices[3] = {
                bufVbo + (triangle * 3 + 0) * program->strideFloats,
                bufVbo + (triangle * 3 + 1) * program->strideFloats,
                bufVbo + (triangle * 3 + 2) * program->strideFloats,
            };
            clippedVertexCount += ClipTriangleAgainstW(
                triangleVertices, program->strideFloats,
                mImpl->clipScratch.data() + clippedVertexCount * program->strideFloats);
        }
        if (clippedVertexCount == 0) {
            return;
        }
        drawVertices = mImpl->clipScratch.data();
        vertexCount = std::min(clippedVertexCount, static_cast<size_t>(kMaxDrawVertices));
    }
    if (mImpl->packedVertexCount + vertexCount > kVertexBufferCapacity) {
        return;
    }

    const size_t firstVertex = mImpl->packedVertexCount;

    bool coverNativeFullscreenTexture = false;
    if (mImpl->activeTarget == mImpl->topTarget &&
        IsNativeFullscreenQuad(drawVertices, vertexCount, program->strideFloats)) {
        for (int texture = 0; texture < 2; ++texture) {
            if (!program->usedTextures[texture] || mImpl->selectedFramebuffers[texture] != 0) {
                continue;
            }
            const uint32_t textureId = mImpl->selectedTextures[texture];
            if (textureId < mImpl->textures.size()) {
                const auto& slot = mImpl->textures[textureId];
                if (slot.initialized && !slot.hasTransparency && slot.sourceWidth == kNativeWidth &&
                    slot.sourceHeight == kNativeHeight) {
                    coverNativeFullscreenTexture = true;
                    break;
                }
            }
        }
    }

    std::array<std::array<float, 4>, 7> constants = {};
    int varyingInput = -1;
    for (uint8_t input = 0; input < program->numInputs; ++input) {
        const uint8_t inputOffset = program->inputOffsets[input];
        for (int component = 0; component < 4; ++component) {
            constants[input][component] =
                component == 3 && !program->alpha ? 1.0f : drawVertices[inputOffset + std::min(component, 2)];
        }
        bool constant = true;
        for (size_t vertex = 1; vertex < vertexCount && constant; ++vertex) {
            const float* source = drawVertices + vertex * program->strideFloats + inputOffset;
            const int componentCount = program->alpha ? 4 : 3;
            for (int component = 0; component < componentCount; ++component) {
                if (std::fabs(source[component] - constants[input][component]) > 1.0e-5f) {
                    constant = false;
                    break;
                }
            }
        }
        if (!constant && varyingInput < 0) {
            varyingInput = input;
        }
    }

    bool useTextureAlpha = false;
    if (!program->alpha) {
        const int cycleCount = program->twoCycle ? 2 : 1;
        for (int cycle = 0; cycle < cycleCount && !useTextureAlpha; ++cycle) {
            for (int term = 0; term < 4 && !useTextureAlpha; ++term) {
                const int source = program->combiner[cycle][1][term];
                int texture = -1;
                if (source == SourceTexel0 || source == SourceTexel0Alpha) {
                    texture = cycle == 0 ? 0 : 1;
                } else if (source == SourceTexel1 || source == SourceTexel1Alpha) {
                    texture = cycle == 0 ? 1 : 0;
                }
                if (texture >= 0) {
                    const uint32_t textureId = mImpl->selectedTextures[texture];
                    useTextureAlpha = textureId < mImpl->textures.size() &&
                                      mImpl->textures[textureId].initialized &&
                                      mImpl->textures[textureId].hasTransparency;
                }
            }
        }
    }

    // Scale an opaque 320x240 full-screen backdrop uniformly to 400x300 and
    // let the top screen crop 30 pixels at the top and bottom. This gives it a
    // centered "cover" presentation without stretching or moving menus, HUD
    // elements, portraits, or other sprites.
    const float coverScale = coverNativeFullscreenTexture ? kFullscreenCoverScale : 1.0f;
    for (size_t vertex = 0; vertex < vertexCount; ++vertex) {
        const float* source = drawVertices + vertex * program->strideFloats;
        float* destination = mImpl->packedVertices + (firstVertex + vertex) * kPackedVertexFloats;
        *destination++ = source[0] * coverScale;
        *destination++ = source[1] * coverScale;
        *destination++ = source[2];
        *destination++ = source[3];

        for (int texture = 0; texture < 2; ++texture) {
            float u = 0.0f;
            float v = 0.0f;
            if (program->usedTextures[texture]) {
                const uint8_t textureOffset = program->textureOffsets[texture];
                u = source[textureOffset];
                v = source[textureOffset + 1];
                uint8_t clampOffset = textureOffset + 2;
                if (program->clamp[texture][0]) {
                    u = std::min(u, source[clampOffset++]);
                }
                if (program->clamp[texture][1]) {
                    v = std::min(v, source[clampOffset]);
                }
                const uint32_t textureId = mImpl->selectedTextures[texture];
                if (textureId < mImpl->textures.size() && mImpl->textures[textureId].initialized) {
                    const auto& slot = mImpl->textures[textureId];
                    u *= static_cast<float>(slot.sourceWidth) / slot.texture.width;
                    v *= static_cast<float>(slot.sourceHeight) / slot.texture.height;
                }
                const int framebufferId = mImpl->selectedFramebuffers[texture];
                if (framebufferId > 0 && framebufferId < static_cast<int>(mImpl->framebuffers.size()) &&
                    mImpl->framebuffers[framebufferId] != nullptr &&
                    mImpl->framebuffers[framebufferId]->initialized) {
                    const auto& slot = *mImpl->framebuffers[framebufferId];
                    u *= static_cast<float>(slot.logicalWidth) / slot.texture.width;
                    v *= static_cast<float>(slot.logicalHeight) / slot.texture.height;
                }
            }
            *destination++ = u;
            *destination++ = v;
        }

        if (varyingInput >= 0) {
            const float* color = source + program->inputOffsets[varyingInput];
            *destination++ = color[0];
            *destination++ = color[1];
            *destination++ = color[2];
            *destination++ = program->alpha ? color[3] : 1.0f;
        } else {
            *destination++ = 1.0f;
            *destination++ = 1.0f;
            *destination++ = 1.0f;
            *destination++ = 1.0f;
        }
    }

    int stage = 0;
    const int cycleCount = program->twoCycle ? 2 : 1;
    for (int cycle = 0; cycle < cycleCount && stage < 6; ++cycle) {
        const ChannelPlan rgbPlan = BuildChannelPlan(program->combiner[cycle][0]);
        const ChannelPlan alphaPlan = program->alpha || useTextureAlpha
                                          ? BuildChannelPlan(program->combiner[cycle][1])
                                          : SingleOperationPlan(PassPrevious());
        const size_t operationCount = std::max(rgbPlan.size(), alphaPlan.size());
        for (size_t operationIndex = 0; operationIndex < operationCount && stage < 6; ++operationIndex, ++stage) {
            const ChannelOperation rgbOperation =
                operationIndex < rgbPlan.size() ? rgbPlan[operationIndex] : PassPrevious();
            const ChannelOperation alphaOperation =
                operationIndex < alphaPlan.size() ? alphaPlan[operationIndex] : PassPrevious();
            C3D_TexEnv* environment = C3D_GetTexEnv(stage);
            C3D_TexEnvInit(environment);
            ConfigureChannel(environment, C3D_RGB, rgbOperation, varyingInput, cycle);
            ConfigureChannel(environment, C3D_Alpha, alphaOperation, varyingInput, cycle);
            ConfigureOperands(environment, rgbOperation, alphaOperation);

            std::array<float, 4> environmentColor = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (int source : rgbOperation.source) {
                if (IsConstantSource(source, varyingInput)) {
                    const auto value = ConstantForSource(source, constants);
                    environmentColor[0] = value[0];
                    environmentColor[1] = value[1];
                    environmentColor[2] = value[2];
                    break;
                }
            }
            for (int source : alphaOperation.source) {
                if (IsConstantSource(source, varyingInput)) {
                    environmentColor[3] = ConstantForSource(source, constants)[3];
                    break;
                }
            }
            C3D_TexEnvColor(environment, PackColor(environmentColor));
        }
    }
    for (; stage < 6; ++stage) {
        C3D_TexEnv* environment = C3D_GetTexEnv(stage);
        C3D_TexEnvInit(environment);
    }

    C3D_AlphaTest(program->textureEdge || program->alphaThreshold, GPU_GREATER,
                  0x08);
    C3D_DepthTest(mImpl->depthTest, mImpl->depthTest ? GPU_GREATER : GPU_ALWAYS,
                  static_cast<GPU_WRITEMASK>(GPU_WRITE_COLOR | (mImpl->depthWrite ? GPU_WRITE_DEPTH : 0)));
    if (useTextureAlpha) {
        if (program->textureEdge) {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        } else {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                           GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
        }
    }
    GSPGPU_FlushDataCache(mImpl->packedVertices + firstVertex * kPackedVertexFloats,
                          vertexCount * kPackedVertexFloats * sizeof(float));
    C3D_DrawArrays(GPU_TRIANGLES, static_cast<int>(firstVertex), static_cast<int>(vertexCount));
    if (useTextureAlpha) {
        SetUseAlpha(mImpl->useAlpha);
    }
    mImpl->packedVertexCount += vertexCount;
}

void GfxRenderingAPICitro3D::Init() {
    if (mImpl->initialized) {
        return;
    }
    gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
    gfxSet3D(false);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
    uint16_t bottomWidth = 0;
    uint16_t bottomHeight = 0;
    uint8_t* bottomFramebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
    if (bottomFramebuffer != nullptr) {
        const size_t bottomBytes = static_cast<size_t>(bottomWidth) * bottomHeight * 3;
        std::memset(bottomFramebuffer, 0, bottomBytes);
        GSPGPU_FlushDataCache(bottomFramebuffer, bottomBytes);
    }
    aptSetHomeAllowed(true);
    aptSetSleepAllowed(true);

    bool newModel = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&newModel)) && newModel) {
        osSetSpeedupEnable(true);
    }

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return;
    }
    mImpl->initialized = true;
    mImpl->topTarget = C3D_RenderTargetCreate(kTopHeight, kTopWidth, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (mImpl->topTarget == nullptr) {
        return;
    }
    C3D_RenderTargetSetOutput(mImpl->topTarget, GFX_TOP, GFX_LEFT, kDisplayTransferFlags);

    mImpl->shaderBinary = DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(fast3d_passthrough_shbin)),
                                         fast3d_passthrough_shbin_size);
    if (mImpl->shaderBinary == nullptr) {
        return;
    }
    shaderProgramInit(&mImpl->shaderProgram);
    shaderProgramSetVsh(&mImpl->shaderProgram, &mImpl->shaderBinary->DVLE[0]);
    C3D_BindProgram(&mImpl->shaderProgram);
    mImpl->projectionUniform = shaderInstanceGetUniformLocation(mImpl->shaderProgram.vertexShader, "projection");
    mImpl->packedVertices =
        static_cast<float*>(linearAlloc(kVertexBufferCapacity * kPackedVertexFloats * sizeof(float)));
    if (mImpl->packedVertices == nullptr) {
        return;
    }
    C3D_AttrInfo* attributeInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attributeInfo);
    AttrInfo_AddLoader(attributeInfo, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attributeInfo, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributeInfo, 2, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributeInfo, 3, GPU_FLOAT, 4);
    C3D_BufInfo* bufferInfo = C3D_GetBufInfo();
    BufInfo_Init(bufferInfo);
    BufInfo_Add(bufferInfo, mImpl->packedVertices, sizeof(float) * kPackedVertexFloats, 4, 0x3210);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0.0f);
    C3D_FrameRate(60.0f);
    SetUseAlpha(false);
    SetDepthTestAndMask(false, false);
    mImpl->ready = true;
}

void GfxRenderingAPICitro3D::OnResize() {
}

void GfxRenderingAPICitro3D::StartFrame() {
    if (!mImpl->ready || mImpl->frameActive) {
        return;
    }
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        return;
    }
    mImpl->frameActive = true;
    mImpl->activeTarget = mImpl->topTarget;
    mImpl->packedVertexCount = 0;
    C3D_FrameDrawOn(mImpl->topTarget);
    C3D_BindProgram(&mImpl->shaderProgram);
    C3D_Mtx depthConversion;
    Mtx_Identity(&depthConversion);
    depthConversion.r[2].z = 0.4999f;
    depthConversion.r[2].w = -0.5f;
    C3D_Mtx tilt;
    Mtx_Identity(&tilt);
    tilt.r[0].x = 0.0f;
    tilt.r[0].y = 1.0f;
    tilt.r[1].x = -1.0f;
    tilt.r[1].y = 0.0f;
    C3D_Mtx projection;
    Mtx_Multiply(&projection, &tilt, &depthConversion);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, mImpl->projectionUniform, &projection);
    SetViewport(0, 0, kTopWidth, kTopHeight);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

void GfxRenderingAPICitro3D::EndFrame() {
    if (!mImpl->frameActive) {
        return;
    }
    C3D_FrameEnd(0);
    mImpl->frameActive = false;
    mImpl->activeTarget = nullptr;
}

void GfxRenderingAPICitro3D::FinishRender() {
    // EndFrame already submitted and closed the Citro3D frame. FrameSplit is
    // only valid while a frame is being recorded; issuing it here can wait on
    // a command queue that no longer has an active frame and hard-lock the
    // first presentation on real hardware.
}

int GfxRenderingAPICitro3D::CreateFramebuffer() {
    mImpl->framebuffers.emplace_back(std::make_unique<Impl::FramebufferSlot>());
    return static_cast<int>(mImpl->framebuffers.size() - 1);
}

void GfxRenderingAPICitro3D::UpdateFramebufferParameters(int fbId, uint32_t width, uint32_t height,
                                                          uint32_t msaaLevel, bool openglInvertY, bool renderTarget,
                                                          bool hasDepthBuffer, bool canExtractDepth) {
    if (fbId <= 0 || fbId >= static_cast<int>(mImpl->framebuffers.size()) || width == 0 || height == 0 ||
        width > kMaxTextureSize || height > kMaxTextureSize) {
        return;
    }
    (void) msaaLevel;
    (void) openglInvertY;
    (void) renderTarget;
    (void) canExtractDepth;

    auto& slot = *mImpl->framebuffers[fbId];
    const uint16_t textureWidth = NextPowerOfTwo(width);
    const uint16_t textureHeight = NextPowerOfTwo(height);
    if (slot.initialized && slot.logicalWidth == width && slot.logicalHeight == height &&
        slot.texture.width == textureWidth && slot.texture.height == textureHeight) {
        return;
    }
    if (slot.target != nullptr) {
        C3D_RenderTargetDelete(slot.target);
        slot.target = nullptr;
    }
    if (slot.initialized) {
        C3D_TexDelete(&slot.texture);
        slot.initialized = false;
    }
    if (!C3D_TexInitVRAM(&slot.texture, textureWidth, textureHeight, GPU_RGBA8)) {
        return;
    }
    slot.target = C3D_RenderTargetCreateFromTex(
        &slot.texture, GPU_TEXFACE_2D, 0, hasDepthBuffer ? C3D_DEPTHTYPE(GPU_RB_DEPTH24_STENCIL8) : C3D_DEPTHTYPE(-1));
    if (slot.target == nullptr) {
        C3D_TexDelete(&slot.texture);
        return;
    }
    slot.initialized = true;
    slot.needsClear = true;
    slot.logicalWidth = static_cast<uint16_t>(width);
    slot.logicalHeight = static_cast<uint16_t>(height);
    C3D_TexSetFilter(&slot.texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&slot.texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
}

void GfxRenderingAPICitro3D::StartDrawToFramebuffer(int fbId, float noiseScale) {
    (void) noiseScale;
    if (!mImpl->frameActive) {
        return;
    }
    C3D_RenderTarget* target = mImpl->topTarget;
    Impl::FramebufferSlot* slot = nullptr;
    if (fbId > 0 && fbId < static_cast<int>(mImpl->framebuffers.size()) &&
        mImpl->framebuffers[fbId] != nullptr && mImpl->framebuffers[fbId]->initialized) {
        slot = mImpl->framebuffers[fbId].get();
        target = slot->target;
    }
    if (mImpl->frameActive && mImpl->activeTarget != target) {
        C3D_FrameSplit(0);
    }
    C3D_FrameDrawOn(target);
    mImpl->activeTarget = target;
    if (slot != nullptr && slot->needsClear) {
        C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000FF, 0);
        slot->needsClear = false;
    }
}

void GfxRenderingAPICitro3D::CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1,
                                              int dstX0, int dstY0, int dstX1, int dstY1) {
    (void) fbDstId;
    (void) fbSrcId;
    (void) srcX0;
    (void) srcY0;
    (void) srcX1;
    (void) srcY1;
    (void) dstX0;
    (void) dstY0;
    (void) dstX1;
    (void) dstY1;
}

void GfxRenderingAPICitro3D::ClearFramebuffer(bool color, bool depth) {
    C3D_ClearBits clear = static_cast<C3D_ClearBits>((color ? C3D_CLEAR_COLOR : 0) | (depth ? C3D_CLEAR_DEPTH : 0));
    if (clear != 0 && mImpl->activeTarget != nullptr) {
        C3D_RenderTargetClear(mImpl->activeTarget, clear, 0x000000FF, 0);
    }
}

void GfxRenderingAPICitro3D::ClearDepthRegion(int x, int y, int width, int height) {
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    ClearFramebuffer(false, true);
}

void GfxRenderingAPICitro3D::ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) {
    (void) fbId;
    if (rgba16Buf != nullptr) {
        std::fill_n(rgba16Buf, static_cast<size_t>(width) * height, uint16_t{0});
    }
}

void GfxRenderingAPICitro3D::ResolveMSAAColorBuffer(int fbIdTarget, int fbIdSource) {
    (void) fbIdTarget;
    (void) fbIdSource;
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPICitro3D::GetPixelDepth(int fbId, const std::set<std::pair<float, float>>& coordinates) {
    (void) fbId;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> result;
    for (const auto& coordinate : coordinates) {
        result[coordinate] = 0;
    }
    return result;
}

void* GfxRenderingAPICitro3D::GetFramebufferTextureId(int fbId) {
    if (fbId <= 0 || fbId >= static_cast<int>(mImpl->framebuffers.size()) ||
        mImpl->framebuffers[fbId] == nullptr || !mImpl->framebuffers[fbId]->initialized) {
        return nullptr;
    }
    return mImpl->framebuffers[fbId]->texture.data;
}

void GfxRenderingAPICitro3D::SelectTextureFb(int fbId) {
    if (fbId <= 0 || fbId >= static_cast<int>(mImpl->framebuffers.size()) ||
        mImpl->framebuffers[fbId] == nullptr || !mImpl->framebuffers[fbId]->initialized) {
        return;
    }
    mImpl->selectedTextureUnit = 0;
    mImpl->selectedTextures[0] = 0;
    mImpl->selectedFramebuffers[0] = fbId;
    C3D_TexBind(0, &mImpl->framebuffers[fbId]->texture);
}

void GfxRenderingAPICitro3D::DeleteTexture(uint32_t textureId) {
    if (textureId == 0 || textureId >= mImpl->textures.size()) {
        return;
    }
    auto& slot = mImpl->textures[textureId];
    if (slot.initialized) {
        C3D_TexDelete(&slot.texture);
        slot.initialized = false;
        slot.allocatedBytes = 0;
    }
}

void GfxRenderingAPICitro3D::SetTextureFilter(FilteringMode mode) {
    mImpl->filteringMode = mode;
}

FilteringMode GfxRenderingAPICitro3D::GetTextureFilter() {
    return mImpl->filteringMode;
}

void GfxRenderingAPICitro3D::SetSrgbMode() {
    mSrgbMode = true;
}

ImTextureID GfxRenderingAPICitro3D::GetTextureById(int id) {
    return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(id));
}

void GfxRenderingAPICitro3D::SetCurrentPrimDepth(float depth) {
    mCurrentPrimDepth = depth;
    mPrimDepthDirty = true;
}

void GfxRenderingAPICitro3D::GetDebugStats(size_t* textureSlots, size_t* initializedTextures,
                                           size_t* textureBytes, size_t* shaderPrograms,
                                           size_t* clipScratchBytes) const {
    size_t live = 0;
    size_t bytes = 0;
    for (const auto& slot : mImpl->textures) {
        if (!slot.initialized) {
            continue;
        }
        ++live;
        bytes += slot.allocatedBytes;
    }
    if (textureSlots != nullptr) {
        *textureSlots = mImpl->textures.size();
    }
    if (initializedTextures != nullptr) {
        *initializedTextures = live;
    }
    if (textureBytes != nullptr) {
        *textureBytes = bytes;
    }
    if (shaderPrograms != nullptr) {
        *shaderPrograms = mImpl->shaderPrograms.size();
    }
    if (clipScratchBytes != nullptr) {
        *clipScratchBytes = mImpl->clipScratch.capacity() * sizeof(float);
    }
}

} // namespace Fast
