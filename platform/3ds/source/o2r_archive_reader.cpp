#include "o2r_archive_reader.hpp"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>

#include <miniz/zip_file.hpp>

namespace mk64_3ds {

struct O2rArchiveReader::Impl {
    explicit Impl(std::string path) : archivePath(std::move(path)) {
    }

    std::string archivePath;
    mz_zip_archive archive = {};
    std::vector<std::string> entries;
    bool open = false;
};

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
    if (!mz_zip_reader_init_file(&mImpl->archive, mImpl->archivePath.c_str(), 0)) {
        return O2rReadResult::OpenFailed;
    }
    mImpl->open = true;

    const mz_uint fileCount = mz_zip_reader_get_num_files(&mImpl->archive);
    mImpl->entries.clear();
    mImpl->entries.reserve(fileCount);
    for (mz_uint index = 0; index < fileCount; ++index) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&mImpl->archive, index, &stat)) {
            Close();
            return O2rReadResult::ReadFailed;
        }
        if (!mz_zip_reader_is_file_a_directory(&mImpl->archive, index)) {
            mImpl->entries.emplace_back(stat.m_filename);
        }
    }
    return O2rReadResult::Ok;
}

void O2rArchiveReader::Close() {
    if (mImpl->open) {
        mz_zip_reader_end(&mImpl->archive);
    }
    std::memset(&mImpl->archive, 0, sizeof(mImpl->archive));
    mImpl->entries.clear();
    mImpl->open = false;
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

    mz_zip_archive_file_stat stat = {};
    if (!mz_zip_reader_file_stat(&mImpl->archive, static_cast<mz_uint>(index), &stat) ||
        stat.m_uncomp_size > bytes->max_size()) {
        return O2rReadResult::ReadFailed;
    }

    bytes->resize(static_cast<size_t>(stat.m_uncomp_size));
    if (!mz_zip_reader_extract_to_mem(&mImpl->archive, static_cast<mz_uint>(index), bytes->data(), bytes->size(), 0)) {
        bytes->clear();
        return O2rReadResult::ReadFailed;
    }
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
