#include "game_runtime_3ds.h"

#include "adaptive_presentation_3ds.hpp"
#include "audio_ndsp_3ds.h"
#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "gfx_citro3d.h"
#include "gfx_window_manager_3ds.h"
#include "settings_3ds.h"

#include <3ds.h>
#include <citro3d.h>
#include <fast/interpreter.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> interpreter);
}

extern "C" size_t Mk64Resource3DSLoadedCount(void);
extern "C" void Mk64GameAudio3DSBeginFrame(void) __attribute__((weak));
extern "C" Gfx* gDisplayListHead;
extern "C" void Mk64FrameInterpolation3DSSetEnabled(bool enabled);
extern "C" bool Mk64FrameInterpolation3DSIsEnabled(void);
extern "C" bool Mk64FrameInterpolation3DSPrepare(float step);
extern "C" void Mk64FrameInterpolation3DSClearPrepared(void);

namespace {
std::unique_ptr<Fast::GfxWindowBackend3DS> sWindow;
std::unique_ptr<Fast::GfxRenderingAPICitro3D> sRenderer;
std::shared_ptr<Fast::Interpreter> sInterpreter;
std::shared_ptr<Fast::GfxDebugger> sDebugger;
const std::unordered_map<Mtx*, MtxF> sNoMatrixReplacements;
uint64_t sFrameCounter = 0;
uint64_t sRendererFaultCounter = 0;
uint64_t sRendererHealthyFrameCounter = 0;
bool sRendererFaulted = false;
bool sHasPresentedTopFrame = false;
constexpr uint64_t kRendererFaultRecoveryResetFrames = 120;
constexpr uint32_t kLogicalWidth = 400;
uint32_t sOutputWidth = 400;
size_t sTextureCacheCapacity = 256;
bool sResolvedNewModel = false;
bool sUseIntermediatePresentation = false;
bool sSuppressNextPresentation = false;
mk64_3ds::AdaptivePresentationState sAdaptivePresentation;
uint64_t sLastPresentationStart = 0;
uint64_t sPreviousPresentationDuration = 0;
size_t sLastObservedResourceCount = 0;
uint64_t sLastObservedTextureUploadCount = 0;
uint64_t sLastObservedTextureUploadBytes = 0;
uint64_t sLastPerformanceDrawCalls = 0;
uint64_t sLastPerformanceTriangles = 0;
uint64_t sLastPerformanceTextureUploads = 0;
uint64_t sLastPerformanceTextureBytes = 0;
uint64_t sLastPerformanceVertexBytes = 0;
uint64_t sLastPerformanceSampleFrame = 0;

constexpr uint32_t kAudioFramesPerGameTick = 896;
// The audio worker schedules at two ticks or less and adds one tick. Treat one
// tick as immediate pressure. Recovery at 1.5 ticks (~50 ms) leaves a real
// safety margin while avoiding an impossible exact-two-tick observation when
// the in-flight worker has not yet published its next block.
constexpr uint32_t kAudioLowWaterFrames = kAudioFramesPerGameTick;
constexpr uint32_t kAudioRecoveryFrames = kAudioFramesPerGameTick * 3 / 2;
// libctru documents osGetTime() in milliseconds.
// A midpoint is expendable as soon as a 30 Hz tick misses by more than a
// small scheduling margin. Recovery requires sustained headroom and a bounded
// probe, keeping race-start upload bursts from repeatedly flapping the mode.
constexpr uint64_t kSlowTickMilliseconds = 34;
constexpr uint64_t kHeavyTextureUploadBytes = 64u * 1024u;
constexpr uint64_t kHeavyTextureUploadCount = 2;
constexpr float kBusyProcessingMilliseconds = 13.0f;
constexpr float kBusyDrawingMilliseconds = 14.0f;
constexpr float kBusyCommandBufferUsage = 0.85f;
constexpr uint64_t kPerformanceSampleFrames = 120;
constexpr size_t kTextureCacheRecoveryFloor = 128;
constexpr size_t kTextureCacheRecoveryStep = 64;

uint32_t PositiveHundredths(float value) {
    return value <= 0.0f ? 0U : static_cast<uint32_t>(std::lround(value * 100.0f));
}

uint32_t PositiveTenths(float value) {
    return value <= 0.0f ? 0U : static_cast<uint32_t>(std::lround(value * 10.0f));
}

uint32_t GetViewportWidth() {
    return Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_ORIGINAL
               ? kLogicalWidth * 4U / 5U
               : kLogicalWidth;
}

void UpdateGameViewport() {
    if (sInterpreter != nullptr) {
        // Keep Fast3D on the direct top-target path. Giving libultraship a
        // smaller game-window viewport makes it render through its desktop
        // composition framebuffer, but the compact 3DS backend intentionally
        // has no CopyFramebuffer implementation. Original aspect ratio is
        // therefore produced by Fast3D's existing clip-space aspect adjustment
        // and the OTR aspect setting while this window viewport continues to
        // match Fast3D's 400x240 logical canvas.
        sInterpreter->mGameWindowViewport = { 0, 0, kLogicalWidth, 240U };
    }
}

void SetRendererStage(const char* stage) {
    // Persist every boundary of the first few submissions. If real hardware
    // blocks inside Citro3D, runtime.log still identifies the last completed
    // boundary without adding SD writes to steady-state rendering.
    if (sFrameCounter <= 3) {
        Mk64Diagnostics3DSCheckpoint(stage);
    } else {
        Mk64Diagnostics3DSSetStage(stage);
    }
}

bool ShouldRenderIntermediatePresentation(uint64_t presentationStart) {
    const size_t resourceCount = Mk64Resource3DSLoadedCount();
    const uint64_t textureUploadCount = sRenderer->GetTextureCacheUploadCount();
    const uint64_t textureUploadBytes = sRenderer->GetTextureCacheUploadBytes();
    const size_t resourceDelta = resourceCount >= sLastObservedResourceCount
                                     ? resourceCount - sLastObservedResourceCount
                                     : 0;
    const uint64_t textureUploadDelta = textureUploadCount >= sLastObservedTextureUploadCount
                                            ? textureUploadCount - sLastObservedTextureUploadCount
                                            : 0;
    const uint64_t textureUploadByteDelta = textureUploadBytes >= sLastObservedTextureUploadBytes
                                                ? textureUploadBytes - sLastObservedTextureUploadBytes
                                                : 0;
    sLastObservedResourceCount = resourceCount;
    sLastObservedTextureUploadCount = textureUploadCount;
    sLastObservedTextureUploadBytes = textureUploadBytes;

    const bool slowStartInterval = sLastPresentationStart != 0 &&
                                   presentationStart - sLastPresentationStart > kSlowTickMilliseconds;
    const bool previousTickSlow = slowStartInterval ||
                                  sPreviousPresentationDuration > kSlowTickMilliseconds;
    sLastPresentationStart = presentationStart;

    const float processingMilliseconds = C3D_GetProcessingTime();
    const float drawingMilliseconds = C3D_GetDrawingTime();
    const bool citro3DBusy = processingMilliseconds > kBusyProcessingMilliseconds ||
                             drawingMilliseconds > kBusyDrawingMilliseconds ||
                             C3D_GetCmdBufUsage() > kBusyCommandBufferUsage;
    const bool uploadActivity = textureUploadDelta >= kHeavyTextureUploadCount ||
                                textureUploadByteDelta >= kHeavyTextureUploadBytes;

    mk64_3ds::AdaptivePresentationInputs inputs = {};
    inputs.hasPriorTopFrame = sHasPresentedTopFrame;
    inputs.audioBufferedFrames = Mk64Audio3DSBufferedFrames();
    inputs.audioLowWaterFrames = kAudioLowWaterFrames;
    inputs.audioRecoveryFrames = kAudioRecoveryFrames;
    // Overall presentation duration includes SYNCDRAW's VBlank wait and cannot
    // be compared with a 16.7 ms CPU budget. Citro3D's processing/drawing
    // timers above exclude that wait. Allow a bounded midpoint probe whenever
    // the mandatory keyframe still fits its 30 Hz interval; the adaptive state
    // immediately backs out if the extra image misses the next interval.
    inputs.keyframeHeadroom = sPreviousPresentationDuration != 0 &&
                              sPreviousPresentationDuration <= kSlowTickMilliseconds &&
                              !citro3DBusy;
    inputs.previousTickSlow = previousTickSlow;
    inputs.resourceActivity = resourceDelta != 0;
    inputs.textureUploadActivity = uploadActivity;
    inputs.citro3DBusy = citro3DBusy;
    return mk64_3ds::UpdateAdaptivePresentation(&sAdaptivePresentation, inputs).renderMidpoint;
}

void LogPerformanceSample() {
    if (sRenderer == nullptr || sFrameCounter == 0 ||
        sFrameCounter - sLastPerformanceSampleFrame < kPerformanceSampleFrames) {
        return;
    }
    const uint64_t drawCalls = sRenderer->GetDrawCallCount();
    const uint64_t triangles = sRenderer->GetTriangleCount();
    const uint64_t textureUploads = sRenderer->GetTextureCacheUploadCount();
    const uint64_t textureBytes = sRenderer->GetTextureCacheUploadBytes();
    const uint64_t vertexBytes = sRenderer->GetVertexUploadBytes();
    Mk64Diagnostics3DSPerformance(
        static_cast<uint32_t>(sFrameCounter),
        PositiveTenths(sRenderer->GetPresentedFps2Seconds()),
        PositiveTenths(sRenderer->GetPresentedFps10Seconds()),
        static_cast<uint32_t>(drawCalls - sLastPerformanceDrawCalls),
        static_cast<uint32_t>(triangles - sLastPerformanceTriangles),
        static_cast<uint32_t>(textureUploads - sLastPerformanceTextureUploads),
        static_cast<uint32_t>((textureBytes - sLastPerformanceTextureBytes) / 1024U),
        static_cast<uint32_t>((vertexBytes - sLastPerformanceVertexBytes) / 1024U),
        Mk64Resource3DSLoadedCount(), PositiveHundredths(C3D_GetProcessingTime()),
        PositiveHundredths(C3D_GetDrawingTime()),
        static_cast<uint32_t>(std::lround(std::max(0.0f, C3D_GetCmdBufUsage()) * 1000.0f)));
    sLastPerformanceDrawCalls = drawCalls;
    sLastPerformanceTriangles = triangles;
    sLastPerformanceTextureUploads = textureUploads;
    sLastPerformanceTextureBytes = textureBytes;
    sLastPerformanceVertexBytes = vertexBytes;
    sLastPerformanceSampleFrame = sFrameCounter;
}
}

