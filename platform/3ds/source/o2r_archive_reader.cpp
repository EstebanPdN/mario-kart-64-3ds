#include "o2r_archive_reader.hpp"
#include "read_ahead_file_3ds.hpp"
#ifdef __3DS__
#include "performance_trace_3ds.hpp"
#endif

#include <cassert>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <miniz/zip_file.hpp>

namespace mk64_3ds {

struct O2rArchiveReader::Impl {
    explicit Impl(std::string path) : archivePath(std::move(path)) {
    }

    std::string archivePath;
    mz_zip_archive archive = {};
    ReadAheadFile file;
    std::vector<std::string> entries;
    std::vector<mz_uint> archiveIndices;
    std::vector<std::unique_ptr<std::uint8_t[]>> residentBlocks;
    std::vector<const std::uint8_t*> residentEntries;
    bool resident = false;
    O2rResidentResult residentResult = O2rResidentResult::NotOpen;
    size_t residentRequired = 0;
    bool open = false;
};

namespace {

O2rReadResult ExtractEntry(mz_zip_archive* archive, mz_uint archiveIndex,
                           std::vector<std::uint8_t>* bytes, const std::uint8_t* compressed = nullptr) {
    mz_zip_archive_file_stat stat = {};
    if (!mz_zip_reader_file_stat(archive, archiveIndex, &stat) ||
        stat.m_uncomp_size > bytes->max_size()) {
        return O2rReadResult::ReadFailed;
    }

    bytes->resize(static_cast<size_t>(stat.m_uncomp_size));
    if (bytes->empty()) {
        return O2rReadResult::Ok;
    }
    if (compressed != nullptr) {
        bool ok = false;
        if (stat.m_method == 0 && stat.m_comp_size == stat.m_uncomp_size) {
            std::memcpy(bytes->data(), compressed, bytes->size());
            ok = true;
        } else if (stat.m_method == MZ_DEFLATED) {
            ok = tinfl_decompress_mem_to_mem(bytes->data(), bytes->size(), compressed,
                                            static_cast<size_t>(stat.m_comp_size), 0) == bytes->size();
        }
        if (ok && mz_crc32(MZ_CRC32_INIT, bytes->data(), bytes->size()) == stat.m_crc32)
            return O2rReadResult::Ok;
        bytes->clear();
        return O2rReadResult::ReadFailed;
    }
    if (!mz_zip_reader_extract_to_mem(archive, archiveIndex, bytes->data(), bytes->size(), 0)) {
        bytes->clear();
        return O2rReadResult::ReadFailed;
    }
    return O2rReadResult::Ok;
}

} // namespace

O2rArchiveReader::O2rArchiveReader(std::string archivePath)
    : mImpl(std::make_unique<Impl>(std::move(archivePath))) {
}

O2rArchiveReader::~O2rArchiveReader() {
    Close();
}

O2rReadResult O2rArchiveReader::Open() {
    if (mImpl->open) {
        return O2rReadResult::Ok;
    }
    if (mImpl->archivePath.empty()) {
        return O2rReadResult::InvalidArgument;
    }

    std::memset(&mImpl->archive, 0, sizeof(mImpl->archive));
    if (!mImpl->file.Open(mImpl->archivePath.c_str())) return O2rReadResult::OpenFailed;
    mImpl->archive.m_pIO_opaque = &mImpl->file;
    mImpl->archive.m_pRead = [](void* opaque, mz_uint64 offset, void* out, size_t size) -> size_t {
        auto* file = static_cast<ReadAheadFile*>(opaque);
#ifdef __3DS__
        const auto start = PerformanceNow();
        const auto calls = file->ReadCalls();
        const auto bytes = file->ReadBytes();
#endif
        const size_t count = file->Read(offset, out, size);
#ifdef __3DS__
        PerformanceArchiveRead(start, static_cast<std::uint32_t>(file->ReadCalls()-calls),
                               static_cast<std::uint32_t>(file->ReadBytes()-bytes));
#endif
        return count;
    };
    if (!mz_zip_reader_init(&mImpl->archive, mImpl->file.Size(), 0)) {
        mImpl->file.Close();
        return O2rReadResult::OpenFailed;
    }
    mImpl->open = true;

    const mz_uint fileCount = mz_zip_reader_get_num_files(&mImpl->archive);
    mImpl->entries.clear();
    mImpl->archiveIndices.clear();
    mImpl->entries.reserve(fileCount);
    mImpl->archiveIndices.reserve(fileCount);
    for (mz_uint index = 0; index < fileCount; ++index) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&mImpl->archive, index, &stat)) {
            Close();
            return O2rReadResult::ReadFailed;
        }
        if (!mz_zip_reader_is_file_a_directory(&mImpl->archive, index)) {
            mImpl->entries.emplace_back(stat.m_filename);
            mImpl->archiveIndices.emplace_back(index);
        }
    }
    return O2rReadResult::Ok;
}

