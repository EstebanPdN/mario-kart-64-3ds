#include "native_program_3ds.h"
#include "native_preload_3ds.hpp"
#include "resource_runtime_3ds.h"
#include "diagnostics_3ds.h"
#include "settings_3ds.h"
#include <3ds.h>
#include <malloc.h>
#include <cstdio>
#include <string>
#include <fast/interpreter.h>
#include <fast/resource/type/Light.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge/resourcebridge.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>
#include "native_programs.generated.hpp"
extern "C" void Mk64NativeProgramTriangleBoundary(int triangle);
namespace {
namespace np=mk64_3ds::native_program;
struct Mutation {uint16_t s=0,t=0;bool tile=false,mirror=false,disabled=false;};
std::array<Mutation,np::kProgramCount> mutations;
std::array<bool,np::kProgramCount> mutableClosure{};
uint64_t calls=0,triangles=0;
struct PreparedResource {uint64_t hash;uint32_t kind;void* data=nullptr;const char* name=nullptr;std::shared_ptr<Fast::Texture> owner;std::shared_ptr<void> dataOwner;};
std::vector<PreparedResource> prepared;
std::vector<mk64_3ds::NativeDependency> dependencies;
int preparedMode=-1;
const PreparedResource* Prepared(uint64_t hash) {
    const auto at=std::lower_bound(prepared.begin(),prepared.end(),hash,[](const auto& r,uint64_t h){return r.hash<h;});
    return at!=prepared.end() && at->hash==hash?&*at:nullptr;
}
void PrepareWorkingSet() {
    prepared.clear();preparedMode=Mk64Settings3DSGetUiTextures();
    const size_t initial=mallinfo().uordblks;
    const size_t budget=mk64_3ds::NativePreloadBudget(envGetHeapSize(),initial);
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    try {prepared.reserve(dependencies.size());}
    catch(const std::bad_alloc&) {Mk64Diagnostics3DSCheckpoint("native-preload optional index unavailable");return;}
    for(const auto& dependency:dependencies) {
        const size_t now=mallinfo().uordblks,used=now>initial?now-initial:0;
        // Bound both the working set and transient decoder headroom. A missed
        // optional preload retains the established on-demand resource path.
        if(used>=budget || dependency.bytes+512>budget-used ||
           mk64_3ds::NativePreloadBudget(envGetHeapSize(),now)<dependency.bytes+512)continue;
        PreparedResource item{dependency.hash,dependency.kind};
        try {
            if(dependency.kind==1) {
                item.name=manager->GetArchiveManager()->HashToCString(dependency.hash);
                if(item.name)item.owner=std::static_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(item.name));
                if(!item.owner || !item.owner->ImageData)continue;
                item.data=item.owner->ImageData;
            } else {
                uint64_t token=0;
                if(Mk64Resource3DSAcquireDataByCrc(dependency.hash,&item.data,&token)!=MK64_RESOURCE_LOAD_OK_3DS || !item.data)continue;
                item.dataOwner=std::shared_ptr<void>(item.data,[token](void*){Mk64Resource3DSReleaseData(token);});
            }
        } catch(const std::bad_alloc&) {break;}
        const size_t after=mallinfo().uordblks;
        if(after>initial && after-initial>budget)break;
        prepared.push_back(std::move(item));
    }
    char report[160];const size_t after=mallinfo().uordblks;
    std::snprintf(report,sizeof report,"native-preload resources=%lu/%lu heap-delta=%lu budget=%lu",
        (unsigned long)prepared.size(),(unsigned long)dependencies.size(),(unsigned long)(after>initial?after-initial:0),(unsigned long)budget);
    Mk64Diagnostics3DSCheckpoint(report);
}

