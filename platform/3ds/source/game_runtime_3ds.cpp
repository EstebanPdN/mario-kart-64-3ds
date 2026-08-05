#include "game_runtime_3ds.h"

#include "gfx_citro3d.h"
#include "gfx_window_manager_3ds.h"

#include <fast/interpreter.h>

#include <cmath>
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
}

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

    sInterpreter->HandleWindowEvents();
    if (!sWindow->IsRunning() || !sInterpreter->IsFrameReady()) {
        return;
    }

    // MK64 simulates at 30 Hz. Present each vanilla state twice so the native
    // display remains synchronized at 60 Hz without accelerating game logic.
    for (int presentation = 0; presentation < 2 && sWindow->IsRunning(); ++presentation) {
        sInterpreter->StartFrame();
        sInterpreter->Run(commands, sNoMatrixReplacements);
        sInterpreter->EndFrame();
    }
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
