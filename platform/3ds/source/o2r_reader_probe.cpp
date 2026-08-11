#include "game_data_archive_3ds.hpp"
#include "o2r_archive_reader.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace {

struct ValidationProgressState {
    std::size_t previousCompleted = 0;
    std::size_t totalEntries = 0;
    std::size_t callbackCount = 0;
    bool regressed = false;
    bool totalChanged = false;
};

void RecordValidationProgress(std::size_t completedEntries, std::size_t totalEntries,
                              void* userData) {
    auto* state = static_cast<ValidationProgressState*>(userData);
    if (state == nullptr) return;
    if (state->callbackCount != 0 && completedEntries < state->previousCompleted) {
        state->regressed = true;
    }
    if (state->callbackCount != 0 && totalEntries != state->totalEntries) {
        state->totalChanged = true;
    }
    state->previousCompleted = completedEntries;
    state->totalEntries = totalEntries;
    ++state->callbackCount;
}

uint32_t ReadU32(const std::uint8_t* bytes, bool bigEndian) {
    if (bigEndian) {
        return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
    }
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

void PrintType(uint32_t type) {
    char code[5] = {
        static_cast<char>((type >> 24) & 0xFF),
        static_cast<char>((type >> 16) & 0xFF),
        static_cast<char>((type >> 8) & 0xFF),
        static_cast<char>(type & 0xFF),
        '\0',
    };
    std::printf("%s", code);
}

} // namespace

