#pragma once

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
    const std::vector<std::string>& Entries() const;
    O2rReadResult ReadEntry(std::string_view entryPath, std::vector<std::uint8_t>* bytes);

    static O2rReadResult ListEntries(std::string_view archivePath, std::vector<std::string>* entries);
    static O2rReadResult ReadEntry(std::string_view archivePath, std::string_view entryPath,
                                   std::vector<std::uint8_t>* bytes);

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace mk64_3ds
