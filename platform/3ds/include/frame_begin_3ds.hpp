#pragma once
#include "startup_trace_3ds.h"
#include <3ds.h>
#include <citro3d.h>

namespace mk64_3ds {
// Preserve display pacing without entering Citro3D's unbounded SYNCDRAW wait.
// Queue ownership must remain fenced even when diagnostic tracing is disabled.
inline bool BeginFrame3DS(unsigned flags, const char* stage) {
    Mk64StartupStage(stage);
    if (flags & C3D_FRAME_SYNCDRAW) {
        const auto top = C3D_FrameCounter(0), bottom = C3D_FrameCounter(1);
        const uint64_t displayStarted = osGetTime();
        while (C3D_FrameCounter(0) == top || C3D_FrameCounter(1) == bottom) {
            // Missing display callbacks do not imply an unfinished GPU queue.
            // Continue to the separate queue fence after this bounded wait.
            if (osGetTime() - displayStarted >= 50) {
                if (Mk64Startup3DSDisplayTimeout) Mk64Startup3DSDisplayTimeout();
                break;
            }
            svcSleepThread(1000000LL);
        }
    }
    const uint64_t started = osGetTime();
    while (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) {
        if (osGetTime() - started >= 5000) {
            Mk64StartupFailure("startup-gpu-queue-timeout-kernel-exit");
            svcExitProcess();
        }
        svcSleepThread(1000000LL);
    }
    Mk64StartupStage("startup-gpu-queue-wait-returned");
    return true;
}
} // namespace mk64_3ds
