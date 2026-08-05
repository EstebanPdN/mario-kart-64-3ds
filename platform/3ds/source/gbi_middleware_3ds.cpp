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
    __gSPDisplayList(packet, reinterpret_cast<Gfx*>(ResolveResource(reinterpret_cast<uintptr_t>(displayList))));
}

extern "C" void gSPDisplayListOffset(Gfx* packet, Gfx* displayList, int offset) {
    auto* resolved = reinterpret_cast<Gfx*>(ResolveResource(reinterpret_cast<uintptr_t>(displayList)));
    __gSPDisplayList(packet, resolved == nullptr ? nullptr : resolved + offset);
}

extern "C" void gSPVertex(Gfx* packet, uintptr_t vertices, int count, int first) {
    __gSPVertex(packet, ResolveResource(vertices), count, first);
}

extern "C" void gSPInvalidateTexCache(Gfx* packet, uintptr_t texture) {
    __gSPInvalidateTexCache(packet, ResolveResource(texture));
}
