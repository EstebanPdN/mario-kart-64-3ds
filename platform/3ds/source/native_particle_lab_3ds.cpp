#include "texture_mode_3ds.hpp"
#include "native_data_path_3ds.hpp"
// Development-only ordered native particles. No source-list interpretation,
// image decode, texture upload, allocation or resource lookup during capture.
#include "native_particle_lab_3ds.h"
#include "native_particle_pack_3ds.hpp"
#include "native_particle_state_3ds.hpp"
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
#include "native_particle_compare_3ds.h"
#include "native_comparison_span_3ds.hpp"
#endif
#include "native_gpu_sprites_3ds.hpp"
#include "texture_lease_3ds.hpp"
#include "improved_texture_pack_3ds.hpp"
#include "settings_3ds.h"
#include "gfx_citro3d.h"
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace ns=mk64_3ds::native_scene;
extern "C" Mtx* Mk64NativeObjectMatrixRecord(const float*);
extern "C" bool Mk64FrameInterpolation3DSLookup(const Mtx*,MtxF*);
namespace {
constexpr unsigned kCapacity=512;
struct Image {
    ns::ParticleImage description{};std::string name;
    std::shared_ptr<Fast::Texture> original,improved;
    uint32_t originalKey=0,improvedKey=0;
};
struct Packet {
    Mk64NativeParticle3DS particle{};const Mtx* matrixKey=nullptr;
    Fast::F3DVtx legacy[4]{};ns::Vertex vertices[8]{};
    uint16_t image=0;Mk64NativeParticleQuad quad=MK64_PARTICLE_QUAD_32;bool improved=false;
};
ns::GpuSprites sprites;
std::vector<Image> images;
// The stock leaf pass sets its source even when no leaves are visible. Keep
// that shared wrapper/source alive, so repeated setup does not inflate and
// reconstruct the same 1 KiB resource every frame. At most one owner per mode.
std::array<std::shared_ptr<Ship::IResource>,2> leafSetupOwners;
void PrepareLeafSetup() {
    const bool improved=mk64_3ds::ImprovedRaceTextures(Mk64Settings3DSGetUiTextures());
    auto& owner=leafSetupOwners[improved?1:0];
    if(owner)return;
    owner=Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
        "textures/common_data/common_texture_particle_leaf");
    if(!owner)throw std::runtime_error("Native leaf setup source missing");
}
std::array<std::array<ns::Vertex,4>,3> quads{};
std::array<Packet,kCapacity> packets{};unsigned used=0;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
std::array<mk64_3ds::NativeComparisonSpan,kCapacity> compareSpans{};
bool referenceParticles=false,eagerExits=false;
uint64_t referenceReached=0;
#endif
bool ready=false;
uint64_t captured=0,drawn=0,interpolated=0,drawTicks=0,exitTicks=0,scopes=0;
uint64_t batchStarted=0,exitApplications=0;
// Native particles never consume the interpreter's texture tiles or loaded
// vertices. Publish only the final writer before any non-particle consumer.
Fast::Interpreter* exitInterpreter=nullptr;
const Packet* exitPacket=nullptr;
std::array<uint64_t,4> quadCaptured{},quadDrawn{};
std::vector<uint8_t> ReadIndex(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)throw std::runtime_error("Native particle index missing");
    std::fseek(f,0,SEEK_END);const long n=std::ftell(f);std::rewind(f);
    if(n<32 || n>32768){std::fclose(f);throw std::runtime_error("Native particle index size");}
    std::vector<uint8_t> bytes(n);const bool ok=std::fread(bytes.data(),1,n,f)==size_t(n);std::fclose(f);
    if(!ok)throw std::runtime_error("Native particle index read");return bytes;
}
std::shared_ptr<Fast::Texture> OriginalOwner(const Image& image) {
    auto lease=std::make_shared<mk64_3ds::TextureLease3DS>(image.name.c_str());
    if(!lease->Valid())throw std::runtime_error("Native particle source lease missing");
    const auto& data=lease->Get();const auto& d=image.description;
    constexpr unsigned types[]={2,6,8};
    if(data.type!=types[d.format] || data.width!=d.width || data.height!=d.height ||
       data.size!=unsigned(d.width)*d.height*(d.format==MK64_PARTICLE_RGBA5551?2:1))
        throw std::runtime_error("Native particle source lease differs from compiled image");
    auto init=std::make_shared<Ship::ResourceInitData>();init->Path=image.name;
    auto owner=std::shared_ptr<Fast::Texture>(new Fast::Texture(init),[lease](Fast::Texture* p){delete p;});
    owner->mImageBuffer=std::make_shared<std::vector<char>>();owner->ImageData=const_cast<uint8_t*>(data.data);
    owner->ImageDataSize=data.size;owner->Width=data.width;owner->Height=data.height;
    owner->Type=static_cast<Fast::TextureType>(data.type);return owner;
}
std::shared_ptr<Fast::Texture> ImprovedOwner(const Image& image) {
    // Match the existing resource manager's replacement namespace and choice.
    if(!image.name.starts_with("textures/common_data/"))return {};
    const auto& d=image.description;
    auto enhanced=mk64_3ds::improved::LocalPack().Load(image.name.c_str(),d.width,d.height,true);
    if(!enhanced || enhanced.width!=d.width || enhanced.height!=d.height)return {};
    if(enhanced.logicalWidth!=d.width || enhanced.logicalHeight!=d.height)
        throw std::runtime_error("Native particle improved logical bounds unsupported");
    auto init=std::make_shared<Ship::ResourceInitData>();init->Path=image.name;
    auto owner=std::make_shared<Fast::Texture>(init);owner->Type=Fast::TextureType::RGBA32bpp;
    owner->Width=d.width;owner->Height=d.height;owner->Flags=TEX_FLAG_LOAD_AS_RAW;
    owner->HByteScale=d.format==MK64_PARTICLE_RGBA5551?2.f:4.f;owner->VPixelScale=1;
    owner->mImageBuffer=enhanced.pixels;owner->ImageData=reinterpret_cast<uint8_t*>(enhanced.pixels->data());
    owner->ImageDataSize=enhanced.pixels->size();return owner;
}
Fast::RawTexMetadata Metadata(const std::shared_ptr<Fast::Texture>& owner) {
    Fast::RawTexMetadata m{};m.width=owner->Width;m.height=owner->Height;m.h_byte_scale=owner->HByteScale;
    m.v_pixel_scale=owner->VPixelScale;m.type=owner->Type;m.resource=owner;return m;
}
void RestoreImage(Fast::Interpreter* gfx,const Packet& p) {
    const auto& image=images[p.image];const auto& owner=p.improved?image.improved:image.original;
    const auto& d=image.description;const unsigned format=d.format==MK64_PARTICLE_RGBA5551?0:
        (d.format==MK64_PARTICLE_INTENSITY8?4:3);
    // LoadTextureBlock always uses a 16-bit load tile; the render tile keeps
    // the authored size, logical bounds and NOMASK-normalized clamp policy.
    gfx->GfxDpSetTextureImage(format,2,1,image.name.c_str(),owner->Flags,Metadata(owner),owner->ImageData);
    gfx->GfxDpSetTile(format,2,0,0,7,0,2,0,0,2,0,0);
    const unsigned size=d.format==MK64_PARTICLE_RGBA5551?2:1;
    gfx->GfxDpSetTile(format,size,((d.width*(size==2?2:1))+7)/8,0,0,0,2,0,0,2,0,0);
    gfx->GfxDpSetTileSize(0,0,0,(d.width-1)*4,(d.height-1)*4);
    auto& loaded=gfx->mRdp->loaded_texture[0];loaded.addr=owner->ImageData;
    loaded.orig_size_bytes=unsigned(d.width)*d.height*(size==2?2:1);
    loaded.size_bytes=loaded.line_size_bytes=loaded.full_image_line_size_bytes=
        uint32_t(loaded.orig_size_bytes*owner->HByteScale*owner->VPixelScale);
    loaded.tex_flags=owner->Flags;loaded.raw_tex_metadata=Metadata(owner);loaded.masked=loaded.blended=false;
    gfx->mRdp->textures_changed[0]=true;
}
void ApplyParticleExit(Fast::Interpreter* gfx,const Packet& p) {
    gfx->GfxSpTexture(32768,32768,0,0,1);gfx->GfxSpVertex(4,0,p.legacy);
    gfx->GfxSpTexture(1,1,0,0,0);RestoreImage(gfx,p);++exitApplications;
}
void ClearParticleExit(){exitInterpreter=nullptr;exitPacket=nullptr;}
void SyncViewportAndDepth(Fast::Interpreter* gfx) {
    const auto& rdp=*gfx->mRdp;auto& state=gfx->mRenderingState;
    const bool depth=((gfx->mRsp->geometry_mode&G_ZBUFFER)||(rdp.other_mode_l&G_ZS_PRIM))&&(rdp.other_mode_l&Z_CMP);
    const bool write=rdp.other_mode_l&Z_UPD,decal=(rdp.other_mode_l&ZMODE_DEC)==ZMODE_DEC;
    gfx->mRapi->SetDepthTestAndMask(depth,write);state.depth_test_and_mask=(depth?1:0)|(write?2:0);
    gfx->mRapi->SetZmodeDecal(decal);state.decal_mode=decal;
    gfx->mRapi->SetViewport(rdp.viewport.x,rdp.viewport.y,rdp.viewport.width,rdp.viewport.height);state.viewport=rdp.viewport;
    gfx->mRapi->SetScissor(rdp.scissor.x,rdp.scissor.y,rdp.scissor.width,rdp.scissor.height);state.scissor=rdp.scissor;
    gfx->mRdp->viewport_or_scissor_changed=false;
}
}
extern "C" void Mk64NativeParticleLoad() {
    if(ready)throw std::runtime_error("Native particle initialization repeated");
    const auto bytes=ReadIndex(MK64_NATIVE_DATA_ROOT "native-particles/index.npf");ns::ParticlePackView view;
    if(!ns::ValidateParticlePack(bytes.data(),bytes.size(),view) || !sprites.LoadEffects(mk64_3ds::improved::DecodePayload))
        throw std::runtime_error("Native particle resources invalid");
    std::memcpy(quads.data(),view.quads,sizeof quads);images.reserve(view.imageCount);
    for(unsigned i=0;i<view.imageCount;++i) {
        Image image;std::memcpy(&image.description,view.images+i*sizeof(ns::ParticleImage),sizeof image.description);
        image.name=reinterpret_cast<const char*>(bytes.data()+image.description.nameOffset);
        image.original=OriginalOwner(image);image.improved=ImprovedOwner(image);
        char path[96];std::snprintf(path,sizeof path,MK64_NATIVE_DATA_ROOT "native-particles/image-%02u.nsi",i);
        if(!sprites.LoadStatic(path,image.originalKey))throw std::runtime_error("Native particle immutable image invalid");
        if(image.improved) {
            std::snprintf(path,sizeof path,MK64_NATIVE_DATA_ROOT "native-particles/improved-%02u.nsi",i);
            if(!sprites.LoadStatic(path,image.improvedKey))throw std::runtime_error("Native particle improved image invalid");
        }else {image.improved=image.original;image.improvedKey=image.originalKey;}
        images.push_back(std::move(image));
    }
    PrepareLeafSetup();
    ready=true;
}
extern "C" void Mk64NativeParticleReset(){
    if(ready)PrepareLeafSetup();
    if(exitPacket)throw std::runtime_error("Native particle exit pending at capture reset");
    used=0;
}
extern "C" void Mk64NativeParticleGpuFrameReady(){
    if(exitPacket)throw std::runtime_error("Native particle exit pending at GPU frame reset");
    if(ready)sprites.BeginAfterCompletion();
}
static void CaptureParticle(const void* anchor,const Mk64NativeParticle3DS* particle,
        Mk64NativeParticleQuad quad,const Mtx* recordedKey) {
    if(!ready || !anchor || used==packets.size() || !Mk64NativeParticleValid(particle,quad))
        throw std::runtime_error("Native particle capture limit/contract");
    const char* name=particle->image;if(std::strncmp(name,"__OTR__",7)==0)name+=7;
    size_t length=0;while(length<256 && name[length])++length;
    if(length==256)throw std::runtime_error("Native particle image name too long");
    const uint64_t identity=ns::ParticleImageIdentity(name,length);
    unsigned image=0;for(;image<images.size();++image) {
        const auto& d=images[image].description;
        if(d.identity==identity && d.format==particle->format && d.width==particle->width && d.height==particle->height && images[image].name==name)break;
    }
    if(image==images.size())throw std::runtime_error("Native particle image was not compiled");
    Packet p;p.particle=*particle;p.quad=quad;p.image=image;
    // Packet ownership is entirely local. No pointers to the producer's stack
    // or mutable vertex buffer remain after capture.
    p.particle.image=nullptr;p.particle.vertices=nullptr;
    p.improved=mk64_3ds::ImprovedRaceTextures(Mk64Settings3DSGetUiTextures());
    if(quad==MK64_PARTICLE_EXPLICIT_QUAD)std::memcpy(p.legacy,particle->vertices,sizeof p.legacy);
    else for(unsigned i=0;i<4;++i) {
        auto& v=p.legacy[i].v;const auto& source=quads[quad][i];
        std::memcpy(v.ob,source.position,6);std::memcpy(v.tc,source.uv,4);std::memcpy(v.cn,source.color,4);
    }
    for(unsigned i=0;i<4;++i) {
        auto& v=p.vertices[i];const auto& source=p.legacy[i].v;
        std::memcpy(v.position,source.ob,6);std::memcpy(v.color,source.cn,4);
        for(unsigned j=0;j<2;++j)v.uv[j]=(int32_t(source.tc[j])*32768)>>16;
    }
    p.matrixKey=recordedKey?recordedKey:Mk64NativeObjectMatrixRecord(particle->model);packets[used]=p;
    Mk64NativeScheduleParticle(anchor,used++);++captured;++quadCaptured[quad];
}
extern "C" void Mk64NativeParticleEmit(const void* anchor,const Mk64NativeParticle3DS* particle,Mk64NativeParticleQuad quad) {
    CaptureParticle(anchor,particle,quad,nullptr);
}
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
extern "C" void Mk64NativeParticleEmitCompared(const void* begin,const void* end,
        const Mk64NativeParticle3DS* particle,Mk64NativeParticleQuad quad,const void* matrixKey) {
    const mk64_3ds::NativeComparisonSpan span{reinterpret_cast<uintptr_t>(begin),reinterpret_cast<uintptr_t>(end)};
    if(!matrixKey || used==kCapacity || !mk64_3ds::ValidComparisonSpan(span) ||
       (used && compareSpans[used-1].end>span.begin))throw std::runtime_error("Native particle comparison span/key");
    compareSpans[used]=span;CaptureParticle(begin,particle,quad,static_cast<const Mtx*>(matrixKey));
}
void Mk64NativeParticleUseReference(bool enabled){referenceParticles=enabled;}
void Mk64NativeParticleUseEagerExits(bool enabled){
    if(exitPacket)throw std::runtime_error("Native particle exit policy changed within batch");
    eagerExits=enabled;
}
uint64_t Mk64NativeParticleExitCount(){return exitApplications;}
bool Mk64NativeParticleIsReference(){return referenceParticles;}
void Mk64NativeParticleValidateComparison(const void* commands,size_t bytes) {
    if(!mk64_3ds::ComparisonSpansWithin(compareSpans.data(),used,reinterpret_cast<uintptr_t>(commands),bytes))
        throw std::runtime_error("Native particle comparison frame ownership");
}
const void* Mk64NativeParticleComparedEnd(unsigned packet,const void* begin) {
    if(packet>=used || compareSpans[packet].begin!=reinterpret_cast<uintptr_t>(begin))
        throw std::runtime_error("Native particle comparison reference/anchor");
    return reinterpret_cast<const void*>(compareSpans[packet].end);
}
void Mk64NativeParticleReferenceReached(unsigned packet) {
    if(packet>=used)throw std::runtime_error("Native particle reference index");++referenceReached;
}
uint64_t Mk64NativeParticleReferenceCount(){return referenceReached;}
uint64_t Mk64NativeParticleDrawCount(){return drawn;}
unsigned Mk64NativeParticlePacketCount(){return used;}
uint64_t Mk64NativeParticleBatchCount(){return scopes;}
uint64_t Mk64NativeParticleGeometryCleanCount(){return sprites.Statistics().geometryCleanCalls;}
#endif
void Mk64NativeParticleBeginBatch(Fast::GfxRenderingAPICitro3D* backend){
    if(exitPacket)throw std::runtime_error("Native particle exit pending at batch begin");
    batchStarted=svcGetSystemTick();backend->BeginNativeSpriteBatch(sprites);++scopes;
}
void Mk64NativeParticleEndBatch(Fast::GfxRenderingAPICitro3D* backend){
    if(exitPacket) {
        auto* gfx=exitInterpreter;const auto* packet=exitPacket;ClearParticleExit();
        if(!gfx || gfx->mRapi!=backend)throw std::runtime_error("Native particle exit owner changed");
        ApplyParticleExit(gfx,*packet);
    }
    backend->EndNativeSpriteBatch();drawTicks+=svcGetSystemTick()-batchStarted;
}
void Mk64NativeParticleDraw(Fast::Interpreter* gfx,unsigned packet) {
    try {
    if(exitInterpreter && exitInterpreter!=gfx)throw std::runtime_error("Native particle batch interpreter changed");
    if(packet>=used)throw std::runtime_error("Native particle reference invalid");
    const auto& p=packets[packet];auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;
    // These inherited features require dedicated native contracts before this
    // development route can cover their callers; never silently approximate.
    if((rsp.geometry_mode&0xc0000u) || (rdp.other_mode_l&G_ZS_PRIM) ||
       (rdp.other_mode_h&(3u<<20)) || rdp.grayscale)
        throw std::runtime_error("Native particle inherited texture-gen/depth/cycle/grayscale unsupported");
    MtxF replacement{};const float (*input)[4]=reinterpret_cast<const float(*)[4]>(p.particle.model);
    if(Mk64FrameInterpolation3DSLookup(p.matrixKey,&replacement)){input=replacement.mf;++interpolated;}
    else {auto it=gfx->mCurMtxReplacements->find(const_cast<Mtx*>(p.matrixKey));
        if(it!=gfx->mCurMtxReplacements->end()){input=it->second.mf;++interpolated;}}
    float model[4][4];for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
        const float v=input[i][j];if(!std::isfinite(v) || v<-32768 || v>=32768)throw std::runtime_error("Native particle model range");
        model[i][j]=static_cast<int32_t>(v*65536.f)*(1.f/65536);
    }
    if(!rsp.modelview_matrix_stack_size)rsp.modelview_matrix_stack_size=1;
    std::memcpy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size-1],model,sizeof model);
    gfx->MatrixMul(rsp.MP_matrix,model,rsp.P_matrix);rsp.lights_changed=true;
    const auto resolved=ns::ResolveParticleState(p.particle,p.quad,rdp.other_mode_h,rdp.other_mode_l,
        rsp.geometry_mode,(rsp.extra_geometry_mode&G_EX_INVERT_CULLING)!=0);
    rsp.geometry_mode=resolved.geometry;rdp.other_mode_h=resolved.high;rdp.other_mode_l=resolved.low;
    rdp.combine_mode=resolved.combine;rdp.prim_lod_fraction=0;std::memcpy(&rdp.prim_color,p.particle.primitive,4);
    if(p.particle.color==MK64_PARTICLE_COLOR_LERP)std::memcpy(&rdp.env_color,p.particle.environment,4);
    SyncViewportAndDepth(gfx);
    const auto& image=images[p.image];
    static_cast<Fast::GfxRenderingAPICitro3D*>(gfx->mRapi)->DrawNativeSprite(sprites,p.improved?image.improvedKey:image.originalKey,
        p.vertices,&rsp.MP_matrix[0][0],gfx->AdjXForAspectRatio(1.f),resolved.style);
    // Intermediate compatibility vertices/tiles have no native consumer.
    // Preserve numeric raster evolution, but materialize their final writer
    // once before the scheduler returns to another family or source command.
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    if(eagerExits)ApplyParticleExit(gfx,p);
    else
