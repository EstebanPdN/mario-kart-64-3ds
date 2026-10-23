#include "texture_retention_3ds.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
struct Resource {
    bool live = true;
    bool retained = false;
    uint32_t retainStamp = 0;
    size_t allocatedBytes = 0;
    unsigned owners = 0;
};
std::array<Resource, 600> resources;
std::vector<uint32_t> evicted;

Resource* Lookup(uint32_t index) {
    return index < resources.size() && resources[index].live ? &resources[index] : nullptr;
}
bool Evict(uint32_t index, Resource& resource) {
    assert(!resource.retained);
    if (resource.owners != 0) return false;
    resource.live = false;
    evicted.push_back(index);
    return true;
}
void Reset(size_t bytes) {
    for (auto& resource : resources) resource = {true, false, 0, bytes, 0};
    evicted.clear();
}
size_t LiveRetainedBytes() {
    size_t bytes = 0;
    for (const auto& resource : resources) bytes += resource.live && resource.retained ? resource.allocatedBytes : 0;
    return bytes;
}
}

int main() {
    // Budget holds four 1 KiB textures; the oldest is evicted first.
    {
        Reset(1024);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        for (uint32_t i = 0; i < 4; ++i) assert(retention.Retain(i, resources[i], Lookup, Evict));
        assert(evicted.empty() && retention.Bytes() == 4096);
        assert(retention.Retain(4, resources[4], Lookup, Evict));
        assert(evicted.size() == 1 && evicted[0] == 0 && retention.Bytes() == 4096);
        assert(LiveRetainedBytes() == retention.Bytes());
    }
    // Re-releasing refreshes recency: a stale older slot must not evict it.
    {
        Reset(1024);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        for (uint32_t i = 0; i < 4; ++i) assert(retention.Retain(i, resources[i], Lookup, Evict));
        retention.Forget(resources[0]);
        ++resources[0].owners;  // Reacquired by the renderer.
        --resources[0].owners;
        assert(retention.Retain(0, resources[0], Lookup, Evict));  // Released again, now newest.
        assert(retention.Bytes() == 4096 && evicted.empty());
        assert(retention.Retain(4, resources[4], Lookup, Evict));
        assert(evicted.size() == 1 && evicted[0] == 1);
        assert(resources[0].live && resources[0].retained);
        assert(LiveRetainedBytes() == retention.Bytes());
    }
    // An owned resource is never freed; Forget keeps byte accounting exact.
    {
        Reset(1024);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        assert(retention.Retain(0, resources[0], Lookup, Evict));
        retention.Forget(resources[0]);
        resources[0].owners = 1;
        assert(retention.Bytes() == 0);
        assert(!retention.EvictOldest(0, 0, Lookup, Evict));
        assert(resources[0].live && evicted.empty() && retention.Slots() == 0);
    }
    // A resource freed elsewhere leaves a stale slot that is skipped safely.
    {
        Reset(1024);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        assert(retention.Retain(0, resources[0], Lookup, Evict));
        assert(retention.Retain(1, resources[1], Lookup, Evict));
        retention.Forget(resources[0]);
        resources[0].live = false;  // Track transition eviction.
        assert(retention.EvictOldest(0, 0, Lookup, Evict));
        assert(evicted.size() == 1 && evicted[0] == 1 && retention.Bytes() == 0 && retention.Slots() == 0);
    }
    // Large resources bypass retention and are evicted by the caller.
    {
        Reset(2048);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        resources[0].allocatedBytes = 4096 / 4 + 1;
        assert(!retention.Retain(0, resources[0], Lookup, Evict));
        assert(!resources[0].retained && retention.Bytes() == 0);
    }
    // Slot exhaustion with tiny entries evicts by count, never overflows.
    {
        Reset(1);
        mk64_3ds::TextureRetention3DS<16> retention(1U << 20);
        for (uint32_t i = 0; i < 100; ++i) assert(retention.Retain(i, resources[i], Lookup, Evict));
        assert(retention.Slots() <= 16 && evicted.size() == 84);
        assert(LiveRetainedBytes() == retention.Bytes());
    }
    // Repeated release of one hot texture never evicts it or grows bytes.
    {
        Reset(512);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        for (int frame = 0; frame < 1000; ++frame) {
            retention.Forget(resources[7]);
            assert(retention.Retain(7, resources[7], Lookup, Evict));
        }
        assert(evicted.empty() && retention.Bytes() == 512 && retention.Slots() <= 8);
    }
    // Reset drops bookkeeping (the runtime frees the resources itself).
    {
        Reset(1024);
        mk64_3ds::TextureRetention3DS<8> retention(4096);
        assert(retention.Retain(0, resources[0], Lookup, Evict));
        retention.Reset();
        assert(retention.Bytes() == 0 && retention.Slots() == 0);
    }
    std::puts("texture retention: ok");
    return 0;
}
