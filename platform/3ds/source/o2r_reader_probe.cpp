#include "o2r_archive_reader.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace {

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
    if (argc != 2 && argc != 3) {
        std::fprintf(stderr, "usage: %s <archive.o2r> [--types]\n", argv[0]);
        return 2;
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

    std::printf("entries=%zu version_bytes=%zu\n", archive.Entries().size(), version.size());
    if (argc == 3 && std::string(argv[2]) == "--types") {
        std::map<std::tuple<uint32_t, uint32_t, bool>, size_t> counts;
        size_t nonResources = 0;
        std::vector<std::uint8_t> bytes;
        for (const std::string& entry : archive.Entries()) {
            if (archive.ReadEntry(entry, &bytes) != mk64_3ds::O2rReadResult::Ok || bytes.size() < 64 || bytes[0] > 1) {
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
