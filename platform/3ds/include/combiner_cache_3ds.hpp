#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds {
// CPU-side configurations, not GPU programs or textures. No stock scene
// needs an unbounded history of configurations from every previous race.
inline constexpr std::size_t kCombinerCacheCapacity3DS = 256;
inline std::atomic<std::uint32_t> sCombinerCacheEntries3DS{0};
inline std::atomic<std::uint32_t> sCombinerCacheResets3DS{0};

inline void PublishCombinerCacheSize3DS(std::size_t entries) {
    sCombinerCacheEntries3DS.store(static_cast<std::uint32_t>(entries),
                                 std::memory_order_relaxed);
}
inline void RecordCombinerCacheLimitReset3DS() {
    sCombinerCacheResets3DS.fetch_add(1, std::memory_order_relaxed);
}
// Diagnostics run on another thread: never inspect the live std::map there.
inline std::uint32_t CombinerCacheEntries3DS() {
    return sCombinerCacheEntries3DS.load(std::memory_order_relaxed);
}
inline std::uint32_t CombinerCacheResets3DS() {
    return sCombinerCacheResets3DS.load(std::memory_order_relaxed);
}
} // namespace mk64_3ds
