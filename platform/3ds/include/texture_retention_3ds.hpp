#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds {
// Bounded LRU of decoded archive resources that currently have no owner.
// Fast3D takes a short-lived owner for each texture lookup, so the last owner
// is often released while the same texture is used again on the next frame.
// Retaining recently released resources avoids re-inflating them every tick.
//
// Resource must expose `bool retained`, `uint32_t retainStamp` and
// `size_t allocatedBytes`. Queue slots are validated by stamp, so a resource
// that was reacquired, released again later or freed elsewhere is skipped.
template <size_t Capacity>
class TextureRetention3DS final {
  public:
    static_assert(Capacity > 1, "Retention needs at least two slots");

    explicit TextureRetention3DS(size_t budgetBytes) : budget_(budgetBytes) {}

    size_t Bytes() const { return bytes_; }
    size_t Slots() const { return count_; }
    size_t Budget() const { return budget_; }
    uint64_t StaleRecordsRemoved() const { return staleRemoved_; }
    uint64_t ByteEvictions() const { return byteEvictions_; }
    uint64_t CountEvictions() const { return countEvictions_; }
    void Reset() { head_ = count_ = bytes_ = 0; }

    // Call before a retained resource gains an owner or is freed.
    template <typename Resource>
    void Forget(Resource& resource) {
        if (!resource.retained) return;
        resource.retained = false;
        bytes_ -= resource.allocatedBytes < bytes_ ? resource.allocatedBytes : bytes_;
    }

    // Returns false when the resource is too large to retain; the caller then
    // evicts it immediately, exactly as before retention existed.
    template <typename Resource, typename Lookup, typename Evict>
    bool Retain(uint32_t index, Resource& resource, Lookup lookup, Evict evict) {
        if (resource.allocatedBytes > budget_ / 4U) return false;
        if (!resource.retained) {
            resource.retained = true;
            bytes_ += resource.allocatedBytes;
        }
        resource.retainStamp = ++stamp_;
        // A hot acquire/release cycle leaves an obsolete stamp behind. Remove
        // *all* obsolete stamps before count pressure can evict a live ownerless
        // resource at the head, even when that head is still the oldest one.
        if (count_ == Capacity) Compact(lookup);
        if (count_ == Capacity) EvictOldest(budget_, Capacity - 1U, lookup, evict);
        slots_[(head_ + count_) % Capacity] = {index, resource.retainStamp};
        ++count_;
        EvictOldest(budget_, Capacity, lookup, evict);
        return true;
    }

    // Evict the oldest valid entries until both limits hold. `evict` receives
    // a resource that is no longer retained and returns whether it was freed.
    template <typename Lookup, typename Evict>
    bool EvictOldest(size_t targetBytes, size_t targetSlots, Lookup lookup, Evict evict) {
        if (count_ > targetSlots) Compact(lookup);
        bool released = false;
        while (count_ != 0 && (bytes_ > targetBytes || count_ > targetSlots)) {
            const Slot oldest = slots_[head_];
            head_ = (head_ + 1U) % Capacity;
            --count_;
            auto* resource = lookup(oldest.index);
            if (resource == nullptr || !resource->retained || resource->retainStamp != oldest.stamp) continue;
            if (bytes_ > targetBytes) ++byteEvictions_; else ++countEvictions_;
            Forget(*resource);
            released = evict(oldest.index, *resource) || released;
        }
        return released;
    }

  private:
    template <typename Lookup>
    void Compact(Lookup lookup) {
        size_t live = 0;
        const auto count = count_;
        for (size_t i = 0; i < count; ++i) {
            const auto slot = slots_[(head_ + i) % Capacity];
            const auto* resource = lookup(slot.index);
            if (resource && resource->retained && resource->retainStamp == slot.stamp) {
                slots_[(head_ + live++) % Capacity] = slot;
            }
        }
        staleRemoved_ += count_ - live;
        count_ = live;
    }
    struct Slot {
        uint32_t index = 0;
        uint32_t stamp = 0;
    };
    std::array<Slot, Capacity> slots_{};
    size_t head_ = 0, count_ = 0, bytes_ = 0, budget_ = 0;
    uint32_t stamp_ = 0;
    uint64_t staleRemoved_ = 0, byteEvictions_ = 0, countEvictions_ = 0;
};
} // namespace mk64_3ds
