#include <3ds.h>

// libctru 2.7.0 creates its internal NDSP worker with a 4 KiB stack. Repeated
// hardware dumps place SP at the bottom of that allocation and fault on the
// first 40-byte push in ndspiReadChnState, which is the signature of exhausted
// worker stack rather than an audio-buffer fault. Interpose only that distinctive
// threadCreate call and leave every application-owned thread unchanged.
extern "C" Thread __real_threadCreate(ThreadFunc entrypoint, void* argument,
                                      size_t stackSize, int priority, int coreId,
                                      bool detached);

extern "C" Thread __wrap_threadCreate(ThreadFunc entrypoint, void* argument,
                                      size_t stackSize, int priority, int coreId,
                                      bool detached) {
    constexpr size_t kLibctruNdspStackSize = 4U * 1024U;
    constexpr size_t kSafeNdspStackSize = 16U * 1024U;
    if (stackSize == kLibctruNdspStackSize && priority == 0x18 && coreId == -2 &&
        detached) {
        stackSize = kSafeNdspStackSize;
    }
    return __real_threadCreate(entrypoint, argument, stackSize, priority, coreId,
                               detached);
}
