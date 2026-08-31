#include "system_3ds.h"

#include <3ds.h>

#include <atomic>
#include <cstdint>
#include <limits>

namespace {

enum class CacheCleanMode : uint8_t {
    Unprobed,
    DirectSvc,
    GspFallback,
};

std::atomic<CacheCleanMode> sCacheCleanMode{ CacheCleanMode::Unprobed };

} // namespace

extern "C" uint64_t Mk64System3DSGetTick(void) {
    return svcGetSystemTick();
}

extern "C" uint64_t Mk64System3DSTicksPerSecond(void) {
    return SYSCLOCK_ARM11;
}

extern "C" bool Mk64System3DSCleanDataCache(const void* address, size_t size) {
    if (address == nullptr || size == 0) return true;
    if (size > std::numeric_limits<u32>::max()) return false;

    const u32 cacheAddress = static_cast<u32>(reinterpret_cast<uintptr_t>(address));
    const u32 cacheSize = static_cast<u32>(size);
    CacheCleanMode mode = sCacheCleanMode.load(std::memory_order_acquire);
    if (mode != CacheCleanMode::GspFallback) {
        // Clean-only is sufficient for CPU-to-GPU/DSP ownership transfers and
        // avoids invalidating useful ARM11 cache lines. Unlike
        // GSPGPU_FlushDataCache, this does not block on a sysmodule scheduled
        // on the Old 3DS auxiliary core.
        const Result directResult =
            svcStoreProcessDataCache(CUR_PROCESS_HANDLE, cacheAddress, cacheSize);
        if (R_SUCCEEDED(directResult)) {
            if (mode == CacheCleanMode::Unprobed) {
                sCacheCleanMode.store(CacheCleanMode::DirectSvc,
                                      std::memory_order_release);
            }
            return true;
        }
        sCacheCleanMode.store(CacheCleanMode::GspFallback,
                              std::memory_order_release);
    }

    // Some launch environments can deny the direct cache SVC. Retain the
    // established libctru service path so a performance optimization can
    // never become a rendering or audio correctness requirement.
    return R_SUCCEEDED(GSPGPU_FlushDataCache(address, cacheSize));
}

extern "C" const char* Mk64System3DSDataCacheMode(void) {
    switch (sCacheCleanMode.load(std::memory_order_acquire)) {
        case CacheCleanMode::DirectSvc: return "direct-svc-store";
        case CacheCleanMode::GspFallback: return "gsp-fallback";
        case CacheCleanMode::Unprobed:
        default: return "unprobed";
    }
}
