#include "fast3d_math_3ds.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

uint32_t NextRandom(uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return state;
}

float RandomCoordinate(uint32_t& state) {
    return static_cast<float>(static_cast<int32_t>(NextRandom(state) % 20001U) - 10000) /
           97.0f;
}

float RandomNonzeroW(uint32_t& state) {
    float value = RandomCoordinate(state);
    if (std::fabs(value) < 0.05f) {
        value = value < 0.0f ? -0.05f : 0.05f;
    }
    return value;
}

int Sign(float value) {
    return (value > 0.0f) - (value < 0.0f);
}

} // namespace

int main() {
    if (mk64_3ds::Fast3DCanProject(0.0f, 1.0f, 1.0f) ||
        mk64_3ds::Fast3DCanProject(1.0e-5f, 1.0f, 1.0f) ||
        !mk64_3ds::Fast3DCanProject(-1.0f, 1.0f, 1.0f)) {
        std::fputs("near-plane projection guard mismatch\n", stderr);
        return 1;
    }

    uint32_t state = 0x4D4B3634U;
    for (int sample = 0; sample < 200000; ++sample) {
        const float x1 = RandomCoordinate(state);
        const float y1 = RandomCoordinate(state);
        const float w1 = RandomNonzeroW(state);
        const float x2 = RandomCoordinate(state);
        const float y2 = RandomCoordinate(state);
        const float w2 = RandomNonzeroW(state);
        const float x3 = RandomCoordinate(state);
        const float y3 = RandomCoordinate(state);
        const float w3 = RandomNonzeroW(state);

        const float reference =
            (x1 / w1 - x2 / w2) * (y3 / w3 - y2 / w2) -
            (y1 / w1 - y2 / w2) * (x3 / w3 - x2 / w2);
        const float optimized = mk64_3ds::Fast3DProjectedCross(
            x1, y1, w1, x2, y2, w2, x3, y3, w3);
        if (std::isfinite(reference) && std::isfinite(optimized) &&
            Sign(reference) != Sign(optimized)) {
            std::fprintf(stderr, "cross-sign mismatch at sample %d\n", sample);
            return 1;
        }
    }

    if (std::fabs(mk64_3ds::Fast3DAspectCorrection(400.0f, 240.0f) - 0.8f) >
        0.00001f) {
        std::fputs("aspect correction mismatch\n", stderr);
        return 1;
    }
    std::puts("fast3d math probe: ok");
    return 0;
}
