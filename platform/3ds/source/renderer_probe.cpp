#include <3ds.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "gfx_citro3d.h"

namespace {

constexpr uint64_t kAlphaOption = uint64_t{1};
constexpr uint64_t kInputOne = uint64_t{1};
constexpr uint64_t kTexelZero = uint64_t{8};
constexpr uint64_t kTexelZeroAlpha = uint64_t{9};
constexpr uint64_t kPrimaryColorCombiner = (kInputOne << 12U) | (kInputOne << 28U);
constexpr uint64_t kTextureCombiner = (kTexelZero << 12U) | (kTexelZeroAlpha << 28U);
// The stock kart formula: (1 - environment) * texture + primitive,
// with primitive alpha * texture alpha. Both inputs are uniform per draw.
constexpr uint64_t kKartCombiner = 12U | (1U << 4U) | (8U << 8U) | (2U << 12U) |
                                     (1U << 16U) | (9U << 24U);

std::array<float, 6 * 14> KartRegressionVertices(float left, float environment,
                                               float primitive, float alpha) {
    std::array<float, 6 * 14> vertices = {};
    constexpr std::array<std::array<float, 2>, 6> corners = {{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}};
    for (size_t i = 0; i < corners.size(); ++i) {
        float* v = vertices.data() + i * 14;
        v[0] = left + corners[i][0] * 0.52f;
        v[1] = -0.94f + corners[i][1] * 0.32f;
        v[2] = 0.2f; v[3] = 1;
        v[4] = corners[i][0]; v[5] = corners[i][1];
        v[6] = v[7] = v[8] = environment; v[9] = alpha;
        v[10] = primitive; v[11] = v[12] = 0; v[13] = 1;
    }
    return vertices;
}

constexpr std::array<float, 6 * 8> kVertices = {
    -0.90f, -0.75f, 0.50f, 1.0f, 0.95f, 0.12f, 0.16f, 1.0f,
     0.00f,  0.85f, 0.50f, 1.0f, 1.00f, 0.82f, 0.12f, 1.0f,
     0.90f, -0.75f, 0.50f, 1.0f, 0.10f, 0.45f, 0.95f, 1.0f,
    -0.64f, -0.45f, 0.40f, 1.0f, 0.12f, 0.94f, 0.62f, 0.72f,
     0.00f,  0.60f, 0.40f, 1.0f, 0.78f, 0.22f, 0.92f, 0.72f,
     0.64f, -0.45f, 0.40f, 1.0f, 0.98f, 0.48f, 0.10f, 0.72f,
};

constexpr std::array<float, 6 * 6> kTextureVertices = {
    -0.46f, -0.38f, 0.30f, 1.0f, 0.0f, 1.0f,
     0.46f, -0.38f, 0.30f, 1.0f, 1.0f, 1.0f,
     0.46f,  0.38f, 0.30f, 1.0f, 1.0f, 0.0f,
    -0.46f, -0.38f, 0.30f, 1.0f, 0.0f, 1.0f,
     0.46f,  0.38f, 0.30f, 1.0f, 1.0f, 0.0f,
    -0.46f,  0.38f, 0.30f, 1.0f, 0.0f, 0.0f,
};

std::array<uint8_t, 16 * 16 * 4> MakeCheckerboard() {
    std::array<uint8_t, 16 * 16 * 4> pixels = {};
    for (size_t y = 0; y < 16; ++y) {
        for (size_t x = 0; x < 16; ++x) {
            const bool light = (((x / 4) ^ (y / 4)) & 1U) == 0;
            const size_t offset = (y * 16 + x) * 4;
            pixels[offset + 0] = light ? 255 : 38;
            pixels[offset + 1] = light ? 255 : 210;
            pixels[offset + 2] = light ? 255 : 245;
            pixels[offset + 3] = 255;
        }
    }
    return pixels;
}

} // namespace

int main() {
    Fast::GfxRenderingAPICitro3D renderer;
    renderer.Init();
    Fast::ShaderProgram* colorProgram = renderer.CreateAndLoadNewShader(kPrimaryColorCombiner, kAlphaOption);
    Fast::ShaderProgram* textureProgram = renderer.CreateAndLoadNewShader(kTextureCombiner, kAlphaOption);
    Fast::ShaderProgram* kartProgram = renderer.CreateAndLoadNewShader(kKartCombiner, kAlphaOption);
    // Bottom row: unchanged checkerboard, half intensity, red tint at half
    // opacity. None may become a solid black tile when all inputs are uniform.
    auto kartNormal = KartRegressionVertices(-0.9f, 0, 0, 1);
    auto kartDim = KartRegressionVertices(-0.26f, 0.5f, 0, 1);
    auto kartTint = KartRegressionVertices(0.38f, 0, 0.5f, 0.5f);
    renderer.SetUseAlpha(true);
    renderer.SetDepthTestAndMask(false, false);
    const auto checkerboard = MakeCheckerboard();
    const uint32_t texture = renderer.NewTexture();
    renderer.SelectTexture(0, texture);
    renderer.UploadTexture(checkerboard.data(), 16, 16);
    renderer.SetSamplerParameters(0, false, 2, 2);

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }
        renderer.StartFrame();
        renderer.ClearFramebuffer(true, true);
        renderer.LoadShader(colorProgram);
        renderer.DrawTriangles(const_cast<float*>(kVertices.data()), kVertices.size(), 2);
        renderer.LoadShader(textureProgram);
        renderer.SelectTexture(0, texture);
        renderer.DrawTriangles(const_cast<float*>(kTextureVertices.data()), kTextureVertices.size(), 2);
        renderer.LoadShader(kartProgram);
        renderer.DrawTriangles(kartNormal.data(), kartNormal.size(), 2);
        renderer.DrawTriangles(kartDim.data(), kartDim.size(), 2);
        renderer.DrawTriangles(kartTint.data(), kartTint.size(), 2);
        renderer.EndFrame();
    }

    return 0;
}
