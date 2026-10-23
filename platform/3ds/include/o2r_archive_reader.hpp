#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mk64_3ds {

enum class O2rReadResult : std::uint8_t {
    Ok,
    InvalidArgument,
    OpenFailed,
    EntryNotFound,
    ReadFailed,
};

enum class O2rResidentResult : std::uint8_t {
    Ready, Released, NotOpen, BudgetExceeded, AllocationFailed, ReadFailed
};

// Read-only ZIP access for an owner-supplied SpaghettiKart .o2r archive.
// The game archive stays on the SD card and is never embedded in the app.
class O2rArchiveReader final {
  public:
    explicit O2rArchiveReader(std::string archivePath);
    ~O2rArchiveReader();

    O2rArchiveReader(const O2rArchiveReader&) = delete;
    O2rArchiveReader& operator=(const O2rArchiveReader&) = delete;

    O2rReadResult Open();
    void Close();
    bool IsOpen() const;
    bool MakeResident(std::size_t budget, void (*progress)(unsigned) = nullptr);
    bool DropResident();
    bool IsResident() const;
    O2rResidentResult ResidentResult() const;
    std::size_t ResidentRequiredBytes() const;
    std::uint64_t ArchiveBytes() const;
    std::uint64_t PhysicalReadCalls() const;
    std::uint64_t PhysicalReadBytes() const;
    bool CacheEntryByIndex(std::size_t entryIndex, std::size_t budget);
    // Loading-boundary preparation for exact course/kart/effect dependencies.
    // Shares the automatic compressed LRU; never allocates a second cache.
    std::size_t CacheEntries(const char* const* names, std::size_t count, std::size_t budget);
    std::size_t CachedBytes() const;
    struct CacheStats {
        std::size_t entries, payloadBytes, allocationBytes;
        std::uint64_t hits, misses, byteEvictions;
    };
    CacheStats StreamingCacheStats() const;
    void SetStreamingCacheBudget(std::size_t budget);
    bool ReclaimPerformanceCache();
    const std::vector<std::string>& Entries() const;
    O2rReadResult ReadEntry(std::string_view entryPath, std::vector<std::uint8_t>* bytes);
    O2rReadResult ReadEntryByIndex(std::size_t entryIndex, std::vector<std::uint8_t>* bytes);
    O2rReadResult GetEntryUncompressedSizeByIndex(std::size_t entryIndex,
                                                  std::size_t* byteCount);

    static O2rReadResult ListEntries(std::string_view archivePath, std::vector<std::string>* entries);
    static O2rReadResult ReadEntry(std::string_view archivePath, std::string_view entryPath,
                                   std::vector<std::uint8_t>* bytes);

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace mk64_3ds
