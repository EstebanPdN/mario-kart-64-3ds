#include "native_data_path_3ds.hpp"
#ifdef MK64_3DS_NATIVE_FULL_GAME
#include "native_program_3ds.h"
extern "C" void Mk64NativeKartPrepareCourse();
#endif
#include "native_course_lab_3ds.hpp"
#ifdef MK64_3DS_NATIVE_COMPARE_LAB
#include "native_comparison_lab_3ds.hpp"
#endif
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
#include "native_vegetation_lab_3ds.h"
#endif
#ifdef MK64_3DS_NATIVE_KART_LAB
extern "C" void Mk64NativeKartSaveStats(unsigned);
extern "C" bool Mk64Graphics3DSCaptureNativeFrame(const char*);
#endif
#include "native_gpu_scene_3ds.hpp"
#include "native_dynamic_texture_3ds.hpp"
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
#include "native_batch_comparison_3ds.hpp"
#endif
#include "native_exit_state_3ds.hpp"
#include "native_course_contract_3ds.hpp"
#include "frame_begin_3ds.hpp"
#include <algorithm>
#include "texture_lease_3ds.hpp"
#include "jumbotron_3ds.h"
#include "gfx_citro3d.h"
#include "diagnostics_3ds.h"
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace ns=mk64_3ds::native_scene;
namespace {
ns::GpuScene scene;
std::vector<ns::ExitState> exits;
std::vector<std::string> names;
std::vector<std::shared_ptr<Fast::Texture>> owners;
std::vector<uint64_t> roots(123);
std::vector<std::pair<uint64_t,unsigned>> rootIndex;
std::vector<ns::IncomingCourseState> incoming;
std::vector<ns::CourseExitExtra> extendedExits;
std::string activeCourse;
uint64_t compatibilityRoots=0;
uint64_t draws=0,drawTicks=0,bridgeTicks=0;
std::vector<uint64_t> hits(123);
bool ready=false;
std::array<mk64_3ds::TextureLease3DS,6> mutableOwners;
std::vector<ns::DynamicTextureRecord> mutableImages;
uint64_t mutableGeneration=0,mutableUpdates=0,mutableTicks=0;
// An exact, bounded CPU copy avoids re-uploading unchanged tiles and retains
// the committed revision while simulation may advance without presentation.
std::array<std::array<uint8_t,4096>,6> mutableCommitted{};
std::array<bool,6> mutableValid{};
#ifdef MK64_3DS_NATIVE_COMPARE_LAB
bool useReference=false;
#endif
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
ns::BatchComparison batchComparison;
#endif
std::vector<uint8_t> Read(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)throw std::runtime_error("Native lab resource missing");
    std::fseek(f,0,SEEK_END);long n=std::ftell(f);std::rewind(f);
    if(n<0 || n>1024*1024){std::fclose(f);throw std::runtime_error("Native lab resource size");}
    std::vector<uint8_t> out(n);const bool ok=std::fread(out.data(),1,n,f)==static_cast<size_t>(n);std::fclose(f);
    if(!ok)throw std::runtime_error("Native lab resource read");return out;
}
Fast::RawTexMetadata Metadata(unsigned id) {
    const auto& texture=owners.at(id);Fast::RawTexMetadata result{};
    result.width=texture->Width;result.height=texture->Height;result.h_byte_scale=texture->HByteScale;
    result.v_pixel_scale=texture->VPixelScale;result.type=texture->Type;result.resource=texture;return result;
}
uint64_t Combine(unsigned kind) {
    auto rgb=[](uint64_t a,uint64_t b,uint64_t c,uint64_t d){return a|(b<<4)|(c<<8)|(d<<13);};
    auto alpha=[](uint64_t a,uint64_t b,uint64_t c,uint64_t d){return a|(b<<3)|(c<<6)|(d<<9);};
    const uint64_t c=kind?rgb(1,15,4,7):rgb(15,15,31,4);
    const uint64_t a=kind==0?alpha(7,7,7,4):(kind==1?alpha(1,7,4,7):alpha(7,7,7,1));
    return c|(a<<16)|(c<<28)|(a<<44);
}
void ApplyExit(Fast::Interpreter* gfx,const ns::ExitState& s,unsigned root) {
    auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;
    if(root<extendedExits.size()) {
        const auto& e=extendedExits[root];
        rsp.geometry_mode=(rsp.geometry_mode&~e.geometryMask)|(e.geometry&e.geometryMask);
        rdp.other_mode_h=(rdp.other_mode_h&~e.highMask)|(e.high&e.highMask);
        rdp.other_mode_l=(rdp.other_mode_l&~e.lowMask)|(e.low&e.lowMask);
        if(e.flags&1)rdp.combine_mode=e.combine;
        auto color=[](auto& dst,uint32_t packed){dst.r=packed>>24;dst.g=packed>>16;dst.b=packed>>8;dst.a=packed;};
        if(e.flags&2)color(rdp.fog_color,e.fog);
        if(e.flags&4)color(rdp.prim_color,e.primitive);
        if(e.flags&8)color(rdp.env_color,e.environment);
        if(e.flags&16){rsp.current_num_lights=e.numLights;rsp.lights_changed=true;}
        if(e.flags&32){rsp.fog_mul=e.fogMul;rsp.fog_offset=e.fogOffset;}
    }
    if(s.cull!=~0u)rsp.geometry_mode=(rsp.geometry_mode&~0x2000u)|(s.cull==2?0x2000u:0u);
    if(s.shader!=~0u)rdp.combine_mode=Combine(s.shader);
    if(s.scaleWritten)gfx->GfxSpTexture(s.scaleS,s.scaleT,0,s.renderTile,1);
    for(unsigned i=0;i<8;++i) {
        const auto& t=s.tiles[i];
        if(s.tileMask&(1u<<i))gfx->GfxDpSetTile(t.format,t.size,t.line,t.memory,i,t.palette,
            t.wrapT,t.maskT,t.shiftT,t.wrapS,t.maskS,t.shiftS);
        if(s.boundsMask&(1u<<i))gfx->GfxDpSetTileSize(i,t.lowS,t.lowT,t.highS,t.highT);
    }
    if(s.sourceWritten)gfx->GfxDpSetTextureImage(0,s.sourceSize,s.sourceWidth,names[s.sourceTexture].c_str(),
        owners[s.sourceTexture]->Flags,Metadata(s.sourceTexture),owners[s.sourceTexture]->ImageData);
    for(unsigned i=0;i<2;++i)if(s.loadedMask&(1u<<i)) {
        const auto& l=s.loaded[i];auto& target=rdp.loaded_texture[i];const auto& owner=owners[l.texture];
        target.addr=owner->ImageData;target.orig_size_bytes=target.size_bytes=l.bytes;
        uint32_t stride=l.bytes,candidate=l.size==0?(l.width+1)/2:l.width*(1u<<(l.size-1));
        if(l.width>1 && candidate>0 && candidate<l.bytes && l.bytes%candidate==0)stride=candidate;
        target.full_image_line_size_bytes=target.line_size_bytes=stride;
        target.tex_flags=owner->Flags;target.raw_tex_metadata=Metadata(l.texture);target.masked=target.blended=false;
        rdp.textures_changed[i]=true;
    }
    // Only the final source slots can be consumed by the next legacy actor.
    // Transform those once with the current camera; no static list is walked.
    Fast::F3DVtx vertices[64]{};
    const uint64_t mask=s.slotMaskLow|(uint64_t(s.slotMaskHigh)<<32);
    for(unsigned i=0;i<64;++i)if((mask>>i)&1) {
        const auto& v=scene.Vertices()[s.vertices[i]];
        for(unsigned j=0;j<3;++j)vertices[i].v.ob[j]=v.position[j];
        std::memcpy(vertices[i].v.cn,v.color,4);
    }
    const auto savedGeometry=rsp.geometry_mode;
    rsp.geometry_mode&=~0xf0000u; // Stored native colors are already resolved.
    for(unsigned at=0;at<64;) {
        if(!((mask>>at)&1)){++at;continue;}
        const unsigned first=at;while(at<64 && ((mask>>at)&1))++at;
        gfx->GfxSpVertex(at-first,first,vertices+first);
        for(unsigned i=first;i<at;++i){const auto& v=scene.Vertices()[s.vertices[i]];rsp.loaded_vertices[i].u=v.uv[0];rsp.loaded_vertices[i].v=v.uv[1];}
    }
    rsp.geometry_mode=savedGeometry;
}
void Save(unsigned frame) {
    mkdir("sdmc:/3ds/MK64/native-lab",0777);
    if(FILE* f=std::fopen("sdmc:/3ds/MK64/native-lab/n4-native.csv","wb")) {
        std::fprintf(f,"# Native course with revisioned mutable images; emulator evidence only\nframe,native_views,native_ticks,bridge_ticks,mutable_updates,mutable_bytes,mutable_ticks,mutable_generation\n%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",frame,
            (unsigned long long)draws,(unsigned long long)drawTicks,(unsigned long long)bridgeTicks,
            (unsigned long long)mutableUpdates,(unsigned long long)scene.MutableUploadBytes(),
            (unsigned long long)mutableTicks,(unsigned long long)mutableGeneration);
        std::fprintf(f,"view,hits\n");for(unsigned i=0;i<roots.size();++i)std::fprintf(f,"%u,%llu\n",i,(unsigned long long)hits[i]);std::fclose(f);
    }
}
}
static void LoadCourseResources(const std::string& directory) {

    if(!scene.Load(directory.c_str()))throw std::runtime_error("Native Luigi GPU resources failed validation");
    auto bytes=Read((directory+"/roots.ncr").c_str());
    if(bytes.size()<8 || std::memcmp(bytes.data(),"NCR1",4))throw std::runtime_error("Native course roots header");
    const unsigned count=ns::ReadLE32(bytes.data()+4);
    if(!count || count>4096 || bytes.size()!=8+size_t(count)*8 || scene.ViewCount()!=count*
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
       4
#else
       2
#endif
       )
        throw std::runtime_error("Native course roots shape");
    roots.resize(count);hits.assign(count,0);std::memcpy(roots.data(),bytes.data()+8,count*8);
    rootIndex.clear();rootIndex.reserve(count);
    for(unsigned i=0;i<count;++i)rootIndex.emplace_back(roots[i],i);
    std::sort(rootIndex.begin(),rootIndex.end());
    for(unsigned i=1;i<count;++i)if(rootIndex[i-1].first==rootIndex[i].first)
        throw std::runtime_error("Duplicate native course root");
    bytes=Read((directory+"/exit.ncx").c_str());uint32_t textureCount=0;
    if(bytes.size()>=4 && !std::memcmp(bytes.data(),"NCX2",4)) {
        if(!ns::ValidateCourseExits(bytes.data(),bytes.size(),scene.VertexCount(),count,exits,extendedExits,textureCount))
            throw std::runtime_error("Native course extended exit state");
    } else if(!ns::ValidateExits(bytes.data(),bytes.size(),scene.VertexCount(),exits,textureCount,count))
        throw std::runtime_error("Native course exit state");
    if(FILE* file=std::fopen((directory+"/incoming.ncc").c_str(),"rb")) {
        std::fclose(file);bytes=Read((directory+"/incoming.ncc").c_str());
        if(bytes.size()!=8+count*sizeof(ns::IncomingCourseState) || std::memcmp(bytes.data(),"NCC1",4) || ns::ReadLE32(bytes.data()+4)!=count)
            throw std::runtime_error("Native course caller contract");
        incoming.resize(count);std::memcpy(incoming.data(),bytes.data()+8,count*sizeof(ns::IncomingCourseState));
    }
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
    bytes=Read(MK64_NATIVE_DATA_ROOT "native-luigi/batch.nbp");
    if(!ns::ValidateBatchComparison(bytes.data(),bytes.size(),roots.size(),scene.ViewCount(),batchComparison))
        throw std::runtime_error("Native batch comparison view contract");
#endif
    bytes=Read((directory+"/exit-textures.txt").c_str());std::string text(bytes.begin(),bytes.end());size_t at=0;
    while(at<text.size()) {const size_t end=text.find('\n',at);if(end==std::string::npos)break;
        names.push_back(text.substr(at,end-at));at=end+1;
        auto resource=std::static_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(names.back().c_str()));
        if(!resource || !resource->ImageData || resource->HByteScale!=1 || resource->VPixelScale!=1)throw std::runtime_error("Native Luigi bridge requires original texture metadata");
        owners.push_back(resource);
    }
    if(owners.size()!=textureCount)throw std::runtime_error("Native Luigi texture owner count");
    if(FILE* file=std::fopen((directory+"/mutable.nmd").c_str(),"rb")) {
    std::fclose(file);bytes=Read((directory+"/mutable.nmd").c_str());
    if(!ns::ValidateDynamicTextureTable(bytes.data(),bytes.size(),scene.TextureCount(),mutableImages) ||
       mutableImages.size()!=mutableOwners.size())throw std::runtime_error("Native Luigi mutable image table");
    for(unsigned i=0;i<mutableImages.size();++i) {
        const auto& image=mutableImages[i];
        if(!mutableOwners[i].Acquire(reinterpret_cast<const char*>(bytes.data()+image.nameOffset)))
            throw std::runtime_error("Native Luigi mutable image owner");
        const auto& resource=mutableOwners[i].Get();
        if(resource.type!=2 || resource.width!=image.width || resource.height!=image.height ||
           resource.size!=size_t(image.stride)*image.height*2 || resource.size!=mutableCommitted[i].size())
            throw std::runtime_error("Native Luigi mutable image source shape");
    }
    }
    ready=true;
}
extern "C" void Mk64NativeLuigiLabLoad() { LoadCourseResources(MK64_NATIVE_DATA_ROOT "native-luigi"); }
#ifdef MK64_3DS_NATIVE_FULL_GAME
extern "C" void Mk64NativeCourseRelease() {
    // The last submitted race can still own these allocations. FrameSync is
    // a display wait, so acquire the GPU command queue before freeing them.
    if(!mk64_3ds::BeginFrame3DS(0,"native-course-release-gpu-queue-wait"))
        throw std::runtime_error("Native course GPU ownership timeout");
    if(!activeCourse.empty()) {
        char report[160];std::snprintf(report,sizeof report,"native-course-stats %s draws=%llu guarded=%llu",activeCourse.c_str(),
            (unsigned long long)draws,(unsigned long long)compatibilityRoots);Mk64Diagnostics3DSCheckpoint(report);
    }
    draws=compatibilityRoots=drawTicks=bridgeTicks=0;
    Mk64NativeProgramReleaseWorkingSet();
    Mk64NativeProgramResetCourseChanges();
    ready=false;scene.ResetAfterCompletion();owners.clear();names.clear();exits.clear();rootIndex.clear();
    roots.clear();hits.clear();incoming.clear();extendedExits.clear();mutableImages.clear();
    for(auto& owner:mutableOwners)owner.Reset();
    mutableValid.fill(false);mutableGeneration=0;activeCourse.clear();
    C3D_FrameEnd(0);
}
extern "C" void Mk64NativeCourseLoad(const char* name) {
    // Resolve only canonical stock names; mod tracks retain the general path.
    if(!name || std::strncmp(name,"mk:",3))return;
    const char* stock=name+3;
    for(const char* p=stock;*p;++p)if(!((*p>='a' && *p<='z') || *p=='_'))return;
    const std::string directory=std::string(MK64_NATIVE_DATA_ROOT "native-courses/")+stock;
    FILE* probe=std::fopen((directory+"/scene.nlc").c_str(),"rb");
    if(!probe){Mk64Diagnostics3DSCheckpoint("native-course-compatibility-working-set");return;}
    std::fclose(probe);LoadCourseResources(directory);activeCourse=stock;
    Mk64NativeKartPrepareCourse();
    Mk64NativeProgramPreload(stock);
    char report[160];std::snprintf(report,sizeof report,"native-course-ready %s roots=%lu",stock,(unsigned long)roots.size());
    Mk64Diagnostics3DSCheckpoint(report);
}
#endif
extern "C" void Mk64NativeLuigiGpuFrameReady() {
    if(!ready)return;
    scene.BeginAfterCompletion();
    if(mutableImages.empty())return;
    Mk64Jumbotron3DSState state{};
    if(!Mk64Jumbotron3DSGetState(&state) || state.generation==mutableGeneration)return;
    for(unsigned i=0;i<mutableOwners.size();++i)
        if(state.tiles[i]!=reinterpret_cast<const uint16_t*>(mutableOwners[i].Get().data))
            throw std::runtime_error("Native mutable image publication owner changed");
    const uint64_t start=svcGetSystemTick();
    for(unsigned i=0;i<mutableOwners.size();++i) {
        const auto& image=mutableImages[i];const auto& source=mutableOwners[i].Get();
        if(mutableValid[i] && !std::memcmp(mutableCommitted[i].data(),source.data,source.size))continue;
        if(!scene.UpdateTextureRgba5551(image.texture,source.data,source.size,
                image.stride,image.width,image.height))
            throw std::runtime_error("Native mutable image upload");
        std::memcpy(mutableCommitted[i].data(),source.data,source.size);mutableValid[i]=true;
    }
    mutableTicks+=svcGetSystemTick()-start;mutableGeneration=state.generation;++mutableUpdates;
}
extern "C" bool Mk64NativeCourseDrawHash(Fast::Interpreter* gfx,uint64_t hash) {
#if defined(MK64_3DS_NATIVE_COMPARE_LAB) && !defined(MK64_3DS_NATIVE_BATCH_COMPARE_LAB)
    if(useReference)return false;
#endif
    if(!ready || !gfx)return false;
#ifdef MK64_3DS_NATIVE_FULL_GAME
    if(Mk64NativeProgramMutableDependency(hash))return false;
#endif
    const auto match=std::lower_bound(rootIndex.begin(),rootIndex.end(),std::make_pair(hash,0u));
    if(match==rootIndex.end() || match->first!=hash)return false;
    const unsigned root=match->second;
    if(!incoming.empty()) {
        const auto& rsp=*gfx->mRsp;const auto& rdp=*gfx->mRdp;
        if(!incoming[root].Matches(rsp.geometry_mode,rdp.other_mode_l,rdp.other_mode_h,rdp.combine_mode,Combine(1),
                                  rsp.texture_scaling_factor.s,rsp.texture_scaling_factor.t)) {
            ++compatibilityRoots;return false;
        }
    }
    if(incoming.empty() && (gfx->mRsp->geometry_mode&0x20000u))throw std::runtime_error("Native Luigi unexpected lighting state");
    gfx->Flush();auto& state=gfx->mRenderingState;const auto& rsp=*gfx->mRsp;const auto& rdp=*gfx->mRdp;
    const bool depth=((rsp.geometry_mode&G_ZBUFFER) || (rdp.other_mode_l&G_ZS_PRIM)) && (rdp.other_mode_l&Z_CMP);
    const bool write=(rdp.other_mode_l&Z_UPD)!=0,decal=(rdp.other_mode_l&ZMODE_DEC)==ZMODE_DEC;
    gfx->mRapi->SetDepthTestAndMask(depth,write);state.depth_test_and_mask=(depth?1:0)|(write?2:0);
    gfx->mRapi->SetZmodeDecal(decal);state.decal_mode=decal;
    gfx->mRapi->SetViewport(rdp.viewport.x,rdp.viewport.y,rdp.viewport.width,rdp.viewport.height);state.viewport=rdp.viewport;
    gfx->mRapi->SetScissor(rdp.scissor.x,rdp.scissor.y,rdp.scissor.width,rdp.scissor.height);state.scissor=rdp.scissor;
    gfx->mRdp->viewport_or_scissor_changed=false;
    const bool mirror=(rsp.extra_geometry_mode&G_EX_INVERT_CULLING)!=0;
    const uint64_t start=svcGetSystemTick();
    unsigned view=root+(mirror?roots.size():0);
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
    view+=useReference?batchComparison.referenceBase:batchComparison.optimizedBase;
#endif
    static_cast<Fast::GfxRenderingAPICitro3D*>(gfx->mRapi)->DrawNativeCourse(scene,view,&rsp.MP_matrix[0][0],gfx->AdjXForAspectRatio(1.f));
    const uint64_t rendered=svcGetSystemTick();ApplyExit(gfx,exits[root],root);
    drawTicks+=rendered-start;bridgeTicks+=svcGetSystemTick()-rendered;++draws;++hits[root];return true;
}
#ifndef MK64_3DS_NATIVE_DYNAMIC
extern "C" bool Mk64NativeLuigiLabDispatch(Fast::Interpreter* gfx,const void* list) {
#if defined(MK64_3DS_NATIVE_COMPARE_LAB) && !defined(MK64_3DS_NATIVE_BATCH_COMPARE_LAB)
    if(useReference)return false;
#endif
    if(!ready || !list)return false;
    const auto* words=static_cast<const uint32_t*>(list);
    if(words[0]>>24!=0x33)return false;
    const uint64_t hash=(uint64_t(words[2])<<32)|words[3];
    return Mk64NativeCourseDrawHash(gfx,hash);
}
#endif
extern "C" bool Mk64NativeLuigiLabTick() {
    static unsigned frame=0;++frame;
    // Counters stay resident during the measured race. Synchronous SD writes
    // here previously stalled every 60 ticks before PerformanceBegin(), causing
    // periodic deadline resets which were absent from the per-tick CPU total.
    // Export only after the final measured tick and before the bounded exit.
#ifndef MK64_3DS_NATIVE_PLAYTEST
    if(frame==900) {
        Save(frame);
#ifdef MK64_3DS_NATIVE_KART_LAB
        Mk64NativeKartSaveStats(frame);
#endif
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
        Mk64NativeVegetationSaveStats(frame);
#endif
#ifdef MK64_3DS_NATIVE_COMPARE_LAB
        Mk64NativeComparisonSave(frame);
#endif
    }
#endif
#ifndef MK64_3DS_NATIVE_PLAYTEST
    if(frame==300 || frame==750) {
        // N5 transfer/BMP previews were malformed. Do not pay for an untrusted
        // readback in N6; use the exact RAM captures and direct visible screen.
        Mk64Diagnostics3DSRequestFullDump();
    }
    return frame<900;
#else
    return true;
#endif
}
#ifdef MK64_3DS_NATIVE_COMPARE_LAB
extern "C" void Mk64NativeCourseUseReference(bool enabled){useReference=enabled;}
extern "C" uint64_t Mk64NativeCourseDrawCount(){return draws;}
#endif
