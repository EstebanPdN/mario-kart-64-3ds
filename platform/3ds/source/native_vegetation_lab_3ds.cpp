#include "texture_mode_3ds.hpp"
#include "native_data_path_3ds.hpp"
#include "native_vegetation_lab_3ds.h"
#include "native_gpu_scene_3ds.hpp"
#include "native_vegetation_exit_3ds.hpp"
#include "gfx_citro3d.h"
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
#include "settings_3ds.h"
#include "texture_lease_3ds.hpp"
#include "improved_texture_pack_3ds.hpp"
#include "native_projectile_contract_3ds.h"
#endif
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
extern "C" int8_t gTLUTRedShell[512];
#endif
namespace ns=mk64_3ds::native_scene;
extern "C" Mtx* Mk64NativeObjectMatrixRecord(const float*);
extern "C" bool Mk64FrameInterpolation3DSLookup(const Mtx*,MtxF*);
namespace {
struct ObjectPacket {const Mtx* key=nullptr;float model[4][4]{};uint32_t root=0;};
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
constexpr unsigned rootCount=72,viewVariants=32,objectCapacity=512;
unsigned greenPaletteOwner=~0u,bluePaletteOwner=~0u;bool redPaletteVerified=false;
std::array<uint64_t,3> paletteCaptured{},paletteApplied{};
std::vector<std::shared_ptr<Fast::Texture>> originalOwners,improvedOwners;
bool ownerModeReady=false,ownerModeImproved=false;
#else
constexpr unsigned rootCount=21,viewVariants=32,objectCapacity=512;
#endif
struct ObjectState {uint32_t clear,set,mode,resetScale;
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
    uint32_t palette=~0u;
#endif
};
std::array<ObjectState,256> objectStates{};unsigned usedStates=0;uint64_t stateCaptured=0,stateApplied=0;
#else
constexpr unsigned rootCount=12,viewVariants=8,objectCapacity=256;
#endif
ns::GpuScene vegetationScene;
std::array<ObjectPacket,objectCapacity> objects{};unsigned usedObjects=0;
std::vector<ns::ExitState> exits;std::vector<ns::VegetationExit> extras;
std::vector<std::string> names;std::vector<std::shared_ptr<Fast::Texture>> owners;
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
constexpr const char* questionName="textures/common_data/common_texture_item_box_question_mark";
std::shared_ptr<Fast::Texture> originalQuestion,improvedQuestion;
unsigned questionOwner=~0u;
std::shared_ptr<Fast::Texture> OriginalTextureOwner(const char* name) {
    auto lease=std::make_shared<mk64_3ds::TextureLease3DS>(name);
    if(!lease->Valid())throw std::runtime_error("Native original item-box owner missing");
    const auto& data=lease->Get();auto init=std::make_shared<Ship::ResourceInitData>();init->Path=name;
    auto texture=std::shared_ptr<Fast::Texture>(new Fast::Texture(init),[lease](Fast::Texture* p){delete p;});
    texture->mImageBuffer=std::make_shared<std::vector<char>>();
    texture->ImageData=const_cast<uint8_t*>(data.data);texture->ImageDataSize=data.size;
    texture->Width=data.width;texture->Height=data.height;texture->Type=static_cast<Fast::TextureType>(data.type);
    return texture;
}
std::shared_ptr<Fast::Texture> ImprovedTextureOwner(const char* name,const Fast::Texture& original) {
    auto image=mk64_3ds::improved::LocalPack().Load(name,original.Width,original.Height,true);
    if(!image || image.width!=original.Width || image.height!=original.Height || image.logicalWidth!=original.Width || image.logicalHeight!=original.Height)return {};
    auto init=std::make_shared<Ship::ResourceInitData>();init->Path=name;auto texture=std::make_shared<Fast::Texture>(init);
    texture->Type=Fast::TextureType::RGBA32bpp;texture->Width=original.Width;texture->Height=original.Height;texture->Flags=TEX_FLAG_LOAD_AS_RAW;
    const unsigned bits[]={0,32,16,4,8,4,8,4,8,16};
    const unsigned type=static_cast<unsigned>(original.Type);if(type>=10 || !bits[type])return {};
    texture->HByteScale=32.f/bits[type];texture->VPixelScale=1;texture->mImageBuffer=image.pixels;
    texture->ImageData=reinterpret_cast<uint8_t*>(image.pixels->data());texture->ImageDataSize=image.pixels->size();return texture;
}
#endif
std::array<uint64_t,rootCount> captured{},drawn{};
uint64_t drawTicks=0,exitTicks=0,scopes=0,interpolated=0;
bool ready=false;
std::vector<uint8_t> Read(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)throw std::runtime_error("Native vegetation file missing");
    std::fseek(f,0,SEEK_END);long n=std::ftell(f);std::rewind(f);
    if(n<=0 || n>1024*1024){std::fclose(f);throw std::runtime_error("Native vegetation file size");}
    std::vector<uint8_t> bytes(n);const bool ok=std::fread(bytes.data(),1,n,f)==size_t(n);std::fclose(f);
    if(!ok)throw std::runtime_error("Native vegetation file read");return bytes;
}
Fast::RawTexMetadata Metadata(unsigned id) {
    const auto& owner=owners.at(id);Fast::RawTexMetadata m{};m.width=owner->Width;m.height=owner->Height;
    m.h_byte_scale=owner->HByteScale;m.v_pixel_scale=owner->VPixelScale;m.type=owner->Type;m.resource=owner;return m;
}
uint64_t Combine(unsigned kind) {
    if(kind==0) {const uint64_t rgb=15|(15<<4)|(31<<8)|(4<<13),alpha=7|(7<<3)|(7<<6)|(4<<9);return rgb|(alpha<<16)|(rgb<<28)|(alpha<<44);}
    const uint64_t rgb=kind==3?(15|(15<<4)|(31<<8)|(3<<13)):(1|(15<<4)|(4<<8)|(7<<13));
    const uint64_t alpha=kind==1?(1|(7<<3)|(4<<6)|(7<<9)):(7|(7<<3)|(7<<6)|(1<<9));
    return rgb|(alpha<<16)|(rgb<<28)|(alpha<<44);
}
void ApplyState(Fast::Interpreter* gfx,unsigned root) {
    const auto& e=extras[root];const auto& s=exits[root];auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;
    rsp.geometry_mode=(rsp.geometry_mode&~e.geometryClear)|e.geometrySet;
    rdp.other_mode_h=(rdp.other_mode_h&~e.highMask)|e.highValue;
    rdp.other_mode_l=(rdp.other_mode_l&~e.lowMask)|e.lowValue;
    rdp.combine_mode=Combine(s.shader);
    if(e.primitiveWritten){std::memcpy(&rdp.prim_color,&e.primitive,4);rdp.prim_lod_fraction=0;}
    if(e.paletteWritten) {
        const auto& owner=owners[e.paletteTexture];
        gfx->GfxDpSetTile(0,0,0,256,7,0,0,0,0,0,0,0);
        gfx->GfxDpSetTextureImage(0,2,1,names[e.paletteTexture].c_str(),owner->Flags,Metadata(e.paletteTexture),owner->ImageData);
        gfx->GfxDpLoadTlut(7,255); // Resource-backed reads are bounded and zero-pad an absent tail.
    }
}
void ApplyTextureAndVertexExit(Fast::Interpreter* gfx,unsigned root) {
    const auto& s=exits[root];auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;
    if(s.scaleWritten)gfx->GfxSpTexture(s.scaleS,s.scaleT,0,s.renderTile,1);
    for(unsigned i=0;i<8;++i) {
        const auto& t=s.tiles[i];
        if(s.tileMask&(1u<<i))gfx->GfxDpSetTile(t.format,t.size,t.line,t.memory,i,t.palette,t.wrapT,t.maskT,t.shiftT,t.wrapS,t.maskS,t.shiftS);
        if(s.boundsMask&(1u<<i))gfx->GfxDpSetTileSize(i,t.lowS,t.lowT,t.highS,t.highT);
    }
    if(s.sourceWritten)gfx->GfxDpSetTextureImage(0,s.sourceSize,s.sourceWidth,names[s.sourceTexture].c_str(),owners[s.sourceTexture]->Flags,Metadata(s.sourceTexture),owners[s.sourceTexture]->ImageData);
    for(unsigned i=0;i<2;++i)if(s.loadedMask&(1u<<i)) {
        const auto& l=s.loaded[i];auto& target=rdp.loaded_texture[i];const auto& owner=owners[l.texture];
        target.addr=owner->ImageData;target.orig_size_bytes=l.bytes;
        target.size_bytes=uint32_t(l.bytes*owner->HByteScale*owner->VPixelScale);
        uint32_t stride=target.size_bytes,candidate=l.size==0?(l.width+1)/2:l.width*(1u<<(l.size-1));
        if(l.width>1 && (owner->HByteScale!=1 || owner->VPixelScale!=1) && owner->Height && target.size_bytes%owner->Height==0)stride=target.size_bytes/owner->Height;
        else if(l.width>1 && candidate && candidate<target.size_bytes && target.size_bytes%candidate==0)stride=candidate;
        target.full_image_line_size_bytes=target.line_size_bytes=stride;target.tex_flags=owner->Flags;
        target.raw_tex_metadata=Metadata(l.texture);target.masked=target.blended=false;rdp.textures_changed[i]=true;
    }
    Fast::F3DVtx raw[64]{};const uint64_t mask=s.slotMaskLow|(uint64_t(s.slotMaskHigh)<<32);
    for(unsigned i=0;i<64;++i)if((mask>>i)&1) {
        const auto& v=vegetationScene.Vertices()[s.vertices[i]];
        for(unsigned j=0;j<3;++j)raw[i].v.ob[j]=v.position[j];std::memcpy(raw[i].v.cn,v.color,4);
    }
    for(unsigned at=0;at<64;) {
        if(!((mask>>at)&1)){++at;continue;}
        const unsigned first=at;while(at<64 && ((mask>>at)&1))++at;
        gfx->GfxSpVertex(at-first,first,raw+first);
        for(unsigned i=first;i<at;++i){const auto& v=vegetationScene.Vertices()[s.vertices[i]];rsp.loaded_vertices[i].u=v.uv[0];rsp.loaded_vertices[i].v=v.uv[1];}
    }
}
}
extern "C" void Mk64NativeVegetationLoad() {
    if(!vegetationScene.Load(MK64_NATIVE_DATA_ROOT "native-vegetation") || vegetationScene.ViewCount()!=rootCount*viewVariants+(rootCount==21?32:0))
        throw std::runtime_error("Native vegetation scene invalid");
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
    const auto map=Read(MK64_NATIVE_DATA_ROOT "native-vegetation/variants.ntr");
    if(map.size()<8 || std::memcmp(map.data(),"NTR1",4))throw std::runtime_error("Native texture variant map invalid");
    const unsigned count=ns::ReadLE32(map.data()+4);
    if(count!=vegetationScene.TextureCount() || map.size()!=8+count*2)throw std::runtime_error("Native texture variant map size");
    std::vector<uint16_t> mapping(count);for(unsigned i=0;i<count;++i)mapping[i]=map[8+i*2]|uint16_t(map[9+i*2])<<8;
    if(!vegetationScene.RegisterTextureVariant(mapping.data(),mapping.size()))throw std::runtime_error("Native texture variant registration failed");
#endif
    auto bytes=Read(MK64_NATIVE_DATA_ROOT "native-vegetation/exit.ncx");uint32_t textures=0;
    if(!ns::ValidateExits(bytes.data(),bytes.size(),vegetationScene.VertexCount(),exits,textures,rootCount,3))
        throw std::runtime_error("Native vegetation exit table invalid");
    bytes=Read(MK64_NATIVE_DATA_ROOT "native-vegetation/extra.nvx");
    if(!ns::ValidateVegetationExits(bytes.data(),bytes.size(),textures,extras,rootCount,rootCount>=21?0x23200u:0x23000u))throw std::runtime_error("Native vegetation state invalid");
    bytes=Read(MK64_NATIVE_DATA_ROOT "native-vegetation/exit-textures.txt");std::string list(bytes.begin(),bytes.end());size_t at=0;
    names.reserve(textures);owners.reserve(textures);
    while(at<list.size()) {
        const size_t end=list.find('\n',at);if(end==std::string::npos)throw std::runtime_error("Native vegetation names invalid");
        names.push_back(list.substr(at,end-at));at=end+1;
        std::shared_ptr<Fast::Texture> owner;
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
        owner=OriginalTextureOwner(names.back().c_str());auto alternative=names.back().starts_with("textures/common_data/")?ImprovedTextureOwner(names.back().c_str(),*owner):nullptr;
        originalOwners.push_back(owner);improvedOwners.push_back(alternative?alternative:owner);
        if(names.back()==questionName){questionOwner=owners.size();originalQuestion=owner;improvedQuestion=alternative;}
        if(names.back()=="textures/common_data/common_tlut_green_shell")greenPaletteOwner=owners.size();
        if(names.back()=="textures/common_data/common_tlut_blue_shell")bluePaletteOwner=owners.size();
#else
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
        if(names.back()==questionName){questionOwner=owners.size();originalQuestion=OriginalTextureOwner(questionName);improvedQuestion=ImprovedTextureOwner(questionName,*originalQuestion);owner=originalQuestion;}
        else
#endif
            owner=std::static_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(names.back().c_str()));
#endif
        if(!owner || !owner->ImageData || owner->HByteScale!=1 || owner->VPixelScale!=1)throw std::runtime_error("Native vegetation source owner invalid");
        owners.push_back(owner);
    }
    if(owners.size()!=textures)throw std::runtime_error("Native vegetation source count invalid");
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
    if(!originalQuestion || !improvedQuestion || greenPaletteOwner>=owners.size() || bluePaletteOwner>=owners.size())throw std::runtime_error("Native projectile owners incomplete");
