#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>

#include "fast/backends/gfx_rendering_api.h"
#include "native_geometry_3ds.hpp"
#include "static_runs_3ds.hpp"
#include "native_lighting_3ds.h"
#ifdef MK64_3DS_NATIVE_LUIGI_LAB
namespace mk64_3ds::native_scene { class GpuScene; class GpuSprites; struct Vertex; struct SpriteStyle; }
#endif

namespace Fast {

struct CompiledTevOperation3DS {
    uint8_t function = 0;
    std::array<uint8_t, 3> source = {};
};

// One recorded static geometry run (static_runs_3ds.hpp), drawn with the
// material the interpreter just selected. `materialVertex` is a single vertex
// in the interpreter's packed layout carrying the constant combiner inputs and
// tile clamps; `varyingInput` is the combiner input fed by the shade colour.
struct StaticRunDraw3DS {
    const void* vertices = nullptr;
    const uint16_t* indices = nullptr;
    uint16_t indexCount = 0;
    uint16_t vertexCount = 0;
    mk64_3ds::StaticRunTransform fogTransform{};
    float center[3] = {};
    bool fogFromVertex = true;
    const float* materialVertex = nullptr;
    int varyingInput = 0;
    // Per-channel shade mask and constant terms in packed (0-255) units.
    bool constantColor = false;
    float color[4] = {};
    float colorMask[4] = {};
    // Per texture: raw-coordinate scale (u, v) and offset (u, v).
    float uvAffine[2][4] = {};
    // Object (centred) to clip rows: X with aspect, Y, (Z-W)/2 depth, W.
    float rows[4][4] = {};
    mk64_3ds::NativeGeometryCull cull = mk64_3ds::NativeGeometryCull::None;
};

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
    // Physical samplers actually read by enabled compiled TEV operations,
    // after the RDP's second-cycle TEXEL0/TEXEL1 swap. usedTextures above is
    // the legacy interpreter coordinate-layout contract and stays unchanged.
    bool sampledTextures[2] = {};
    bool clamp[2][2] = {};
    bool alpha = false;
    bool fog = false;
    bool grayscale = false;
    bool textureEdge = false;
    bool alphaThreshold = false;
    bool invisible = false;
    bool twoCycle = false;
    bool primitiveDepth = false;
    int combiner[2][2][4] = {};
    // Immutable combiner plans, compiled once when the shader is created.
    CompiledTevOperation3DS tevOperations[2][2][2] = {};
    uint8_t tevOperationCount[2][2] = {};
};

class GfxRenderingAPICitro3D final : public GfxRenderingAPI {
  public:
    GfxRenderingAPICitro3D();
    ~GfxRenderingAPICitro3D() override;

    bool IsInitialized() const;
    void SetPresentationUncapped(bool uncapped);
    void* GetTopRenderTarget() const;
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
    void UploadNativeKartTexture(const uint8_t* source, uint32_t width, uint32_t height,
        size_t logicalOffset, size_t lineBytes, size_t sourceStep, size_t loadedBytes);
    void SetSamplerParameters(int sampler, bool linearFilter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depthTest, bool zUpdate) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float bufVbo[], size_t bufVboLen, size_t bufVboNumTris) override;
    void DrawStaticRun(const StaticRunDraw3DS& draw);
    bool NativeGeometryMatches(const mk64_3ds::NativeGeometryDraw* state) const;
    void SetNativeGeometry(const mk64_3ds::NativeGeometryDraw* state);
    bool NativeLightingMatches(const mk64_3ds::NativeLightingState* state) const;
    void SetNativeLighting(const mk64_3ds::NativeLightingState* state);
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
    uint32_t GetFrameBeginWaitMicroseconds() const;
    uint32_t GetPendingPresentWaitMicroseconds() const;
    float GetActiveProcessingMilliseconds() const;
    void ResetPresentedFps();
    float GetPresentedFps2Seconds() const;
    float GetPresentedFps10Seconds() const;
    uint64_t GetDrawCallCount() const;
    uint64_t GetTriangleCount() const;
    uint64_t GetTextureCacheUploadCount() const;
    uint64_t GetTextureCacheUploadBytes() const;
    uint64_t GetVertexUploadCount() const;
    uint64_t GetVertexUploadBytes() const;
    uint64_t GetLinearHeapFlushFrameCount() const;
    void ReleaseTextureAllocations();
    bool FenceTextureReuseBatch();
    void* PrepareForExternalDraw();
    void SetExternalLinearBufferRange(const void* address, size_t size);
    void MarkExternalLinearBuffersDirty();
#ifdef MK64_3DS_NATIVE_KART_LAB
    void BeginNativeSpriteBatch(mk64_3ds::native_scene::GpuSprites&);
    void EndNativeSpriteBatch();
    void DrawNativeSprite(mk64_3ds::native_scene::GpuSprites&,uint32_t key,
        const mk64_3ds::native_scene::Vertex*,const float matrix[16],float horizontalScale,
        const mk64_3ds::native_scene::SpriteStyle&);
    bool CaptureNativeFrame(const char* path);
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    size_t NativeComparisonFrameBytes() const;
    // Caller owns a 128-byte-aligned linear buffer until this synchronous
    // transfer returns. Invoke only after EndFrame, outside timed submission.
    bool CaptureNativeComparisonPixels(void* pixels,size_t bytes);
#endif
#endif
#ifdef MK64_3DS_NATIVE_LUIGI_LAB
    void BeginNativeSceneBatch(mk64_3ds::native_scene::GpuScene&);
    void EndNativeSceneBatch();
    void DrawNativeCourse(mk64_3ds::native_scene::GpuScene& scene,uint32_t view,
                          const float matrix[16],float horizontalScale);
#endif

  private:
#ifdef MK64_3DS_NATIVE_LUIGI_LAB
    mk64_3ds::native_scene::GpuScene* mNativeSceneBatch=nullptr;
#endif
#ifdef MK64_3DS_NATIVE_KART_LAB
    mk64_3ds::native_scene::GpuSprites* mNativeSpriteBatch=nullptr;
#endif
    void FlushPackedVertices();
    bool EnsurePresentationResources();
    void UploadProjectionForActiveTarget(bool viewportSlide = true);
    void ApplyViewportScissor();
    void PresentSceneToTopTarget();
    void RestoreFast3DState();
    void ApplyDepthMapping();
    void ApplyNativeVertexState();
    bool DrawTargetVisible() const;
    std::array<float, 4> SelectedTextureScale(const ShaderProgram* program, float coverScale) const;
    void ApplyCombinerState(ShaderProgram* program, int varyingInput,
                            const std::array<std::array<float, 4>, 7>& constants,
                            const std::array<float, 4>& grayscaleColor, bool reachesDistanceFog,
                            const std::array<float, 4>& fogColor = {});
    void BindPackedVertices();
    float GetPresentedFps(uint64_t windowMilliseconds) const;

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace Fast
