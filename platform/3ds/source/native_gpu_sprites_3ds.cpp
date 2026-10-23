#include "native_gpu_sprites_3ds.hpp"
#include "native_sprite_page_trace_3ds.hpp"
#include "native_mesh_draw_3ds.hpp"
#include "native_course_shbin.h"
#include "gpu_command_budget_3ds.hpp"
#include "system_3ds.h"
#include "native_sprite_cache_budget_3ds.hpp"
#include "game_runtime_3ds.h"
#include "performance_trace_3ds.hpp"
#include "native_visibility_3ds.hpp"
#include "native_fast_page_3ds.hpp"
#include <stdexcept>
namespace mk64_3ds::native_scene {
namespace {
uint32_t Color(const uint8_t rgba[4]) {return uint32_t(rgba[0])|uint32_t(rgba[1])<<8|uint32_t(rgba[2])<<16|uint32_t(rgba[3])<<24;}
}
GpuSprites::~GpuSprites() {
    // As for GpuScene, the owner must fence queued commands before destruction.
    for(unsigned i=0;i<allocatedTextures_;++i)C3D_TexDelete(&textures_[i]);
    for(unsigned i=0;i<allocatedStatic_;++i)C3D_TexDelete(&staticTextures_[i]);
    if(vertices_)linearFree(vertices_);
    if(indices_)linearFree(indices_);
    if(programReady_)shaderProgramFree(&program_);
    if(binary_)DVLB_Free(binary_);
}
bool GpuSprites::Load(const char* path,Decoder decoder) {
    if(binary_ || !decoder)return false;
    const std::string fastPath=std::string(path)+".fast";
    FILE* fastFile=std::fopen(fastPath.c_str(),"rb");
    const bool haveFastFile=fastFile!=nullptr;if(fastFile)std::fclose(fastFile);
    if(!pack_.Open(haveFastFile?fastPath.c_str():path))return false;
    originalTilesPath_=std::string(path)+".original.ntl";
    decode_=decoder;
    if(pack_.IsPaged()) {
        if(!pageCache_.Initialize(4))return false;
        size_t maximum=0;
        for(unsigned i=0;const auto* entry=pack_.At(i);++i)for(unsigned v=0;v<8;++v)maximum=std::max(maximum,size_t(entry->packed[v]));
        if(pack_.IsFastPaged())oversizedPacked_.resize(kSpritePackedCapacity+kSpritePageBytes);
        else if(maximum>packed_.size())oversizedPacked_.resize(maximum);
    }
    return Initialize(kPackets,true);
}
bool GpuSprites::PrepareTextureMode(bool improved,size_t budget,bool loadingRetry) {
    const int mode=improved?1:0;
    if(preparedMode_==mode && (!loadingRetry || ResidentBytes()))return ResidentBytes()!=0;
    preparedMode_=mode;
    // Reclaim an earlier decoded-cache top-up exactly once. budget already
    // includes replacement of the compressed/raw range and the 8 MiB reserve.
    const size_t base=4*kSpritePageBytes;
    budget+=pageCache_.Bytes()>base?pageCache_.Bytes()-base:0;
    const size_t desired=DecodedSpritePageCapacity(improved,budget,pack_.RequiredResidentBytes(improved));
    pageCache_.Resize(desired); // Allocation failure retains the smaller cache.
    const size_t extra=pageCache_.Bytes()>base?pageCache_.Bytes()-base:0;
    budget=budget>extra?budget-extra:0;
    if(!improved) {
        pack_.DropResident();
        if(originalTiles_.Load(originalTilesPath_.c_str(),budget,pack_))return true;
    } else originalTiles_.Drop();
    return pack_.MakeResident(improved,budget);
}
bool GpuSprites::LoadEffects(Decoder decoder) {
    if(binary_ || !decoder)return false;
    decode_=decoder;return Initialize(256,false);
}
bool GpuSprites::Initialize(unsigned packets,bool characterCache) {
    packetCapacity_=packets;deferGeometryCleaning_=true;
    vertices_=static_cast<Vertex*>(linearMemAlign(packets*8*sizeof(Vertex),128));
    indices_=static_cast<uint16_t*>(linearMemAlign(packets*12*sizeof(uint16_t),128));
    if(!vertices_ || !indices_)return false;
    if(!PrepareSpriteIndices(packets*2,indices_,packets*12) ||
       !Mk64System3DSCleanDataCache(indices_,packets*12*sizeof(uint16_t)))return false;
    if(characterCache) {
        const bool newProfile = Mk64Graphics3DSResolvedPerformanceProfile()==MK64_PERFORMANCE_PROFILE_NEW_3DS &&
                                Mk64Graphics3DSResolvedOutputWidth()==400;
        const unsigned slots = Mk64Graphics3DSResolvedOutputWidth()==400 ? NativeSpriteCacheSlots(newProfile,linearSpaceFree()) : 32;
        for(unsigned i=0;i<slots;++i) {
            auto& texture=textures_[i];
            // Additional slots are an optimization, not a boot requirement.
            if(!C3D_TexInit(&texture,64,64,GPU_RGBA8)) {
                if(allocatedTextures_<32)return false;
                break;
            }
            ++allocatedTextures_;
            C3D_TexSetFilter(&texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
        }
        if(!residency_.SetCapacity(allocatedTextures_))return false;
    }
    binary_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(native_course_shbin)),native_course_shbin_size);if(!binary_)return false;
    if(R_FAILED(shaderProgramInit(&program_)))return false;
    programReady_=true;
    if(R_FAILED(shaderProgramSetVsh(&program_,&binary_->DVLE[0])))return false;
    matrixUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"mvp");
    uvUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"uvScaleOffset");
    clampUniform_=shaderInstanceGetUniformLocation(program_.vertexShader,"uvClamp");
    ready_=matrixUniform_>=0 && uvUniform_>=0 && clampUniform_>=0;
    return ready_;
}
bool GpuSprites::LoadStatic(const char* path,uint32_t& key) {
    if(!ready_ || statistics_.draws || allocatedStatic_==staticTextures_.size())return false;
    FILE* file=std::fopen(path,"rb");if(!file)return false;
    uint8_t header[20]{};
    bool ok=std::fread(header,1,sizeof header,file)==sizeof header &&
        std::memcmp(header,"NSI1",4)==0 && ReadLE32(header+4)==1 && ReadLE32(header+8)==16384;
    const uint32_t packed=ReadLE32(header+16);
    ok=ok && packed && packed<=packed_.size();
    if(ok)ok=std::fread(packed_.data(),1,packed,file)==packed && std::fgetc(file)==EOF;
    std::fclose(file);if(!ok)return false;
    auto& texture=staticTextures_[allocatedStatic_];
    if(!C3D_TexInit(&texture,64,64,GPU_RGBA8))return false;
    ++allocatedStatic_;
    if(!decode_(packed_.data(),packed,static_cast<uint8_t*>(texture.data),16384,ReadLE32(header+12)))return false;
    C3D_TexSetFilter(&texture,GPU_LINEAR,GPU_LINEAR);
    C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);C3D_TexFlush(&texture);
    key=0x80000000u|(allocatedStatic_-1);return true;
}
void GpuSprites::BeginAfterCompletion(){
    if(!ready_)throw std::runtime_error("Native sprite resources are not ready");
    if(deferGeometryCleaning_ && (flushedVertices_!=usedVertices_ || flushedIndices_!=usedIndices_))
        throw std::runtime_error("Unpublished native effect geometry at frame reset");
    residency_.BeginAfterCompletion();usedVertices_=usedIndices_=0;
    flushedVertices_=flushedIndices_=0;
    batchBound_=materialValid_=false;boundTexture_=nullptr;
    matrixCache_.Invalidate();
}
void GpuSprites::BeginBatch() {
    if(deferGeometryCleaning_ && (flushedVertices_!=usedVertices_ || flushedIndices_!=usedIndices_))
        throw std::runtime_error("Unpublished native effect geometry at batch begin");
    batchBound_=materialValid_=false;boundTexture_=nullptr;++statistics_.batches;
    matrixCache_.Invalidate();
}
void GpuSprites::EndBatch() {
    if(!deferGeometryCleaning_)return;
    const size_t vertexBytes=(usedVertices_-flushedVertices_)*sizeof(Vertex);
    if(!vertexBytes)return;
    if(!Mk64System3DSCleanDataCache(vertices_+flushedVertices_,vertexBytes))
        throw std::runtime_error("Native effect batch buffer flush");
    ++statistics_.geometryCleanCalls;statistics_.geometryCleanBytes+=vertexBytes;
    flushedVertices_=usedVertices_;flushedIndices_=usedIndices_;
}
bool GpuSprites::Resolve(const char* name,const uint8_t* palette,bool improved,uint32_t& key) const {
    if(!ready_)return false;
    unsigned wheel=0;uint32_t index=0;const auto* entry=pack_.Find(SpriteIdentity(name,wheel),index);if(!entry)return false;
    if(improved){key=index*8+4+wheel;return true;}
    if(!palette)return false;
    const uint64_t hash=SpritePaletteHash(palette,entry->paletteMask);
    for(unsigned i=0;i<4;++i)if(entry->paletteHash[i]==hash){key=index*8+i;return true;}
    return false; // An uncompiled palette is explicit; never show another racer.
}
void GpuSprites::Draw(uint32_t key,const Vertex vertices[8],const Camera& camera,const SpriteStyle& style) {
    if(style.cull==3)return;
    if(style.cull>3)throw std::runtime_error("Native sprite culling state");
    if(!ready_ || !decode_ || !style.quads || style.quads>2 || style.technique>SpriteStyle::ColorLerp ||
       !style.logicalWidth || style.logicalWidth>64 || !style.logicalHeight || style.logicalHeight>64 ||
       usedVertices_+style.quads*4>packetCapacity_*8 || usedIndices_+style.quads*6>packetCapacity_*12)
        throw std::runtime_error("Native sprite packet capacity/style");
    uint32_t* commandBuffer=nullptr;uint32_t capacity=0,offset=0;GPUCMD_GetBuffer(&commandBuffer,&capacity,&offset);
    if(!commandBuffer || !GpuCommandRoom(capacity,offset,kGpuCommandDrawWords+kGpuCommandTailWords))throw GpuCommandPressure();
    C3D_Tex* texture=nullptr;
    if(key&0x80000000u) {
        const unsigned slot=key&0x7fffffffu;
        if(slot>=allocatedStatic_)throw std::runtime_error("Native immutable sprite key");
        texture=&staticTextures_[slot];
    } else {
    if(!allocatedTextures_)throw std::runtime_error("Effect renderer cannot use a character key");
    auto& performance=PerformanceCurrent();
    performance.native_cache_slots=allocatedTextures_;
    performance.native_resident_bytes=ResidentBytes();
    performance.native_raw_resident_bytes=originalTiles_.ResidentBytes();
    int slot=residency_.Find(key);
    if(slot<0) {
        slot=residency_.Candidate();if(slot<0)throw std::runtime_error("Native sprite residency exhausted before GPU completion");
        const uint32_t index=key/8,variant=key%8;const auto* entry=pack_.At(index);size_t packed=0;
        ++performance.native_cache_misses;
        bool decoded=false;
        auto read=[&]() {
            PerformanceTimer timer(performance.native_io_us);
            auto* data=oversizedPacked_.empty()?packed_.data():oversizedPacked_.data();
            const size_t capacity=oversizedPacked_.empty()?packed_.size():oversizedPacked_.size();
            const bool ok=entry && pack_.ReadPacked(index,variant,data,capacity,packed);
            performance.native_io_bytes+=pack_.LastReadBytes();return ok;
        };
        const bool rawOriginal=originalTiles_.Copy(key,textures_[slot].data,16384);
        if(rawOriginal) {
            decoded=true;++performance.native_raw_uploads;
            performance.native_raw_resident_bytes=originalTiles_.ResidentBytes();
        } else if(pack_.IsPaged()) {
            bool pageDecoded=false;
            const auto* page=pageCache_.Load(index*2+variant/4,[&](uint8_t* page) {
                pageDecoded=true;
                if(!read())return false;
                ++performance.native_page_decodes;
                PerformanceTimer timer(performance.native_decode_us);
                if(pack_.IsFastPaged())return DecodeFastSpritePage(oversizedPacked_.data(),packed,page,
                    oversizedPacked_.data()+kSpritePackedCapacity);
                return DecodeSpritePage(oversizedPacked_.empty()?packed_.data():oversizedPacked_.data(),packed,page,decode_);
            });
            gNativeSpritePageTrace.Record(index*2+variant/4,pageCache_.ActiveCapacity(),pageDecoded);
            decoded=entry && ExtractSpritePage(page,variant%4,entry->crc[variant],static_cast<uint8_t*>(textures_[slot].data));
        } else if(read()) {
            PerformanceTimer timer(performance.native_decode_us);
            decoded=decode_(packed_.data(),packed,static_cast<uint8_t*>(textures_[slot].data),16384,entry->crc[variant]);
        }
        performance.native_resident_bytes=ResidentBytes();
        if(!decoded)throw std::runtime_error("Native sprite payload failed validation");
        textures_[slot].fmt=rawOriginal?GPU_RGBA5551:GPU_RGBA8;
        // These are fixed 64x64 RGBA8 linear allocations. Publish exactly the
        // written image through the existing direct-cache helper, retaining
        // its service fallback without unconditional GSP IPC on every miss.
        const size_t uploadedBytes=rawOriginal?8192:kSpriteImageBytes;
        if(!Mk64System3DSCleanDataCache(textures_[slot].data,uploadedBytes))
            throw std::runtime_error("Native sprite texture publication failed");
        if(!residency_.Publish(slot,key))throw std::runtime_error("Native sprite residency publication rejected");
        ++statistics_.misses;statistics_.uploadBytes+=uploadedBytes;
    }else ++statistics_.hits;
    texture=&textures_[slot];
    }
    const unsigned vertexCount=style.quads*4,indexCount=style.quads*6;
    auto* output=vertices_+usedVertices_;auto* indices=indices_+usedIndices_;
    std::memcpy(output,vertices,vertexCount*sizeof(Vertex));usedVertices_+=vertexCount;usedIndices_+=indexCount;
    if(style.technique==SpriteStyle::TextureTint || style.technique==SpriteStyle::ColorLerp)
        for(unsigned i=0;i<vertexCount;++i)std::memcpy(output[i].color,style.primitive,4);
    if(!deferGeometryCleaning_) {
        if(!Mk64System3DSCleanDataCache(output,vertexCount*sizeof(Vertex)))throw std::runtime_error("Native sprite buffer flush");
        ++statistics_.geometryCleanCalls;
        statistics_.geometryCleanBytes+=vertexCount*sizeof(Vertex);
    }
    if(!batchBound_) {
        C3D_BindProgram(&program_);ConfigureVertexLayout();BindMesh({vertices_,indices_,0});
        C3D_FVUnifSet(GPU_VERTEX_SHADER,uvUniform_,1.f/2048,1.f/2048,.5f/64,.5f/64);
    }
    if(!batchBound_ || boundClampLogical_!=style.clampLogical ||
       boundLogicalWidth_!=style.logicalWidth || boundLogicalHeight_!=style.logicalHeight) {
        C3D_FVUnifSet(GPU_VERTEX_SHADER,clampUniform_,
            style.clampLogical?(style.logicalWidth-.5f)/64:1e6,
            style.clampLogical?(style.logicalHeight-.5f)/64:1e6,0,0);
        boundClampLogical_=style.clampLogical;boundLogicalWidth_=style.logicalWidth;
        boundLogicalHeight_=style.logicalHeight;
    }
    batchBound_=true;
    if(matrixCache_.Update(camera)) {
        C3D_Mtx matrix;
        for(unsigned i=0;i<4;++i){auto& r=matrix.r[i];const auto* p=camera.rows[i];r.x=p[0];r.y=p[1];r.z=p[2];r.w=p[3];}
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,matrixUniform_,&matrix);
        ++gNativeVisibilityStatistics.matrixUploads;
    }else ++gNativeVisibilityStatistics.matrixReuses;
    if(!materialValid_ || !SameSpriteMaterial(previousStyle_,style)) {
    C3D_TexEnvBufUpdate(C3D_Both,0);C3D_TexEnvBufColor(0xffffffff);for(unsigned i=0;i<6;++i)C3D_TexEnvInit(C3D_GetTexEnv(i));
    if(style.technique==SpriteStyle::TextureTint || style.technique==SpriteStyle::ColorLerp) {
        auto* product=C3D_GetTexEnv(0);
        // Match the established backend's primitive input through the vertex
        // shader/primary-color quantization, not a higher-precision constant.
        C3D_TexEnvSrc(product,C3D_RGB,GPU_TEXTURE0,GPU_PRIMARY_COLOR);
        C3D_TexEnvOpRgb(product,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR);
        C3D_TexEnvFunc(product,C3D_RGB,GPU_MODULATE);
        C3D_TexEnvSrc(product,C3D_Alpha,GPU_TEXTURE0,GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(product,C3D_Alpha,GPU_MODULATE);
        if(style.technique==SpriteStyle::ColorLerp) {
            C3D_TexEnvSrc(product,C3D_RGB,GPU_PRIMARY_COLOR,GPU_CONSTANT,GPU_TEXTURE0);
            C3D_TexEnvFunc(product,C3D_RGB,GPU_INTERPOLATE);
            C3D_TexEnvColor(product,Color(style.environment));
        }
    } else {
    // Kart color is (1-environment)*texture+primitive; alpha is texture*primitive.a.
    auto* product=C3D_GetTexEnv(0);C3D_TexEnvSrc(product,C3D_RGB,GPU_TEXTURE0,GPU_CONSTANT);
    C3D_TexEnvOpRgb(product,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR);C3D_TexEnvFunc(product,C3D_RGB,GPU_MODULATE);
    C3D_TexEnvSrc(product,C3D_Alpha,GPU_TEXTURE0);C3D_TexEnvFunc(product,C3D_Alpha,GPU_REPLACE);C3D_TexEnvColor(product,Color(style.environment));
    auto* addition=C3D_GetTexEnv(1);C3D_TexEnvSrc(addition,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT);C3D_TexEnvFunc(addition,C3D_RGB,GPU_ADD);
    C3D_TexEnvSrc(addition,C3D_Alpha,GPU_PREVIOUS,GPU_CONSTANT);C3D_TexEnvFunc(addition,C3D_Alpha,GPU_MODULATE);C3D_TexEnvColor(addition,Color(style.primitive));
    if(style.technique==SpriteStyle::ConstantRgb) {
        C3D_TexEnvSrc(product,C3D_RGB,GPU_CONSTANT);C3D_TexEnvFunc(product,C3D_RGB,GPU_REPLACE);
        C3D_TexEnvColor(product,Color(style.primitive));
        C3D_TexEnvSrc(addition,C3D_RGB,GPU_PREVIOUS);C3D_TexEnvFunc(addition,C3D_RGB,GPU_REPLACE);
    }
    }
    C3D_AlphaTest(style.alphaTest,GPU_GREATER,style.cutout?48:8);
    C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,style.blend?GPU_SRC_ALPHA:GPU_ONE,style.blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO,
        GPU_ONE,style.blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO);
    C3D_CullFace(style.cull==2?GPU_CULL_BACK_CCW:(style.cull==1?GPU_CULL_FRONT_CCW:GPU_CULL_NONE));
    previousStyle_=style;materialValid_=true;++statistics_.materialChanges;
    }
    if(boundTexture_!=texture){C3D_TexBind(0,texture);boundTexture_=texture;++statistics_.textureBinds;}
    DrawIndices(indices,indexCount);++statistics_.draws;
}
}
