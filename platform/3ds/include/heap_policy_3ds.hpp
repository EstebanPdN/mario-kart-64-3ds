#pragma once

#include <cstddef>
#include <cstdint>

namespace mk64_3ds {
constexpr std::size_t kMiB = 1024u * 1024u;
constexpr std::size_t kArchiveReserveBytes = 8u * kMiB;
constexpr std::size_t kStreamingCacheBytes = 2u * kMiB;

// Use the process's actual application-memory limit, not an APT call before
// services exist. Mode 3 Old 3DS and memory-limited 3DSX launches get more
// ordinary heap; expanded New 3DS launches retain their existing GPU budget.
constexpr std::uint32_t LinearHeapBytesForCommitLimit(std::uint64_t limit) {
    return (limit != 0 && limit <= 80u * kMiB ? 20u : 24u) * kMiB;
}

constexpr std::size_t ArchiveResidencyBudget(std::size_t heap, std::size_t used,
                                            std::size_t replacing = 0) {
    // Compute after reclaiming the previous range, before clamping to zero.
    // Adding its size to an already-clamped budget would hide a reserve deficit.
    const auto retained = used - (replacing < used ? replacing : used);
    return heap > retained && heap - retained > kArchiveReserveBytes
        ? heap - retained - kArchiveReserveBytes : 0;
}
}
