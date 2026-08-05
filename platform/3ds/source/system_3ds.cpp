#include "system_3ds.h"

#include <3ds.h>

extern "C" uint64_t Mk64System3DSGetTick(void) {
    return svcGetSystemTick();
}

extern "C" uint64_t Mk64System3DSTicksPerSecond(void) {
    return SYSCLOCK_ARM11;
}
