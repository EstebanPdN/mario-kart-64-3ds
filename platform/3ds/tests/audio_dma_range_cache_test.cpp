#include "audio_dma_range_cache_3ds.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>
#include <vector>

int main() {
    mk64_3ds::AudioDmaRangeCache cache;
    std::vector<std::pair<uintptr_t, uintptr_t>> reference;
    auto add = [&](uintptr_t start, size_t length) {
        cache.Register(start, length);
        if (!start || !length || length > UINTPTR_MAX - start) return;
        const auto range = std::make_pair(start, start + length);
        if (reference.size() < 1024 &&
            std::find(reference.begin(), reference.end(), range) == reference.end())
            reference.push_back(range);
    };
    auto check = [&](uintptr_t address, size_t bytes) {
        size_t expected = bytes;
        for (auto r : reference) if (address >= r.first && address < r.second) {
            expected = std::min(bytes, r.second - address); break;
        }
        assert(cache.Clamp(address, bytes) == expected);
    };
    // Later large ranges cannot bypass an earlier short overlapping range.
    add(300, 10); add(100, 900); add(350, 10); add(300, 10);
    for (uintptr_t a = 0; a < 1100; ++a) {
        check(a, 4096); check(a, 0); check(a, 1);
    }
    // A miss must be invalidated when a new blob covers the cached hole.
    check(2000, 100); add(1990, 20); check(2000, 100);
    add(UINTPTR_MAX - 64, 64); add(UINTPTR_MAX - 8, 16);
    check(UINTPTR_MAX - 1, SIZE_MAX); check(UINTPTR_MAX, SIZE_MAX);
    add(0, 10); add(500, 0);
    std::mt19937_64 rng(0xE3D4A);
    for (size_t round = 0; round < 8; ++round) {
        cache.Clear(); reference.clear();
        for (size_t i = 0; i < 1100; ++i) {
            const uintptr_t base = 1 + rng() % (1u << 20);
            add(base, 1 + rng() % 8192);
            for (size_t j = 0; j < 12; ++j) {
                check(base + j * 16, rng() % 16384);
                check(rng() % (1u << 20), rng() % 16384);
            }
        }
        for (size_t i = 0; i < 50000; ++i) check(rng() % (1u << 20), rng() % 16384);
    }
    cache.Clear(); reference.clear();
    check(300, 9999); add(300, 5); check(300, 9999);
    std::puts("PASS: DMA cache preserves first-registration clamps, holes, overlaps, capacity, overflow and clear");
}
