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
    std::unique_ptr<std::uint8_t[]> resident;
    std::vector<std::size_t> residentOffsets;
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
    mImpl->resident.reset();
    mImpl->residentOffsets.clear();
    mImpl->entries.clear();
    mImpl->archiveIndices.clear();
    mImpl->open = false;
}

std::size_t O2rArchiveReader::CachedBytes() const { return mImpl->file.CachedBytes(); }
bool O2rArchiveReader::MakeResident(std::size_t budget, void (*progress)(unsigned)) {
    if (!mImpl->open) return false;
    if (mImpl->resident) return true;
    try {
        // The central directory is already resident in miniz. Retain only
        // compressed payloads here, avoiding a second copy of ZIP names and
        // headers. This fits the supported Old-model 80MB launch mode.
        const auto count = mImpl->archiveIndices.size();
        if (count > budget / sizeof(size_t)) return false;
        std::vector<size_t> offsets(count);
        size_t total = 0;
        for (size_t i = 0; i < count; ++i) {
            mz_zip_archive_file_stat stat = {};
            if (!mz_zip_reader_file_stat(&mImpl->archive, mImpl->archiveIndices[i], &stat) ||
                stat.m_comp_size > budget - count * sizeof(size_t) - total) return false;
            offsets[i] = total;
            total += static_cast<size_t>(stat.m_comp_size);
        }
        std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[std::max<size_t>(1, total)]);
        if (!data) return false;
        if (progress) progress(0);
        for (size_t i = 0; i < count; ++i) {
            const size_t length = (i + 1 < count ? offsets[i + 1] : total) - offsets[i];
            if (!mz_zip_reader_extract_to_mem(&mImpl->archive, mImpl->archiveIndices[i],
                                             data.get() + offsets[i], length, MZ_ZIP_FLAG_COMPRESSED_DATA)) return false;
            if (progress) progress(static_cast<unsigned>((i + 1) * 100ULL / count));
        }
        mImpl->resident = std::move(data);
        mImpl->residentOffsets = std::move(offsets);
        mImpl->file.CloseBackingFile();
        return true;
    } catch (const std::bad_alloc&) { return false; }
}
bool O2rArchiveReader::IsResident() const { return mImpl->resident != nullptr; }
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
        mImpl->resident ? mImpl->resident.get() + mImpl->residentOffsets[entryIndex] : nullptr);
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
