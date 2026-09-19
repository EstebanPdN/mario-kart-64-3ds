#include "native_geometry_3ds.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <iostream>

struct Vertex { float x, y, z, w; std::uint8_t clip_rej; };

static Vertex Reference(const std::int16_t* p, const float m[4][4], float aspect) {
    Vertex result{};
    float* components[] = {&result.x, &result.y, &result.z, &result.w};
    for (int j = 0; j < 4; ++j)
        *components[j] = p[0] * m[0][j] + p[1] * m[1][j] + p[2] * m[2][j] + m[3][j];
    result.x *= aspect;
    result.clip_rej = mk64_3ds::NativeGeometryState::ClipMask(result.x, result.y, result.z, result.w);
    return result;
}

static void Equal(const Vertex& a, const Vertex& b) {
    assert(a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w && a.clip_rej == b.clip_rej);
}

int main() {
    using namespace mk64_3ds;
    NativeGeometryState state;
    Vertex loaded[68]{};
    float matrix[4][4]{};
    matrix[0][0] = matrix[1][1] = matrix[2][2] = matrix[3][3] = 1;
    std::int16_t points[3][3] = {{-2, 0, 0}, {0, 3, 0}, {2, 0, 2}};
    state.BeginLoad(0, 3, matrix, 0.8f);
    for (int i = 0; i < 3; ++i) state.Capture(i, points[i], loaded[i]);
    assert(state.counters.materialized == 0 && loaded[2].clip_rej == 32);
    auto draw = state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::Back);
    assert(draw.enabled && draw.aspect == 0.8f && draw.cull == NativeGeometryCull::Back);
    assert(draw.matrixGeneration != 0);
    assert(SameNativeGeometryDraw(draw, state.PrepareTriangle(0, 1, 2, loaded, true,
                                                            NativeGeometryCull::Back)));
    auto changedCull = draw;
    changedCull.cull = NativeGeometryCull::Front;
    assert(!SameNativeGeometryDraw(draw, changedCull));
    auto changedAspect = draw;
    changedAspect.aspect = 1.0f;
    assert(!SameNativeGeometryDraw(draw, changedAspect));
    auto externalDraw = draw;
    externalDraw.matrixGeneration = 0;
    assert(SameNativeGeometryDraw(draw, externalDraw));
    externalDraw.matrix[3][0] += 1.0f;
    assert(!SameNativeGeometryDraw(draw, externalDraw));
    assert(state.counters.materialized == 0);
    // G_CULLDL restores complete XY outcodes using the captured load-time state.
    state.Materialize(0, loaded[0]);
    assert(loaded[0].clip_rej == 1);
    matrix[3][0] = 10;
    state.BeginLoad(2, 1, matrix, 1.0f);
    state.Capture(2, points[2], loaded[2]);
    const auto mixed = state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None);
    assert(!mixed.enabled && state.counters.fallbackMixedMatrix == 1);
    assert(loaded[0].x == -1.6f && loaded[1].y == 3 && loaded[2].x == 12);
    assert(draw.matrix[3][0] == 0); // buffered draw owns the original matrix.

    // Persist all 64 distinct matrices, then continually overwrite individual
    // slots. Unchanged vertices must never observe a recycled matrix snapshot.
    Vertex expected[64]{};
    for (int round = 0; round < 8; ++round) {
        for (int slot = 0; slot < 64; ++slot) {
            matrix[3][0] = round * 64 + slot;
            state.BeginLoad(slot, 1, matrix, 0.8f);
            state.Capture(slot, points[0], loaded[slot]);
            expected[slot] = Reference(points[0], matrix, 0.8f);
        }
        for (int slot = 0; slot < 64; ++slot) {
            state.Materialize(slot, loaded[slot]);
            Equal(loaded[slot], expected[slot]);
        }
    }

    std::mt19937 random(0x64);
    std::uniform_real_distribution<float> coefficient(-3.0f, 3.0f);
    for (int iteration = 0; iteration < 20000; ++iteration) {
        for (auto& row : matrix) for (float& entry : row) entry = coefficient(random);
        const float aspect = iteration % 2 ? 0.8f : 1.0f;
        const std::int16_t point[3] = {static_cast<std::int16_t>(random()),
            static_cast<std::int16_t>(random()), static_cast<std::int16_t>(random())};
        state.BeginLoad(0, 1, matrix, aspect);
        state.Capture(0, point, loaded[0]);
        const Vertex reference = Reference(point, matrix, aspect);
        assert(loaded[0].z == reference.z && loaded[0].w == reference.w);
        state.Materialize(0, loaded[0]);
        Equal(loaded[0], reference);
    }

    // Near-eye triangles retain the established homogeneous CPU clip path.
    matrix[3][3] = 0.00001f;
    const std::int16_t origin[3]{};
    state.BeginLoad(0, 3, matrix, 1);
    for (int i = 0; i < 3; ++i) state.Capture(i, origin, loaded[i]);
    assert(!state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    assert(state.counters.fallbackNearEye == 1);
    matrix[3][3] = 1;
    state.BeginLoad(0, 3, matrix, 1);
    for (int i = 0; i < 3; ++i) state.Capture(i, origin, loaded[i]);
    state.enabled = false;
    assert(!state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    state.enabled = true;
    assert(state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    assert(!state.PrepareTriangle(64, 65, 66, loaded, false, NativeGeometryCull::None).enabled);
    loaded[0].w = std::numeric_limits<float>::infinity();
    assert(!state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    matrix[0][0] = std::numeric_limits<float>::quiet_NaN();
    state.BeginLoad(0, 3, matrix, 1);
    for (int i = 0; i < 3; ++i) state.Capture(i, origin, loaded[i]);
    assert(std::isfinite(loaded[0].w) && std::isfinite(loaded[0].z));
    assert(!state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    state.ResetVertices();
    assert(!state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::None).enabled);
    // Recycled pool entries, ResetVertices and another state instance must not
    // alias a still-buffered draw's immutable identity.
    matrix[0][0] = matrix[1][1] = matrix[2][2] = matrix[3][3] = 1;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            if (row != col) matrix[row][col] = 0;
    state.BeginLoad(0, 3, matrix, 0.8f);
    for (int i = 0; i < 3; ++i) state.Capture(i, points[i], loaded[i]);
    auto afterReset = state.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::Back);
    assert(afterReset.matrixGeneration != draw.matrixGeneration);
    assert(SameNativeGeometryDraw(draw, afterReset)); // exact matrix still batches
    NativeGeometryState otherState;
    matrix[3][0] = 9;
    otherState.BeginLoad(0, 3, matrix, 0.8f);
    for (int i = 0; i < 3; ++i) otherState.Capture(i, points[i], loaded[i]);
    auto otherDraw = otherState.PrepareTriangle(0, 1, 2, loaded, true, NativeGeometryCull::Back);
    assert(otherDraw.matrixGeneration != afterReset.matrixGeneration);
    assert(!SameNativeGeometryDraw(afterReset, otherDraw));
    std::cout << "native geometry: 20000 reference transforms, snapshot recycling, partial loads, "
                 "far/XY outcodes, eye fallback, rectangle slots, runtime toggle passed\n";
}