void SetRendererFault(const char* stage, const char* reason, bool frameStateRecovered) {
    ++sRendererFaultCounter;
    sRendererHealthyFrameCounter = 0;
    sRendererFaulted = !frameStateRecovered;
    if (frameStateRecovered) {
        sTextureCacheCapacity = std::max(kTextureCacheRecoveryFloor,
                                         sTextureCacheCapacity > kTextureCacheRecoveryStep
                                             ? sTextureCacheCapacity - kTextureCacheRecoveryStep
                                             : kTextureCacheRecoveryFloor);
        if (sUseIntermediatePresentation) {
            sUseIntermediatePresentation = false;
            Mk64FrameInterpolation3DSSetEnabled(false);
        }
    }
    sAdaptivePresentation = {};
    sAdaptivePresentation.cooldownTicks =
        mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks;
    size_t textureSlots = 0;
    size_t initializedTextures = 0;
    size_t textureBytes = 0;
    size_t shaderPrograms = 0;
    size_t clipScratchBytes = 0;
    if (sRenderer != nullptr) {
        sRenderer->GetDebugStats(&textureSlots, &initializedTextures, &textureBytes,
                                 &shaderPrograms, &clipScratchBytes);
    }
    // A recovered allocation failure costs one dropped presentation. Persist
    // the first event and the terminal event without forcing an SD flush for
    // every retry while the renderer is trying to recover its memory budget.
    if (sRendererFaultCounter == 1 || sRendererFaulted) {
        Mk64Diagnostics3DSMemory(stage, Mk64Resource3DSLoadedCount(), textureSlots,
                                 initializedTextures, textureBytes, shaderPrograms,
                                 clipScratchBytes);
        Mk64Diagnostics3DSFailure(stage, reason);
    }
    if (sRendererFaulted) {
        Mk64Diagnostics3DSSetStage("renderer-fault-limit-reached");
    }
}

