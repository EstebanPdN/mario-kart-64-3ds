#include "native_lighting_3ds.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <random>

struct Color { uint8_t r, g, b, a; };

// Independent formulation of the original one-light interpreter loop.
static std::array<uint8_t, 3> Original(const mk64_3ds::NativeLightingState& s, const int8_t n[3]) {
    int r = s.ambient[0], g = s.ambient[1], b = s.ambient[2];
    float amount = 0;
    amount += n[0] * s.direction[0];
    amount += n[1] * s.direction[1];
    amount += n[2] * s.direction[2];
    amount /= 127.0f;
    if (amount > 0) { r += amount * s.diffuse[0]; g += amount * s.diffuse[1]; b += amount * s.diffuse[2]; }
    return {static_cast<uint8_t>(r > 255 ? 255 : r), static_cast<uint8_t>(g > 255 ? 255 : g),
            static_cast<uint8_t>(b > 255 ? 255 : b)};
}

// Sensitivity model, NOT an emulator of every PICA silicon rounding detail.
// Compare 16 fraction-bit rounding-to-nearest and truncation after every
// operation, because an ordinary float32 Azahar shader cannot establish
// hardware byte parity. Values exercised here are finite normal-range values.
static float Float24(float value, bool truncate) {
    if (value == 0) return 0;
    int exponent;
    const float fraction = std::frexp(value, &exponent);
    const float scaled = std::ldexp(fraction, 17);
    return std::ldexp(truncate ? std::trunc(scaled) : std::round(scaled), exponent - 17);
}
static std::array<uint8_t, 3> ShaderModel(const mk64_3ds::NativeLightingState& s,
                                         const int8_t n[3], bool truncate) {
    const auto q = [truncate](float x) { return Float24(x, truncate); };
    float terms[3];
    for (int c = 0; c < 3; ++c) terms[c] = q(float(n[c]) * q(s.direction[c]));
    float intensity = q(q(terms[0] + terms[1]) + terms[2]);
    intensity = std::max(0.0f, q(intensity * q(1.0f / 127.0f)));
    std::array<uint8_t,3> result{};
    for (int c = 0; c < 3; ++c) {
        const float lit = q(q(intensity * s.diffuse[c]) + s.ambient[c]);
        result[c] = static_cast<uint8_t>(std::min(255.0f, std::floor(lit)));
    }
    return result;
}

int main() {
    using namespace mk64_3ds;
    NativeLightingState s{{0,0,1},{31,85,175},{255,254,100}};
    NativeLightingVertices<68> slots;
    int8_t forward[3]{0,0,127}, backward[3]{0,0,-128};
    assert((EvaluateNativeLighting(s, forward) == std::array<uint8_t,3>{255,255,255}));
    assert((EvaluateNativeLighting(s, backward) == std::array<uint8_t,3>{31,85,175}));
    for (int n = -128; n <= 127; ++n) assert(int(EncodeLightingNormal(static_cast<int8_t>(n))) - 128 == n);
    slots.Capture(0,s,forward); slots.Capture(1,s,forward); slots.Capture(2,s,backward);
    assert(slots.CanDraw(0,1,2));
    Color color{0,0,0,71};
    assert(slots.Materialize(0,color));
    assert(color.r == 255 && color.g == 255 && color.b == 255 && color.a == 71);
    assert(!slots.Materialize(0,color));
    // Loading a changed light leaves previously loaded slots intact, even
    // when an old vertex has been materialized for a CPU fallback triangle.
    s.ambient[0] = 0;
    slots.Capture(2,s,backward);
    assert(!slots.CanDraw(0,1,2));
    assert(slots.Get(0).state.ambient[0] == 31);
    assert(slots.Get(2).state.ambient[0] == 0);
    slots.Disable(0); assert(!slots.CanDraw(0,1,1));
    slots.Reset(); assert(!slots.CanDraw(1,1,1));
    s.direction[1] = std::numeric_limits<float>::quiet_NaN();
    slots.Capture(0,s,forward); assert(!slots.CanDraw(0,0,0));
    assert(!slots.CanDraw(68,0,0));
    std::mt19937 generator(0x4d4b3634);
    std::uniform_real_distribution<float> direction(-1,1);
    size_t differingChannels[2]{};
    int maxError[2]{};
    for (int sample = 0; sample < 300000; ++sample) {
        int8_t n[3];
        float length2 = 0;
        for (int c = 0; c < 3; ++c) { s.direction[c] = direction(generator); length2 += s.direction[c]*s.direction[c]; }
        const float length = std::sqrt(length2);
        for (int c = 0; c < 3; ++c) {
            s.direction[c] /= length;
            s.ambient[c] = generator() & 255; s.diffuse[c] = generator() & 255;
            n[c] = static_cast<int8_t>(static_cast<int>(generator() & 255)-128);
        }
        const auto original = Original(s,n);
        assert(EvaluateNativeLighting(s,n) == original);
        for (int rounding = 0; rounding < 2; ++rounding) {
            const auto candidate = ShaderModel(s,n,rounding != 0);
            for (int c = 0; c < 3; ++c) {
                const int error = std::abs(int(candidate[c]) - int(original[c]));
                maxError[rounding] = std::max(maxError[rounding],error);
                differingChannels[rounding] += error != 0;
                assert(error <= 1);
            }
        }
    }
    // Axis-aligned and representative stock light directions; exhaustive
    // signed normal values on each independent axis include -128, grazing
    // angles, zero lights, saturation and integer color boundaries.
    const int lightDirections[][3]{{0,0,120},{0,-120,0},{40,40,20},{-66,82,-55},{0,0,0}};
    for (const auto& light : lightDirections) {
        float length = std::sqrt(float(light[0]*light[0]+light[1]*light[1]+light[2]*light[2]));
        for (int c = 0; c < 3; ++c) {
            s.direction[c] = length > 0 ? light[c]/length : 0;
            s.ambient[c] = c == 0 ? 0 : c == 1 ? 85 : 175;
            s.diffuse[c] = c == 2 ? 100 : 255;
        }
        for (int axis = 0; axis < 3; ++axis) for (int value = -128; value <= 127; ++value) {
            int8_t n[3]{}; n[axis] = static_cast<int8_t>(value);
            const auto original = Original(s,n);
            assert(EvaluateNativeLighting(s,n) == original);
            for (int rounding = 0; rounding < 2; ++rounding) {
                const auto candidate = ShaderModel(s,n,rounding != 0);
                for (int c = 0; c < 3; ++c) assert(std::abs(int(candidate[c])-int(original[c])) <= 1);
            }
        }
    }
    std::printf("Float24 sensitivity (900000 random channels): nearest %zu differing, max %d; truncation %zu differing, max %d.\n", differingChannels[0],maxError[0],differingChannels[1],maxError[1]);
    std::puts("Native lighting: signed normals, one-light CPU parity (300000 cases), snapshots, fallback, alpha, invalidation passed.");
}
