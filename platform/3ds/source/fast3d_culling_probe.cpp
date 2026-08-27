#include "fast3d_culling_3ds.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::size_t kMaximumVertices = 64;
constexpr std::uint32_t kF3DEXOpcode = 0xBE000000u;
constexpr std::uint32_t kF3DEX2Opcode = 0x03000000u;
constexpr std::uint32_t kF3DOpcode = 0xBE000000u;
constexpr std::array<std::uint8_t, 6> kClipPlaneBits = {
    0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u,
};

[[noreturn]] void Fail(const char* expression, int line) {
    std::fprintf(stderr, "fast3d culling probe failed at line %d: %s\n", line,
                 expression);
    std::abort();
}

#define CHECK(expression)                 \
    do {                                  \
        if (!(expression)) {              \
            Fail(#expression, __LINE__);  \
        }                                 \
    } while (false)

void CheckRange(const mk64_3ds::Fast3DCullRange& actual, bool valid,
                std::size_t first = 0, std::size_t last = 0) {
    CHECK(actual.valid == valid);
    if (valid) {
        CHECK(actual.first == first);
        CHECK(actual.last == last);
    }
}

void TestDirectRangeValidation() {
    for (std::size_t first = 0; first <= kMaximumVertices + 1; ++first) {
        for (std::size_t last = 0; last <= kMaximumVertices + 1; ++last) {
            const bool expected = first <= last && last < kMaximumVertices;
            CheckRange(mk64_3ds::ValidateFast3DCullRange(
                           first, last, kMaximumVertices),
                       expected, first, last);
        }
    }
    CheckRange(mk64_3ds::ValidateFast3DCullRange(0, 0, 0), false);
}

void TestTriangleVertexValidation() {
    CHECK(mk64_3ds::AreFast3DTriangleVerticesValid(
        kMaximumVertices - 3, kMaximumVertices - 2, kMaximumVertices - 1,
        false, kMaximumVertices));
    CHECK(!mk64_3ds::AreFast3DTriangleVerticesValid(
        kMaximumVertices, 0, 1, false, kMaximumVertices));

    // Textured rectangles use the four scratch vertices immediately after the
    // regular Fast3D range.
    CHECK(mk64_3ds::AreFast3DTriangleVerticesValid(
        kMaximumVertices, kMaximumVertices + 1, kMaximumVertices + 3, true,
        kMaximumVertices));
    CHECK(!mk64_3ds::AreFast3DTriangleVerticesValid(
        kMaximumVertices + 4, kMaximumVertices, kMaximumVertices + 1, true,
        kMaximumVertices));
}

void TestModernDecoder(mk64_3ds::Fast3DCullEncoding encoding,
                       std::uint32_t opcode) {
    constexpr std::uint32_t kEncodedLimit =
        static_cast<std::uint32_t>(kMaximumVertices * 2 + 3);
    for (std::uint32_t encodedFirst = 0; encodedFirst <= kEncodedLimit;
         ++encodedFirst) {
        for (std::uint32_t encodedLast = 0; encodedLast <= kEncodedLimit;
             ++encodedLast) {
            const bool aligned = (encodedFirst & 1u) == 0 &&
                                 (encodedLast & 1u) == 0;
            const std::size_t first = encodedFirst / 2u;
            const std::size_t last = encodedLast / 2u;
            const bool expected = aligned && first <= last &&
                                  last < kMaximumVertices;
            CheckRange(mk64_3ds::DecodeFast3DCullRange(
                           opcode | encodedFirst, encodedLast, encoding,
                           kMaximumVertices),
                       expected, first, last);
        }
    }

    CheckRange(mk64_3ds::DecodeFast3DCullRange(
                   opcode | 0x00010000u, 0, encoding, kMaximumVertices),
               false);
    CheckRange(mk64_3ds::DecodeFast3DCullRange(
                   opcode, 0x00010000u, encoding, kMaximumVertices),
               false);
}

void TestLegacyDecoder() {
    constexpr std::uint32_t kStride = 40u;
    constexpr std::uint32_t kEncodedLimit =
        static_cast<std::uint32_t>((kMaximumVertices + 2) * kStride - 1);
    for (std::uint32_t encodedFirst = 0; encodedFirst <= kEncodedLimit;
         ++encodedFirst) {
        for (std::uint32_t encodedEndExclusive = 0;
             encodedEndExclusive <= kEncodedLimit; ++encodedEndExclusive) {
            const bool aligned = encodedFirst % kStride == 0 &&
                                 (encodedEndExclusive == 0 ||
                                  encodedEndExclusive % kStride == 0);
            const std::size_t first = encodedFirst / kStride;
            const std::size_t endExclusive =
                encodedEndExclusive == 0 ? 16 : encodedEndExclusive / kStride;
            const bool expected = aligned && first < endExclusive &&
                                  endExclusive <= kMaximumVertices;
            CheckRange(mk64_3ds::DecodeFast3DCullRange(
                           kF3DOpcode | encodedFirst, encodedEndExclusive,
                           mk64_3ds::Fast3DCullEncoding::F3D,
                           kMaximumVertices),
                       expected, first,
                       endExclusive == 0 ? 0 : endExclusive - 1);
        }
    }
}

void TestInclusiveClipEvaluation() {
    std::array<std::uint8_t, kMaximumVertices> clips = {};

    for (std::size_t first = 0; first < kMaximumVertices; ++first) {
        for (std::size_t last = first; last < kMaximumVertices; ++last) {
            const auto range =
                mk64_3ds::ValidateFast3DCullRange(first, last,
                                                  kMaximumVertices);
            for (const std::uint8_t plane : kClipPlaneBits) {
                clips.fill(0);
                std::fill(clips.begin() + static_cast<std::ptrdiff_t>(first),
                          clips.begin() + static_cast<std::ptrdiff_t>(last + 1),
                          plane);
                CHECK(mk64_3ds::ShouldCullFast3DRange(
                    range, [&clips](std::size_t index) { return clips[index]; }));

                clips[last] = 0;
                CHECK(!mk64_3ds::ShouldCullFast3DRange(
                    range, [&clips](std::size_t index) { return clips[index]; }));
            }
        }
    }

    const auto threeVertices =
        mk64_3ds::ValidateFast3DCullRange(10, 12, kMaximumVertices);
    for (std::uint32_t firstMask = 0; firstMask < 64; ++firstMask) {
        for (std::uint32_t middleMask = 0; middleMask < 64; ++middleMask) {
            for (std::uint32_t lastMask = 0; lastMask < 64; ++lastMask) {
                clips[10] = static_cast<std::uint8_t>(firstMask);
                clips[11] = static_cast<std::uint8_t>(middleMask);
                clips[12] = static_cast<std::uint8_t>(lastMask);
                const bool expected = (firstMask & middleMask & lastMask) != 0;
                CHECK(mk64_3ds::ShouldCullFast3DRange(
                          threeVertices,
                          [&clips](std::size_t index) { return clips[index]; }) ==
                      expected);
            }
        }
    }

    clips[10] = 0x01u;
    clips[11] = 0x02u;
    clips[12] = 0x04u;
    CHECK(!mk64_3ds::ShouldCullFast3DRange(
        threeVertices, [&clips](std::size_t index) { return clips[index]; }));

    std::size_t invalidReads = 0;
    CHECK(!mk64_3ds::ShouldCullFast3DRange(
        {}, [&invalidReads](std::size_t) {
            ++invalidReads;
            return 0x01u;
        }));
    CHECK(invalidReads == 0);
}

} // namespace