struct Context {
    Fast::Interpreter* g;
    unsigned depth=0;
    bool Enter(unsigned id) {
        if(mutations[id].disabled){Mk64NativeProgramTriangleBoundary(0);g->mMarkerOn=false;return false;}
        if(++depth>32)throw std::runtime_error("Native procedure nesting limit");
        ++calls;Mk64NativeProgramTriangleBoundary(false);g->mMarkerOn=true;return true;
    }
    void Leave(){--depth;g->mMarkerOn=false;Mk64NativeProgramTriangleBoundary(false);}
    template<class F> void State(F&& f){Mk64NativeProgramTriangleBoundary(false);f();}
    [[gnu::noinline]] void Triangle(unsigned id,unsigned a,unsigned b,unsigned c) {
        if(mutations[id].mirror)std::swap(b,c);
        Mk64NativeProgramTriangleBoundary(true);g->GfxSpTri1(a,b,c,false);
        Mk64NativeProgramTriangleBoundary(-1);
        ++triangles;
    }
    [[gnu::noinline]] void Primitives(unsigned id,const np::Primitive* data,unsigned count) {
        // Native index data, not graphics instructions. Preserve one material
        // generation across consecutive triangles and the quad diagonal.
        const bool mirror=mutations[id].mirror;
        Mk64NativeProgramTriangleBoundary(true);
        for(unsigned i=0;i<count;++i) {
            auto p=data[i];if(mirror)std::swap(p.b,p.c);
            g->GfxSpTri1(p.a,p.b,p.c,false);++triangles;
            if(p.d!=255){g->GfxSpTri1(p.a,p.c,p.d,false);++triangles;}
        }
        Mk64NativeProgramTriangleBoundary(-1);
    }
    [[gnu::noinline]] void Quad(unsigned id,unsigned a,unsigned b,unsigned c,unsigned d) {
        // Preserve the source mirror producer's byte swap for quads, including
        // its shared diagonal. Do not infer this from the camera cull flag.
        if(mutations[id].mirror)std::swap(b,c);
        Mk64NativeProgramTriangleBoundary(true);
        g->GfxSpTri1(a,b,c,false);g->GfxSpTri1(a,c,d,false);
        Mk64NativeProgramTriangleBoundary(-1);triangles+=2;
    }
    void Vertex(uint64_t hash,unsigned count,unsigned first,unsigned offset) {
        State([&]{
            const auto* cached=Prepared(hash);
            auto* raw=cached && cached->kind==2?cached->data:Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
            if(raw)g->GfxSpVertex(count,first,reinterpret_cast<const Fast::F3DVtx*>(static_cast<const uint8_t*>(raw)+offset));
        });
    }
    void Texture(uint64_t hash,unsigned format,unsigned size,unsigned width) {
        State([&]{
            const auto* cached=Prepared(hash);
            const char* name=nullptr;std::shared_ptr<Fast::Texture> owner;
            if(cached && cached->kind==1){name=cached->name;owner=cached->owner;}
            else {
                auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
                name=manager->GetArchiveManager()->HashToCString(hash);
                if(name)owner=std::static_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(name));
            }
            if(!name)return;
            Fast::RawTexMetadata m{};uint32_t flags=0;
            if(owner && owner->ImageData) {
                m.width=owner->Width;m.height=owner->Height;m.h_byte_scale=owner->HByteScale;
                m.v_pixel_scale=owner->VPixelScale;m.type=owner->Type;m.resource=owner;flags=owner->Flags;
            }
            g->GfxDpSetTextureImage(format,size,width,name,flags,m,owner?owner->ImageData:nullptr);
        });
    }
    void Light(uint64_t hash,unsigned index,unsigned offset,unsigned hasOffset) {
        State([&]{
            const auto* cached=Prepared(hash);
            auto* light=static_cast<Fast::LightEntry*>(cached && cached->kind==3?cached->data:Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash));
            if(light)g->GfxSpMovememF3d(index,offset,reinterpret_cast<const uint8_t*>(&light->Ambient)+(hasOffset==1?8:0));
        });
    }
    void Bounds(unsigned id,unsigned ordinal,unsigned tile,unsigned s,unsigned t,unsigned endS,unsigned endT) {
        if(ordinal==0 && mutations[id].tile){s=mutations[id].s;t=mutations[id].t;}
        State([&]{g->GfxDpSetTileSize(tile,s,t,endS,endT);});
    }
};
int Find(uint64_t hash) {
    const auto* first=np::kPrograms<Context>;const auto* end=first+np::kProgramCount;
    const auto* at=std::lower_bound(first,end,hash,[](const auto& e,uint64_t h){return e.hash<h;});
    return at!=end && at->hash==hash?int(at-first):-1;
}
}
extern "C" bool Mk64NativeProgramDraw(Fast::Interpreter* renderer,uint64_t hash) {
    if(!renderer)return false;Context context{renderer};return np::Execute(context,hash);
}
extern "C" void Mk64NativeProgramSetMirror(const char* name,int swapped) {
    if(!name)return;if(!std::strncmp(name,"__OTR__",7))name+=7;
    const int id=Find(ResourceGetCrcByName(name));if(id>=0)mutations[id].mirror=swapped!=0;
}
extern "C" void Mk64NativeProgramResetMirrors() {for(auto& m:mutations)m.mirror=false;}
extern "C" void Mk64NativeProgramSetTileBounds(uintptr_t resource,int s,int t) {
    if(!resource)return;
    // Both caller forms occur: an asset name and its loaded, marked resource.
    const auto* bytes=reinterpret_cast<const uint8_t*>(resource);uint64_t hash=0;
    if(!std::strncmp(reinterpret_cast<const char*>(bytes),"__OTR__",7))
        hash=ResourceGetCrcByName(reinterpret_cast<const char*>(bytes)+7);
    else {
        size_t entry=0;
        if(!Mk64Resource3DSDisplayListIdentity(reinterpret_cast<const void*>(resource),&hash,&entry) || entry)return;
    }
    const int id=Find(hash);if(id<0)return;
    mutations[id].s=static_cast<uint16_t>(s)&4095;mutations[id].t=static_cast<uint16_t>(t)&4095;
    mutations[id].tile=true;
}

