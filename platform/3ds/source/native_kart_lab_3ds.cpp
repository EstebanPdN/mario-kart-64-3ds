#ifdef MK64_3DS_NATIVE_FULL_GAME
#include "native_program_3ds.h"
#endif
#include "texture_mode_3ds.hpp"
#include "native_data_path_3ds.hpp"
#include "native_kart_lab_3ds.h"
#include "native_anchor_queue_3ds.hpp"
#include "native_character_contract_3ds.hpp"
#include "alpha_policy_3ds.hpp"
#include "native_draw_reference_3ds.hpp"
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
#include "native_particle_lab_3ds.h"
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
#include "native_particle_compare_3ds.h"
#endif
#endif
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
#include "native_vegetation_lab_3ds.h"
#endif
#include "native_gpu_sprites_3ds.hpp"
#include "native_wheel_palettes_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "gfx_citro3d.h"
#include "game_runtime_3ds.h"
#include "improved_texture_pack_3ds.hpp"
#include "settings_3ds.h"
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#include <malloc.h>
#include "heap_policy_3ds.hpp"
#include "startup_trace_3ds.h"
#include "diagnostics_3ds.h"
namespace ns=mk64_3ds::native_scene;
extern "C" bool Mk64FrameInterpolation3DSLookup(const Mtx*,MtxF*);
namespace {
struct Packet {
    const void* matrixKey=nullptr;float model[4][4]{};Fast::F3DVtx legacy[8]{};ns::Vertex vertices[8]{};
    const char* name=nullptr;uint8_t palette[512]{};ns::SpriteStyle style{};uint32_t textureKey=0,renderMode=0;
    Mk64NativeCharacterKind kind=MK64_NATIVE_BODY;bool improved=false;
};
ns::GpuSprites sprites;
ns::WheelPalettes wheelPalettes;
std::array<Packet,64> characterPackets{};unsigned usedCharacters=0;
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
mk64_3ds::NativeAnchorQueue<mk64_3ds::NativeDrawReference,1344,4096> queue;
#elif defined(MK64_3DS_NATIVE_ITEM_BOX_LAB)
mk64_3ds::NativeAnchorQueue<mk64_3ds::NativeDrawReference,832,2048> queue;
#else
mk64_3ds::NativeAnchorQueue<mk64_3ds::NativeDrawReference,320,1024> queue;
#endif
uint32_t shadowKey=0,creditsShadowKey=0;
std::array<uint8_t,65536> paletteIdentityTokens{};
bool ready=false;const char* pendingTexture=nullptr;const uint8_t sentinel[16]={0x4e,0x41,0x54,0x49,0x56,0x45};
uint64_t captured=0,drawn=0,lazyRestores=0,interpolated=0,drawTicks=0;
uint64_t shadowCaptured=0,shadowDrawn=0,batchTicks=0,batches=0;
uint64_t dispatchCalls=0,dispatchMatches=0;
std::array<uint64_t,MK64_NATIVE_CHARACTER_KINDS> kindCaptured{},kindDrawn{};
size_t SpriteResidencyBudget() {
    // The generic 8 MiB planning reserve included the old initial CI8 kart
    // prefetch (1,891,968 bytes in the matched Old race). Full native no longer runs
    // that prefetch. Account for its 1.13 MiB replacement pool once, instead
    // of retaining both the old future allowance and the live replacement.
    // The physical heap still retains over 6.8 MiB before course allocations;
    // VRAM retains its independent 1.5 MiB floor. No draw-time allocation.
    size_t replacing=sprites.ResidentHeapBytes();
#ifdef MK64_3DS_NATIVE_FULL_GAME
    if(!Mk64Graphics3DSResolvedNewModel()) replacing+=wheelPalettes.ResidentBytes();
#endif
    return mk64_3ds::ArchiveResidencyBudget(envGetHeapSize(),mallinfo().uordblks,replacing);
}
bool UseImprovedCharacters() {
#ifdef MK64_3DS_NATIVE_CHARACTER_SCENARIOS
    return false; // Explicit Original-character fixture; global UI settings stay unchanged.
#else
    return mk64_3ds::ImprovedRaceTextures(Mk64Settings3DSGetUiTextures());
#endif
}
uint64_t Combine(bool shadow) {
    const uint64_t rgb=shadow ? (15|(15<<4)|(31<<8)|(3<<13)) : (6|(5<<4)|(1<<8)|(3<<13));
    const uint64_t alpha=shadow ? (1|(7<<3)|(3<<6)|(7<<9)) : (3|(7<<3)|(1<<6)|(7<<9));
    return rgb|(alpha<<16)|(rgb<<28)|(alpha<<44);
}
void Quantize(const float input[4][4],float output[4][4]) {
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){const float v=input[i][j];
        if(!std::isfinite(v) || v<-32768 || v>=32768)throw std::runtime_error("Native kart model range");
        output[i][j]=static_cast<int32_t>(v*65536.f)*(1.f/65536);}
}
void SyncViewportAndDepth(Fast::Interpreter* gfx) {
    const auto& rdp=*gfx->mRdp;auto& state=gfx->mRenderingState;
    const bool depth=((gfx->mRsp->geometry_mode&G_ZBUFFER) || (rdp.other_mode_l&G_ZS_PRIM)) && (rdp.other_mode_l&Z_CMP);
    const bool write=(rdp.other_mode_l&Z_UPD)!=0,decal=(rdp.other_mode_l&ZMODE_DEC)==ZMODE_DEC;
    gfx->mRapi->SetDepthTestAndMask(depth,write);state.depth_test_and_mask=(depth?1:0)|(write?2:0);
    gfx->mRapi->SetZmodeDecal(decal);state.decal_mode=decal;
    gfx->mRapi->SetViewport(rdp.viewport.x,rdp.viewport.y,rdp.viewport.width,rdp.viewport.height);state.viewport=rdp.viewport;
    gfx->mRapi->SetScissor(rdp.scissor.x,rdp.scissor.y,rdp.scissor.width,rdp.scissor.height);state.scissor=rdp.scissor;
    gfx->mRdp->viewport_or_scissor_changed=false;
}
void Draw(Fast::Interpreter* gfx,Packet& p) {
    auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;float model[4][4];MtxF replacement{};
    const bool shadow=mk64_3ds::CharacterIsShadow(p.kind);
    if(!shadow && !mk64_3ds::CharacterWritesPalette(p.kind) && !p.improved) {
        uint8_t palette[512];
        if(!rdp.palettes[0] || !rdp.palettes[1])throw std::runtime_error("Native effect has no inherited palette");
        std::memcpy(palette,rdp.palettes[0],256);std::memcpy(palette+256,rdp.palettes[1],256);
        uint32_t inherited=0;
        if(!sprites.Resolve(p.name,palette,false,inherited) || inherited!=p.textureKey)
            throw std::runtime_error("Native effect inherited a different palette");
    }
    if(Mk64FrameInterpolation3DSLookup(static_cast<const Mtx*>(p.matrixKey),&replacement)) {Quantize(replacement.mf,model);++interpolated;}
    else {auto it=gfx->mCurMtxReplacements->find((Mtx*)p.matrixKey);
        if(it!=gfx->mCurMtxReplacements->end()){Quantize(it->second.mf,model);++interpolated;}else Quantize(p.model,model);}
    if(rsp.modelview_matrix_stack_size==0)rsp.modelview_matrix_stack_size=1;
    std::memcpy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size-1],model,sizeof model);
    gfx->MatrixMul(rsp.MP_matrix,model,rsp.P_matrix);rsp.lights_changed=true;rsp.geometry_mode&=~0x20000u;
    rdp.other_mode_h=(rdp.other_mode_h&~((3u<<12)|(3u<<14)|(1u<<19)))|(2u<<12)|(shadow?0u:(2u<<14))|(1u<<19);
    rdp.other_mode_l=(rdp.other_mode_l&7u)|p.renderMode;
    if(!shadow)rdp.other_mode_l&=~3u;
    rdp.combine_mode=Combine(shadow);rdp.prim_lod_fraction=0;
    std::memcpy(&rdp.prim_color,p.style.primitive,4);
    if(!shadow)std::memcpy(&rdp.env_color,p.style.environment,4);
    SyncViewportAndDepth(gfx);
    ns::SpriteStyle style=p.style;style.cull=(rsp.geometry_mode&0x1000?1:0)|(rsp.geometry_mode&0x2000?2:0);
    if(shadow)style.alphaTest=(rdp.other_mode_l&3u)==G_AC_THRESHOLD;
    if(rsp.extra_geometry_mode&G_EX_INVERT_CULLING){if(style.cull==1)style.cull=2;else if(style.cull==2)style.cull=1;}
    const uint64_t start=svcGetSystemTick();
    static_cast<Fast::GfxRenderingAPICitro3D*>(gfx->mRapi)->DrawNativeSprite(sprites,p.textureKey,p.vertices,&rsp.MP_matrix[0][0],gfx->AdjXForAspectRatio(1.f),style);
    drawTicks+=svcGetSystemTick()-start;++drawn;++kindDrawn[p.kind];if(shadow)++shadowDrawn;
    // Typed migration exit state, without executing any kart graphics commands.
    gfx->GfxSpTexture(32768,32768,0,0,1);gfx->GfxSpVertex(shadow?4:8,0,p.legacy);
    const unsigned format=shadow?4:2;
    gfx->GfxDpSetTile(format,2,0,0,7,0,2,0,0,2,0,0);gfx->GfxDpSetTile(format,1,8,0,0,0,2,0,0,2,0,0);
    gfx->GfxDpSetTileSize(0,0,0,252,(mk64_3ds::CharacterImageHeight(p.kind)-1)*4);gfx->GfxSpTexture(1,1,0,0,0);
    if(mk64_3ds::CharacterWritesPalette(p.kind)) {
    std::memcpy(rdp.palette_staging,p.palette,512);rdp.palettes[0]=rdp.palette_staging[0];rdp.palettes[1]=rdp.palette_staging[1];
    // These addresses are stable identity tokens, never pixel sources. Reusing
    // a packet slot must not make a changed palette match an old cache key.
    rdp.palette_dram_addr[0]=paletteIdentityTokens.data()+p.textureKey*2;
    rdp.palette_dram_addr[1]=paletteIdentityTokens.data()+p.textureKey*2+1;
    }
    rdp.texture_to_load.addr=sentinel;rdp.texture_to_load.siz=2;rdp.texture_to_load.width=1;rdp.texture_to_load.tex_flags=0;rdp.texture_to_load.raw_tex_metadata={};
    auto& loaded=rdp.loaded_texture[0];loaded.addr=sentinel;
    loaded.orig_size_bytes=loaded.size_bytes=loaded.line_size_bytes=loaded.full_image_line_size_bytes=64*mk64_3ds::CharacterImageHeight(p.kind);
    loaded.tex_flags=0;loaded.raw_tex_metadata={};loaded.masked=loaded.blended=false;pendingTexture=p.name;
    rdp.textures_changed[0]=rdp.textures_changed[1]=true;
}
}
extern "C" const uint8_t* Mk64NativeKartPaletteData(const char* name) {
    return wheelPalettes.Find(name);
}
extern "C" void Mk64NativeKartLabLoad(){
#ifdef MK64_3DS_NATIVE_FULL_GAME
    // Full native karts do not consume the old prefetched CI8 pixel resources.
    // Reuse that memory for all exact wheel palettes before budgeting sprite tiles.
    Mk64Resource3DSSetStreamingCacheBudget(256 * 1024);
    const bool palettesReady=wheelPalettes.Load(MK64_NATIVE_DATA_ROOT "native-wheel-palettes.nwp");
    Mk64Diagnostics3DSCheckpoint(palettesReady?"native-wheel-palettes-resident":"native-wheel-palettes-fallback");
#endif
    if(!sprites.Load(MK64_NATIVE_DATA_ROOT "native-karts.nsp",mk64_3ds::improved::DecodePayload) ||
       !sprites.LoadStatic(MK64_NATIVE_DATA_ROOT "native-shadow.nsi",shadowKey) ||
       !sprites.LoadStatic(MK64_NATIVE_DATA_ROOT "native-credits-shadow.nsi",creditsShadowKey))throw std::runtime_error("Native kart/shadow resources invalid");
    ready=true;
    const size_t budget=SpriteResidencyBudget();
    const bool resident=sprites.PrepareTextureMode(UseImprovedCharacters(),budget);
    Mk64StartupStage(resident?"native-kart-mode-resident":"native-kart-mode-streaming");
    char report[128];std::snprintf(report,sizeof report,"native-kart-residency bytes=%lu budget=%lu",(unsigned long)sprites.ResidentBytes(),(unsigned long)budget);
    Mk64Diagnostics3DSCheckpoint(report);
}
extern "C" void Mk64NativeKartPrepareCourse() {
    if(!ready)return;
    const size_t budget=SpriteResidencyBudget();
    sprites.PrepareTextureMode(UseImprovedCharacters(),budget,true);
    char report[160];std::snprintf(report,sizeof report,"native-kart-course-residency mode=%s bytes=%lu heap=%lu budget=%lu",
        UseImprovedCharacters()?"improved":"original",(unsigned long)sprites.ResidentBytes(),
        (unsigned long)sprites.ResidentHeapBytes(),(unsigned long)budget);
    Mk64Diagnostics3DSCheckpoint(report);
}
extern "C" bool Mk64NativeKartReclaimCache(){return sprites.DropResident();}
extern "C" void Mk64NativeKartCaptureReset(){queue.Reset();usedCharacters=0;
#ifdef MK64_3DS_NATIVE_FULL_GAME
    Mk64NativeProgramPrepareMode();
#endif
    // Frame construction is serialized with audio and precedes GPU recording.
    // Mode residency is attempted once per setting transition, never on draws.
    if(ready && !sprites.TextureModePrepared(UseImprovedCharacters())) {
        const size_t budget=SpriteResidencyBudget();
        sprites.PrepareTextureMode(UseImprovedCharacters(),budget);
    }
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
    Mk64NativeParticleReset();
#endif
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
    Mk64NativeVegetationReset();
#endif
}
static void CaptureCharacter(const void* anchor,const Packet& packet) {
    if(usedCharacters==characterPackets.size())throw std::runtime_error("Native character storage exhausted");
    characterPackets[usedCharacters]=packet;
    if(!queue.Push(reinterpret_cast<uintptr_t>(anchor),{mk64_3ds::NativeDrawFamily::Character,uint16_t(usedCharacters)}))
        throw std::runtime_error("Native shared draw scheduler exhausted");
    ++usedCharacters;
}
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
extern "C" void Mk64NativeScheduleVegetation(const void* anchor,unsigned packet) {
    if(packet>=512 || !queue.Push(reinterpret_cast<uintptr_t>(anchor),{mk64_3ds::NativeDrawFamily::Vegetation,uint16_t(packet)}))
        throw std::runtime_error("Native vegetation scheduler exhausted");
}
#endif
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
extern "C" void Mk64NativeScheduleObjectState(const void* anchor,unsigned packet) {
    if(packet>=256 || !queue.Push(reinterpret_cast<uintptr_t>(anchor),{mk64_3ds::NativeDrawFamily::ObjectState,uint16_t(packet)}))
        throw std::runtime_error("Native object state scheduler exhausted");
}
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
extern "C" void Mk64NativeScheduleParticle(const void* anchor,unsigned packet) {
    if(packet>=512 || !queue.Push(reinterpret_cast<uintptr_t>(anchor),{mk64_3ds::NativeDrawFamily::Particle,uint16_t(packet)}))
        throw std::runtime_error("Native particle scheduler exhausted");
}
#endif
extern "C" void Mk64NativeKartGpuFrameReady(){if(ready)sprites.BeginAfterCompletion();
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
    Mk64NativeParticleGpuFrameReady();
#endif
}
extern "C" void Mk64NativeKartBeginReplay(){queue.BeginReplay();}
extern "C" void Mk64NativeKartEndReplay(){
    if(!queue.Remaining())return;
    if(FILE* f=std::fopen("sdmc:/3ds/MK64/native-lab/n5-packet-fault.txt","wb")) {
        std::fprintf(f,"packets=%u remaining=%u\n",unsigned(queue.Size()),unsigned(queue.Remaining()));
        queue.InspectEntries([&](uintptr_t anchor,bool consumed){std::fprintf(f,"%08lx %u\n",(unsigned long)anchor,consumed);});std::fclose(f);
    }
    throw std::runtime_error("Native kart packet was not reached in source order");
}
extern "C" void Mk64NativeKartEnqueue(const void* anchor,const void* matrixKey,const float* model,const void* vertices,const char* texture,const void* palette,const uint8_t colors[8],uint32_t renderMode,bool blend,Mk64NativeCharacterKind kind) {
    if(kind<MK64_NATIVE_BODY || kind>MK64_NATIVE_REFLECTION)throw std::runtime_error("Native character packet kind");
    if(!ready)throw std::runtime_error("Native kart capture before initialization");Packet packet;packet.name=texture;packet.matrixKey=matrixKey;packet.renderMode=renderMode;
    packet.kind=kind;packet.improved=UseImprovedCharacters();
    std::memcpy(packet.model,model,64);std::memcpy(packet.legacy,vertices,sizeof packet.legacy);std::memcpy(packet.palette,palette,512);
    std::memcpy(packet.style.primitive,colors,4);std::memcpy(packet.style.environment,colors+4,4);
    const auto alpha=mk64_3ds::ResolveFast3DAlpha(blend,(renderMode&CVG_X_ALPHA)!=0,false);
    packet.style.blend=alpha.blend;packet.style.cutout=alpha.cutout;packet.style.alphaTest=alpha.alphaTest;
    if(!sprites.Resolve(texture,packet.palette,packet.improved,packet.textureKey))throw std::runtime_error("Native kart frame/palette was not precompiled");
    for(unsigned i=0;i<8;++i){const auto& v=packet.legacy[i].v;auto& out=packet.vertices[i];
        for(unsigned j=0;j<3;++j)out.position[j]=v.ob[j];for(unsigned j=0;j<2;++j)out.uv[j]=(int32_t(v.tc[j])*32768)>>16;std::memcpy(out.color,v.cn,4);}
    CaptureCharacter(anchor,packet);++captured;++kindCaptured[kind];
}
extern "C" void Mk64NativeShadowEnqueue(const void* anchor,const void* matrixKey,const float* model,const void* vertices,const char* texture,uint32_t renderMode,uint8_t alpha,Mk64NativeCharacterKind kind) {
    if(!mk64_3ds::CharacterIsShadow(kind))throw std::runtime_error("Native shadow packet kind");
    if(!ready)throw std::runtime_error("Native shadow capture before initialization");
    Packet p;p.name=texture;p.matrixKey=matrixKey;p.renderMode=renderMode;p.kind=kind;
    p.textureKey=kind==MK64_NATIVE_CREDITS_SHADOW?creditsShadowKey:shadowKey;
    std::memcpy(p.model,model,64);std::memcpy(p.legacy,vertices,4*sizeof(Fast::F3DVtx));
    p.style.quads=1;p.style.technique=ns::SpriteStyle::ConstantRgb;
    p.style.primitive[3]=alpha;p.style.blend=true;p.style.cutout=false;p.style.alphaTest=false;
    for(unsigned i=0;i<4;++i){const auto& v=p.legacy[i].v;auto& out=p.vertices[i];
        for(unsigned j=0;j<3;++j)out.position[j]=v.ob[j];
        for(unsigned j=0;j<2;++j)out.uv[j]=(int32_t(v.tc[j])*32768)>>16;std::memcpy(out.color,v.cn,4);}
    CaptureCharacter(anchor,p);
    ++captured;++shadowCaptured;++kindCaptured[kind];
}
extern "C" bool Mk64NativeKartDispatch(Fast::Interpreter* gfx,const void* anchor){
    auto* backend=static_cast<Fast::GfxRenderingAPICitro3D*>(gfx->mRapi);unsigned active=0;
    // Most legacy command addresses have no native descriptors. Do not enter
    // the kernel just to time an empty lookup; start timing at the first match.
    ++dispatchCalls;uint64_t start=0;bool prepared=false;
    auto end=[&](){
        if(active==1)backend->EndNativeSpriteBatch();
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
        if(active==2)Mk64NativeVegetationEndBatch(backend);
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
        if(active==3)Mk64NativeParticleEndBatch(backend);
#endif
        active=0;
    };
    try {
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        // Dual capture inserts source bytes between native descriptors that
        // ordinarily share one anchor. Keep those contiguous native packets
        // in one scope, as in the native-only producer, while skipping bytes.
        const void* currentAnchor=anchor;
        for(;;) {
        const bool matched=queue.Consume(reinterpret_cast<uintptr_t>(currentAnchor),[&](const mk64_3ds::NativeDrawReference& p){
#else
        const bool matched=queue.Consume(reinterpret_cast<uintptr_t>(anchor),[&](const mk64_3ds::NativeDrawReference& p){
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
            if(p.family==mk64_3ds::NativeDrawFamily::Particle && Mk64NativeParticleIsReference()) {
                Mk64NativeParticleComparedEnd(p.index,currentAnchor);
                // Reference bookkeeping must not introduce a flush, shader
                // scope or triangle-run invalidation into the source path.
                end();Mk64NativeParticleReferenceReached(p.index);return;
            }
#endif
            if(!prepared){start=svcGetSystemTick();++dispatchMatches;gfx->Flush();prepared=true;}
            if(p.family==mk64_3ds::NativeDrawFamily::Character) {
                if(p.index>=usedCharacters)throw std::runtime_error("Native character reference invalid");
                if(active!=1){end();backend->BeginNativeSpriteBatch(sprites);active=1;++batches;}
                Draw(gfx,characterPackets[p.index]);
            }
#ifdef MK64_3DS_NATIVE_VEGETATION_LAB
            else if(p.family==mk64_3ds::NativeDrawFamily::Vegetation) {
                if(active!=2){end();Mk64NativeVegetationBeginBatch(backend);active=2;}
                Mk64NativeVegetationDraw(gfx,p.index);
            }
#endif
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
            else if(p.family==mk64_3ds::NativeDrawFamily::ObjectState) {
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
                // Object state can overwrite matrices/texture/vertex inputs.
                // Commit the particle tail before allowing that boundary.
                if(active==3)end();
#endif
                Mk64NativeObjectStateApply(gfx,p.index);
            }
#endif
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
            else if(p.family==mk64_3ds::NativeDrawFamily::Particle) {
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
                const void* next=Mk64NativeParticleComparedEnd(p.index,currentAnchor);
#endif
                if(active!=3){end();Mk64NativeParticleBeginBatch(backend);active=3;}
                Mk64NativeParticleDraw(gfx,p.index);
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
                Fast::g_exec_stack.currCmd()=static_cast<Fast::F3DGfx*>(const_cast<void*>(next));
#endif
            }
#endif
            else throw std::runtime_error("Native draw family invalid");
        });
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        if(!matched || Fast::g_exec_stack.currCmd()==currentAnchor)break;
        currentAnchor=Fast::g_exec_stack.currCmd();
        }
        if(prepared){end();batchTicks+=svcGetSystemTick()-start;}
        return prepared;
#else
        if(active){end();batchTicks+=svcGetSystemTick()-start;}
        return matched;
#endif
    }catch(...){end();throw;}
}
extern "C" void Mk64NativeKartRestoreInheritedTexture(Fast::Interpreter* gfx) {
    auto& rdp=*gfx->mRdp;if(rdp.loaded_texture[0].addr!=sentinel || !pendingTexture)return;
    // Only a legacy consumer that actually inherits the kart image needs its
    // CPU resource wrapper. The native character draw never uses this decoder.
    auto resource=std::static_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(pendingTexture));
    if(!resource || !resource->ImageData)throw std::runtime_error("Native kart legacy inheritance resource missing");
    Fast::RawTexMetadata metadata{};metadata.width=resource->Width;metadata.height=resource->Height;metadata.h_byte_scale=resource->HByteScale;metadata.v_pixel_scale=resource->VPixelScale;metadata.type=resource->Type;metadata.resource=resource;
    auto& loaded=rdp.loaded_texture[0];loaded.addr=resource->ImageData;loaded.tex_flags=resource->Flags;loaded.raw_tex_metadata=metadata;
    loaded.size_bytes=loaded.line_size_bytes=loaded.full_image_line_size_bytes=uint32_t(loaded.orig_size_bytes*resource->HByteScale*resource->VPixelScale);
    if(rdp.texture_to_load.addr==sentinel){rdp.texture_to_load.addr=resource->ImageData;rdp.texture_to_load.tex_flags=resource->Flags;rdp.texture_to_load.raw_tex_metadata=metadata;}
    rdp.textures_changed[0]=true;++lazyRestores;
}
extern "C" void Mk64NativeKartSaveStats(unsigned frame) {
#ifdef MK64_3DS_NATIVE_PARTICLE_LAB
    Mk64NativeParticleSaveStats(frame);
#endif
    mkdir("sdmc:/3ds/MK64/native-lab",0777);if(FILE* f=std::fopen(
#ifdef MK64_3DS_NATIVE_CACHE_LAB
        "sdmc:/3ds/MK64/native-lab/n13-sprites.csv"
#elif defined(MK64_3DS_NATIVE_PROJECTILE_LAB)
        "sdmc:/3ds/MK64/native-lab/n10-sprites.csv"
#elif defined(MK64_3DS_NATIVE_ITEM_BOX_LAB)
        "sdmc:/3ds/MK64/native-lab/n9-sprites.csv"
#else
        "sdmc:/3ds/MK64/native-lab/n8-sprites.csv"
#endif
,"wb")){
    const auto& s=sprites.Statistics();std::fprintf(f,"frame,captured,drawn,lazy_restores,interpolated,draw_ticks,hits,misses,upload_bytes,shadow_captured,shadow_drawn,batches,batch_ticks,material_changes,texture_binds,dispatch_calls,dispatch_matches\n%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",frame,
            (unsigned long long)captured,(unsigned long long)drawn,(unsigned long long)lazyRestores,(unsigned long long)interpolated,(unsigned long long)drawTicks,(unsigned long long)s.hits,(unsigned long long)s.misses,(unsigned long long)s.uploadBytes,
            (unsigned long long)shadowCaptured,(unsigned long long)shadowDrawn,(unsigned long long)batches,(unsigned long long)batchTicks,
            (unsigned long long)s.materialChanges,(unsigned long long)s.textureBinds,
            (unsigned long long)dispatchCalls,(unsigned long long)dispatchMatches);std::fclose(f);}
    if(FILE* f=std::fopen(
#ifdef MK64_3DS_NATIVE_CACHE_LAB
        "sdmc:/3ds/MK64/native-lab/n13-kinds.csv"
#elif defined(MK64_3DS_NATIVE_PROJECTILE_LAB)
        "sdmc:/3ds/MK64/native-lab/n10-kinds.csv"
#elif defined(MK64_3DS_NATIVE_ITEM_BOX_LAB)
        "sdmc:/3ds/MK64/native-lab/n9-kinds.csv"
#else
        "sdmc:/3ds/MK64/native-lab/n8-kinds.csv"
#endif
,"wb")) {
        const char* names[]={"body","ghost","trail","reflection","ground_shadow","credits_shadow"};
        std::fprintf(f,"kind,captured,drawn\n");
        for(unsigned i=0;i<MK64_NATIVE_CHARACTER_KINDS;++i)std::fprintf(f,"%s,%llu,%llu\n",names[i],
            (unsigned long long)kindCaptured[i],(unsigned long long)kindDrawn[i]);std::fclose(f);
    }
}
