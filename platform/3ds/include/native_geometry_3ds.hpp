#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mk64_3ds {

enum class NativeGeometryCull : std::uint8_t { None, Front, Back };

// A draw owns a copy: loaded vertex slots can be replaced while the previous
// triangle batch is still pending in the interpreter.
struct NativeGeometryDraw {
    float matrix[4][4] = {};
    // Nonzero identity belongs to an immutable matrix snapshot. It survives
    // pool-slot reuse and vertex reset; zero keeps externally built draws safe.
    std::uint64_t matrixGeneration = 0;
    float aspect = 1.0f;
    NativeGeometryCull cull = NativeGeometryCull::None;
    bool enabled = false;
};

inline bool SameNativeGeometryDraw(const NativeGeometryDraw& a,
                                   const NativeGeometryDraw& b) {
    return a.enabled == b.enabled && (!a.enabled ||
        (a.cull == b.cull && a.aspect == b.aspect &&
         ((a.matrixGeneration != 0 && a.matrixGeneration == b.matrixGeneration) ||
          std::memcmp(a.matrix, b.matrix, sizeof(a.matrix)) == 0)));
}

struct NativeGeometryCounters {
    std::uint64_t loaded = 0;
    std::uint64_t materialized = 0;
    std::uint64_t nativeTriangles = 0;
    std::uint64_t fallbackDisabled = 0;
    std::uint64_t fallbackMixedMatrix = 0;
    std::uint64_t fallbackNearEye = 0;
    std::uint64_t fallbackIneligible = 0;
    std::uint64_t matrixSnapshots = 0;
    std::uint64_t stateSwitches = 0;
};

// Bounded RSP sidecar. Only X/Y transforms are deferred: exact CPU Z/W retain
// N64 fog alpha, distance/LOD, BranchZ, far-plane rejection and eye clipping.
// PICA performs the complete object-to-clip transform for eligible triangles.
// The 64 RSP slots may each retain a distinct load-time matrix. A spare pool
// entry lets a replacement load acquire its matrix before capturing vertices.
class NativeGeometryState {
public:
    static constexpr std::size_t kVertexCount = 64;
    static constexpr float kClipWEpsilon = 1.0e-4f;

    struct Vertex {
        float object[3] = {};
        std::uint8_t matrix = 0;
        bool captured = false;
        bool materialized = false;
    };

    bool enabled = true;
    NativeGeometryCounters counters;

    void ResetVertices() {
        vertices_ = {};
        matrices_ = {};
        loadMatrix_ = 0;
        cachedDraw_ = {};
        cachedGeneration_ = 0;
    }

    void BeginLoad(std::size_t first, std::size_t count,
                   const float matrix[4][4], float aspect) {
        if (first >= kVertexCount || count > kVertexCount - first) return;
        for (std::size_t i = first; i < first + count; ++i) {
            if (vertices_[i].captured) --matrices_[vertices_[i].matrix].references;
            vertices_[i].captured = false;
        }
        Matrix& previous = matrices_[loadMatrix_];
        if (previous.valid && previous.aspect == aspect &&
            std::memcmp(previous.value, matrix, sizeof(previous.value)) == 0) return;
        for (std::size_t i = 0; i < matrices_.size(); ++i) {
            if (matrices_[i].references != 0) continue;
            Matrix& destination = matrices_[i];
            std::memcpy(destination.value, matrix, sizeof(destination.value));
            destination.aspect = aspect;
            destination.valid = true;
            destination.finite = std::isfinite(aspect);
            for (const auto& row : destination.value)
                for (float component : row) destination.finite &= std::isfinite(component);
            destination.generation = ++nextGeneration_;
            loadMatrix_ = static_cast<std::uint8_t>(i);
            ++counters.matrixSnapshots;
            return;
        }
        // 65 entries for at most 64 referenced slots makes this unreachable.
    }

    template <typename Coordinate, typename LoadedVertex>
    void Capture(std::size_t index, const Coordinate* object, LoadedVertex& loaded) {
        const Matrix& matrix = matrices_[loadMatrix_];
        const float position[3] = { float(object[0]), float(object[1]), float(object[2]) };
        CapturePosition(index, object, loaded,
            TransformComponent(position, matrix.value, 2),
            TransformComponent(position, matrix.value, 3));
    }