extern "C" bool Mk64NativeProgramMutableDependency(uint64_t hash) {
    const int id=Find(hash);return id>=0 && mutableClosure[id];
}
extern "C" void Mk64NativeProgramResetCourseChanges() {
    for(auto& change:mutations)change={};mutableClosure.fill(false);
}
extern "C" void Mk64NativeProgramDisable(uintptr_t resource) {
    uint64_t hash=0;size_t entry=0;
    if(!Mk64Resource3DSDisplayListIdentity(reinterpret_cast<const void*>(resource),&hash,&entry) || entry)
        throw std::runtime_error("Native mutable procedure identity");
    const int id=Find(hash);if(id<0)throw std::runtime_error("Native mutable procedure missing");
    mutations[id].disabled=true;mutableClosure[id]=true;
    bool changed;
    do {changed=false;for(const auto& edge:np::kParentEdges)
        if(mutableClosure[edge.child] && !mutableClosure[edge.parent]){mutableClosure[edge.parent]=true;changed=true;}
    } while(changed);
}

extern "C" void Mk64NativeProgramReleaseWorkingSet() {
    prepared.clear();dependencies.clear();preparedMode=-1;
}
extern "C" void Mk64NativeProgramPrepareMode() {
    if(!dependencies.empty() && preparedMode!=int(Mk64Settings3DSGetUiTextures()))PrepareWorkingSet();
}
extern "C" void Mk64NativeProgramPreload(const char* course) {
    Mk64NativeProgramReleaseWorkingSet();if(!course)return;
    const std::string path=std::string("romfs:/native-program-dependencies/")+course+"/resources.npr";
    FILE* f=std::fopen(path.c_str(),"rb");if(!f)return;
    bool ok=std::fseek(f,0,SEEK_END)==0;long size=ok?std::ftell(f):-1;
    std::vector<uint8_t> bytes;
    if(size>=8 && size<=8+8192*16 && !std::fseek(f,0,SEEK_SET)){bytes.resize(size);ok=std::fread(bytes.data(),1,size,f)==size;}else ok=false;
    std::fclose(f);
    if(!ok || !mk64_3ds::ValidateNativeDependencies(bytes.data(),bytes.size(),dependencies))
        throw std::runtime_error("Native preload dependency manifest invalid");
    PrepareWorkingSet();
}