#endif
    {exitInterpreter=gfx;exitPacket=&p;}
    if(p.quad!=MK64_PARTICLE_EXPLICIT_QUAD)rdp.other_mode_l&=~3u;
    ++drawn;++quadDrawn[p.quad];
    }catch(...){ClearParticleExit();throw;}
}
extern "C" void Mk64NativeParticleSaveStats(unsigned frame) {
    if(FILE* f=std::fopen("sdmc:/3ds/MK64/native-lab/n17-particles.csv","wb")) {
        const auto& s=sprites.Statistics();
        std::fprintf(f,"frame,captured,drawn,interpolated,scopes,draw_ticks,exit_ticks,used_packets,packet_bytes,image_count,draw_upload_bytes,texture_binds,material_changes,geometry_clean_calls,geometry_clean_bytes,exit_applications\n%u,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu\n",frame,
            (unsigned long long)captured,(unsigned long long)drawn,(unsigned long long)interpolated,(unsigned long long)scopes,
            (unsigned long long)drawTicks,(unsigned long long)exitTicks,used,unsigned(sizeof packets),unsigned(images.size()),
            (unsigned long long)s.uploadBytes,(unsigned long long)s.textureBinds,(unsigned long long)s.materialChanges,
            (unsigned long long)s.geometryCleanCalls,(unsigned long long)s.geometryCleanBytes,(unsigned long long)exitApplications);
        std::fprintf(f,"quad,captured,drawn\n");for(unsigned i=0;i<4;++i)std::fprintf(f,"%u,%llu,%llu\n",i,
            (unsigned long long)quadCaptured[i],(unsigned long long)quadDrawn[i]);
        std::fprintf(f,"timing_scope,whole_native_particle_batch_including_exit_and_clean\n");std::fclose(f);
    }
}