void O2rArchiveReader::Close() {
    if (mImpl->open) {
        mz_zip_reader_end(&mImpl->archive);
    }
    std::memset(&mImpl->archive, 0, sizeof(mImpl->archive));
    mImpl->file.Close();
    mImpl->residentBlocks.clear();
    mImpl->residentEntries.clear();
    mImpl->resident = false;
    mImpl->residentResult = O2rResidentResult::NotOpen;
    mImpl->residentRequired = 0;
    mImpl->entries.clear();
    mImpl->archiveIndices.clear();
    mImpl->open = false;
}

std::size_t O2rArchiveReader::CachedBytes() const { return mImpl->file.CachedBytes(); }
bool O2rArchiveReader::MakeResident(std::size_t budget, void (*progress)(unsigned)) {
    if (!mImpl->open) return false;
    if (mImpl->resident) return true;
    mImpl->residentResult = O2rResidentResult::AllocationFailed;
    try {
        // Keep each compressed entry contiguous, but never require one giant
        // allocation for the entire archive. Old 3DS can have enough total
        // free memory split across smaller holes after startup resource loads.
        constexpr size_t blockLimit = 256u * 1024u;
        const auto count = mImpl->archiveIndices.size();
        if (count > SIZE_MAX / sizeof(const uint8_t*)) {
            mImpl->residentResult = O2rResidentResult::ReadFailed; return false;
        }
        size_t total = count * sizeof(const uint8_t*);
        size_t blockCount = 0, blockBytes = 0;
        for (size_t i = 0; i < count; ++i) {
            mz_zip_archive_file_stat stat = {};
            if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[i], &stat) ||
                stat.m_comp_size > SIZE_MAX - total) {
                mImpl->residentResult = O2rResidentResult::ReadFailed;
                return false;
            }
            // Zero-size entries still get an address (they need no payload).
            const size_t length = std::max<size_t>(1, stat.m_comp_size);
            if (length > SIZE_MAX - total) {
                mImpl->residentResult = O2rResidentResult::ReadFailed; return false;
            }
            if (blockBytes && length > blockLimit - std::min(blockLimit, blockBytes)) {
                ++blockCount; blockBytes = 0;
            }
            blockBytes += length;
            total += length;
        }
        if (blockBytes) ++blockCount;
        if (blockCount > (SIZE_MAX - total) / sizeof(std::unique_ptr<uint8_t[]>)) {
            mImpl->residentResult = O2rResidentResult::ReadFailed;
            return false;
        }
        total += blockCount * sizeof(std::unique_ptr<uint8_t[]>);
        mImpl->residentRequired = total;
        if (total > budget) {
            mImpl->residentResult = O2rResidentResult::BudgetExceeded;
            return false;
        }
        std::vector<const uint8_t*> entries(count);
        std::vector<std::unique_ptr<uint8_t[]>> blocks;
        blocks.reserve(blockCount);
        struct BulkReadScope {
            ReadAheadFile& file;
            explicit BulkReadScope(ReadAheadFile& value) : file(value) { file.BeginBulkRead(); }
            ~BulkReadScope() { file.EndBulkRead(); }
        } bulkRead(mImpl->file);
        if (progress) progress(0);
        for (size_t first = 0; first < count;) {
            size_t end = first, bytes = 0;
            while (end < count) {
                mz_zip_archive_file_stat stat = {};
                if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[end], &stat)) {
                    mImpl->residentResult = O2rResidentResult::ReadFailed; return false;
                }
                const size_t length = std::max<size_t>(1, stat.m_comp_size);
                if (end > first && length > blockLimit - std::min(blockLimit, bytes)) break;
                bytes += length; ++end;
            }
            std::unique_ptr<uint8_t[]> block(new (std::nothrow) uint8_t[bytes]);
            if (!block) return false;
            size_t offset = 0;
            for (size_t i = first; i < end; ++i) {
                mz_zip_archive_file_stat stat = {};
                if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[i], &stat) ||
                    !mz_zip_reader_extract_to_mem(&mImpl->archive, mImpl->archiveIndices[i],
                        block.get() + offset, stat.m_comp_size, MZ_ZIP_FLAG_COMPRESSED_DATA)) {
                    mImpl->residentResult = O2rResidentResult::ReadFailed; return false;
                }
                entries[i] = block.get() + offset;
                offset += std::max<size_t>(1, stat.m_comp_size);
                if (progress) progress(static_cast<unsigned>((i + 1) * 100ULL / count));
            }
            blocks.push_back(std::move(block));
            first = end;
        }
        mImpl->residentBlocks = std::move(blocks);
        mImpl->residentEntries = std::move(entries);
        mImpl->resident = true;
        mImpl->residentResult = O2rResidentResult::Ready;
        mImpl->file.CloseBackingFile();
        return true;
    } catch (const std::bad_alloc&) { return false; }
}
bool O2rArchiveReader::IsResident() const { return mImpl->resident; }
O2rResidentResult O2rArchiveReader::ResidentResult() const { return mImpl->residentResult; }
std::size_t O2rArchiveReader::ResidentRequiredBytes() const { return mImpl->residentRequired; }
std::uint64_t O2rArchiveReader::ArchiveBytes() const { return mImpl->file.Size(); }
std::uint64_t O2rArchiveReader::PhysicalReadCalls() const { return mImpl->file.ReadCalls(); }
std::uint64_t O2rArchiveReader::PhysicalReadBytes() const { return mImpl->file.ReadBytes(); }

