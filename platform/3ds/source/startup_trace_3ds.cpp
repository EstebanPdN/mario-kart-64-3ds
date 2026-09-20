#include "startup_trace_3ds.h"
#include <3ds.h>
#include <atomic>
#include <cstdio>
#include <unistd.h>

namespace {
std::atomic<bool> active{false}, running{false};
std::atomic<bool> asynchronous{false}, urgent{false};
std::atomic<int> gameState{-1}, menu{-1};
std::atomic<unsigned> submittedFrames{0};
std::atomic<unsigned> displayTimeouts{0};
std::atomic<const char*> stage{"trace-start"}, loader{"not-started"};
std::atomic<unsigned> sequence{1}, acknowledged{0};
Thread worker = nullptr;
FILE* output = nullptr;
uint64_t started = 0;

bool Write(const char* kind, const char* current, uint64_t duration) {
    // This file and its stdio lock belong exclusively to this worker. No
    // game/log/HID/renderer locks or main-thread dump requests are needed.
    return std::fprintf(output, "%llu %s stage=%s unchanged_ms=%llu loader=%s frames=%u game_state=%d menu=%d display_timeouts=%u\n",
        static_cast<unsigned long long>(osGetTime() - started), kind, current,
        static_cast<unsigned long long>(duration), loader.load(std::memory_order_acquire),
        submittedFrames.load(std::memory_order_acquire), gameState.load(), menu.load(), displayTimeouts.load()) > 0 &&
        std::fflush(output) == 0 && fsync(fileno(output)) == 0;
}
void Watch(void*) {
    unsigned last = 0;
    uint64_t changed = osGetTime(), reported = changed;
    while (running.load(std::memory_order_acquire)) {
        const auto now = osGetTime();
        const bool forced = urgent.exchange(false, std::memory_order_acq_rel);
        const auto serial = sequence.load(std::memory_order_acquire);
        const char* current = stage.load(std::memory_order_acquire);
        if (serial != last && (forced || !asynchronous.load(std::memory_order_acquire) || now - reported >= 1000)) {
            if (!Write("stage", current, 0)) break;
            last = serial;
            changed = reported = now;
            acknowledged.store(serial, std::memory_order_release);
        } else if (now - reported >= 5000) {
            if (!Write("waiting", current, now - changed)) break;
            reported = now;
        }
        svcSleepThread(10000000LL);
    }
    std::fclose(output);
    output = nullptr;
    active.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
}
}
extern "C" void Mk64Startup3DSStart(void) {
    // Append preserves the evidence if the owner relaunches after a freeze.
    output = std::fopen("sdmc:/3ds/MK64/dump/startup-e13.log", "ab");
    if (!output) return;
    std::setvbuf(output, nullptr, _IONBF, 0);
    std::fprintf(output, "\nSTART build=%s watchdog_ms=5000 stage_ack_ms=250\n", MK64_3DS_VERSION);
    std::fflush(output);
    started = osGetTime();
    submittedFrames.store(0, std::memory_order_relaxed);
    displayTimeouts.store(0);
    asynchronous.store(false);
    urgent.store(false);
    gameState.store(-1);
    menu.store(-1);
    running.store(true, std::memory_order_release);
    worker = threadCreate(Watch, nullptr, 32u * 1024u, 0x30, -2, false);
    if (!worker) { running.store(false); std::fclose(output); output = nullptr; return; }
    active.store(true, std::memory_order_release);
    Mk64Startup3DSStage("startup-trace-ready");
}
extern "C" bool Mk64Startup3DSActive(void) { return active.load(std::memory_order_acquire); }
static void PublishStage(const char* value, bool wait) {
    if (!active.load(std::memory_order_acquire)) return;
    stage.store(value, std::memory_order_release);
    const unsigned serial = sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (!wait) return;
    urgent.store(true, std::memory_order_release);
    const auto start = osGetTime();
    // A stalled SD service must not introduce an unlimited wait in the game.
    while (running.load(std::memory_order_acquire) &&
           acknowledged.load(std::memory_order_acquire) != serial && osGetTime() - start < 250)
        svcSleepThread(1000000LL);
}
extern "C" void Mk64Startup3DSStage(const char* value) {
    PublishStage(value, !asynchronous.load(std::memory_order_acquire));
}
extern "C" void Mk64Startup3DSFailure(const char* value) { PublishStage(value, true); }
extern "C" void Mk64Startup3DSDisplayTimeout(void) { displayTimeouts.fetch_add(1); }
extern "C" void Mk64Startup3DSAsync(void) { asynchronous.store(true, std::memory_order_release); }
extern "C" void Mk64Startup3DSGameState(int value, int selection) {
    gameState.store(value);
    menu.store(selection);
}
extern "C" void Mk64Startup3DSLoaderPhase(const char* value) { loader.store(value, std::memory_order_release); }
extern "C" void Mk64Startup3DSFrameSubmitted(void) {
    if (active.load(std::memory_order_acquire)) submittedFrames.fetch_add(1, std::memory_order_release);
}
extern "C" bool Mk64Startup3DSHasSubmittedFrames(void) {
    // Switch to sampled tracing after the first submissions. This is neither
    // proof of a completed startup nor a reason to stop the worker.
    return submittedFrames.load(std::memory_order_acquire) >= 4;
}
extern "C" void Mk64Startup3DSStop(void) {
    active.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
    if (worker) {
        // Never detach or free the stack of a worker blocked in filesystem IPC.
        if (R_FAILED(threadJoin(worker, 1000000000ULL))) svcExitProcess();
        threadFree(worker);
        worker = nullptr;
    }
}
