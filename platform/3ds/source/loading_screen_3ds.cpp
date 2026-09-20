#include "loading_screen_3ds.h"
#include "loading_animation_3ds.hpp"
#include "o2r_archive_reader.hpp"
#include "diagnostics_3ds.h"
#include "startup_trace_3ds.h"
#include <3ds.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <sys/stat.h>

namespace {
using mk64_3ds::LoadingAnimation;
LoadingAnimation* sAnimation = nullptr;
Thread sThread = nullptr;
std::atomic<bool> sRunning{false};
bool sOwnsGraphics = false;
uint64_t sStartedAt = 0;
constexpr const char* kCache = "sdmc:/3ds/MK64/cache/loading-lakitu-v1.bin";
constexpr const char* kTemp = "sdmc:/3ds/MK64/cache/loading-lakitu-v1.tmp";

bool Prepare(const char* path) {
    struct stat source{};
    if (!path || stat(path, &source) != 0 || source.st_size <= 0) return false;
    void* memory = linearAlloc(sizeof(LoadingAnimation));
    if (!memory) return false;
    sAnimation = new (memory) LoadingAnimation{};
    FILE* file = std::fopen(kCache, "rb");
    const bool cached = mk64_3ds::ReadLoadingCache(file, source.st_size, source.st_mtime, *sAnimation);
    if (file) std::fclose(file);
    if (cached) return true;
    bool ready = false;
    try {
        mk64_3ds::O2rArchiveReader archive(path);
        ready = archive.Open() == mk64_3ds::O2rReadResult::Ok &&
            mk64_3ds::LoadAnimationFromArchive(*sAnimation, [&](const char* name, std::vector<uint8_t>& bytes) {
                return archive.ReadEntry(name, &bytes) == mk64_3ds::O2rReadResult::Ok;
            });
    } catch (const std::bad_alloc&) { ready = false; }
    if (!ready) { linearFree(sAnimation); sAnimation = nullptr; return false; }
    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/MK64", 0777); mkdir("sdmc:/3ds/MK64/cache", 0777);
    file = std::fopen(kTemp, "wb");
    if (file) {
        bool ok = mk64_3ds::WriteLoadingCache(file, source.st_size, source.st_mtime, *sAnimation);
        if (std::fclose(file) != 0) ok = false;
        if (ok) { std::remove(kCache); ok = std::rename(kTemp, kCache) == 0; }
        if (!ok) std::remove(kTemp);
    }
    return true; // Cache write failure must never prevent the game from booting.
}
bool Draw(bool clear) {
    if (!aptIsActive()) return false;
    u16 width = 0, height = 0;
    auto* top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    if (!top || gfxGetScreenFormat(GFX_TOP) != GSP_BGR8_OES) return false;
    if (clear) std::memset(top, 0, size_t(width) * height * 3);
    // libctru reports the rotated buffer dimensions (240 by 400/800).
    mk64_3ds::DrawLoadingAnimation(*sAnimation, osGetTime() - sStartedAt, top, height, width);
    Mk64StartupLoaderPhase("loader-top-cache-flush");
    GSPGPU_FlushDataCache(top, size_t(width) * height * 3);
    auto* bottom = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height);
    if (clear && bottom && gfxGetScreenFormat(GFX_BOTTOM) == GSP_BGR8_OES) {
        std::memset(bottom, 0, size_t(width) * height * 3);
        Mk64StartupLoaderPhase("loader-bottom-cache-flush");
        GSPGPU_FlushDataCache(bottom, size_t(width) * height * 3);
    }
    Mk64StartupLoaderPhase("loader-swap");
    gfxSwapBuffers();
    Mk64StartupLoaderPhase("loader-draw-returned");
    return true;
}
void Animate(void*) {
    unsigned frames = 0;
    while (sRunning.load(std::memory_order_acquire)) {
        if (!aptIsActive()) frames = 0;
        else if (Draw(frames < 2)) ++frames;
        // Sleep is bounded; graphics IPC above can still stall. Stop must
        // enforce its own deadline before any buffers or services are freed.
        Mk64StartupLoaderPhase("loader-sleep");
        svcSleepThread(30000000LL);
    }
}
void Launch() {
    if (!sAnimation || sThread) return;
    sRunning.store(true, std::memory_order_release);
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    sThread = threadCreate(Animate, nullptr, 16u * 1024u, std::max<s32>(0x30, priority - 1), -2, false);
    if (!sThread) { sRunning.store(false); Draw(true); }
}
}
extern "C" bool Mk64Loading3DSActive(void) { return sAnimation != nullptr; }
extern "C" void Mk64Loading3DSStart(const char* archivePath) {
    if (sAnimation) return;
    if (!archivePath) {
        archivePath = "sdmc:/3ds/MK64/mk64.o2r";
        struct stat info{};
        if (stat(archivePath, &info) != 0) archivePath = "sdmc:/3ds/mk64-3ds/mk64.o2r";
    }
    if (!Prepare(archivePath)) return;
    sStartedAt = osGetTime();
    gfxInitDefault(); gfxSet3D(false);
    sOwnsGraphics = true;
    Launch();
}
extern "C" void Mk64Loading3DSPauseDisplay(void) {
    sRunning.store(false, std::memory_order_release);
    if (sThread) {
        Mk64StartupStage("loading-worker-join-enter");
        if (R_FAILED(threadJoin(sThread, 2000000000ULL))) {
            Mk64StartupFailure("loading-worker-join-timeout-kernel-exit");
            // All threads die together. Do not free live animation/stack data
            // or enter libctru heap teardown while the worker still uses it.
            svcExitProcess();
        }
        threadFree(sThread);
        sThread = nullptr;
        Mk64StartupStage("loading-worker-join-returned");
    }
    if (sOwnsGraphics) {
        Mk64StartupStage("loading-gfx-exit-enter");
        gfxExit(); sOwnsGraphics = false;
        Mk64StartupStage("loading-gfx-exit-returned");
    }
}
extern "C" void Mk64Loading3DSResumeDisplay(void) {
    // The game renderer has initialized its buffers, but has not begun any
    // frames. Stop joins this worker before the first vanilla display list.
    Launch();
}
extern "C" void Mk64Loading3DSStop(void) {
    Mk64StartupStage("loading-stop-enter");
    Mk64Loading3DSPauseDisplay();
    Mk64StartupStage("loading-animation-free-enter");
    if (sAnimation) { sAnimation->~LoadingAnimation(); linearFree(sAnimation); sAnimation = nullptr; }
    Mk64StartupStage("loading-stop-returned");
}
