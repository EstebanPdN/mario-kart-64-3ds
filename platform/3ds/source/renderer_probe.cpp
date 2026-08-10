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
        renderer.EndFrame();
    }

    return 0;
}
