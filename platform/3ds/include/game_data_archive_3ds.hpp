#pragma once

#include "o2r_archive_reader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace mk64_3ds {

enum class Mk64O2rValidationError : std::uint8_t {
    Ok,
    InvalidPath,
    OpenFailed,
    EmptyArchive,
    IncompleteArchive,
    MissingVersion,
    InvalidVersion,
    WrongGameVersion,
    MissingRequiredResource,
    InvalidRequiredResource,
    OutOfMemory,
    UnexpectedFailure,
};

struct Mk64O2rValidationResult {
    Mk64O2rValidationError error = Mk64O2rValidationError::UnexpectedFailure;
    std::size_t entryCount = 0;
    const char* component = nullptr;

    bool IsValid() const {
        return error == Mk64O2rValidationError::Ok;
    }
};

inline const char* Mk64O2rValidationMessage(Mk64O2rValidationError error) {
    switch (error) {
        case Mk64O2rValidationError::Ok:
            return "archive validation passed";
        case Mk64O2rValidationError::InvalidPath:
            return "archive path is empty";
        case Mk64O2rValidationError::OpenFailed:
            return "ZIP directory could not be read";
        case Mk64O2rValidationError::EmptyArchive:
            return "archive contains no files";
        case Mk64O2rValidationError::IncompleteArchive:
            return "archive contains fewer entries than a complete MK64 extraction";
        case Mk64O2rValidationError::MissingVersion:
            return "archive version entry is missing";
        case Mk64O2rValidationError::InvalidVersion:
            return "archive version entry is malformed";
        case Mk64O2rValidationError::WrongGameVersion:
            return "archive targets a different game revision";
        case Mk64O2rValidationError::MissingRequiredResource:
            return "required game data is missing";
        case Mk64O2rValidationError::InvalidRequiredResource:
            return "required game data is corrupt or incompatible";
        case Mk64O2rValidationError::OutOfMemory:
            return "not enough memory to validate the archive";
        case Mk64O2rValidationError::UnexpectedFailure:
        default:
            return "archive validation stopped unexpectedly";
    }
}