bool O2rArchiveReader::CacheEntryByIndex(std::size_t entryIndex, std::size_t budget) {
    if (!mImpl->open || entryIndex >= mImpl->archiveIndices.size()) return false;
    if (mImpl->resident) return true;
    mz_zip_archive_file_stat stat = {};
    if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[entryIndex], &stat)) return false;
    unsigned char header[30];
    if (mImpl->file.Read(stat.m_local_header_ofs, header, sizeof(header)) != sizeof(header) ||
        header[0] != 'P' || header[1] != 'K' || header[2] != 3 || header[3] != 4) return false;
    const std::uint64_t length = 30ULL + header[26] + (header[27] << 8U) +
        header[28] + (header[29] << 8U) + stat.m_comp_size;
    if (length > SIZE_MAX) return false;
    return mImpl->file.CacheRange(stat.m_local_header_ofs, static_cast<std::size_t>(length), budget);
}

bool O2rArchiveReader::IsOpen() const {
    return mImpl->open;
}

const std::vector<std::string>& O2rArchiveReader::Entries() const {
    return mImpl->entries;
}

O2rReadResult O2rArchiveReader::ReadEntry(std::string_view entryPath, std::vector<std::uint8_t>* bytes) {
    if (bytes == nullptr || entryPath.empty()) {
        return O2rReadResult::InvalidArgument;
    }
    if (!mImpl->open) {
        const O2rReadResult openResult = Open();
        if (openResult != O2rReadResult::Ok) {
            return openResult;
        }
    }

    bytes->clear();
    const std::string expectedPath(entryPath);
    const int index = mz_zip_reader_locate_file(&mImpl->archive, expectedPath.c_str(), nullptr, 0);
    if (index < 0) {
        return O2rReadResult::EntryNotFound;
    }

    if (mImpl->resident) {
        const auto found = std::lower_bound(mImpl->archiveIndices.begin(), mImpl->archiveIndices.end(), static_cast<mz_uint>(index));
        if (found == mImpl->archiveIndices.end() || *found != static_cast<mz_uint>(index)) return O2rReadResult::EntryNotFound;
        return ReadEntryByIndex(static_cast<size_t>(found - mImpl->archiveIndices.begin()), bytes);
    }
    return ExtractEntry(&mImpl->archive, static_cast<mz_uint>(index), bytes);
}

O2rReadResult O2rArchiveReader::ReadEntryByIndex(std::size_t entryIndex,
                                                 std::vector<std::uint8_t>* bytes) {
    if (bytes == nullptr) {
        return O2rReadResult::InvalidArgument;
    }
    if (!mImpl->open) {
        const O2rReadResult openResult = Open();
        if (openResult != O2rReadResult::Ok) {
            return openResult;
        }
    }

    bytes->clear();
    if (entryIndex >= mImpl->archiveIndices.size()) {
        return O2rReadResult::EntryNotFound;
    }
    return ExtractEntry(&mImpl->archive, mImpl->archiveIndices[entryIndex], bytes,
        mImpl->resident ? mImpl->residentEntries[entryIndex] : nullptr);
}

O2rReadResult O2rArchiveReader::GetEntryUncompressedSizeByIndex(std::size_t entryIndex,
                                                                std::size_t* byteCount) {
    if (byteCount == nullptr) {
        return O2rReadResult::InvalidArgument;
    }
    *byteCount = 0;
    if (!mImpl->open) {
        const O2rReadResult openResult = Open();
        if (openResult != O2rReadResult::Ok) {
            return openResult;
        }
    }
    if (entryIndex >= mImpl->archiveIndices.size()) {
        return O2rReadResult::EntryNotFound;
    }

    mz_zip_archive_file_stat stat = {};
    if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[entryIndex], &stat) ||
        stat.m_uncomp_size > std::numeric_limits<std::size_t>::max()) {
        return O2rReadResult::ReadFailed;
    }
    *byteCount = static_cast<std::size_t>(stat.m_uncomp_size);
    return O2rReadResult::Ok;
}

O2rReadResult O2rArchiveReader::ListEntries(std::string_view archivePath, std::vector<std::string>* entries) {
    if (entries == nullptr || archivePath.empty()) {
        return O2rReadResult::InvalidArgument;
    }
    O2rArchiveReader archive{std::string(archivePath)};
    const O2rReadResult result = archive.Open();
    if (result != O2rReadResult::Ok) {
        return result;
    }
    *entries = archive.Entries();
    return O2rReadResult::Ok;
}

O2rReadResult O2rArchiveReader::ReadEntry(std::string_view archivePath, std::string_view entryPath,
                                          std::vector<std::uint8_t>* bytes) {
    O2rArchiveReader archive{std::string(archivePath)};
    return archive.ReadEntry(entryPath, bytes);
}

} // namespace mk64_3ds