#endif
    ready=true;
}
extern "C" void Mk64NativeVegetationReset(){usedObjects=0;
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
    usedStates=0;
#endif
}
extern "C" void Mk64NativeVegetationEnqueue(unsigned root,const void* anchor,const float* model) {
    if(!ready || root>=rootCount || usedObjects==objects.size())throw std::runtime_error("Native vegetation capture limit/state");
    auto& p=objects[usedObjects];p.root=root;std::memcpy(p.model,model,sizeof p.model);p.key=Mk64NativeObjectMatrixRecord(model);
    Mk64NativeScheduleVegetation(anchor,usedObjects);++usedObjects;++captured[root];
}
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
extern "C" void Mk64NativeShellEnqueue(const void* anchor,const float* model,unsigned species,unsigned frame,unsigned reverse) {
    if(species>=3 || frame>=8 || reverse>1)throw std::runtime_error("Native shell animation contract");
    Mk64NativeVegetationEnqueue(24+species*16+frame*2+(reverse?0:1),anchor,model);
}
extern "C" void Mk64NativeProjectilePalette(const void* anchor,unsigned species) {
    if(!ready || species>=3 || usedStates==objectStates.size())throw std::runtime_error("Native projectile palette capture contract");
    objectStates[usedStates]={0,0,~0u,0,species};Mk64NativeScheduleObjectState(anchor,usedStates++);++stateCaptured;++paletteCaptured[species];
}
void ApplyProjectilePalette(Fast::Interpreter* gfx,unsigned species) {
    if(species>=3)throw std::runtime_error("Native projectile palette selection invalid");
    gfx->GfxDpSetTile(0,0,0,256,7,0,0,0,0,0,0,0);
    if(species==1) {
        if(!redPaletteVerified){uint8_t reference[512];Mk64NativeBuildRedShellPalette(originalOwners[greenPaletteOwner]->ImageData,reference);
            if(std::memcmp(reference,gTLUTRedShell,512))throw std::runtime_error("Native red-shell palette differs from converted N64 source");redPaletteVerified=true;}
        gfx->GfxDpSetTextureImage(0,2,1,nullptr,0,Fast::RawTexMetadata{},gTLUTRedShell);
    }else {
        const unsigned owner=species==0?greenPaletteOwner:bluePaletteOwner;
        gfx->GfxDpSetTextureImage(0,2,1,names[owner].c_str(),owners[owner]->Flags,Metadata(owner),owners[owner]->ImageData);
    }
    gfx->GfxDpLoadTlut(7,255);++paletteApplied[species];
}
#endif
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
extern "C" void Mk64NativeItemBoxState(const void* anchor,uint32_t clear,uint32_t set,uint32_t mode,unsigned resetScale) {
    if(!ready || (clear|set)&~0x23200u || (clear&set) || resetScale>1 || usedStates==objectStates.size())
        throw std::runtime_error("Native object state limit/contract");
    objectStates[usedStates]={clear,set,mode,resetScale};Mk64NativeScheduleObjectState(anchor,usedStates++);++stateCaptured;
}
void Mk64NativeObjectStateApply(Fast::Interpreter* gfx,unsigned packet) {
    if(packet>=usedStates)throw std::runtime_error("Native object state reference invalid");
    const auto& s=objectStates[packet];auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;
    rsp.geometry_mode=(rsp.geometry_mode&~s.clear)|s.set;
    if(s.mode!=~0u)rdp.other_mode_l=(rdp.other_mode_l&7u)|(s.mode&~7u);
    if(s.resetScale)gfx->GfxSpTexture(65535,65535,0,0,1);
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
    if(s.palette!=~0u)ApplyProjectilePalette(gfx,s.palette);
#endif
    ++stateApplied;
}
#endif
void Mk64NativeVegetationBeginBatch(Fast::GfxRenderingAPICitro3D* backend){backend->BeginNativeSceneBatch(vegetationScene);++scopes;}
void Mk64NativeVegetationEndBatch(Fast::GfxRenderingAPICitro3D* backend){backend->EndNativeSceneBatch();}
void Mk64NativeVegetationDraw(Fast::Interpreter* gfx,unsigned packet) {
    if(packet>=usedObjects)throw std::runtime_error("Native vegetation reference invalid");
    const auto& p=objects[packet];auto& rsp=*gfx->mRsp;auto& rdp=*gfx->mRdp;MtxF replacement{};const float (*input)[4]=p.model;
    if(Mk64FrameInterpolation3DSLookup(p.key,&replacement)){input=replacement.mf;++interpolated;}
    else {auto it=gfx->mCurMtxReplacements->find(const_cast<Mtx*>(p.key));if(it!=gfx->mCurMtxReplacements->end()){input=it->second.mf;++interpolated;}}
    float model[4][4];for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
        const float v=input[i][j];if(!std::isfinite(v) || v<-32768 || v>=32768)throw std::runtime_error("Native vegetation transform invalid");
        model[i][j]=static_cast<int32_t>(v*65536.f)*(1.f/65536);
    }
    if(!rsp.modelview_matrix_stack_size)rsp.modelview_matrix_stack_size=1;
    std::memcpy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size-1],model,sizeof model);
    gfx->MatrixMul(rsp.MP_matrix,model,rsp.P_matrix);rsp.lights_changed=true;
    const unsigned incomingCull=(rsp.geometry_mode&0x1000?1:0)|(rsp.geometry_mode&0x2000?2:0);
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
    if(p.root>=12 && (rdp.other_mode_l&3u))throw std::runtime_error("Native objects inherited alpha compare needs conversion");
    const bool improved=mk64_3ds::ImprovedRaceTextures(Mk64Settings3DSGetUiTextures());
    if(questionOwner>=owners.size() || !originalQuestion || !improvedQuestion)throw std::runtime_error("Native item-box owner selection invalid");
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
    if(!ownerModeReady || ownerModeImproved!=improved){for(unsigned i=0;i<owners.size();++i)owners[i]=improved?improvedOwners[i]:originalOwners[i];
        vegetationScene.UseTextureVariant(improved);ownerModeReady=true;ownerModeImproved=improved;}
