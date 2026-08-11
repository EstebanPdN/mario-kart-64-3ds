#include "resource_prefetch_3ds.h"

#include <string.h>

static size_t Mk64PrefetchHash3DS(uint64_t resourceKey) {
    resourceKey ^= resourceKey >> 33;
    resourceKey *= UINT64_C(0xff51afd7ed558ccd);
    resourceKey ^= resourceKey >> 33;
    resourceKey *= UINT64_C(0xc4ceb9fe1a85ec53);
    resourceKey ^= resourceKey >> 33;
    return (size_t) resourceKey & (MK64_3DS_TRACK_PREFETCH_HASH_SLOTS - 1u);
}

static size_t Mk64PrefetchFindSlot3DS(const Mk64PrefetchBudget3DS* budget,
                                      uint64_t resourceKey, bool* found) {
    size_t slot = Mk64PrefetchHash3DS(resourceKey);
    for (size_t probe = 0; probe < MK64_3DS_TRACK_PREFETCH_HASH_SLOTS; ++probe) {
        const uint16_t stored = budget->seenSlots[slot];
        if (stored == 0) {
            *found = false;
            return slot;
        }
        const size_t keyIndex = (size_t) stored - 1u;
        if (keyIndex < budget->seenEntries && budget->seenKeys[keyIndex] == resourceKey) {
            *found = true;
            return slot;
        }
        slot = (slot + 1u) & (MK64_3DS_TRACK_PREFETCH_HASH_SLOTS - 1u);
    }
    *found = false;
    return MK64_3DS_TRACK_PREFETCH_HASH_SLOTS;
}

void Mk64PrefetchBudget3DSReset(Mk64PrefetchBudget3DS* budget, size_t byteLimit,
                                size_t entryLimit) {
    if (budget == NULL) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    budget->byteLimit = byteLimit;
    budget->entryLimit = entryLimit < MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES
                             ? entryLimit
                             : MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES;
}

bool Mk64PrefetchBudget3DSContains(const Mk64PrefetchBudget3DS* budget,
                                   uint64_t resourceKey) {
    if (budget == NULL) {
        return false;
    }
    bool found = false;
    (void) Mk64PrefetchFindSlot3DS(budget, resourceKey, &found);
    return found;
}

Mk64PrefetchDecision3DS Mk64PrefetchBudget3DSTryReserve(Mk64PrefetchBudget3DS* budget,
                                                        uint64_t resourceKey,
                                                        size_t chargedBytes) {
    if (budget == NULL) {
        return MK64_PREFETCH_UNAVAILABLE;
    }
    bool found = false;
    const size_t hashSlot = Mk64PrefetchFindSlot3DS(budget, resourceKey, &found);
    if (found) {
        ++budget->duplicateEntries;
        return MK64_PREFETCH_DUPLICATE;
    }
    if (budget->seenEntries >= budget->entryLimit ||
        hashSlot >= MK64_3DS_TRACK_PREFETCH_HASH_SLOTS) {
        ++budget->entryLimitSkips;
        return MK64_PREFETCH_ENTRY_LIMIT;
    }

    budget->seenKeys[budget->seenEntries] = resourceKey;
    budget->seenSlots[hashSlot] = (uint16_t) (budget->seenEntries + 1u);
    ++budget->seenEntries;
    if (chargedBytes == 0) {
        ++budget->unavailableEntries;
        return MK64_PREFETCH_UNAVAILABLE;
    }
    if (budget->chargedBytes > budget->byteLimit ||
        chargedBytes > budget->byteLimit - budget->chargedBytes) {
        ++budget->byteLimitSkips;
        return MK64_PREFETCH_BYTE_LIMIT;
    }

    budget->chargedBytes += chargedBytes;
    ++budget->reservedEntries;
    return MK64_PREFETCH_RESERVE;
}
