#include <3ds.h>
#include "audio_ndsp_3ds.h"
#include <cassert>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

extern "C" bool Mk64System3DSCleanDataCache(const void*, size_t) { return testClean; }

int main() {
    Mk64Audio3DSWakeRefill();
    Mk64Audio3DSWaitForRefill(1);
    testInitFail = true; assert(!Mk64Audio3DSInit(26800)); testInitFail = false;
    testAllocationBudget = 2;
    assert(!Mk64Audio3DSInit(26800));
    assert(testAllocations == 0 && testCallback == nullptr);
    testAllocationBudget = -1;
    assert(Mk64Audio3DSInit(26800));
    assert(testCallback && testAllocations == 6);
    // Signals before waiting are retained; explicit close wakes a parked worker.
    testCallback(testCallbackData);
    Mk64Audio3DSWaitForRefill(1000000000ULL);
    auto wait = std::async(std::launch::async, [] { Mk64Audio3DSWaitForRefill(1000000000ULL); });
    Mk64Audio3DSWakeRefill();
    assert(wait.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready);
    wait.get();
    // Callback and teardown notifications share the same persistent event.
    wait = std::async(std::launch::async, [] { Mk64Audio3DSWaitForRefill(1000000000ULL); });
    testCallback(testCallbackData);
    assert(wait.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready);
    wait.get();
    Mk64Audio3DSWaitForRefill(1); // bounded no-callback fallback
    uint32_t token = 0;
    assert(!Mk64Audio3DSAcquireStereoS16(0, &token));
    int16_t* samples = Mk64Audio3DSAcquireStereoS16(896, &token);
    assert(samples && token);
    testClean = false;
    assert(!Mk64Audio3DSCommitStereoS16(token, 896));
    testClean = true;
    std::vector<int16_t> block(896 * 2, 1234);
    for (unsigned i = 0; i < 6; ++i) assert(Mk64Audio3DSQueueStereoS16(block.data(), 896));
    assert(!Mk64Audio3DSQueueStereoS16(block.data(), 896));
    assert(Mk64Audio3DSBufferedFrames() == 6 * 896);
    Mk64Audio3DSSetPaused(true); assert(testPaused);
    Mk64Audio3DSShutdown();
    assert(testCallback == nullptr && testAllocations == 0);
    assert(Mk64Audio3DSInit(26800)); // APT reinitialization
    assert(testCallback && testAllocations == 6);
    Mk64Audio3DSShutdown();
    assert(testCallback == nullptr && testAllocations == 0);
    std::puts("PASS: actual NDSP producer notifications, bounded fallback, wave ownership, failed init and restart");
}
