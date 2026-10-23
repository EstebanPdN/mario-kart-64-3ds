#pragma once

#include "native_mesh_format_3ds.hpp"

namespace mk64_3ds::native_scene {

// Explicit non-owning GPU view. Both buffers must be persistent linear memory,
// validated/uploaded and cache-clean before submission. The owner must outlive
// GPU completion; waiting only for VBlank is not a completion fence. The caller
// owns frame, projection, material, textures, depth and culling state.
struct GpuMeshView {
    const Vertex* vertices;
    const std::uint16_t* indices;
    std::uint32_t indexCount;
};

void ConfigureVertexLayout();
void BindMesh(const GpuMeshView& mesh);
void DrawIndices(const std::uint16_t* indices, std::uint32_t indexCount);
void DrawMesh(const GpuMeshView& mesh);

} // namespace mk64_3ds::native_scene
