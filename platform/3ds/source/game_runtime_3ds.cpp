#include "game_runtime_3ds.h"

#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "gfx_citro3d.h"
#include "gfx_window_manager_3ds.h"
#include "settings_3ds.h"

#include <3ds.h>
#include <fast/interpreter.h>

#include <cmath>
#include <exception>
#include <memory>
#include <unordered_map>

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> interpreter);
}

extern "C" size_t Mk64Resource3DSLoadedCount(void);
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
bool sRendererFaulted = false;
bool sHasPresentedTopFrame = false;
constexpr uint64_t kRendererFaultTolerance = 16;
constexpr uint32_t kLogicalWidth = 400;
uint32_t sOutputWidth = 400;
bool sResolvedNewModel = false;
bool sUseIntermediatePresentation = false;

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
}

void SetRendererFault(const char* stage, const char* reason) {
    ++sRendererFaultCounter;
    sRendererFaulted = true;
    size_t textureSlots = 0;
    size_t initializedTextures = 0;
    size_t textureBytes = 0;
    size_t shaderPrograms = 0;
    size_t clipScratchBytes = 0;
    if (sRenderer != nullptr) {
        sRenderer->GetDebugStats(&textureSlots, &initializedTextures, &textureBytes,
                                 &shaderPrograms, &clipScratchBytes);
    }
    Mk64Diagnostics3DSMemory(stage, Mk64Resource3DSLoadedCount(), textureSlots, initializedTextures,
                             textureBytes, shaderPrograms, clipScratchBytes);
    Mk64Diagnostics3DSFailure(stage, reason);
    if (sRendererFaultCounter > kRendererFaultTolerance) {
        Mk64Diagnostics3DSSetStage("renderer-fault-limit-reached");
    }
}

extern "C" bool Mk64Graphics3DSInit() {
    if (sInterpreter != nullptr) {
        return true;
    }

    sFrameCounter = 0;
    sRendererFaultCounter = 0;
    sRendererFaulted = false;
    sHasPresentedTopFrame = false;
    // Diagnostics resolves the hardware model once during process startup.
    // Every graphics component consumes this same conservative answer so a
    // failed APT query cannot produce mismatched target sizes/frame rates.
    sResolvedNewModel = Mk64Diagnostics3DSIsNewModel();
    sOutputWidth = Mk64Settings3DSGetResolutionWidth();
    // Wide output is supported across the 3DS family except the Old 2DS.
    // Keep 400 as the default/performance mode, but honor an explicit 800 px
    // choice on Old 3DS/XL just like the referenced SM64 3DS implementation.
    if (sOutputWidth == 800 && !Mk64Diagnostics3DSSupportsWideMode()) {
        sOutputWidth = 400;
    }
    sUseIntermediatePresentation = sResolvedNewModel && sOutputWidth == 400;
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
    sOutputWidth = 400;
    sResolvedNewModel = false;
    sUseIntermediatePresentation = false;
    sHasPresentedTopFrame = false;
}

extern "C" void Graphics_PushFrame(Gfx* commands) {
    if (commands == nullptr || sInterpreter == nullptr || sWindow == nullptr) {
        return;
    }

    const uintptr_t listBegin = reinterpret_cast<uintptr_t>(commands);
    const uintptr_t listEnd = reinterpret_cast<uintptr_t>(gDisplayListHead);
    const size_t listBytes = listEnd >= listBegin ? listEnd - listBegin : 0;
    Mk64Diagnostics3DSSetDisplayList(commands, listBytes);
    Mk64Diagnostics3DSSetStage("renderer-frame-start");
    SetRendererStage("renderer-window-events");
    sInterpreter->HandleWindowEvents();
    if (sRendererFaulted) {
        // Keep gameplay/input alive if a frame decoding exception occurs.
        // Events must still be pumped so APT suspend/exit can complete and the
        // user can capture an L+R+A dump instead of trapping the title forever.
        return;
    }
    if (!sWindow->IsRunning() || !sInterpreter->IsFrameReady()) {
        return;
    }

    // Simulation remains the original 30 Hz. At 400 px, New 3DS may present a
    // bounded matrix-interpolated midpoint followed by the key frame; Old 3DS
    // and the 800 px quality mode present only the key frame.
    ++sFrameCounter;
    SetRendererStage("renderer-prepare");
    try {
        UpdateGameViewport();
        if (sUseIntermediatePresentation) {
            const bool interpolated = Mk64FrameInterpolation3DSPrepare(0.5f);
            Mk64Diagnostics3DSSetFrame(sFrameCounter, 1);
            sInterpreter->mInterpolationIndex = 0;
            sInterpreter->mInterpolationIndexTarget = 0;
            sInterpreter->mInterpolationT = 0.5f;
            if (interpolated) {
                sInterpreter->StartFrame();
                SetRendererStage("renderer-run-intermediate");
                sInterpreter->Run(commands, sNoMatrixReplacements);
            } else {
                // Every New-3DS 400 px simulation tick must still consume two
                // 60 Hz presentation intervals. When topology/camera safety
                // rejects interpolation, repeat the already displayed top
                // image with a cheap Citro3D frame instead of decoding the
                // complete display list a second time or advancing at 2x.
                SetRendererStage("renderer-repeat-intermediate");
                sRenderer->StartFrame();
                if (!sHasPresentedTopFrame) {
                    // The top target has no defined retained contents before
                    // its first completed presentation. Clear that one repeat
                    // to black so boot can never flash uninitialized VRAM.
                    sRenderer->ClearFramebuffer(true, true);
                }
            }
            Mk64FrameInterpolation3DSClearPrepared();
            Mk64BottomUI3DSDraw(sRenderer->PrepareForExternalDraw());
            SetRendererStage("renderer-end-intermediate");
            if (interpolated) {
                sInterpreter->EndFrame();
            } else {
                sRenderer->EndFrame();
            }
            sHasPresentedTopFrame = true;
        }

        Mk64Diagnostics3DSSetFrame(sFrameCounter, sUseIntermediatePresentation ? 2 : 1);
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
    } catch (const std::exception& exception) {
        // Leave Citro3D in a closed state even if Fast3D rejects a malformed
        // command or runs out of memory. The diagnostic thread and runtime log
        // remain usable instead of terminating through std::terminate.
        Mk64FrameInterpolation3DSClearPrepared();
        SetRendererFault("renderer-exception", exception.what());
        sRenderer->EndFrame();
        return;
    } catch (...) {
        Mk64FrameInterpolation3DSClearPrepared();
        sRenderer->EndFrame();
        SetRendererFault("renderer-exception", "unknown C++ exception");
        return;
    }
    SetRendererStage("renderer-frame-presented");
}

extern "C" void GameEngine_ProcessGfxCommands(Gfx* commands) {
    Graphics_PushFrame(commands);
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
extern "C" uint32_t Mk64Graphics3DSResolvedOutputWidth() {
    return sOutputWidth;
}
extern "C" bool Mk64Graphics3DSUsesIntermediatePresentation() {
    return sUseIntermediatePresentation;
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