extern "C" bool Mk64Graphics3DSInit() {
    if (sInterpreter != nullptr) {
        return true;
    }

    sFrameCounter = 0;
    sRendererFaultCounter = 0;
    sRendererHealthyFrameCounter = 0;
    sRendererFaulted = false;
    sHasPresentedTopFrame = false;
    sSuppressNextPresentation = false;
    sAdaptivePresentation = {};
    sLastPresentationStart = 0;
    sPreviousPresentationDuration = 0;
    sLastObservedResourceCount = Mk64Resource3DSLoadedCount();
    sLastObservedTextureUploadCount = 0;
    sLastObservedTextureUploadBytes = 0;
    sLastPerformanceDrawCalls = 0;
    sLastPerformanceTriangles = 0;
    sLastPerformanceTextureUploads = 0;
    sLastPerformanceTextureBytes = 0;
    sLastPerformanceVertexBytes = 0;
    sLastPerformanceSampleFrame = 0;
    // Diagnostics resolves the hardware model once during process startup.
    // Every graphics component consumes this same conservative answer so a
    // failed APT query cannot produce mismatched target sizes/frame rates.
    sResolvedNewModel = Mk64Diagnostics3DSIsNewModel();
    Mk64Diagnostics3DSCheckpoint(sResolvedNewModel ? "performance-profile-new-3ds"
                                                   : "performance-profile-old-3ds");
    sOutputWidth = Mk64Settings3DSGetResolutionWidth();
    // Wide output is supported across the 3DS family except the Old 2DS.
    // Keep 400 as the default/performance mode, but honor an explicit 800 px
    // choice on Old 3DS/XL just like the referenced SM64 3DS implementation.
    if (sOutputWidth == 800 && !Mk64Diagnostics3DSSupportsWideMode()) {
        sOutputWidth = 400;
    }
    sUseIntermediatePresentation = sResolvedNewModel && sOutputWidth == 400;
    sTextureCacheCapacity = sResolvedNewModel && sOutputWidth == 400 ? 384U : 256U;
    Mk64FrameInterpolation3DSSetEnabled(sUseIntermediatePresentation);
    sUseIntermediatePresentation = sUseIntermediatePresentation &&
                                   Mk64FrameInterpolation3DSIsEnabled();

    sWindow = std::make_unique<Fast::GfxWindowBackend3DS>();
    sRenderer = std::make_unique<Fast::GfxRenderingAPICitro3D>();
    sInterpreter = std::make_shared<Fast::Interpreter>();
    sDebugger = std::make_shared<Fast::GfxDebugger>();
    Fast::GfxSetInstance(sInterpreter);
    sInterpreter->SetGfxDebugger(sDebugger);
    // Upstream leaves these fields uninitialized because desktop always fills
    // them through GameEngine::RunCommands. The native 3DS path drives the
    // interpreter directly and must establish deterministic key-frame state.
    sInterpreter->mInterpolationIndex = 1;
    sInterpreter->mInterpolationIndexTarget = 1;
    sInterpreter->mInterpolationT = 1.0f;
    // TextureCacheClear recycles every live renderer ID. Reserve its complete
    // bounded profile capacity now so an out-of-memory recovery never needs to
    // grow this vector while the ordinary heap is already under pressure.
    sInterpreter->mTextureCache.free_texture_ids.reserve(
        Mk64Graphics3DSTextureCacheCapacity());
    // Wide mode is a 2x horizontal-density output, not an 800-unit gameplay
    // canvas. Fast3D must retain its 400x240 logical aspect and coordinates;
    // the Citro3D backend scales the top-target viewport/scissor to 800.
    sInterpreter->Init(sWindow.get(), sRenderer.get(), "Mario Kart 64 3DS", true,
                       kLogicalWidth, 240, 0, 0);
    // Mario Kart 64 emits Fast3DEX display lists. Interpreter::Init defaults
    // to F3DEX2, whose vertex command layout is incompatible and caused the
    // real-hardware crash in gfx_vtx_handler_f3dex2.
    Fast::gfx_set_target_ucode(ucode_f3dex);
    if (!sRenderer->IsInitialized()) {
        Mk64Graphics3DSShutdown();
        return false;
    }
    sLastObservedTextureUploadCount = sRenderer->GetTextureCacheUploadCount();
    sLastObservedTextureUploadBytes = sRenderer->GetTextureCacheUploadBytes();
    sLastPerformanceDrawCalls = sRenderer->GetDrawCallCount();
    sLastPerformanceTriangles = sRenderer->GetTriangleCount();
    sLastPerformanceTextureUploads = sRenderer->GetTextureCacheUploadCount();
    sLastPerformanceTextureBytes = sRenderer->GetTextureCacheUploadBytes();
    sLastPerformanceVertexBytes = sRenderer->GetVertexUploadBytes();
    // Desktop builds receive this rectangle from the ImGui viewport. The 3DS
    // runtime has no desktop UI, so bind the game viewport to the top LCD.
    UpdateGameViewport();
    return true;
}

