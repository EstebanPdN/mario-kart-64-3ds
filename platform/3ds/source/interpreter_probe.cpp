#include <memory>
#include <unordered_map>

#include "gfx_citro3d.h"
#include "gfx_window_manager_3ds.h"
#include "input_3ds.h"
#include "fast/interpreter.h"

extern "C" {
#include "libultraship/libultra/gbi.h"
}

namespace Fast {
void GfxSetInstance(std::shared_ptr<Interpreter> interpreter);
}

namespace {

Mtx sProjection = {};
Mtx sModelView = {};

MtxF MakeProjection() {
    MtxF matrix = {};
    matrix.mf[0][0] = 1.0f / 160.0f;
    matrix.mf[1][1] = 1.0f / 120.0f;
    matrix.mf[2][2] = 1.0f;
    matrix.mf[3][3] = 1.0f;
    return matrix;
}

MtxF MakeIdentity() {
    MtxF matrix = {};
    matrix.mf[0][0] = 1.0f;
    matrix.mf[1][1] = 1.0f;
    matrix.mf[2][2] = 1.0f;
    matrix.mf[3][3] = 1.0f;
    return matrix;
}

Vtx sTriangle[] = {
    { { { -130, -85, 0 }, 0, { 0, 0 }, { 250, 45, 55, 255 } } },
    { { { 0, 100, 0 }, 0, { 0, 0 }, { 255, 215, 45, 255 } } },
    { { { 130, -85, 0 }, 0, { 0, 0 }, { 30, 120, 250, 255 } } },
};

constexpr int sTriangleBase[][2] = {
    { -130, -85 },
    { 0, 100 },
    { 130, -85 },
};

Gfx sDisplayList[] = {
    gsSPMatrix(&sProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH),
    gsSPMatrix(&sModelView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BOTH),
    gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetRenderMode(G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPVertex(sTriangle, 3, 0),
    gsSP1Triangle(0, 1, 2, 0),
    gsSPEndDisplayList(),
};

} // namespace

int main() {
    Fast::GfxWindowBackend3DS window;
    Fast::GfxRenderingAPICitro3D renderer;
    auto interpreter = std::make_shared<Fast::Interpreter>();
    Fast::GfxSetInstance(interpreter);
    Mk64Input3DSInit();
    interpreter->SetGfxDebugger(std::make_shared<Fast::GfxDebugger>());
    interpreter->Init(&window, &renderer, "Mario Kart 64 3DS", true, 400, 240, 0, 0);
    Fast::gfx_set_target_ucode(ucode_f3dex);

    std::unordered_map<Mtx*, MtxF> replacements = {
        { &sProjection, MakeProjection() },
        { &sModelView, MakeIdentity() },
    };

    while (window.IsRunning()) {
        interpreter->HandleWindowEvents();
        if (!interpreter->IsFrameReady()) {
            continue;
        }
        Mk64Pad3DS pad = {};
        Mk64Input3DSPoll(&pad);
        for (size_t i = 0; i < 3; ++i) {
            sTriangle[i].v.ob[0] = static_cast<int16_t>(sTriangleBase[i][0] + pad.stickX);
            sTriangle[i].v.ob[1] = static_cast<int16_t>(sTriangleBase[i][1] + pad.stickY);
        }
        interpreter->StartFrame();
        interpreter->Run(reinterpret_cast<::Gfx*>(sDisplayList), replacements);
        interpreter->EndFrame();
    }

    interpreter->Destroy();
    return 0;
}
