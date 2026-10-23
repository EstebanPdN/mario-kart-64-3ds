// Direct native frame recording. Source addresses are identities, never commands.
#define MK64_NATIVE_GBI_IMPLEMENTATION 1
#include "native_dynamic_3ds.h"
#include "native_program_3ds.h"
#include "resource_runtime_3ds.h"
#include "native_geometry_3ds.hpp"
#include "native_emit.generated.h"
#include <fast/interpreter.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge/resourcebridge.h>
#include <fast/resource/type/Texture.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <limits>
extern "C" void Mk64NativeProgramTriangleBoundary(int);
extern "C" bool Mk64NativeKartDispatch(Fast::Interpreter*,const void*);
extern "C" bool Mk64NativeCourseDrawHash(Fast::Interpreter*,uint64_t);
extern "C" bool Mk64NativeEmbeddedDraw(const void*);
namespace {
using Callback=void(*)(void*,const uint32_t*,size_t);
static_assert(sizeof(void*)==4 && sizeof(Gfx)==8,"Native producer ABI requires the 32-bit 3DS target");
constexpr size_t kNodes=16384,kSlots=32768,kArgs=131072;
struct Node {Callback callback;uint32_t arguments;uint16_t count,span;};
struct Slot {uintptr_t key=0;uint16_t generation=0,node=0;};
std::array<Node,kNodes> nodes;
std::array<Slot,kSlots> slots;
std::array<uint32_t,kArgs> arguments;
uint16_t generation=0;
size_t count=0,argCount=0,executed=0;
bool recording=false,replaying=false;
Fast::Interpreter* renderer=nullptr;
std::array<bool,64> stopped;
size_t depth=0;
Slot* Lookup(uintptr_t key,bool insert) {
    if(!key || key%alignof(Gfx))throw std::runtime_error("Native frame anchor alignment");
    size_t at=((key>>3)*2654435761u)&(kSlots-1);
    for(size_t i=0;i<kSlots;++i,at=(at+1)&(kSlots-1)) {
        auto& slot=slots[at];
        if(slot.generation!=generation) {
            if(!insert)return nullptr;
            slot={key,generation,0};return &slot;
        }
        if(slot.key==key)return &slot;
    }
    throw std::runtime_error("Native frame anchor capacity");
}
void NativeEnd(){if(!depth)throw std::runtime_error("Native end outside procedure");stopped[depth-1]=true;if(renderer)renderer->mMarkerOn=false;}
void NativeRecord(void* packet,unsigned span,Callback callback,const uint32_t* data,size_t size) {
    if(!packet) {
        if(!replaying || !renderer || !depth)throw std::runtime_error("Native immediate call outside replay");
        if(++executed>250000)throw std::runtime_error("Native frame command limit");
        callback(renderer,data,size);return;
    }
    if(!recording)return; // Discard scene setup superseded by config_gfx_pool.
    if(replaying || !span || span>65535 || size>32 || count==kNodes || size>kArgs-argCount)
        throw std::runtime_error("Native frame recording capacity or phase");
    // A producer can intentionally overwrite a previously emitted packet.
    auto* slot=Lookup(reinterpret_cast<uintptr_t>(packet),false);
    if(!slot){slot=Lookup(reinterpret_cast<uintptr_t>(packet),true);slot->node=count++;}
    auto& node=nodes[slot->node];node={callback,uint32_t(argCount),uint16_t(size),uint16_t(span)};
    if(size)std::memcpy(arguments.data()+argCount,data,size*sizeof(uint32_t));argCount+=size;
}
struct ProcedureScope {
    ProcedureScope(){if(depth==stopped.size())throw std::runtime_error("Native procedure nesting limit");stopped[depth++]=false;}
    ~ProcedureScope(){--depth;}
};
void Execute(const void* address) {
    if(!address)return;
    ProcedureScope procedure;
    if(Mk64NativeEmbeddedDraw(address))return;
    auto cursor=reinterpret_cast<uintptr_t>(address);
    while(!stopped[depth-1]) {
        auto* slot=Lookup(cursor,false);
        if(!slot)throw std::runtime_error("Unconverted native frame producer");
        const auto& node=nodes[slot->node];
        if(++executed>250000)throw std::runtime_error("Native frame command limit");
        if(Mk64NativeKartDispatch(renderer,reinterpret_cast<const void*>(cursor)))Mk64NativeProgramTriangleBoundary(0);
        node.callback(renderer,arguments.data()+node.arguments,node.count);
        cursor+=size_t(node.span)*sizeof(Gfx);
    }
}
bool IsResource(uintptr_t address) {
    return address>=0x10000 && !(address&1) && !std::memcmp(reinterpret_cast<const void*>(address),"__OTR__",7);
}
void NativeCall(Fast::Interpreter& g,uintptr_t address,unsigned offset,bool branch) {
    if(!address)return;
    if(IsResource(address)) {
        if(offset)throw std::runtime_error("Native resource entry offset not converted");
        const auto hash=ResourceGetCrcByName(reinterpret_cast<const char*>(address));
        if(!Mk64NativeCourseDrawHash(&g,hash) && !Mk64NativeProgramDraw(&g,hash))
            throw std::runtime_error("Unconverted native resource procedure");
    } else {
        const auto* resolved=static_cast<const uint8_t*>(g.SegAddr(address));
        if(!resolved)return;
        {
            uint64_t hash=0;size_t entry=0;
            if(Mk64Resource3DSDisplayListIdentity(resolved,&hash,&entry)) {
                if(offset || entry)throw std::runtime_error("Native resource entry offset not converted");
                if(!Mk64NativeCourseDrawHash(&g,hash) && !Mk64NativeProgramDraw(&g,hash))
                    throw std::runtime_error("Unconverted loaded native procedure");
            } else Execute(resolved+offset*sizeof(Gfx));
        }
    }
    if(branch)NativeEnd();
}
void NativeVertex(Fast::Interpreter& g,uintptr_t address,unsigned count,unsigned first) {
    const void* data=IsResource(address)?ResourceGetDataByName(reinterpret_cast<const char*>(address)):g.SegAddr(address);
    if(data)g.GfxSpVertex(count,first,static_cast<const Fast::F3DVtx*>(data));
}
void NativeTexture(Fast::Interpreter& g,unsigned fmt,unsigned size,unsigned width,uintptr_t address) {
    auto* data=g.SegAddr(address);const char* name=static_cast<const char*>(data);
    Fast::RawTexMetadata metadata{};metadata.h_byte_scale=metadata.v_pixel_scale=1;
    uint32_t flags=0;
    if(IsResource(reinterpret_cast<uintptr_t>(data))) {
        auto owner=std::static_pointer_cast<Fast::Texture>(Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(name));
        data=nullptr;
        if(owner && owner->ImageData) {
            metadata.width=owner->Width;metadata.height=owner->Height;
            metadata.h_byte_scale=owner->HByteScale;metadata.v_pixel_scale=owner->VPixelScale;
            metadata.type=owner->Type;metadata.resource=owner;flags=owner->Flags;data=owner->ImageData;
        }
    }
    g.GfxDpSetTextureImage(fmt,size,width,name,flags,metadata,data);
}
void NativeInvalidate(Fast::Interpreter& g,uintptr_t address) {
    if(!address){g.TextureCacheClear();return;}
    const void* data=IsResource(address)?ResourceGetDataByName(reinterpret_cast<const char*>(address)):reinterpret_cast<const void*>(address);
    g.TextureCacheDelete(static_cast<const uint8_t*>(data));
}
void NativeTriangle(Fast::Interpreter& g,unsigned a,unsigned b,unsigned c,unsigned rotation) {
    const unsigned v[]={a&127,b&127,c&127};rotation=rotation==0?0:rotation==1?1:2;
    g.GfxSpTri1(v[rotation],v[(rotation+1)%3],v[(rotation+2)%3],false);
}
void NativeQuad(Fast::Interpreter& g,unsigned a,unsigned b,unsigned c,unsigned d,unsigned rotation) {
    const unsigned v[]={a&127,b&127,c&127,d&127};rotation=rotation<3?rotation:3;
    const unsigned x=v[rotation],y=v[(rotation+1)%4],z=v[(rotation+2)%4],w=v[(rotation+3)%4];
    g.GfxSpTri1(x,y,z,false);g.GfxSpTri1(x,z,w,false);
}
void NativeCull(Fast::Interpreter& g,unsigned first,unsigned last) {
    if(first>last || last>=MAX_VERTICES)return;
    unsigned rejected=0xff;
    for(unsigned i=first;i<=last;++i){mk64_3ds::gNativeGeometry3DS.Materialize(i,g.mRsp->loaded_vertices[i]);rejected&=g.mRsp->loaded_vertices[i].clip_rej;}
    if(rejected)NativeEnd();
}
void NativeReadFramebuffer(Fast::Interpreter& g,unsigned source,uintptr_t pointer,unsigned width,unsigned height,bool swap) {
    auto* buffer=reinterpret_cast<uint16_t*>(pointer);g.Flush();
    g.mRapi->ReadFramebufferToCPU(source,width,height,buffer);
    if(swap)for(size_t i=0;i<size_t(width)*height;++i)buffer[i]=uint16_t((buffer[i]>>8)|(buffer[i]<<8));
}
void NativeSetFramebuffer(Fast::Interpreter& g,int32_t framebuffer) {
    g.Flush();
    if(framebuffer){g.SetFrameBuffer(framebuffer,1.f);g.mActiveFrameBuffer=g.mFrameBuffers.find(framebuffer);g.mFbActive=true;}
    else {g.ResetFrameBuffer();g.mFbActive=false;g.mActiveFrameBuffer=g.mFrameBuffers.end();}
}
void NativeResetFramebuffer(Fast::Interpreter& g) {
    g.Flush();g.mFbActive=false;g.mActiveFrameBuffer=g.mFrameBuffers.end();
    g.mRapi->StartDrawToFramebuffer(g.mRendersToFb?g.mGameFb:0,float(g.mCurDimensions.height)/g.mNativeDimensions.height);
    g.mRdp->viewport_or_scissor_changed=true;g.mRenderingState.viewport={};g.mRenderingState.scissor={};
}
}
extern "C" void Mk64NativeFrameBegin() {
    if(replaying)throw std::runtime_error("Native frame reset during replay");
    if(++generation==0){slots={};++generation;}
    count=argCount=0;recording=true;
}
extern "C" int Mk64NativeListStopped(){return depth && stopped[depth-1];}
extern "C" void Mk64NativeFrameReplay(Fast::Interpreter* gfx,const void* root) {
    if(!recording || replaying || !gfx)throw std::runtime_error("Native frame replay phase");
    replaying=true;renderer=gfx;executed=0;
    struct Guard {~Guard(){renderer=nullptr;replaying=false;depth=0;}} guard;
    Execute(root);
}
#include "native_emit.generated.inc"
