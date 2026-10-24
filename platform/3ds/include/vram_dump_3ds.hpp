#pragma once
#include "dump_crc_3ds.hpp"
#include <algorithm>
#include <array>
#include <cstdio>

namespace mk64_3ds {
struct VramDumpResult {
    bool complete = false;
    std::size_t bytes = 0;
    std::uint32_t crc32 = 0;
};

// The caller owns the idle GPU until this returns. Copy in bounded aligned
// blocks: no six-MiB allocation and no progress drawing may alter the source.
template<class Copy>
VramDumpResult WriteVramSnapshot(const char* path, std::size_t bytes, Copy copy) {
    VramDumpResult result;
    if (path == nullptr || bytes == 0) return result;
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return result;
    alignas(8) std::array<unsigned char, 32 * 1024> block;
    while (result.bytes < bytes) {
        const auto count = std::min(block.size(), bytes - result.bytes);
        if (!copy(result.bytes, block.data(), count) ||
            std::fwrite(block.data(), 1, count, file) != count) break;
        result.crc32 = DumpCrc32(result.crc32, block.data(), count);
        result.bytes += count;
    }
    const bool written = std::ferror(file) == 0 && result.bytes == bytes;
    result.complete = std::fclose(file) == 0 && written;
    if (!result.complete) std::remove(path);
    return result;
}
} // namespace mk64_3ds
