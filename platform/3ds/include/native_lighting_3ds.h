#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mk64_3ds {

// One directional light plus ambient, captured at G_VTX time after the
// interpreter transforms/normalizes the light into the object's normal space.
// Byte colors remain in [0,255]; direction is NOT predivided by 127, so CPU
// fallback retains the interpreter's floating-point evaluation order.
struct NativeLightingState {
    float direction[3]{};
    uint8_t ambient[3]{};
    uint8_t diffuse[3]{};

    bool operator==(const NativeLightingState& other) const {
        for (size_t c = 0; c < 3; ++c) {
            if (direction[c] != other.direction[c] || ambient[c] != other.ambient[c] ||
                diffuse[c] != other.diffuse[c]) return false;
        }
        return true;
    }

    bool IsFinite() const {
        return std::isfinite(direction[0]) && std::isfinite(direction[1]) &&
               std::isfinite(direction[2]);
    }
};

inline uint8_t EncodeLightingNormal(int8_t normal) {
    return static_cast<uint8_t>(static_cast<int>(normal) + 128);
}

// This reference deliberately matches int += float (truncate AFTER adding
// ambient), rather than rounding or flooring each product independently.
inline std::array<uint8_t, 3> EvaluateNativeLighting(const NativeLightingState& state,
                                                   const int8_t normal[3]) {
    float intensity = 0.0f;
    intensity += normal[0] * state.direction[0];
    intensity += normal[1] * state.direction[1];
    intensity += normal[2] * state.direction[2];
    intensity /= 127.0f;
    std::array<uint8_t, 3> result{};
    for (size_t c = 0; c < 3; ++c) {
        int color = state.ambient[c];
        if (intensity > 0.0f) color += intensity * state.diffuse[c];
        result[c] = static_cast<uint8_t>(color > 255 ? 255 : color);
    }
    return result;
}

// Each loaded vertex owns its light snapshot. Display lists may change lights
// or modelview, load only some slots, then draw old and new slots together.
// No current global light state may be substituted for this captured state.
template <size_t SlotCount> class NativeLightingVertices {
  public:
    struct Vertex {
        NativeLightingState state{};
        int8_t normal[3]{};
        bool eligible = false;
        bool materialized = false;
    };

    void Reset() { for (auto& vertex : vertices_) vertex.eligible = false; }
    void Disable(size_t slot) {
        if (slot < SlotCount) vertices_[slot].eligible = false;
    }
    void Capture(size_t slot, const NativeLightingState& state, const int8_t normal[3]) {
        if (slot >= SlotCount) return;
        auto& vertex = vertices_[slot];
        vertex.state = state;
        for (size_t c = 0; c < 3; ++c) vertex.normal[c] = normal[c];
        vertex.eligible = state.IsFinite();
        vertex.materialized = false;
    }
    bool CanDraw(size_t a, size_t b, size_t c) const {
        return a < SlotCount && b < SlotCount && c < SlotCount &&
               vertices_[a].eligible && vertices_[b].eligible && vertices_[c].eligible &&
               vertices_[a].state == vertices_[b].state && vertices_[a].state == vertices_[c].state;
    }
    const Vertex& Get(size_t slot) const { return vertices_[slot]; }

    // Preserve alpha, including the interpreter's fog alpha. Return true only
    // for a newly materialized vertex so diagnostics count real CPU work.
    template <typename Color> bool Materialize(size_t slot, Color& color) {
        if (slot >= SlotCount || !vertices_[slot].eligible || vertices_[slot].materialized) return false;
        auto& vertex = vertices_[slot];
        const auto rgb = EvaluateNativeLighting(vertex.state, vertex.normal);
        color.r = rgb[0]; color.g = rgb[1]; color.b = rgb[2];
        vertex.materialized = true;
        return true;
    }

  private:
    std::array<Vertex, SlotCount> vertices_{};
};

} // namespace mk64_3ds

// Runtime work counters. The interpreter owns increments; diagnostics may
// sample these without introducing timers into the per-vertex hot path.
inline uint64_t gMk64NativeLightLoaded3DS = 0;
inline uint64_t gMk64NativeLightMaterialized3DS = 0;
inline uint64_t gMk64NativeLightTriangles3DS = 0;