    template <typename Coordinate, typename LoadedVertex>
    void CapturePosition(std::size_t index, const Coordinate* object, LoadedVertex& loaded,
                         float z, float w) {
        if (index >= kVertexCount) return;
        Vertex& vertex = vertices_[index];
        if (vertex.captured) --matrices_[vertex.matrix].references;
        for (int i = 0; i < 3; ++i) vertex.object[i] = object[i];
        vertex.matrix = loadMatrix_;
        vertex.captured = true;
        vertex.materialized = false;
        Matrix& matrix = matrices_[loadMatrix_];
        ++matrix.references;
        loaded.x = loaded.y = 0.0f;
        loaded.z = z;
        loaded.w = w;
        loaded.clip_rej = loaded.z > loaded.w ? 32 : 0;
        ++counters.loaded;
    }

    template <typename LoadedVertex>
    void Materialize(std::size_t index, LoadedVertex& loaded) {
        if (index >= kVertexCount) return;
        Vertex& vertex = vertices_[index];
        if (!vertex.captured || vertex.materialized) return;
        const Matrix& matrix = matrices_[vertex.matrix];
        loaded.x = TransformComponent(vertex.object, matrix.value, 0) * matrix.aspect;
        loaded.y = TransformComponent(vertex.object, matrix.value, 1);
        loaded.clip_rej = ClipMask(loaded.x, loaded.y, loaded.z, loaded.w);
        vertex.materialized = true;
        ++counters.materialized;
    }

    // Eligibility must be decided before CPU culling and before writing the
    // triangle to the VBO. The caller flushes before changing its draw state.
    template <typename LoadedVertex>
    const NativeGeometryDraw& PrepareTriangle(std::size_t a, std::size_t b, std::size_t c,
                                       LoadedVertex* loaded, bool eligible,
                                       NativeGeometryCull cull) {
        NativeGeometryDraw& result = cachedDraw_;
        result.enabled = false;
        const bool valid = a < kVertexCount && b < kVertexCount && c < kVertexCount;
        if (!enabled) {
            ++counters.fallbackDisabled;
        } else if (!eligible || !valid || !vertices_[a].captured ||
                   !vertices_[b].captured || !vertices_[c].captured) {
            ++counters.fallbackIneligible;
        } else if (vertices_[a].matrix != vertices_[b].matrix ||
                   vertices_[a].matrix != vertices_[c].matrix) {
            ++counters.fallbackMixedMatrix;
        } else if (!SafeClip(loaded[a]) || !SafeClip(loaded[b]) || !SafeClip(loaded[c]) ||
                   !matrices_[vertices_[a].matrix].finite) {
            ++counters.fallbackNearEye;
        } else {
            const Matrix& matrix = matrices_[vertices_[a].matrix];
            if (cachedGeneration_ != matrix.generation) {
                std::memcpy(result.matrix, matrix.value, sizeof(result.matrix));
                result.aspect = matrix.aspect;
                result.matrixGeneration = matrix.generation;
                cachedGeneration_ = matrix.generation;
            }
            result.cull = cull;
            result.enabled = true;
            ++counters.nativeTriangles;
            return result;
        }
        Materialize(a, loaded[a]);
        Materialize(b, loaded[b]);
        Materialize(c, loaded[c]);
        return result;
    }

    const Vertex& GetVertex(std::size_t index) const { return vertices_[index]; }

    static std::uint8_t ClipMask(float x, float y, float z, float w) {
        return (x < -w ? 1 : 0) | (x > w ? 2 : 0) |
               (y < -w ? 4 : 0) | (y > w ? 8 : 0) | (z > w ? 32 : 0);
    }

private:
    struct Matrix {
        float value[4][4] = {};
        float aspect = 1.0f;
        std::uint8_t references = 0;
        bool valid = false;
        bool finite = false;
        std::uint64_t generation = 0;
    };
    std::array<Vertex, kVertexCount> vertices_ = {};
    std::array<Matrix, kVertexCount + 1> matrices_ = {};
    std::uint8_t loadMatrix_ = 0;
    NativeGeometryDraw cachedDraw_;
    std::uint64_t cachedGeneration_ = 0;
    // Shared across state instances so a draw from a different interpreter
    // sidecar cannot accidentally reuse the same identity. Reset never rewinds it.
    inline static std::uint64_t nextGeneration_ = 0;

    static float TransformComponent(const float* object, const float matrix[4][4], int column) {
        return object[0] * matrix[0][column] + object[1] * matrix[1][column] +
               object[2] * matrix[2][column] + matrix[3][column];
    }
    template <typename LoadedVertex>
    static bool SafeClip(const LoadedVertex& vertex) {
        return std::isfinite(vertex.w) && std::isfinite(vertex.z) && vertex.w >= kClipWEpsilon;
    }
};

// The port has one Fast3D interpreter. Metadata follows its persistent vertex
// slots across Run calls; resetting it per frame would lose load-time matrices.
inline NativeGeometryState gNativeGeometry3DS;

} // namespace mk64_3ds
