#pragma once
#include "native_mesh_draw_3ds.hpp"
#include "native_material_3ds.hpp"
#include "native_camera_3ds.hpp"
#include "native_visibility_3ds.hpp"
#include <vector>
#include <memory>

namespace mk64_3ds::native_scene {
// Resident geometry/materials, with explicit updates for mutable images.
// Load before submitting frames; the
// caller must fence GPU ownership before destroying a loaded instance.
class GpuScene {
public:
    ~GpuScene();
    bool Load(const char* directory);
    // Caller owns a completed GPU queue and no active frame may reference us.
    void ResetAfterCompletion() { Release(); }
    const Vertex* Vertices() const { return mesh_.vertices; }
    uint32_t VertexCount() const { return vertexCount_; }
    uint32_t ViewCount() const { return views_.size(); }
    uint32_t TextureCount() const { return textures_.size(); }
    // Startup-only mapping; records own their copy. Selection allocates no data.
    bool RegisterTextureVariant(const uint16_t* mapping,size_t count);
    // Call only after the GPU-completion fence. Updates are rejected after a
    // draw has started; geometry/material tables and GPU addresses stay fixed.
    void BeginAfterCompletion(){mutableWritesAllowed_=true;}
    bool UpdateTextureRgba5551(uint32_t texture,const uint8_t* source,size_t bytes,
        uint32_t stride,uint32_t width,uint32_t height);
    uint64_t MutableUploadBytes()const{return mutableUploadBytes_;}
    void UseTextureVariant(bool enabled) { useVariant_=enabled && !textureVariant_.empty(); }
    void BeginBatch();
    void SetLinearFiltering(bool linear) { if(binder_)binder_->SetLinearFiltering(linear); }
    // Retained full-range path for matched diagnosis, without changing assets.
    void SetVisibilityEnabled(bool enabled){visibilityEnabled_=enabled;}
    size_t VisibilityBytes()const{return visibility_.Bytes();}
    uint32_t Draw(uint32_t view,const Camera& camera,uint64_t& triangles,bool inBatch=false);
private:
    void Release();
    void* storage_=nullptr;
    GpuMeshView mesh_{};
    uint32_t vertexCount_=0;
    DVLB_s* binary_=nullptr;
    shaderProgram_s program_{};
    bool programReady_=false;
    int matrixUniform_=-1,uvUniform_=-1,clampUniform_=-1;
    std::vector<C3D_Tex> textures_;
    std::vector<Material> materials_;
    std::vector<RasterState> rasters_;
    std::vector<DrawRange> ranges_;
    std::vector<View> views_;
    std::vector<uint16_t> references_;
    std::unique_ptr<MaterialBinder> binder_;
    bool batchBound_=false;
    CameraUniformCache matrixCache_;
    NativeVisibilityPlan visibility_;bool visibilityEnabled_=true;
    std::vector<uint16_t> textureVariant_;bool useVariant_=false;
    bool mutableWritesAllowed_=false;
    uint64_t mutableUploadBytes_=0;
};
}
