#pragma once

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

namespace mk64_3ds {
// ZIP reads a local header and then its payload at nearby offsets. On SD,
// repeated small seek/read requests can cost milliseconds each. Keep two
// bounded read-ahead pages; large sequential reads bypass them. miniz still
// performs all decompression, bounds validation and CRC checks.
class ReadAheadFile {
  public:
    ~ReadAheadFile() { Close(); }
    bool Open(const char* path) {
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
        if (file) std::fclose(file);
        cached.clear(); cachedBytes = 0;
        resident.reset();
        file = nullptr; size = position = readCalls = readBytes = 0; next = 0;
        for (auto& page : pages) page.valid = 0;
    }
    std::uint64_t Size() const { return size; }
    std::uint64_t ReadCalls() const { return readCalls; }
    std::uint64_t ReadBytes() const { return readBytes; }
    void CloseBackingFile() { if (file) std::fclose(file); file = nullptr; cached.clear(); cachedBytes = 0; }
    bool IsResident() const { return resident != nullptr; }
    // Commit only after a complete sequential read. Closing the SD handle is
    // deliberate: successful residency cannot silently fall back to disk.
    bool MakeResident(std::size_t budget, void (*progress)(unsigned) = nullptr) {
        if (resident) return true;
        if (!file || size == 0 || size > budget || size > SIZE_MAX) return false;
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
        cached.clear(); cachedBytes = 0;
        return true;
    }
    std::size_t Read(std::uint64_t offset, void* destination, std::size_t length) {
        if ((!file && !resident) || !destination || offset >= size || length == 0) return 0;
        length = static_cast<std::size_t>(std::min<std::uint64_t>(length, size-offset));
        if (resident) {
            std::memcpy(destination, resident.get() + offset, length);
            return length;
        }
        auto range = std::upper_bound(cached.begin(), cached.end(), offset,
            [](std::uint64_t at, const CachedRange& item) { return at < item.offset; });
        if (range != cached.begin()) {
            --range;
            if (offset >= range->offset && offset - range->offset <= range->length &&
                length <= range->length - (offset - range->offset)) {
                std::memcpy(destination, range->bytes.get() + (offset - range->offset), length);
                return length;
            }
        }
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
    // Pin complete ZIP header/payload ranges while a race is loading. These
    // immutable compressed bytes do not retain decoded textures or GPU objects.
    bool CacheRange(std::uint64_t offset, std::size_t length, std::size_t budget) {
        if (offset > size || length == 0 || length > size - offset) return false;
        if (resident) return true;
        auto at = std::lower_bound(cached.begin(), cached.end(), offset,
            [](const CachedRange& item, std::uint64_t pos) { return item.offset < pos; });
        if (at != cached.end() && at->offset == offset) return at->length >= length;
        if (cachedBytes > budget || length > budget - cachedBytes) return false;
        CachedRange item;
        item.offset = offset; item.length = length;
        item.bytes.reset(new (std::nothrow) unsigned char[length]);
        if (!item.bytes || Read(offset, item.bytes.get(), length) != length) return false;
        try { cached.insert(at, std::move(item)); } catch (const std::bad_alloc&) { return false; }
        cachedBytes += length;
        return true;
    }
    std::size_t CachedBytes() const { return cachedBytes; }
  private:
    struct CachedRange {
        std::uint64_t offset = 0;
        std::size_t length = 0;
        std::unique_ptr<unsigned char[]> bytes;
    };
    std::vector<CachedRange> cached;
    std::unique_ptr<unsigned char[]> resident;
    std::size_t cachedBytes = 0;
  private:
    ReadAheadFile(const ReadAheadFile&) = delete;
    ReadAheadFile& operator=(const ReadAheadFile&) = delete;
  public:
    ReadAheadFile() = default;
  private:
    std::size_t ReadDirect(std::uint64_t offset, void* destination, std::size_t length) {
        if (offset > LONG_MAX) return 0;
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
