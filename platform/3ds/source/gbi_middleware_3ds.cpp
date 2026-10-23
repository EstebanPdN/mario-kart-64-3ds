#ifdef MK64_3DS_NATIVE_DYNAMIC
#include "native_emit.generated.h"
#endif
#include <libultraship.h>
#include <libultraship/bridge/resourcebridge.h>

extern "C" {
#include "align_asset_macro.h"
}

namespace {
uintptr_t ResolveResource(uintptr_t address) {
    if (address == 0 || !GameEngine_OTRSigCheck(reinterpret_cast<char*>(address))) {
        return address;
    }
    return reinterpret_cast<uintptr_t>(ResourceGetDataByName(reinterpret_cast<const char*>(address)));
}
}

extern "C" void gSPDisplayList(Gfx* packet, Gfx* displayList) {
#ifdef MK64_3DS_NATIVE_DYNAMIC
    Mk64NativeEmit_gSPDisplayList(packet,reinterpret_cast<uintptr_t>(displayList));
#else
    __gSPDisplayList(packet, reinterpret_cast<Gfx*>(ResolveResource(reinterpret_cast<uintptr_t>(displayList))));
#endif
}

extern "C" void gSPDisplayListOffset(Gfx* packet, Gfx* displayList, int offset) {
#ifdef MK64_3DS_NATIVE_DYNAMIC
    Mk64NativeEmit_gSPDisplayListOffset(packet,reinterpret_cast<uintptr_t>(displayList),offset);
#else
    auto* resolved = reinterpret_cast<Gfx*>(ResolveResource(reinterpret_cast<uintptr_t>(displayList)));
    __gSPDisplayList(packet, resolved == nullptr ? nullptr : resolved + offset);
#endif
}

extern "C" void gSPVertex(Gfx* packet, uintptr_t vertices, int count, int first) {
#ifdef MK64_3DS_NATIVE_DYNAMIC
    Mk64NativeEmit_gSPVertex(packet,vertices,count,first);
#else
    __gSPVertex(packet, ResolveResource(vertices), count, first);
#endif
}

extern "C" void gSPInvalidateTexCache(Gfx* packet, uintptr_t texture) {
#ifdef MK64_3DS_NATIVE_DYNAMIC
    Mk64NativeEmit_gSPInvalidateTexCache(packet,texture);
#else
    __gSPInvalidateTexCache(packet, ResolveResource(texture));
#endif
}