extern "C" void Mk64Graphics3DSShutdown() {
    if (sInterpreter != nullptr) {
        sInterpreter->Destroy();
    }
    Fast::GfxSetInstance(std::shared_ptr<Fast::Interpreter>{});
    sDebugger.reset();
    sInterpreter.reset();
    sRenderer.reset();
    sWindow.reset();
    Mk64FrameInterpolation3DSSetEnabled(false);
    sRendererFaultCounter = 0;
    sRendererHealthyFrameCounter = 0;
    sOutputWidth = 400;
    sTextureCacheCapacity = 256;
    sResolvedNewModel = false;
    sUseIntermediatePresentation = false;
    sSuppressNextPresentation = false;
    sHasPresentedTopFrame = false;
    sAdaptivePresentation = {};
    sLastPresentationStart = 0;
    sPreviousPresentationDuration = 0;
    sLastObservedResourceCount = 0;
    sLastObservedTextureUploadCount = 0;
    sLastObservedTextureUploadBytes = 0;
    sLastPerformanceDrawCalls = 0;
    sLastPerformanceTriangles = 0;
    sLastPerformanceTextureUploads = 0;
    sLastPerformanceTextureBytes = 0;
    sLastPerformanceVertexBytes = 0;
    sLastPerformanceSampleFrame = 0;
}

