#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mk64_3ds {
inline std::uint32_t DumpCrc32(std::uint32_t previous, const unsigned char* data, std::size_t size) {
    static const auto table = [] {
        std::array<std::uint32_t,256> result{};
        for (unsigned n=0;n<256;++n) {
            std::uint32_t c=n;
            for(unsigned bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320U:0);
            result[n]=c;
        }
        return result;
    }();
    std::uint32_t crc=previous^0xffffffffU;
    for(std::size_t i=0;i<size;++i)crc=table[(crc^data[i])&255]^(crc>>8);
    return crc^0xffffffffU;
}
}
