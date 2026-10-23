#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace mk64_3ds {

// Ranges retain registration order: the first containing blob defines the
// clamp, even when later registrations overlap it. A cached interval stops at
// every earlier range boundary, rather than incorrectly caching a whole blob.
class AudioDmaRangeCache {
  public:
    void Register(uintptr_t begin, size_t size) {
        if (!begin || !size || size > std::numeric_limits<uintptr_t>::max() - begin) return;
        const uintptr_t end = begin + size;
        for (size_t i = 0; i < count_; ++i)
            if (ranges_[i].begin == begin && ranges_[i].end == end) return;
        if (count_ == ranges_.size()) return;
        ranges_[count_++] = {begin, end};
        Invalidate();
    }

    size_t Clamp(uintptr_t address, size_t bytes) {
        auto& cached = cache_[(address >> 12) & (cache_.size() - 1)];
        if (cached.valid && address >= cached.begin && address < cached.end)
            return Limit(address, bytes, cached.ownerEnd);
        uintptr_t lower = 0;
        uintptr_t upper = std::numeric_limits<uintptr_t>::max();
        for (size_t i = 0; i < count_; ++i) {
            const auto& range = ranges_[i];
            if (address >= range.begin && address < range.end) {
                if (range.begin > lower) lower = range.begin;
                if (range.end < upper) upper = range.end;
                cached = {lower, upper, range.end, true};
                return Limit(address, bytes, range.end);
            }
            if (range.end <= address && range.end > lower) lower = range.end;
            if (range.begin > address && range.begin < upper) upper = range.begin;
        }
        // Unregistered addresses preserve the legacy copy size. UINTPTR_MAX
        // cannot be represented by a half-open interval and simply misses.
        cached = {lower, upper, 0, true};
        return bytes;
    }

    void Clear() { count_ = 0; Invalidate(); }

  private:
    struct Range { uintptr_t begin = 0, end = 0; };
    struct CachedRange {
        uintptr_t begin = 0, end = 0, ownerEnd = 0;
        bool valid = false;
    };
    static size_t Limit(uintptr_t address, size_t bytes, uintptr_t end) {
        return end && bytes > end - address ? end - address : bytes;
    }
    void Invalidate() { for (auto& entry : cache_) entry.valid = false; }
    std::array<Range, 1024> ranges_{};
    std::array<CachedRange, 8> cache_{};
    size_t count_ = 0;
};

} // namespace mk64_3ds
