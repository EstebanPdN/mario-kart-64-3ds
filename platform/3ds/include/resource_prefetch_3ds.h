#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MK64_3DS_TRACK_PREFETCH_MAX_BYTES (1024u * 1024u)
#define MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES 2048u
#define MK64_3DS_TRACK_PREFETCH_HASH_SLOTS 4096u

typedef enum Mk64PrefetchDecision3DS {
    MK64_PREFETCH_RESERVE = 0,
    MK64_PREFETCH_DUPLICATE,
    MK64_PREFETCH_BYTE_LIMIT,
    MK64_PREFETCH_ENTRY_LIMIT,
    MK64_PREFETCH_UNAVAILABLE,
} Mk64PrefetchDecision3DS;

typedef struct Mk64PrefetchBudget3DS {
    size_t byteLimit;
    size_t entryLimit;
    size_t chargedBytes;
    size_t seenEntries;
    size_t reservedEntries;
    size_t duplicateEntries;
    size_t byteLimitSkips;
    size_t entryLimitSkips;
    size_t unavailableEntries;
    uint64_t seenKeys[MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES];
    // Open-addressed indices keep duplicate checks constant-time without any
    // heap allocation during the race-start collision walk. Zero is empty;
    // stored values are seenKeys indices plus one.
    uint16_t seenSlots[MK64_3DS_TRACK_PREFETCH_HASH_SLOTS];
} Mk64PrefetchBudget3DS;

void Mk64PrefetchBudget3DSReset(Mk64PrefetchBudget3DS* budget, size_t byteLimit,
                                size_t entryLimit);
bool Mk64PrefetchBudget3DSContains(const Mk64PrefetchBudget3DS* budget,
                                   uint64_t resourceKey);
Mk64PrefetchDecision3DS Mk64PrefetchBudget3DSTryReserve(Mk64PrefetchBudget3DS* budget,
                                                        uint64_t resourceKey,
                                                        size_t chargedBytes);

#ifdef __cplusplus
}
#endif