int main(int argc, char** argv) {
    bool printTypes = false;
    bool checkIndexParity = false;
    bool verifyAllEntries = false;
    for (int argument = 2; argument < argc; ++argument) {
        const std::string option(argv[argument]);
        if (option == "--types") {
            printTypes = true;
        } else if (option == "--index-parity") {
            checkIndexParity = true;
        } else if (option == "--full") {
            verifyAllEntries = true;
        } else {
            std::fprintf(stderr,
                         "usage: %s <archive.o2r> [--types] [--index-parity] [--full]\n",
                         argv[0]);
            return 2;
        }
    }
    if (argc < 2 || argc > 5) {
        std::fprintf(stderr,
                     "usage: %s <archive.o2r> [--types] [--index-parity] [--full]\n",
                     argv[0]);
        return 2;
    }

    ValidationProgressState validationProgress{};
    const mk64_3ds::Mk64O2rValidationResult validation =
        mk64_3ds::ValidateMk64O2rArchive(
            argv[1], verifyAllEntries,
            verifyAllEntries ? RecordValidationProgress : nullptr,
            verifyAllEntries ? &validationProgress : nullptr);
    if (!validation.IsValid()) {
        if (validation.component != nullptr) {
            std::fprintf(stderr, "archive validation failed: %s (%s)\n",
                         mk64_3ds::Mk64O2rValidationMessage(validation.error), validation.component);
        } else {
            std::fprintf(stderr, "archive validation failed: %s\n",
                         mk64_3ds::Mk64O2rValidationMessage(validation.error));
        }
        return 1;
    }
    if (verifyAllEntries &&
        (validationProgress.callbackCount < 2 || validationProgress.regressed ||
         validationProgress.totalChanged ||
         validationProgress.previousCompleted != validation.entryCount ||
         validationProgress.totalEntries != validation.entryCount)) {
        std::fprintf(stderr,
                     "full-validation progress callback contract failed "
                     "(calls=%zu completed=%zu total=%zu entries=%zu)\n",
                     validationProgress.callbackCount,
                     validationProgress.previousCompleted,
                     validationProgress.totalEntries, validation.entryCount);
        return 1;
    }

    mk64_3ds::O2rArchiveReader archive(argv[1]);
    if (archive.Open() != mk64_3ds::O2rReadResult::Ok) {
        std::fprintf(stderr, "failed to index archive\n");
        return 1;
    }

    std::vector<std::uint8_t> version;
    if (archive.ReadEntry("version", &version) != mk64_3ds::O2rReadResult::Ok) {
        std::fprintf(stderr, "failed to read version entry\n");
        return 1;
    }

    const auto versionEntry = std::find(archive.Entries().begin(), archive.Entries().end(), "version");
    std::vector<std::uint8_t> indexedVersion;
    if (versionEntry == archive.Entries().end() ||
        archive.ReadEntryByIndex(static_cast<std::size_t>(versionEntry - archive.Entries().begin()),
                                 &indexedVersion) != mk64_3ds::O2rReadResult::Ok ||
        indexedVersion != version) {
        std::fprintf(stderr, "name/index parity failed for version entry\n");
        return 1;
    }
    indexedVersion.assign(1, 0xff);
    if (archive.ReadEntryByIndex(archive.Entries().size(), &indexedVersion) !=
            mk64_3ds::O2rReadResult::EntryNotFound ||
        !indexedVersion.empty()) {
        std::fprintf(stderr, "out-of-range index contract failed\n");
        return 1;
    }
    std::size_t indexedVersionSize = 0;
    if (archive.GetEntryUncompressedSizeByIndex(
            static_cast<std::size_t>(versionEntry - archive.Entries().begin()),
            &indexedVersionSize) != mk64_3ds::O2rReadResult::Ok ||
        indexedVersionSize != version.size()) {
        std::fprintf(stderr, "indexed size query failed for version entry\n");
        return 1;
    }
    indexedVersionSize = 1;
    if (archive.GetEntryUncompressedSizeByIndex(archive.Entries().size(),
                                                &indexedVersionSize) !=
            mk64_3ds::O2rReadResult::EntryNotFound ||
        indexedVersionSize != 0) {
        std::fprintf(stderr, "out-of-range indexed size contract failed\n");
        return 1;
    }

    std::printf("entries=%zu version_bytes=%zu\n", archive.Entries().size(), version.size());
    if (verifyAllEntries) {
        std::printf("full_validation=ok checked=%zu\n", archive.Entries().size());
    }
    if (checkIndexParity) {
        std::vector<std::uint8_t> namedBytes;
        std::vector<std::uint8_t> indexedBytes;
        for (std::size_t entryIndex = 0; entryIndex < archive.Entries().size(); ++entryIndex) {
            std::size_t indexedSize = 0;
            if (archive.ReadEntry(archive.Entries()[entryIndex], &namedBytes) !=
                    mk64_3ds::O2rReadResult::Ok ||
                archive.ReadEntryByIndex(entryIndex, &indexedBytes) !=
                    mk64_3ds::O2rReadResult::Ok ||
                archive.GetEntryUncompressedSizeByIndex(entryIndex, &indexedSize) !=
                    mk64_3ds::O2rReadResult::Ok ||
                indexedSize != indexedBytes.size() ||
                namedBytes != indexedBytes) {
                std::fprintf(stderr, "name/index parity failed at entry %zu (%s)\n",
                             entryIndex, archive.Entries()[entryIndex].c_str());
                return 1;
            }
        }
        std::printf("index_parity=ok checked=%zu\n", archive.Entries().size());
    }
    if (printTypes) {
        std::map<std::tuple<uint32_t, uint32_t, bool>, size_t> counts;
        size_t nonResources = 0;
        std::vector<std::uint8_t> bytes;
        for (std::size_t entryIndex = 0; entryIndex < archive.Entries().size(); ++entryIndex) {
            if (archive.ReadEntryByIndex(entryIndex, &bytes) != mk64_3ds::O2rReadResult::Ok ||
                bytes.size() < 64 || bytes[0] > 1) {
                ++nonResources;
                continue;
            }
            const bool bigEndian = bytes[0] == 1;
            const uint32_t type = ReadU32(bytes.data() + 4, bigEndian);
            const uint32_t resourceVersion = ReadU32(bytes.data() + 8, bigEndian);
            ++counts[{type, resourceVersion, bigEndian}];
        }
        for (const auto& [key, count] : counts) {
            const auto [type, resourceVersion, bigEndian] = key;
            std::printf("type=");
            PrintType(type);
            std::printf(" version=%u endian=%s count=%zu\n", resourceVersion, bigEndian ? "big" : "little", count);
        }
        std::printf("non_resources=%zu\n", nonResources);
    }
    return 0;
}
