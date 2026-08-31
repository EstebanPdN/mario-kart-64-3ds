#include "system_3ds.h"

#include <3ds.h>

#include <atomic>
#include <cstdint>
#include <limits>

namespace {

std::atomic<bool> sCaptureActive{ false };
std::atomic<uintptr_t> sCaptureBegin{ std::numeric_limits<uintptr_t>::max() };
std::atomic<uintptr_t> sCaptureEnd{ 0 };
std::atomic<size_t> sCapturedSize{ 0 };

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
    const uintptr_t begin = reinterpret_cast<uintptr_t>(address);
    if (size > std::numeric_limits<uintptr_t>::max() - begin) return;
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
    sCaptureBegin.store(std::numeric_limits<uintptr_t>::max(),
                        std::memory_order_relaxed);
    sCaptureEnd.store(0, std::memory_order_relaxed);
    sCapturedSize.store(0, std::memory_order_relaxed);
    sCaptureActive.store(true, std::memory_order_release);
}

extern "C" bool Mk64System3DSEndLinearAllocationCapture(const void** address,
                                                          size_t* size) {
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
    sCapturedSize.store(capturedSize, std::memory_order_release);
    return true;
}

extern "C" size_t Mk64System3DSCapturedLinearAllocationSize(void) {
    return sCapturedSize.load(std::memory_order_acquire);
}
