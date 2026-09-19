#include "system_3ds.h"

#include <3ds.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <array>

namespace {

std::atomic<bool> sCaptureActive{ false };
std::atomic<uintptr_t> sCaptureBegin{ std::numeric_limits<uintptr_t>::max() };
std::atomic<uintptr_t> sCaptureEnd{ 0 };
std::atomic<size_t> sCapturedSize{ 0 };
struct Range { const void* address; size_t size; };
std::array<Range, 32> sRanges{};
size_t sRangeCount = 0;
bool sRangeOverflow = false;
std::atomic_flag sRangeLock = ATOMIC_FLAG_INIT;
struct CaptureLock {
    CaptureLock() { while (sRangeLock.test_and_set(std::memory_order_acquire)) {} }
    ~CaptureLock() { sRangeLock.clear(std::memory_order_release); }
};

void AtomicMinimum(std::atomic<uintptr_t>& destination, uintptr_t value) {
    uintptr_t current = destination.load(std::memory_order_relaxed);
    while (value < current &&
           !destination.compare_exchange_weak(current, value,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
    }
}

void AtomicMaximum(std::atomic<uintptr_t>& destination, uintptr_t value) {
    uintptr_t current = destination.load(std::memory_order_relaxed);
    while (value > current &&
           !destination.compare_exchange_weak(current, value,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
    }
}

void RecordAllocation(void* address, size_t size) {
    if (!sCaptureActive.load(std::memory_order_acquire) || address == nullptr ||
        size == 0) {
        return;
    }
    CaptureLock lock;
    if (!sCaptureActive.load(std::memory_order_acquire)) return;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(address);
    if (size > std::numeric_limits<uintptr_t>::max() - begin) return;
    if (sRangeCount < sRanges.size()) sRanges[sRangeCount++] = {address, size};
    else sRangeOverflow = true;
    AtomicMinimum(sCaptureBegin, begin);
    AtomicMaximum(sCaptureEnd, begin + size);
}

} // namespace

extern "C" void* __real_linearAlloc(size_t size);

extern "C" void* __wrap_linearAlloc(size_t size) {
    void* allocation = __real_linearAlloc(size);
    RecordAllocation(allocation, size);
    return allocation;
}

extern "C" void Mk64System3DSBeginLinearAllocationCapture(void) {
    CaptureLock lock;
    sRangeCount = 0;
    sRangeOverflow = false;
    sCaptureBegin.store(std::numeric_limits<uintptr_t>::max(),
                        std::memory_order_relaxed);
    sCaptureEnd.store(0, std::memory_order_relaxed);
    sCapturedSize.store(0, std::memory_order_relaxed);
    sCaptureActive.store(true, std::memory_order_release);
}

extern "C" bool Mk64System3DSEndLinearAllocationCapture(const void** address,
                                                          size_t* size) {
    CaptureLock lock;
    sCaptureActive.store(false, std::memory_order_release);
    const uintptr_t begin = sCaptureBegin.load(std::memory_order_acquire);
    const uintptr_t end = sCaptureEnd.load(std::memory_order_acquire);
    if (address != nullptr) *address = nullptr;
    if (size != nullptr) *size = 0;
    if (begin == std::numeric_limits<uintptr_t>::max() || end <= begin ||
        end - begin > std::numeric_limits<size_t>::max()) {
        return false;
    }
    const size_t capturedSize = static_cast<size_t>(end - begin);
    if (address != nullptr) *address = reinterpret_cast<const void*>(begin);
    if (size != nullptr) *size = capturedSize;
    size_t total = 0;
    for (size_t i = 0; i < sRangeCount; ++i) total += sRanges[i].size;
    sCapturedSize.store(sRangeOverflow ? capturedSize : total, std::memory_order_release);
    return true;
}

extern "C" size_t Mk64System3DSCapturedLinearAllocationSize(void) {
    return sCapturedSize.load(std::memory_order_acquire);
}

extern "C" bool Mk64System3DSCleanCapturedLinearAllocations(void) {
    CaptureLock lock;
    if (sCaptureActive.load(std::memory_order_acquire) || sRangeOverflow || sRangeCount == 0)
        return false;
    for (size_t i = 0; i < sRangeCount; ++i)
        if (!Mk64System3DSCleanDataCache(sRanges[i].address, sRanges[i].size)) return false;
    return true;
}