extern "C" void Graphics_PushFrame(Gfx* commands) {
    if (commands == nullptr || sInterpreter == nullptr || sWindow == nullptr) {
        return;
    }
    // The audio runtime may dispatch synthesis to a worker here. Keep the
    // weak hook before event/frame-readiness early returns so every valid game
    // frame offers exactly one opportunity to overlap synthesis with graphics.
    if (Mk64GameAudio3DSBeginFrame != nullptr) {
        Mk64GameAudio3DSBeginFrame();
    }

    const uintptr_t listBegin = reinterpret_cast<uintptr_t>(commands);
    const uintptr_t listEnd = reinterpret_cast<uintptr_t>(gDisplayListHead);
    const size_t listBytes = listEnd >= listBegin ? listEnd - listBegin : 0;
    Mk64Diagnostics3DSSetDisplayList(commands, listBytes);
    Mk64Diagnostics3DSSetStage("renderer-frame-start");
    SetRendererStage("renderer-window-events");
    sInterpreter->HandleWindowEvents();
    const bool suppressPresentation = sSuppressNextPresentation;
    sSuppressNextPresentation = false;
    if (suppressPresentation) {
        Mk64FrameInterpolation3DSClearPrepared();
        Mk64Diagnostics3DSSetStage("renderer-presentation-suppressed");
        return;
    }
    if (sRendererFaulted) {
        // Keep gameplay/input alive if a frame decoding exception occurs.
        // Events must still be pumped so APT suspend/exit can complete and the
        // user can capture a SELECT dump instead of trapping the title forever.
        return;
    }
    if (!sWindow->IsRunning() || !sInterpreter->IsFrameReady()) {
        return;
    }

    // Simulation remains the original 30 Hz. At 400 px, New 3DS may present a
    // bounded matrix-interpolated midpoint followed by the key frame; Old 3DS
    // and the 800 px quality mode present only the key frame.
    ++sFrameCounter;
    const uint64_t presentationStart = osGetTime();
    const bool renderIntermediate = sUseIntermediatePresentation &&
                                    ShouldRenderIntermediatePresentation(presentationStart);
    SetRendererStage("renderer-prepare");
    bool didPresentIntermediate = false;
    try {
        UpdateGameViewport();
        if (renderIntermediate) {
            // A midpoint is optional. Under CPU/GPU, resource-upload or audio
            // pressure, skip the entire extra presentation instead of paying
            // for a retained-image Citro3D frame and an additional VBlank.
            // The mandatory key frame below then gets the full 30 Hz budget.
            const bool interpolated = Mk64FrameInterpolation3DSPrepare(0.5f);
            if (interpolated) {
                Mk64Diagnostics3DSSetFrame(sFrameCounter, 1);
                sInterpreter->mInterpolationIndex = 0;
                sInterpreter->mInterpolationIndexTarget = 0;
                sInterpreter->mInterpolationT = 0.5f;
                sInterpreter->StartFrame();
                SetRendererStage("renderer-run-intermediate");
                sInterpreter->Run(commands, sNoMatrixReplacements);
                // Count every image actually sent to the display. The lower
                // HUD remains key-frame-only, but the optional top FPS glyph
                // must be redrawn here or it flickers at 30 Hz and reports
                // simulation ticks instead of presentation rate.
                Mk64BottomUI3DSRecordPresentation();
                if (Mk64Settings3DSGetShowFpsEnabled()) {
                    Mk64BottomUI3DSDrawTopFps(sRenderer->PrepareForExternalDraw());
                }
                SetRendererStage("renderer-end-intermediate");
                sInterpreter->EndFrame();
                didPresentIntermediate = true;
                sHasPresentedTopFrame = true;
            }
            Mk64FrameInterpolation3DSClearPrepared();
        }

        Mk64Diagnostics3DSSetFrame(sFrameCounter, didPresentIntermediate ? 2 : 1);
        sInterpreter->mInterpolationIndex = 1;
        sInterpreter->mInterpolationIndexTarget = 1;
        sInterpreter->mInterpolationT = 1.0f;
        sInterpreter->StartFrame();
        SetRendererStage("renderer-run-display-list");
        sInterpreter->Run(commands, sNoMatrixReplacements);
        Mk64BottomUI3DSDraw(sRenderer->PrepareForExternalDraw());
        SetRendererStage("renderer-end-frame");
        sInterpreter->EndFrame();
        sHasPresentedTopFrame = true;
    } catch (const std::length_error& exception) {
        // The backend has already doubled E5's packed-vertex budget. If an
        // unusually dense display list still exceeds it, close the partial
        // frame and resume on the next simulation tick instead of freezing the
        // renderer permanently. The adaptive fallback removes optional 60 Hz
        // work while the scene remains above budget.
        Mk64FrameInterpolation3DSClearPrepared();
        sRenderer->EndFrame();
        sInterpreter->mBufVboLen = 0;
        sInterpreter->mBufVboNumTris = 0;
        sInterpreter->mRdp->textures_changed[0] = true;
        sInterpreter->mRdp->textures_changed[1] = true;
        Mk64Diagnostics3DSSetFrame(sFrameCounter, didPresentIntermediate ? 1U : 0U);
        SetRendererFault("renderer-vertex-pressure", exception.what(), true);
        sPreviousPresentationDuration = osGetTime() - presentationStart;
        return;
    } catch (const std::bad_alloc& exception) {
        // A full animated-texture working set used to fragment the ordinary
        // heap while rotating map/list nodes. Close the Citro3D frame, release
        // cache ownership, and retry on the next 30 Hz keyframe. One allocation
        // failure must not permanently freeze rendering or send the title HOME.
        Mk64FrameInterpolation3DSClearPrepared();
        sRenderer->EndFrame();
        // Flush can throw from the backend before clearing Fast3D's pending
        // batch. Discard it explicitly, and force both retained RDP texture
        // slots to import again after the cache nodes are released.
        sInterpreter->mBufVboLen = 0;
        sInterpreter->mBufVboNumTris = 0;
        sInterpreter->mRdp->textures_changed[0] = true;
        sInterpreter->mRdp->textures_changed[1] = true;
        bool cacheReleased = false;
        try {
            // ReleaseTextureAllocations first waits for the submitted frame.
            // Keep the Fast3D owners alive until that synchronization completes
            // because pending texture uploads can still reference their bytes.
            sRenderer->ReleaseTextureAllocations();
            sInterpreter->TextureCacheClear();
            cacheReleased = true;
        } catch (...) {
        }
        Mk64Diagnostics3DSSetFrame(sFrameCounter, didPresentIntermediate ? 1U : 0U);
        SetRendererFault("renderer-memory-pressure", exception.what(), cacheReleased);
        sPreviousPresentationDuration = osGetTime() - presentationStart;
        return;
    } catch (const std::exception& exception) {
        // Leave Citro3D in a closed state if Fast3D rejects a malformed
        // command. Unlike the explicitly repaired allocation path above, an
        // arbitrary exception fails closed rather than reusing unknown state.
        Mk64FrameInterpolation3DSClearPrepared();
        sRenderer->EndFrame();
        Mk64Diagnostics3DSSetFrame(sFrameCounter, didPresentIntermediate ? 1U : 0U);
        SetRendererFault("renderer-exception", exception.what(), false);
        sPreviousPresentationDuration = osGetTime() - presentationStart;
        return;
    } catch (...) {
        Mk64FrameInterpolation3DSClearPrepared();
        sRenderer->EndFrame();
        Mk64Diagnostics3DSSetFrame(sFrameCounter, didPresentIntermediate ? 1U : 0U);
        SetRendererFault("renderer-exception", "unknown C++ exception", false);
        sPreviousPresentationDuration = osGetTime() - presentationStart;
        return;
    }
    if (renderIntermediate && !didPresentIntermediate) {
        // Do not count unavailable interpolation state as a successful probe.
        // Wait for a fresh sustained-headroom window before trying it again.
        sAdaptivePresentation = {};
        sAdaptivePresentation.cooldownTicks =
            mk64_3ds::kAdaptivePresentationFailedProbeCooldownTicks;
    }
    if (sRendererFaultCounter != 0 &&
        ++sRendererHealthyFrameCounter >= kRendererFaultRecoveryResetFrames) {
        sRendererFaultCounter = 0;
        sRendererHealthyFrameCounter = 0;
    }
    LogPerformanceSample();
    sPreviousPresentationDuration = osGetTime() - presentationStart;
    SetRendererStage("renderer-frame-presented");
}

