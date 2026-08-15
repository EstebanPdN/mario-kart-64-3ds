#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>

#include "fast/backends/gfx_rendering_api.h"

namespace Fast {

struct ShaderProgram {
    uint64_t shaderId0 = 0;
    uint64_t shaderId1 = 0;
    uint8_t numInputs = 0;
    uint8_t strideFloats = 0;
    uint8_t textureOffsets[2] = {};
    uint8_t inputOffsets[7] = {};
    uint8_t fogOffset = 0;
    uint8_t grayscaleOffset = 0;
    bool usedTextures[6] = {};
    bool clamp[2][2] = {};
    bool alpha = false;
    bool fog = false;
    bool grayscale = false;
    bool textureEdge = false;
    bool alphaThreshold = false;
    bool invisible = false;
    bool twoCycle = false;
    int combiner[2][2][4] = {};
};

class GfxRenderingAPICitro3D final : public GfxRenderingAPI {
  public:
    GfxRenderingAPICitro3D();
    ~GfxRenderingAPICitro3D() override;

    bool IsInitialized() const;
    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;
    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    void ClearShaderCache() override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint64_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint64_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linearFilter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depthTest, bool zUpdate) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float bufVbo[], size_t bufVboLen, size_t bufVboNumTris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fbId, uint32_t width, uint32_t height, uint32_t msaaLevel,
                                     bool openglInvertY, bool renderTarget, bool hasDepthBuffer,
                                     bool canExtractDepth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0,
                         int dstY0, int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ClearDepthRegion(int x, int y, int width, int height) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarget, int fbIdSource) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fbId, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    void DeleteTexture(uint32_t textureId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;
    void SetSrgbMode() override;
    ImTextureID GetTextureById(int id) override;
    void SetCurrentPrimDepth(float depth) override;
    void GetDebugStats(size_t* textureSlots, size_t* initializedTextures, size_t* textureBytes,
                       size_t* shaderPrograms, size_t* clipScratchBytes) const;
    float GetPresentedFps2Seconds() const;
    float GetPresentedFps10Seconds() const;
    uint64_t GetDrawCallCount() const;
    uint64_t GetTriangleCount() const;
    uint64_t GetTextureCacheUploadCount() const;
    uint64_t GetTextureCacheUploadBytes() const;
    uint64_t GetVertexUploadCount() const;
    uint64_t GetVertexUploadBytes() const;
    void ReleaseTextureAllocations();
    void* PrepareForExternalDraw();

  private:
    void FlushPackedVertices();
    void RestoreFast3DState();
    float GetPresentedFps(uint64_t windowMilliseconds) const;

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace Fast
