#include "native_gpu_scene_3ds.hpp"
#include "native_dynamic_texture_3ds.hpp"
#include "native_course_shbin.h"
#include "gpu_command_budget_3ds.hpp"
#include "system_3ds.h"
#include <cstdio>
#include <cstring>
#include <string>
#ifdef __3DS__
#include "audio_runtime_3ds.h"
#endif

namespace mk64_3ds::native_scene {
namespace {
bool Read(const std::string& path,std::vector<uint8_t>& bytes) {
    FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;
    bool ok=std::fseek(f,0,SEEK_END)==0;long n=ok?std::ftell(f):-1;
    ok=n>0 && n<=8*1024*1024 && std::fseek(f,0,SEEK_SET)==0;
    if(ok) {
        bytes.resize(n);
        for(size_t at=0;ok && at<bytes.size();at+=65536) {
#ifdef __3DS__
            Mk64GameAudio3DSServiceLoading();
#endif
            const size_t count=std::min(size_t(65536),bytes.size()-at);
            ok=std::fread(bytes.data()+at,1,count,f)==count;
        }
    }
    std::fclose(f);return ok;
}
template<class T> void Copy(std::vector<T>& out,const uint8_t* data,uint32_t count) {
    out.resize(count);std::memcpy(out.data(),data,sizeof(T)*count);
}
}
GpuScene::~GpuScene(){Release();}
void GpuScene::Release() {
    binder_.reset();materials_.clear();ranges_.clear();views_.clear();references_.clear();
    visibility_.Reset();matrixCache_.Invalidate();
    textureVariant_.clear();useVariant_=false;batchBound_=false;mutableWritesAllowed_=false;
    rasters_.clear();
    for(auto& texture:textures_)C3D_TexDelete(&texture);
    textures_.clear();
    if(storage_)linearFree(storage_);
    storage_=nullptr;mesh_={};vertexCount_=0;
    if(programReady_)shaderProgramFree(&program_);
    programReady_=false;
    if(binary_)DVLB_Free(binary_);
    binary_=nullptr;
}
bool GpuScene::Load(const char* directory) {
    if(storage_ || binary_)return false;
    std::vector<uint8_t> mesh,data;MeshView checkedMesh;SceneView checked;
    const std::string root=std::string(directory)+"/";
    if(!Read(root+"mesh.ncm",mesh) || !ValidateMesh(mesh.data(),mesh.size(),checkedMesh) ||
       !Read(root+"scene.nlc",data) || !ValidateScene(data.data(),data.size(),checkedMesh.indexCount,checked))return false;
    materials_.resize(checked.materialCount);
    rasters_.clear();
    if(checked.materialStride>sizeof(Material))rasters_.resize(checked.materialCount);
    for(uint32_t i=0;i<checked.materialCount;++i) {
        const auto* record=checked.materials+i*checked.materialStride;
        std::memcpy(&materials_[i],record,sizeof(Material));
        if(!rasters_.empty())std::memcpy(&rasters_[i],record+sizeof(Material),sizeof(RasterState));
    }
    Copy(ranges_,checked.ranges,checked.rangeCount);
    Copy(views_,checked.views,checked.viewCount);Copy(references_,checked.references,checked.referenceCount);
    const size_t bytes=mesh.size()-16;
    storage_=linearMemAlign(bytes,128);if(!storage_)return false;
    std::memcpy(storage_,checkedMesh.vertices,bytes);
    if(R_FAILED(GSPGPU_FlushDataCache(storage_,bytes)))return false;
    vertexCount_=checkedMesh.vertexCount;
    mesh_={static_cast<Vertex*>(storage_),reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(storage_)+vertexCount_*16),checkedMesh.indexCount};
    // Optional CPU metadata only. A rejected allocation retains every draw.
    visibility_.Build(mesh_.vertices,vertexCount_,mesh_.indices,mesh_.indexCount,ranges_.data(),ranges_.size());
    textures_.reserve(checked.textureCount);
    for(uint32_t i=0;i<checked.textureCount;++i) {
        TextureInfo info;std::memcpy(&info,checked.textures+i*sizeof(info),sizeof(info));
        char name[48];std::snprintf(name,sizeof name,"texture-%03lu.ctex",static_cast<unsigned long>(i));
        std::vector<uint8_t> pixels;if(!Read(root+name,pixels) || pixels.size()!=info.bytes)return false;
        C3D_Tex texture{};if(!C3D_TexInit(&texture,info.width,info.height,GPU_RGBA8))return false;
        std::memcpy(texture.data,pixels.data(),pixels.size());C3D_TexFlush(&texture);textures_.push_back(texture);
    }
    binary_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(native_course_shbin)),native_course_shbin_size);
    if(!binary_)return false;
    if(R_FAILED(shaderProgramInit(&program_)))return false;
    programReady_=true;
    if(R_FAILED(shaderProgramSetVsh(&program_,&binary_->DVLE[0])))return false;
    matrixUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"mvp");
    uvUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"uvScaleOffset");
    clampUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"uvClamp");
    if(matrixUniform_<0 || uvUniform_<0 || clampUniform_<0)return false;
    binder_=std::make_unique<MaterialBinder>(uvUniform_,clampUniform_);return true;
}
bool GpuScene::RegisterTextureVariant(const uint16_t* mapping,size_t count) {
    if(!binder_ || !mapping || count!=textures_.size() || !textureVariant_.empty())return false;
    for(size_t i=0;i<count;++i)if(mapping[i]>=count)return false;
    textureVariant_.assign(mapping,mapping+count);return true;
}
void GpuScene::BeginBatch(){batchBound_=false;matrixCache_.Invalidate();if(binder_)binder_->Reset();}
bool GpuScene::UpdateTextureRgba5551(uint32_t id,const uint8_t* source,size_t bytes,
        uint32_t stride,uint32_t width,uint32_t height) {
    if(!mutableWritesAllowed_ || id>=textures_.size())return false;
    auto& texture=textures_[id];
    if(texture.width!=width || texture.height!=height || !texture.data ||
       !TileRgba5551(source,bytes,stride,width,height,static_cast<uint8_t*>(texture.data),
                    size_t(width)*height*4))return false;
    if(!Mk64System3DSCleanDataCache(texture.data,size_t(width)*height*4))return false;
    mutableUploadBytes_+=size_t(width)*height*4;return true;
}
uint32_t GpuScene::Draw(uint32_t viewId,const Camera& camera,uint64_t& triangles,bool inBatch) {
    mutableWritesAllowed_=false;
    if(!inBatch)BeginBatch();
    if(viewId>=views_.size() || !binder_)return 0;
    if(visibilityEnabled_)visibility_.SetCamera(camera);
    uint32_t draws=0;bool cameraChecked=false;const auto& view=views_[viewId];
    for(uint32_t i=0;i<view.referenceCount;++i) {
        const auto id=references_[view.firstReference+i];const auto& range=ranges_[id];
        NativeVisibleRanges selected;
        if(visibilityEnabled_)selected=visibility_.Select(id,range);
        else selected.Append({range.first,range.count});
        if(!selected.count)continue;
        uint32_t* buffer=nullptr;uint32_t capacity=0,offset=0;GPUCMD_GetBuffer(&buffer,&capacity,&offset);
        if(!buffer || !GpuCommandRoom(capacity,offset,kGpuCommandDrawWords+kGpuCommandTailWords))throw GpuCommandPressure();
        if(!batchBound_) {
            C3D_BindProgram(&program_);ConfigureVertexLayout();BindMesh(mesh_);
            C3D_TexEnvBufUpdate(C3D_Both,0);C3D_TexEnvBufColor(0xffffffff);
            for(int stage=0;stage<6;++stage)C3D_TexEnvInit(C3D_GetTexEnv(stage));
            batchBound_=true;
        }
        if(!cameraChecked) {
            if(matrixCache_.Update(camera)) {
                C3D_Mtx matrix;
                for(unsigned r=0;r<4;++r){auto& row=matrix.r[r];const auto* p=camera.rows[r];row.x=p[0];row.y=p[1];row.z=p[2];row.w=p[3];}
                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,matrixUniform_,&matrix);
                ++gNativeVisibilityStatistics.matrixUploads;
            }else ++gNativeVisibilityStatistics.matrixReuses;
            cameraChecked=true;
        }
        const auto& original=materials_[range.material];const Material* material=&original;Material variant;
        if(useVariant_ && textureVariant_[original.texture]!=original.texture) {
            variant=original;variant.texture=textureVariant_[original.texture];material=&variant;
        }
        const auto* raster=rasters_.empty()?nullptr:&rasters_[range.material];
        if(!binder_->Apply(*material,textures_.data(),raster))continue;
        for(unsigned span=0;span<selected.count;++span) {
            uint32_t* buffer=nullptr;uint32_t capacity=0,offset=0;GPUCMD_GetBuffer(&buffer,&capacity,&offset);
            if(!buffer || !GpuCommandRoom(capacity,offset,kGpuCommandDrawWords+kGpuCommandTailWords))throw GpuCommandPressure();
            const auto& part=selected.spans[span];
            DrawIndices(mesh_.indices+part.first,part.count);++draws;triangles+=part.count/3;
        }
    }
    return draws;
}
}
