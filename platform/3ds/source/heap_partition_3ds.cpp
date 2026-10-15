#include "heap_policy_3ds.hpp"
#ifdef __3DS__
extern "C" {
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/result.h>
}
#else
#include <3ds.h>
#endif

extern "C" {
extern std::uint32_t __ctru_linear_heap_size;
void __real___system_allocateHeaps(void);

// Runs before C++ constructors, appInit and heap allocation. Only kernel
// queries are safe here. Delegate allocation and all failure handling to
// libctru rather than copying its allocator or growing the process limit.
void __wrap___system_allocateHeaps(void) {
    Handle limitHandle = 0;
    s64 commitLimit = 0;
    ResourceLimitType type = RESLIMIT_COMMIT;
    if (R_SUCCEEDED(svcGetResourceLimit(&limitHandle, CUR_PROCESS_HANDLE))) {
        if (R_SUCCEEDED(svcGetResourceLimitLimitValues(&commitLimit, limitHandle, &type, 1)) &&
            commitLimit > 0) {
            __ctru_linear_heap_size = mk64_3ds::LinearHeapBytesForCommitLimit(commitLimit);
        }
        svcCloseHandle(limitHandle);
    }
    __real___system_allocateHeaps();
}
}
