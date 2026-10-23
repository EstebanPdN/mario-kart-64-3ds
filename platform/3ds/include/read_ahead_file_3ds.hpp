#pragma once

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <map>
#include <new>
#include <vector>

namespace mk64_3ds {
// ZIP reads a local header and then its payload at nearby offsets. On SD,
// repeated small seek/read requests can cost milliseconds each. Keep two
// bounded read-ahead pages; large sequential reads bypass them. miniz still
// performs all decompression, bounds validation and CRC checks.
class ReadAheadFile {
    struct MutationScope {
        ReadAheadFile& owner;
        bool& flag;
        MutationScope(ReadAheadFile& value, bool& active) : owner(value), flag(active) { flag = true; }
        ~MutationScope() {
            flag = false;
            if (owner.closePending && !owner.cacheMutation && !owner.residentMutation) {
                owner.closePending = false;
                owner.Close();
            }
        }
    };
  public:
    ~ReadAheadFile() { Close(); }
    bool Open(const char* path) {
        if (cacheMutation || residentMutation) return false;
        Close();
        file = std::fopen(path, "rb");
        if (!file) return false;
        std::setvbuf(file, nullptr, _IONBF, 0);
        if (std::fseek(file, 0, SEEK_END) != 0) { Close(); return false; }
        const long end = std::ftell(file);
        if (end < 0) { Close(); return false; }
        size = static_cast<std::uint64_t>(end);
        position = size;
        return true;
    }
    void Close() {
        if (cacheMutation || residentMutation) { closePending = true; return; }
        if (file) std::fclose(file);
        ClearCache();
        resident.reset();
        EndBulkRead();
        file = nullptr; size = position = readCalls = readBytes = 0; next = 0;
        for (auto& page : pages) page.valid = 0;
        cacheHits = cacheMisses = cacheByteEvictions = 0;
    }
    std::uint64_t Size() const { return size; }
    std::uint64_t ReadCalls() const { return readCalls; }
    std::uint64_t ReadBytes() const { return readBytes; }
    void CloseBackingFile() {
        if (cacheMutation || residentMutation) return;
        if (file) std::fclose(file); file = nullptr; ClearCache();
    }
    bool ReopenBackingFile(const char* path) {
        if (cacheMutation || residentMutation) return false;
        if (file) return true;
        if (!path || !path[0]) return false;
        FILE* reopened = std::fopen(path, "rb");
        if (!reopened) return false;
        std::setvbuf(reopened, nullptr, _IONBF, 0);
        if (std::fseek(reopened, 0, SEEK_END) != 0) {
            std::fclose(reopened);
            return false;
        }
        const long end = std::ftell(reopened);
        if (end < 0 || static_cast<std::uint64_t>(end) != size) {
            std::fclose(reopened);
            return false;
        }
        file = reopened;
        position = size;
        for (auto& page : pages) page.valid = 0;
        return true;
    }
    bool IsResident() const { return resident != nullptr; }
    // Startup walks tens of thousands of tiny ZIP entries. Amortize SD IPC
    // with one temporary large page while keeping compact payload residency.
    void BeginBulkRead() {
        bulk.reset(new (std::nothrow) unsigned char[kBulkSize]);
        bulkValid = 0;
    }
    void EndBulkRead() { bulk.reset(); bulkValid = 0; }
    // Commit only after a complete sequential read. Closing the SD handle is
    // deliberate: successful residency cannot silently fall back to disk.
    bool MakeResident(std::size_t budget, void (*progress)(unsigned) = nullptr) {
        if (cacheMutation || residentMutation) return false;
        if (resident) return true;
        if (!file || size == 0 || size > budget || size > SIZE_MAX) return false;
        MutationScope mutation(*this, residentMutation);
        std::unique_ptr<unsigned char[]> bytes(new (std::nothrow) unsigned char[static_cast<std::size_t>(size)]);
        if (!bytes) return false;
        if (progress) progress(0);
        for (std::size_t at = 0; at < size;) {
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(256u * 1024u, size - at));
            if (ReadDirect(at, bytes.get() + at, count) != count) return false;
            at += count;
            if (progress) progress(static_cast<unsigned>(at * 100ULL / size));
        }
        resident = std::move(bytes);
        std::fclose(file); file = nullptr;
        ClearCache();
        return true;
    }
    std::size_t Read(std::uint64_t offset, void* destination, std::size_t length) {
        if ((!file && !resident) || !destination || offset >= size || length == 0) return 0;
        length = static_cast<std::size_t>(std::min<std::uint64_t>(length, size-offset));
        if (resident) {
            std::memcpy(destination, resident.get() + offset, length);
            return length;
        }
        if (bulk) {
            auto* out = static_cast<unsigned char*>(destination);
            std::size_t done = 0;
            while (done < length) {
                const auto at = offset + done;
                if (bulkValid == 0 || at < bulkOffset || at - bulkOffset >= bulkValid) {
                    bulkOffset = at;
                    bulkValid = ReadDirect(at, bulk.get(), static_cast<std::size_t>(
                        std::min<std::uint64_t>(kBulkSize, size - at)));
                    if (bulkValid == 0) break;
                }
                const auto within = static_cast<std::size_t>(at - bulkOffset);
                const auto count = std::min(length - done, bulkValid - within);
                std::memcpy(out + done, bulk.get() + within, count);
                done += count;
            }
            return done;
        }
        auto range = cached.upper_bound(offset);
        if (range != cached.begin()) {
            --range;
            if (offset >= range->second.offset && offset - range->second.offset <= range->second.length &&
                length <= range->second.length - (offset - range->second.offset)) {
                std::memcpy(destination, range->second.bytes.get() + (offset - range->second.offset), length);
                Touch(range->second);
                ++cacheHits;
                return length;
            }
        }
        ++cacheMisses;
        if (length >= kPageSize) return ReadDirect(offset, destination, length);
        auto* out = static_cast<unsigned char*>(destination);
        std::size_t done = 0;
        while (done < length) {
            const std::uint64_t at = offset + done;
            const std::uint64_t base = at / kPageSize * kPageSize;
            Page* found = nullptr;
            for (auto& page : pages) {
                if (page.valid != 0 && page.offset == base) { found = &page; break; }
            }
            if (!found) {
                found = &pages[next]; next = (next + 1) % pages.size();
                found->offset = base;
                found->valid = ReadDirect(base, found->bytes.data(),
                    static_cast<std::size_t>(std::min<std::uint64_t>(kPageSize, size-base)));
            }
            const std::size_t within = static_cast<std::size_t>(at-base);
            if (within >= found->valid) break;
            const std::size_t count = std::min(length-done, found->valid-within);
            std::memcpy(out+done, found->bytes.data()+within, count);
            done += count;
        }
        return done;
    }
    // Stable indexed nodes hold immutable, non-overlapping ZIP ranges. Count
    // does not cap small palettes/lists before their byte allowance is used.
    // Charge tree bookkeeping as well as payload bytes to that same allowance.
    bool CacheRange(std::uint64_t offset, std::size_t length, std::size_t budget) {
        if (cacheMutation || residentMutation) return false;
        MutationScope mutation(*this, cacheMutation);
        TrimCacheUnsafe(budget);
        if (offset > size || !length || length > size - offset ||
            length > budget || kRangeOverhead > budget - length) return false;
        if (resident) return true;
        if (!file) return false;
        auto at = cached.lower_bound(offset);
        if (at != cached.end() && at->first == offset) {
            if (at->second.length < length) return false;
            Touch(at->second);
            return true;
        }
        // Overlap would make a predecessor lookup ambiguous. ZIP's immutable
        // local-header/payload ranges are disjoint; reject other callers safely.
        if (at != cached.end() && length > at->first - offset) return false;
        if (at != cached.begin()) {
            const auto& previous = std::prev(at)->second;
            if (offset - previous.offset < previous.length) return false;
        }
        const auto charge = length + kRangeOverhead;
        TrimCacheUnsafe(budget - charge);
        std::unique_ptr<unsigned char[]> bytes(new (std::nothrow) unsigned char[length]);
        if (!bytes || Read(offset, bytes.get(), length) != length) return false;
        try {
            auto result = cached.try_emplace(offset);
            if (!result.second) return false;
            auto& item = result.first->second;
            item.offset = offset; item.length = length; item.bytes = std::move(bytes);
            cachedBytes += length;
            Touch(item);
        } catch (const std::bad_alloc&) { return false; }
        return true;
    }
    std::size_t CachedBytes() const { return cachedBytes; }
    std::size_t CachedRanges() const { return cached.size(); }
    std::size_t CacheAllocationBytes() const { return cachedBytes + cached.size()*kRangeOverhead; }
    std::uint64_t CacheHits() const { return cacheHits; }
    std::uint64_t CacheMisses() const { return cacheMisses; }
    std::uint64_t CacheByteEvictions() const { return cacheByteEvictions; }
    bool TryReclaimCache() {
        if (cacheMutation || residentMutation || cached.empty()) return false;
        ClearCacheUnsafe();
        return true;
    }
    void ClearCache() { if (!cacheMutation) ClearCacheUnsafe(); }
    void TrimCache(std::size_t budget) { if (!cacheMutation) TrimCacheUnsafe(budget); }
  private:
    struct CachedRange {
        std::uint64_t offset = 0;
        std::size_t length = 0;
        std::unique_ptr<unsigned char[]> bytes;
        CachedRange* previous = nullptr;
        CachedRange* next = nullptr;
    };
    // Red/black tree node: three links and aligned color, plus its key.
    // Include slack for both allocations' heap bookkeeping and rounding.
    static constexpr std::size_t kRangeOverhead = sizeof(CachedRange) + sizeof(std::uint64_t) + 4*sizeof(void*) + 32;
    void Unlink(CachedRange& range) {
        if (range.previous) range.previous->next = range.next; else oldest = range.next;
        if (range.next) range.next->previous = range.previous; else newest = range.previous;
        range.previous = range.next = nullptr;
    }
    void Touch(CachedRange& range) {
        if (newest == &range) return;
        if (range.previous || range.next || oldest == &range) Unlink(range);
        range.previous = newest;
        if (newest) newest->next = &range; else oldest = &range;
        newest = &range;
    }
    void EvictOldest() {
        if (!oldest) return;
        const auto offset = oldest->offset;
        cachedBytes -= oldest->length;
        Unlink(*oldest);
        cached.erase(offset);
        ++cacheByteEvictions;
    }
    void ClearCacheUnsafe() {
        oldest = newest = nullptr;
        cached.clear(); cachedBytes = 0;
    }
    void TrimCacheUnsafe(std::size_t budget) {
        while (!cached.empty() && CacheAllocationBytes() > budget) EvictOldest();
    }
    std::map<std::uint64_t, CachedRange> cached;
    CachedRange* oldest = nullptr;
    CachedRange* newest = nullptr;
    bool cacheMutation = false, residentMutation = false, closePending = false;
    std::uint64_t cacheHits = 0, cacheMisses = 0, cacheByteEvictions = 0;
    std::unique_ptr<unsigned char[]> resident;
    std::size_t cachedBytes = 0;
    static constexpr std::size_t kBulkSize = 256u * 1024u;
    std::unique_ptr<unsigned char[]> bulk;
    std::uint64_t bulkOffset = 0;
    std::size_t bulkValid = 0;
  private:
    ReadAheadFile(const ReadAheadFile&) = delete;
    ReadAheadFile& operator=(const ReadAheadFile&) = delete;
  public:
    ReadAheadFile() = default;
  private:
    std::size_t ReadDirect(std::uint64_t offset, void* destination, std::size_t length) {
        if (!file || offset > LONG_MAX) return 0;
        if (position != offset && std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) return 0;
        ++readCalls;
        const std::size_t count = std::fread(destination, 1, length, file);
        position = offset+count;
        readBytes += count;
        return count;
    }
    static constexpr std::size_t kPageSize = 16384;
    struct Page {
        std::array<unsigned char, kPageSize> bytes;
        std::uint64_t offset = 0;
        std::size_t valid = 0;
    };
    std::array<Page, 2> pages;
    std::size_t next = 0;
    FILE* file = nullptr;
    std::uint64_t size = 0, position = 0, readCalls = 0, readBytes = 0;
};
}
