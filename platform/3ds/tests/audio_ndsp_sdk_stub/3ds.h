#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <chrono>
using u16 = uint16_t;
using u32 = uint32_t;
using Result = int;
#define R_FAILED(r) ((r) < 0)
enum { RESET_ONESHOT = 0, NDSP_WBUF_FREE = 0, NDSP_WBUF_DONE, NDSP_WBUF_QUEUED,
       NDSP_WBUF_PLAYING, NDSP_OUTPUT_STEREO, NDSP_INTERP_LINEAR, NDSP_FORMAT_STEREO_PCM16 };
struct LightLock { std::mutex mutex; };
inline void LightLock_Init(LightLock*) {}
inline void LightLock_Lock(LightLock* l) { l->mutex.lock(); }
inline void LightLock_Unlock(LightLock* l) { l->mutex.unlock(); }
struct LightEvent { std::mutex mutex; std::condition_variable cv; bool ready = false; };
inline void LightEvent_Init(LightEvent* e, int) { e->ready = false; }
inline void LightEvent_Signal(LightEvent* e) {
    std::lock_guard<std::mutex> guard(e->mutex); e->ready = true; e->cv.notify_one();
}
inline int LightEvent_WaitTimeout(LightEvent* e, int64_t ns) {
    std::unique_lock<std::mutex> guard(e->mutex);
    if (!e->cv.wait_for(guard, std::chrono::nanoseconds(ns), [&]{ return e->ready; })) return 1;
    e->ready = false; return 0;
}
struct ndspWaveBuf { void* data_vaddr; u32 nsamples; bool looping; int status; u16 sequence_id; };
using ndspCallback = void (*)(void*);
inline ndspCallback testCallback = nullptr;
inline void* testCallbackData = nullptr;
inline bool testClean = true;
inline bool testInitFail = false;
inline int testAllocationBudget = -1;
inline unsigned testAllocations = 0;
inline unsigned testExits = 0;
inline bool testPaused = false;
inline void* linearAlloc(size_t bytes) {
    if (testAllocationBudget == 0) return nullptr;
    if (testAllocationBudget > 0) --testAllocationBudget;
    ++testAllocations; return std::malloc(bytes);
}
inline void linearFree(void* p) { --testAllocations; std::free(p); }
inline int ndspInit() { return testInitFail ? -1 : 0; }
inline void ndspExit() { ++testExits; }
inline void ndspSetCallback(ndspCallback f, void* p) { testCallback = f; testCallbackData = p; }
inline void ndspSetOutputMode(int) {}
inline void ndspChnReset(int) {}
inline void ndspChnWaveBufClear(int) {}
inline void ndspChnSetInterp(int,int) {}
inline void ndspChnSetRate(int,float) {}
inline void ndspChnSetFormat(int,int) {}
inline void ndspChnSetMix(int,float*) {}
inline void ndspChnSetPaused(int,bool b) { testPaused = b; }
inline u16 ndspChnGetWaveBufSeq(int) { return 0; }
inline u32 ndspChnGetSamplePos(int) { return 0; }
inline void ndspChnWaveBufAdd(int,ndspWaveBuf* b) { b->status = NDSP_WBUF_QUEUED; }