int main() {
    static_assert(mk64_3ds::DecodeFast3DCullRange(
                      kF3DEXOpcode, 14u,
                      mk64_3ds::Fast3DCullEncoding::F3DEX,
                      kMaximumVertices)
                      .last == 7);
    static_assert(mk64_3ds::DecodeFast3DCullRange(
                      kF3DEX2Opcode, 14u,
                      mk64_3ds::Fast3DCullEncoding::F3DEX2,
                      kMaximumVertices)
                      .last == 7);
    static_assert(mk64_3ds::DecodeFast3DCullRange(
                      kF3DOpcode, 8u * 40u,
                      mk64_3ds::Fast3DCullEncoding::F3D,
                      kMaximumVertices)
                      .last == 7);
    static_assert(mk64_3ds::DecodeFast3DCullRange(
                      kF3DOpcode, 0,
                      mk64_3ds::Fast3DCullEncoding::F3D,
                      kMaximumVertices)
                      .last == 15);

    TestDirectRangeValidation();
    TestTriangleVertexValidation();
    TestModernDecoder(mk64_3ds::Fast3DCullEncoding::F3DEX, kF3DEXOpcode);
    TestModernDecoder(mk64_3ds::Fast3DCullEncoding::F3DEX2, kF3DEX2Opcode);
    TestLegacyDecoder();
    TestInclusiveClipEvaluation();

    std::puts("fast3d culling policy: ok");
    return 0;
}
