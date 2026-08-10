#include "game_runtime_3ds.h"

#include "diagnostics_3ds.h"
#include "gfx_citro3d.h"
#include "gfx_window_manager_3ds.h"

#include <fast/interpreter.h>

#include <cmath>
#include <exception>
#include <memory>
#include <unordered_map>

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> interpreter);
}

namespace {
std::unique_ptr<Fast::GfxWindowBackend3DS> sWindow;
std::unique_ptr<Fast::GfxRenderingAPICitro3D> sRenderer;
std::shared_ptr<Fast::Interpreter> sInterpreter;
std::shared_ptr<Fast::GfxDebugger> sDebugger;
const std::unordered_map<Mtx*, MtxF> sNoMatrixReplacements;
uint64_t sFrameCounter = 0;
uint64_t sRendererFaultCounter = 0;
bool sRendererFaulted = false;
constexpr uint64_t kRendererFaultTolerance = 16;

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
    Mk64Diagnostics3DSFailure(stage, reason);
    if (sRendererFaultCounter > kRendererFaultTolerance) {
        Mk64Diagnostics3DSSetStage("renderer-fault-limit-reached");
    }
}

extern "C" Gfx* gDisplayListHead;

extern "C" bool Mk64Graphics3DSInit() {
    if (sInterpreter != nullptr) {
        return true;
    }

    sWindow = std::make_unique<Fast::GfxWindowBackend3DS>();
    sRenderer = std::make_unique<Fast::GfxRenderingAPICitro3D>();
    sInterpreter = std::make_shared<Fast::Interpreter>();
    sDebugger = std::make_shared<Fast::GfxDebugger>();
    Fast::GfxSetInstance(sInterpreter);
    sInterpreter->SetGfxDebugger(sDebugger);
    sInterpreter->Init(sWindow.get(), sRenderer.get(), "Mario Kart 64 3DS", true, 400, 240, 0, 0);
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
    sInterpreter->mGameWindowViewport = { 0, 0, 400, 240 };
    return true;
}

extern "C" void Mk64Graphics3DSShutdown() {
    if (sInterpreter != nullptr) {
        sInterpreter->Destroy();
    }
    sDebugger.reset();
    sInterpreter.reset();
    sRenderer.reset();
    sWindow.reset();
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
    if (sRendererFaulted) {
        // Keep gameplay/input alive if a frame decoding exception occurs.
        // Repeated failures are surfaced through diagnostics; we avoid ending
        // the window in-place so the user can still capture L+R+A dump.
        return;
    }
    SetRendererStage("renderer-window-events");
    sInterpreter->HandleWindowEvents();
    if (!sWindow->IsRunning() || !sInterpreter->IsFrameReady()) {
        return;
    }

    // Preserve MK64's stable 30 Hz simulation/render cadence. The desktop
    // interpolation recorder is intentionally disabled on 3DS because its
    // per-frame dynamic tree caused v0.11 to terminate during startup.
    ++sFrameCounter;
    Mk64Diagnostics3DSSetFrame(sFrameCounter, 1);
    SetRendererStage("renderer-prepare");
    try {
        sInterpreter->StartFrame();
        SetRendererStage("renderer-run-display-list");
        sInterpreter->Run(commands, sNoMatrixReplacements);
        SetRendererStage("renderer-end-frame");
        sInterpreter->EndFrame();
    } catch (const std::exception& exception) {
        // Leave Citro3D in a closed state even if Fast3D rejects a malformed
        // command or runs out of memory. The diagnostic thread and runtime log
        // remain usable instead of terminating through std::terminate.
        SetRendererFault("renderer-exception", exception.what());
        sRenderer->EndFrame();
        return;
    } catch (...) {
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
    return 5.0f / 3.0f;
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
    return 400;
}
extern "C" uint32_t OTRGetGameRenderHeight() {
    return 240;
}
extern "C" uint32_t OTRGetGameViewportWidth() {
    return 400;
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
