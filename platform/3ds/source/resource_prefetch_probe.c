#include "resource_prefetch_3ds.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    Mk64PrefetchBudget3DS budget;
    Mk64PrefetchBudget3DSReset(&budget, MK64_3DS_TRACK_PREFETCH_MAX_BYTES,
                               MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);

    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 1, 4096) == MK64_PREFETCH_RESERVE);
    assert(Mk64PrefetchBudget3DSContains(&budget, 1));
    assert(!Mk64PrefetchBudget3DSContains(&budget, 2));
    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 1, 4096) == MK64_PREFETCH_DUPLICATE);
    assert(budget.chargedBytes == 4096);
    assert(budget.reservedEntries == 1);
    assert(budget.seenEntries == 1);

    assert(Mk64PrefetchBudget3DSTryReserve(
               &budget, 2, MK64_3DS_TRACK_PREFETCH_MAX_BYTES - 4096) ==
           MK64_PREFETCH_RESERVE);
    assert(budget.chargedBytes == MK64_3DS_TRACK_PREFETCH_MAX_BYTES);
    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 3, 1) == MK64_PREFETCH_BYTE_LIMIT);
    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 4, 0) == MK64_PREFETCH_UNAVAILABLE);
    assert(budget.chargedBytes == MK64_3DS_TRACK_PREFETCH_MAX_BYTES);

    Mk64PrefetchBudget3DSReset(&budget, SIZE_MAX, MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);
    for (uint64_t key = 1; key <= MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES; ++key) {
        assert(Mk64PrefetchBudget3DSTryReserve(&budget, key, 1) == MK64_PREFETCH_RESERVE);
    }
    assert(Mk64PrefetchBudget3DSTryReserve(
               &budget, MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES + 1u, 1) ==
           MK64_PREFETCH_ENTRY_LIMIT);
    assert(budget.seenEntries == MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);
    assert(budget.reservedEntries == MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);
    assert(budget.chargedBytes == MK64_3DS_TRACK_PREFETCH_MAX_ENTRIES);

    // Zero and clustered values exercise the index-plus-one sentinel and
    // open-addressing collision path.
    Mk64PrefetchBudget3DSReset(&budget, SIZE_MAX, 8);
    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 0, 1) == MK64_PREFETCH_RESERVE);
    for (uint64_t key = 0x1000; key <= 0x7000; key += 0x1000) {
        assert(Mk64PrefetchBudget3DSTryReserve(&budget, key, 1) == MK64_PREFETCH_RESERVE);
        assert(Mk64PrefetchBudget3DSContains(&budget, key));
    }
    assert(Mk64PrefetchBudget3DSContains(&budget, 0));
    assert(Mk64PrefetchBudget3DSTryReserve(&budget, 0, 1) == MK64_PREFETCH_DUPLICATE);

    puts("resource prefetch policy: ok");
    return 0;
}
