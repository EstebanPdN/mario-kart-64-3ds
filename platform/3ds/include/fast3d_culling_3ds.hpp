#pragma once

#include <cstddef>
#include <cstdint>

namespace mk64_3ds {

enum class Fast3DCullEncoding : std::uint8_t {
    F3D,
    F3DEX,
    F3DEX2,
};

struct Fast3DCullRange {
    std::size_t first = 0;
    std::size_t last = 0;
    bool valid = false;
};

constexpr bool AreFast3DTriangleVerticesValid(std::size_t first,
                                               std::size_t second,
                                               std::size_t third,
                                               bool isRectangle,
                                               std::size_t maximumVertices) {
    const std::size_t vertexLimit =
        maximumVertices + (isRectangle ? 4U : 0U);
    return first < vertexLimit && second < vertexLimit && third < vertexLimit;
}

constexpr Fast3DCullRange ValidateFast3DCullRange(std::size_t first,
                                                  std::size_t last,
                                                  std::size_t maximumVertices) {
    if (maximumVertices == 0 || first > last || last >= maximumVertices) {
        return {};
    }
    return { first, last, true };
}

constexpr Fast3DCullRange DecodeModernFast3DCullRange(std::uint32_t word0,
                                                       std::uint32_t word1,
                                                       std::size_t maximumVertices) {
    constexpr std::uint32_t kFieldMask = 0x0000FFFFu;
    constexpr std::uint32_t kReservedWord0Mask = 0x00FF0000u;
    constexpr std::uint32_t kReservedWord1Mask = 0xFFFF0000u;

    const std::uint32_t encodedFirst = word0 & kFieldMask;
    const std::uint32_t encodedLast = word1 & kFieldMask;
    if ((word0 & kReservedWord0Mask) != 0 ||
        (word1 & kReservedWord1Mask) != 0 || (encodedFirst & 1u) != 0 ||
        (encodedLast & 1u) != 0) {
        return {};
    }

    return ValidateFast3DCullRange(encodedFirst / 2u, encodedLast / 2u,
                                   maximumVertices);
}

constexpr Fast3DCullRange DecodeLegacyFast3DCullRange(std::uint32_t word0,
                                                       std::uint32_t word1,
                                                       std::size_t maximumVertices) {
    constexpr std::uint32_t kLegacyStride = 40u;
    const std::uint32_t encodedFirst = word0 & 0x00FFFFFFu;
    const std::uint32_t encodedEndExclusive = word1;
    if (encodedFirst % kLegacyStride != 0 ||
        (encodedEndExclusive != 0 && encodedEndExclusive % kLegacyStride != 0)) {
        return {};
    }

    const std::size_t first = encodedFirst / kLegacyStride;
    // F3D masks (vend + 1) to four bits, so vend == 15 wraps to zero.
    const std::size_t endExclusive =
        encodedEndExclusive == 0 ? 16 : encodedEndExclusive / kLegacyStride;
    if (first >= endExclusive || endExclusive > maximumVertices) {
        return {};
    }
    return { first, endExclusive - 1, true };
}

constexpr Fast3DCullRange DecodeFast3DCullRange(std::uint32_t word0,
                                                 std::uint32_t word1,
                                                 Fast3DCullEncoding encoding,
                                                 std::size_t maximumVertices) {
    switch (encoding) {
        case Fast3DCullEncoding::F3D:
            return DecodeLegacyFast3DCullRange(word0, word1, maximumVertices);
        case Fast3DCullEncoding::F3DEX:
        case Fast3DCullEncoding::F3DEX2:
            return DecodeModernFast3DCullRange(word0, word1, maximumVertices);
    }
    return {};
}

template <typename ClipRejectAt>
constexpr bool ShouldCullFast3DRange(const Fast3DCullRange& range,
                                     ClipRejectAt clipRejectAt) {
    if (!range.valid) {
        return false;
    }

    std::uint8_t sharedClipPlanes =
        static_cast<std::uint8_t>(clipRejectAt(range.first));
    for (std::size_t index = range.first + 1; index <= range.last; ++index) {
        sharedClipPlanes &= static_cast<std::uint8_t>(clipRejectAt(index));
        if (sharedClipPlanes == 0) {
            return false;
        }
    }
    return sharedClipPlanes != 0;
}

} // namespace mk64_3ds