namespace detail {

constexpr std::size_t kOtrHeaderSize = 64;
// The corrected bounded-memory extractor emits 32,445 canonical resources.
// Older complete archives contain two redundant legacy vertex aliases and
// therefore also pass this lower bound.
constexpr std::size_t kMinimumExpectedEntryCount = 32445;

constexpr std::uint32_t MakeResourceType(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
}

// Torch serializes Cartridge::GetCRC() in this entry. This is the value its
// reader obtains from the supported USA ROM header; ROM acceptance itself
// remains guarded by the full SHA-1 check in game_data_3ds.cpp.
constexpr std::uint32_t kExpectedArchiveGameVersion = 0xB655503E;

struct RequiredResource {
    std::string_view path;
    std::uint32_t type;
    const char* component;
};

// These are the same representative startup/gameplay resources exercised by
// resource_runtime_probe. Reading each entry also makes miniz verify that its
// compressed data and CRC can actually be consumed, rather than trusting only
// the ZIP central directory.
constexpr std::array<RequiredResource, 7> kRequiredResources = {{
    { "textures/common_data/common_texture_item_box_question_mark", MakeResourceType('O', 'T', 'E', 'X'),
      "item texture" },
    { "models/ceremony_data/ceremony_data_seg11_vtx_340", MakeResourceType('O', 'V', 'T', 'X'),
      "ceremony vertex data" },
    { "models/ceremony_data/silver_trophy_dl", MakeResourceType('O', 'D', 'L', 'T'),
      "ceremony display list" },
    { "models/startup_logo/dl1", MakeResourceType('O', 'D', 'L', 'T'), "startup display list" },
    { "sound/samples/sample_0", MakeResourceType('A', 'U', 'F', 'C'), "audio sample" },
    { "sound/banks/bank_0", MakeResourceType('B', 'A', 'N', 'K'), "audio bank" },
    { "sound/sequences/sound_player", MakeResourceType('S', 'E', 'Q', 'C'), "audio sequence" },
}};

inline bool ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t offset, bool bigEndian,
                    std::uint32_t* value) {
    if (value == nullptr || offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t)) {
        return false;
    }
    if (bigEndian) {
        *value = (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                 (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
                 (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
                 static_cast<std::uint32_t>(bytes[offset + 3]);
    } else {
        *value = static_cast<std::uint32_t>(bytes[offset]) |
                 (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                 (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                 (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }
    return true;
}

} // namespace detail

inline Mk64O2rValidationResult ValidateMk64O2rArchive(std::string_view archivePath,
                                                      bool verifyAllEntries = false) {
    Mk64O2rValidationResult validation{};
    if (archivePath.empty()) {
        validation.error = Mk64O2rValidationError::InvalidPath;
        return validation;
    }

    try {
        O2rArchiveReader archive{ std::string(archivePath) };
        if (archive.Open() != O2rReadResult::Ok) {
            validation.error = Mk64O2rValidationError::OpenFailed;
            return validation;
        }
        validation.entryCount = archive.Entries().size();
        if (validation.entryCount == 0) {
            validation.error = Mk64O2rValidationError::EmptyArchive;
            return validation;
        }
        if (validation.entryCount < detail::kMinimumExpectedEntryCount) {
            validation.error = Mk64O2rValidationError::IncompleteArchive;
            validation.component = "archive entry set";
            return validation;
        }

        std::vector<std::uint8_t> bytes;
        const O2rReadResult versionRead = archive.ReadEntry("version", &bytes);
        if (versionRead == O2rReadResult::EntryNotFound) {
            validation.error = Mk64O2rValidationError::MissingVersion;
            return validation;
        }
        if (versionRead != O2rReadResult::Ok || bytes.size() != 5 || bytes[0] > 1) {
            validation.error = Mk64O2rValidationError::InvalidVersion;
            return validation;
        }

        std::uint32_t gameVersion = 0;
        if (!detail::ReadU32(bytes, 1, bytes[0] == 1, &gameVersion)) {
            validation.error = Mk64O2rValidationError::InvalidVersion;
            return validation;
        }
        if (gameVersion != detail::kExpectedArchiveGameVersion) {
            validation.error = Mk64O2rValidationError::WrongGameVersion;
            return validation;
        }

        for (const detail::RequiredResource& required : detail::kRequiredResources) {
            const O2rReadResult readResult = archive.ReadEntry(required.path, &bytes);
            if (readResult == O2rReadResult::EntryNotFound) {
                validation.error = Mk64O2rValidationError::MissingRequiredResource;
                validation.component = required.component;
                return validation;
            }
            // The compact 3DS runtime reads OTR resource bodies in the native
            // little-endian layout emitted by the supported Torch builds.
            if (readResult != O2rReadResult::Ok || bytes.size() <= detail::kOtrHeaderSize || bytes[0] != 0) {
                validation.error = Mk64O2rValidationError::InvalidRequiredResource;
                validation.component = required.component;
                return validation;
            }

            std::uint32_t resourceType = 0;
            std::uint32_t resourceVersion = 0;
            if (!detail::ReadU32(bytes, 4, false, &resourceType) ||
                !detail::ReadU32(bytes, 8, false, &resourceVersion) || resourceType != required.type ||
                resourceVersion != 0) {
                validation.error = Mk64O2rValidationError::InvalidRequiredResource;
                validation.component = required.component;
                return validation;
            }
        }

        if (verifyAllEntries) {
            // A freshly extracted archive is read back completely before its
            // atomic rename. miniz verifies each stored payload and CRC, so an
            // SD write error cannot be mistaken for a successful install.
            for (std::size_t entryIndex = 0; entryIndex < validation.entryCount; ++entryIndex) {
                if (archive.ReadEntryByIndex(entryIndex, &bytes) != O2rReadResult::Ok) {
                    validation.error = Mk64O2rValidationError::InvalidRequiredResource;
                    validation.component = "archive payload";
                    return validation;
                }
            }
        }

        validation.error = Mk64O2rValidationError::Ok;
        return validation;
    } catch (const std::bad_alloc&) {
        validation.error = Mk64O2rValidationError::OutOfMemory;
        return validation;
    } catch (...) {
        validation.error = Mk64O2rValidationError::UnexpectedFailure;
        return validation;
    }
}

} // namespace mk64_3ds