extern "C" void GameEngine_ProcessGfxCommands(Gfx* commands) {
    Graphics_PushFrame(commands);
}

extern "C" void Mk64Graphics3DSEvictSourceTexture(const void* address) {
    if (address != nullptr && sInterpreter != nullptr) {
        sInterpreter->TextureCacheDelete(static_cast<const uint8_t*>(address));
    }
}

extern "C" bool WindowIsRunning() {
    return sWindow != nullptr && sWindow->IsRunning();
}

extern "C" void Mk64Graphics3DSGetDebugStats(size_t* textureSlots,
                                               size_t* initializedTextures,
                                               size_t* textureBytes,
                                               size_t* shaderPrograms,
                                               size_t* clipScratchBytes) {
    if (sRenderer == nullptr) {
        if (textureSlots != nullptr) *textureSlots = 0;
        if (initializedTextures != nullptr) *initializedTextures = 0;
        if (textureBytes != nullptr) *textureBytes = 0;
        if (shaderPrograms != nullptr) *shaderPrograms = 0;
        if (clipScratchBytes != nullptr) *clipScratchBytes = 0;
        return;
    }
    sRenderer->GetDebugStats(textureSlots, initializedTextures, textureBytes,
                             shaderPrograms, clipScratchBytes);
}

extern "C" void GfxDebuggerRequestDebugging() {
    if (sDebugger != nullptr) sDebugger->RequestDebugging();
}
extern "C" bool GfxDebuggerIsDebugging() {
    return sDebugger != nullptr && sDebugger->IsDebugging();
}
extern "C" bool GfxDebuggerIsDebuggingRequested() {
    return sDebugger != nullptr && sDebugger->IsDebuggingRequested();
}
extern "C" void GfxDebuggerDebugDisplayList(void* commands) {
    if (sDebugger != nullptr) {
        sDebugger->DebugDisplayList(reinterpret_cast<Fast::F3DGfx*>(commands));
    }
}

