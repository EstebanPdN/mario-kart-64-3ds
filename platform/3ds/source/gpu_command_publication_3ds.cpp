#include <3ds.h>

// A split command list must be published independently of geometry. EndFrame
// can clean only C2D allocations and may have no remaining list to flush.
extern "C" Result __real_GX_ProcessCommandList(u32*, u32, u8);
extern "C" void Mk64Gpu3DSRecordCommands(u32*, u32, u8) __attribute__((weak));
extern "C" Result __wrap_GX_ProcessCommandList(u32* commands, u32 bytes, u8 flags) {
    const u8 publishedFlags = flags | GX_CMDLIST_FLUSH;
    if (Mk64Gpu3DSRecordCommands) Mk64Gpu3DSRecordCommands(commands, bytes, publishedFlags);
    return __real_GX_ProcessCommandList(commands, bytes, publishedFlags);
}
