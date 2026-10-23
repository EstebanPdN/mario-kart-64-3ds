#include "native_mesh_draw_3ds.hpp"

#include <citro3d.h>

namespace mk64_3ds::native_scene {

void ConfigureVertexLayout() {
    auto* attributes = C3D_GetAttrInfo();
    AttrInfo_Init(attributes);
    AttrInfo_AddLoader(attributes, 0, GPU_SHORT, 3);
    AttrInfo_AddLoader(attributes, 1, GPU_SHORT, 2);
    AttrInfo_AddLoader(attributes, 2, GPU_UNSIGNED_BYTE, 4);
    // Consume the final two bytes explicitly. A 16-byte stride must describe
    // all 16 bytes; do not rely on emulator-tolerated implicit tail padding.
    // v3 is reserved and the native shader does not use it.
    AttrInfo_AddLoader(attributes, 3, GPU_SHORT, 1);
}

void BindMesh(const GpuMeshView& mesh) {
    auto* buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, mesh.vertices, sizeof(Vertex), 4, 0x3210);
}

void DrawIndices(const std::uint16_t* indices, std::uint32_t indexCount) {
    C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(indexCount),
                     C3D_UNSIGNED_SHORT, indices);
}

void DrawMesh(const GpuMeshView& mesh) {
    if (!mesh.vertices || !mesh.indices || !mesh.indexCount) return;
    BindMesh(mesh);
    DrawIndices(mesh.indices, mesh.indexCount);
}

} // namespace mk64_3ds::native_scene