extern "C" float OTRGetAspectRatio() {
    return Mk64Settings3DSGetAspectRatio() == MK64_ASPECT_RATIO_3DS_ORIGINAL
               ? 4.0f / 3.0f
               : 5.0f / 3.0f;
}
extern "C" float OTRGetDimensionFromLeftEdge(float value) {
    return 160.0f - 120.0f * OTRGetAspectRatio() + value;
}
extern "C" float OTRGetDimensionFromRightEdge(float value) {
    return 160.0f + 120.0f * OTRGetAspectRatio() - (320.0f - value);
}
extern "C" int16_t OTRGetRectDimensionFromLeftEdge(float value) {
    return static_cast<int16_t>(std::floor(OTRGetDimensionFromLeftEdge(value)));
}
extern "C" int16_t OTRGetRectDimensionFromRightEdge(float value) {
    return static_cast<int16_t>(std::ceil(OTRGetDimensionFromRightEdge(value)));
}
extern "C" uint32_t OTRGetGameRenderWidth() {
    return kLogicalWidth;
}
extern "C" uint32_t OTRGetGameRenderHeight() {
    return 240;
}
extern "C" bool Mk64Graphics3DSResolvedNewModel() {
    return sResolvedNewModel;
}
extern "C" Mk64PerformanceProfile3DS Mk64Graphics3DSResolvedPerformanceProfile() {
    return sResolvedNewModel ? MK64_PERFORMANCE_PROFILE_NEW_3DS
                             : MK64_PERFORMANCE_PROFILE_OLD_3DS;
}
extern "C" uint32_t Mk64Graphics3DSBottomHudRefreshDivisor() {
    // Menu/modal redraws remain immediate. Race HUD work stays at 10 Hz on
    // Old hardware, in 800 px mode, and while New-400 is pressured, probing,
    // or cooling down. Restore 15 Hz only after sustained midpoint headroom;
    // any pressure clears that state immediately and preserves HUD updates.
    if (!sResolvedNewModel || sOutputWidth != 400 || !sAdaptivePresentation.midpointEnabled) {
        return 3U;
    }
    return 2U;
}
extern "C" size_t Mk64Graphics3DSTextureCacheCapacity() {
    // Old 3DS keeps the proven E4 footprint. New 3DS at 400 px spends its
    // additional CPU budget on a larger animated-kart working set; 800 px mode
    // retains the smaller cache to leave linear memory for the wider target.
    return sTextureCacheCapacity;
}
extern "C" uint32_t Mk64Graphics3DSResolvedOutputWidth() {
    return sOutputWidth;
}
extern "C" bool Mk64Graphics3DSUsesIntermediatePresentation() {
    return sUseIntermediatePresentation;
}
extern "C" void Mk64Graphics3DSSuppressNextPresentation(bool suppress) {
    sSuppressNextPresentation = suppress;
}
extern "C" uint32_t OTRGetGameViewportWidth() {
    return GetViewportWidth();
}
extern "C" uint32_t OTRGetGameViewportHeight() {
    return 240;
}
extern "C" uint32_t OTRCalculateCenterOfAreaFromRightEdge(int32_t center) {
    return static_cast<uint32_t>((OTRGetDimensionFromRightEdge(320.0f) - 320.0f) / 2.0f + center);
}
extern "C" uint32_t OTRCalculateCenterOfAreaFromLeftEdge(int32_t center) {
    return static_cast<uint32_t>((OTRGetDimensionFromLeftEdge(0.0f) - 320.0f) / 2.0f + center);
}
