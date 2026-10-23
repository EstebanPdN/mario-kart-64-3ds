#pragma once
#include <cstddef>
namespace mk64_3ds {
constexpr unsigned NativeSpriteCacheSlots(bool newProfile, std::size_t linearFree) {
    // At most 2 MiB, retaining a 6 MiB New / 4 MiB Old linear reserve.
    // The compressed resident pages live in the separate ordinary heap.
    return linearFree >= (newProfile?6u:4u)*1024u*1024u + 128u*16384u ? 128u : 32u;
}
}