#else
    owners[questionOwner]=improved?improvedQuestion:originalQuestion;
#endif
    const auto rawMode=rdp.other_mode_l;
    const bool inheritedBlend=(((rawMode>>20)&3)==1 && ((rawMode>>16)&3)==0) || (((rawMode>>22)&3)==1 && ((rawMode>>18)&3)==0);
    const bool inheritedEdge=rawMode&0x1000u;
    const unsigned alpha=inheritedBlend?(inheritedEdge?2:3):(inheritedEdge?1:0);
#endif
    ApplyState(gfx,p.root);
    if(rsp.geometry_mode&0x20000u)throw std::runtime_error("Native vegetation inherited lighting needs conversion");
    if((rdp.other_mode_l&G_ZS_PRIM) || !(rdp.other_mode_h&G_TP_PERSP) || (rdp.other_mode_l&3u)==G_AC_DITHER)
        throw std::runtime_error("Native vegetation inherited depth/perspective/dither needs conversion");
    auto& state=gfx->mRenderingState;
    const bool depth=((rsp.geometry_mode&G_ZBUFFER)||(rdp.other_mode_l&G_ZS_PRIM))&&(rdp.other_mode_l&Z_CMP);
    const bool write=rdp.other_mode_l&Z_UPD,decal=(rdp.other_mode_l&ZMODE_DEC)==ZMODE_DEC;
    gfx->mRapi->SetDepthTestAndMask(depth,write);state.depth_test_and_mask=(depth?1:0)|(write?2:0);
    gfx->mRapi->SetZmodeDecal(decal);state.decal_mode=decal;
    gfx->mRapi->SetViewport(rdp.viewport.x,rdp.viewport.y,rdp.viewport.width,rdp.viewport.height);state.viewport=rdp.viewport;
    gfx->mRapi->SetScissor(rdp.scissor.x,rdp.scissor.y,rdp.scissor.width,rdp.scissor.height);state.scissor=rdp.scissor;rdp.viewport_or_scissor_changed=false;
    const unsigned cull=incomingCull;
    const bool mirror=rsp.extra_geometry_mode&G_EX_INVERT_CULLING;
    vegetationScene.SetLinearFiltering((rdp.other_mode_h&(3u<<12))!=0);
    const uint64_t started=svcGetSystemTick();
    static_cast<Fast::GfxRenderingAPICitro3D*>(gfx->mRapi)->DrawNativeCourse(vegetationScene,
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
        p.root*32+cull*8+mirror*4+alpha,
#else
        (p.root==13 && improved?21:p.root)*32+cull*8+mirror*4+alpha,
#endif
#else
        p.root*8+cull*2+mirror,
#endif
&rsp.MP_matrix[0][0],gfx->AdjXForAspectRatio(1.f));
    const uint64_t submitted=svcGetSystemTick();ApplyTextureAndVertexExit(gfx,p.root);
    drawTicks+=submitted-started;exitTicks+=svcGetSystemTick()-submitted;++drawn[p.root];
}
extern "C" void Mk64NativeVegetationSaveStats(unsigned frame) {
    mkdir("sdmc:/3ds/MK64/native-lab",0777);
    if(FILE* f=std::fopen(
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
#ifdef MK64_3DS_NATIVE_CACHE_LAB
        "sdmc:/3ds/MK64/native-lab/n13-objects.csv",
#elif defined(MK64_3DS_NATIVE_PROJECTILE_LAB)
        "sdmc:/3ds/MK64/native-lab/n10-objects.csv",
#else
        "sdmc:/3ds/MK64/native-lab/n9-objects.csv",
#endif
#else
        "sdmc:/3ds/MK64/native-lab/n8-vegetation.csv",
#endif
        "wb")) {
        std::fprintf(f,"frame,scopes,draw_ticks,exit_ticks,interpolated,used_objects\n%u,%llu,%llu,%llu,%llu,%u\nroot,captured,drawn\n",frame,
            (unsigned long long)scopes,(unsigned long long)drawTicks,(unsigned long long)exitTicks,(unsigned long long)interpolated,usedObjects);
        for(unsigned i=0;i<rootCount;++i)std::fprintf(f,"%u,%llu,%llu\n",i,(unsigned long long)captured[i],(unsigned long long)drawn[i]);
#ifdef MK64_3DS_NATIVE_ITEM_BOX_LAB
        std::fprintf(f,"state_captured,state_applied,used_states\n%llu,%llu,%u\n",(unsigned long long)stateCaptured,(unsigned long long)stateApplied,usedStates);
#endif
#ifdef MK64_3DS_NATIVE_PROJECTILE_LAB
        std::fprintf(f,"palette_species,captured,applied\n");for(unsigned i=0;i<3;++i)std::fprintf(f,"%u,%llu,%llu\n",i,(unsigned long long)paletteCaptured[i],(unsigned long long)paletteApplied[i]);
#endif
        std::fclose(f);
    }
}
