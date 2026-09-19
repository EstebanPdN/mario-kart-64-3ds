#include "fast3d_reuse_3ds.hpp"

#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include <random>

using namespace mk64_3ds;

struct Vertex {
    struct {
        std::int16_t ob[3];
        std::uint16_t flag;
        std::int16_t tc[2];
        std::uint8_t cn[4];
    } v;
};
static_assert(sizeof(Vertex) == 16);

static void MatrixTests() {
    std::mt19937 random(0x163d);
    alignas(16) std::int32_t source[16]{};
    float output[4][4];
    float reference[4][4];
    for (int iteration = 0; iteration < 20000; ++iteration) {
        for (auto& word : source) {
            const auto bits = static_cast<std::uint32_t>(random());
            std::memcpy(&word, &bits, sizeof(word));
        }
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                const int shift = (col & 1) ? 0 : 16;
                const auto integerWord = static_cast<std::uint32_t>(source[row * 2 + col / 2]);
                const auto fractionWord = static_cast<std::uint32_t>(source[8 + row * 2 + col / 2]);
                const auto upper = (integerWord >> shift) & 65535U;
                const auto lower = (fractionWord >> shift) & 65535U;
                const std::int64_t integer = upper < 32768U ? upper : static_cast<std::int64_t>(upper) - 65536;
                reference[row][col] = static_cast<float>(integer * 65536 + lower) / 65536.0f;
            }
        }
        const auto misses = gFast3DReuseStats.matrixMisses;
        LoadFast3DFixedMatrix(output, source);
        assert(gFast3DReuseStats.matrixMisses == misses + 1);
        assert(std::memcmp(output, reference, sizeof(output)) == 0);
        const auto hits = gFast3DReuseStats.matrixHits;
        LoadFast3DFixedMatrix(output, source);
        assert(gFast3DReuseStats.matrixHits == hits + 1);
        assert(std::memcmp(output, reference, sizeof(output)) == 0);
    }
}

static void LifetimeAndCollisionTests() {
    using Cache = Fast3DExactBlockCache<std::uint32_t, std::uint32_t,
                                       std::uint32_t, 2, 1>;
    Cache cache;
    std::uint32_t first[2] = {1, 2}, second[2] = {3, 4};
    std::uint32_t result[2] = {10, 20};
    cache.Store(first, 2, 4, result);
    assert(cache.Find(first, 2, 4)[1] == 20);
    assert(cache.Find(first, 1, 4) == nullptr);
    assert(cache.Find(first, 2, 5) == nullptr);
    assert(cache.Find(second, 2, 4) == nullptr);
    first[1] = 9;
    assert(cache.Find(first, 2, 4) == nullptr);
    cache.Store(second, 2, 4, result);
    assert(cache.Find(first, 2, 4) == nullptr);
    assert(cache.Find(second, 2, 4));
    cache.Clear();
    assert(cache.Find(second, 2, 4) == nullptr);
    assert(cache.Find(nullptr, 2, 4) == nullptr);
    assert(cache.Find(second, 0, 4) == nullptr);
    assert(cache.Find(second, 3, 4) == nullptr);
    auto dynamic = std::make_unique<std::uint32_t[]>(2);
    dynamic[0] = 20;
    dynamic[1] = 21;
    cache.Store(dynamic.get(), 2, 4, result);
    dynamic.reset();
    assert(cache.Find(second, 2, 4) == nullptr);
    cache.Store(second, 2, 4, result);
}

static void PositionTests() {
    std::mt19937 random(0x64);
    alignas(16) Vertex vertices[33]{};
    float matrix[4][4]{};
    for (int iteration = 0; iteration < 2000; ++iteration) {
        for (auto& vertex : vertices) {
            for (auto& position : vertex.v.ob) position = static_cast<std::int16_t>(random());
            for (auto& channel : vertex.v.cn) channel = static_cast<std::uint8_t>(random());
        }
        for (auto& row : matrix) {
            for (auto& value : row) value = static_cast<int>(random() % 2001) / 1000.0f - 1;
        }
        const auto count = static_cast<std::size_t>(16 + random() % 17);
        auto misses = gFast3DReuseStats.positionMisses;
        const auto* output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output);
        assert(gFast3DReuseStats.positionMisses == misses + 1);
        for (std::size_t index = 0; index < count; ++index) {
            float reference[2];
            for (int col = 2; col < 4; ++col) {
                reference[col - 2] = vertices[index].v.ob[0] * matrix[0][col] +
                    vertices[index].v.ob[1] * matrix[1][col] +
                    vertices[index].v.ob[2] * matrix[2][col] + matrix[3][col];
            }
            assert(output[index].z == reference[0] && output[index].w == reference[1]);
        }
        auto hits = gFast3DReuseStats.positionHits;
        output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output && gFast3DReuseStats.positionHits == hits + 1);
        // x/y, aspect and shading remain live outside the cached z/w stage.
        matrix[0][0] += 0.5f;
        output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output && gFast3DReuseStats.positionHits == hits + 2);
        // Camera change, midpoint interpolation, and same-address source edits
        // must never reuse stale transformed depths.
        matrix[3][3] += 0.25f;
        output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output && gFast3DReuseStats.positionMisses == misses + 2);
        vertices[0].v.cn[0] ^= 1;
        output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output && gFast3DReuseStats.positionMisses == misses + 3);
        vertices[count - 1].v.ob[2] ^= 1;
        output = BeginFast3DPositionBlock(vertices, count, matrix);
        assert(output && gFast3DReuseStats.positionMisses == misses + 4);
        for (size_t small = 0; small < 16; ++small)
            assert(BeginFast3DPositionBlock(vertices, small, matrix) == nullptr);
        assert(BeginFast3DPositionBlock(vertices, 33, matrix) == nullptr);
        assert(BeginFast3DPositionBlock<Vertex>(nullptr, count, matrix) == nullptr);
    }
}

int main() {
    MatrixTests();
    LifetimeAndCollisionTests();
    PositionTests();
    std::puts("PASS: exact matrix/vertex-depth reuse, mutations, camera changes, capacities, ownership");
}
