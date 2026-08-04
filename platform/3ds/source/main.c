#include <3ds.h>
#include <citro2d.h>

#include "platform_3ds.h"

enum {
    TEXT_BUFFER_GLYPHS = 768,
};

static C2D_TextBuf sTextBuffer;
static C2D_Text sTitleText;
static C2D_Text sStatusText;
static C2D_Text sDetailText;
static C2D_Text sExitText;

static void parse_static_texts(bool is_new_3ds) {
    C2D_TextBufClear(sTextBuffer);

    C2D_TextParse(&sTitleText, sTextBuffer, "MARIO KART 64 3DS");
    C2D_TextParse(&sStatusText, sTextBuffer, "v0.1 PLATFORM BOOTSTRAP");
    C2D_TextParse(&sDetailText, sTextBuffer,
                  is_new_3ds ? "NEW 3DS ENHANCEMENT ENABLED" : "OLD 3DS BASELINE ACTIVE");
    C2D_TextParse(&sExitText, sTextBuffer, "No ROM or Nintendo assets are included.  START: Exit");

    C2D_TextOptimize(&sTitleText);
    C2D_TextOptimize(&sStatusText);
    C2D_TextOptimize(&sDetailText);
    C2D_TextOptimize(&sExitText);
}

static void draw_top_screen(C3D_RenderTarget* top_target, u64 frame_index) {
    const u32 background = C2D_Color32(8, 13, 23, 255);
    const u32 panel = C2D_Color32(18, 34, 56, 255);
    const u32 accent = C2D_Color32(255, 196, 53, 255);
    const u32 red = C2D_Color32(219, 59, 74, 255);
    const u32 blue = C2D_Color32(67, 147, 219, 255);
    const u32 white = C2D_Color32(241, 246, 250, 255);
    const u32 muted = C2D_Color32(169, 187, 203, 255);
    const float pulse = (frame_index % 60U) < 30U ? 1.0f : 0.72f;

    C2D_TargetClear(top_target, background);
    C2D_SceneBegin(top_target);

    for (int x = 0; x < 400; x += 32) {
        const u32 stripe_color = ((x / 32) & 1) == 0 ? red : blue;
        C2D_DrawRectSolid((float) x, 0.0f, 0.0f, 32.0f, 8.0f, stripe_color);
    }

    C2D_DrawRectSolid(18.0f, 30.0f, 0.0f, 364.0f, 166.0f, panel);
    C2D_DrawRectSolid(18.0f, 30.0f, 0.0f, 6.0f, 166.0f, accent);
    C2D_DrawRectSolid(28.0f, 177.0f, 0.0f, 344.0f, 2.0f, C2D_Color32(64, 94, 124, 255));

    C2D_DrawText(&sTitleText, C2D_WithColor | C2D_AlignCenter, MK64_3DS_TOP_WIDTH / 2.0f, 52.0f, 0.0f, 0.72f,
                 0.72f, white);
    C2D_DrawText(&sStatusText, C2D_WithColor | C2D_AlignCenter, MK64_3DS_TOP_WIDTH / 2.0f, 94.0f, 0.0f, 0.45f,
                 0.45f, accent);
    C2D_DrawText(&sDetailText, C2D_WithColor | C2D_AlignCenter, MK64_3DS_TOP_WIDTH / 2.0f, 123.0f, 0.0f, 0.38f,
                 0.38f, muted);
    C2D_DrawText(&sExitText, C2D_WithColor | C2D_AlignCenter, MK64_3DS_TOP_WIDTH / 2.0f, 193.0f, 0.0f, 0.28f,
                 0.28f, C2D_Color32(241, 246, 250, (u8) (255.0f * pulse)));
}

int main(void) {
    bool is_new_3ds = false;
    u64 frame_index = 0;

    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    gfxSet3D(false);
    aptSetHomeAllowed(true);
    aptSetSleepAllowed(true);

    if (R_SUCCEEDED(APT_CheckNew3DS(&is_new_3ds)) && is_new_3ds) {
        osSetSpeedupEnable(true);
    }

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    sTextBuffer = C2D_TextBufNew(TEXT_BUFFER_GLYPHS);
    parse_static_texts(is_new_3ds);

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        draw_top_screen(top_target, frame_index++);
        C2D_TargetClear(bottom_target, C2D_Color32(0, 0, 0, 255));
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(sTextBuffer);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
