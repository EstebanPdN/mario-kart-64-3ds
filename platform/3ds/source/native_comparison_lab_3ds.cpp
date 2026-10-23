#include "native_comparison_lab_3ds.hpp"
#include "gfx_citro3d.h"
#include "system_3ds.h"
#include <fast/interpreter.h>
#include <3ds.h>
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
#include "native_particle_compare_3ds.h"
#include "native_frame_comparison_3ds.hpp"
#include <citro3d.h>
#include <cmath>
#include <sys/stat.h>
#endif

namespace {
struct Sample {
    uint64_t runTicks=0,endTicks=0,draws=0,triangles=0,uploads=0,nativeRoots=0;
    uint32_t waitUs=0,pair=0,order=0;bool reference=false,warmup=false;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    uint64_t particleDraws=0,particleReferences=0,captureTicks=0,imageHash=0;
    uint64_t particleBatches=0,geometryCleans=0,exitApplications=0;
    uint32_t gpuQueueUs=0;
#endif
};
constexpr unsigned kWarmupPairs=2,kMeasuredPairs=12;
std::array<Sample,(kWarmupPairs+kMeasuredPairs)*2> samples{};
unsigned used=0;bool attempted=false,complete=false;
uint64_t commandHash=0;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
std::array<mk64_3ds::NativeFrameDifference,kWarmupPairs+kMeasuredPairs> imageDifferences{};
std::array<uint64_t,2> stableImageHash{};std::array<bool,2> hasImageHash{};
bool deterministicImages=true,nonuniformImages=true;
unsigned particlePackets=0;size_t imageBytes=0;
void SaveImage(const char* path,const void* pixels,size_t bytes) {
    FILE* f=std::fopen(path,"wb");if(!f)throw std::runtime_error("Native particle comparison image open");
    const bool written=std::fwrite(pixels,1,bytes,f)==bytes;
    const bool closed=std::fclose(f)==0;
    if(!written || !closed)throw std::runtime_error("Native particle comparison image write");
}
#endif
uint64_t Hash(const void* commands,size_t bytes) {
    uint64_t hash=14695981039346656037ULL;
    const auto* input=static_cast<const uint8_t*>(commands);
    for(size_t i=0;i<bytes;++i){hash^=input[i];hash*=1099511628211ULL;}return hash;
}
struct Restore {
    Fast::Interpreter* interpreter;
    // RDP copy retains shared CPU resource owners. Its palette pointers refer
    // to the original interpreter's staging array, whose address never moves.
    Fast::RSP rsp;Fast::RDP rdp;
    const std::unordered_map<Mtx*,MtxF>* replacements;
    unsigned interpolationIndex,interpolationTarget;float interpolationT;
    explicit Restore(Fast::Interpreter* p):interpreter(p),rsp(*p->mRsp),rdp(*p->mRdp),
        replacements(p->mCurMtxReplacements),interpolationIndex(p->mInterpolationIndex),
        interpolationTarget(p->mInterpolationIndexTarget),interpolationT(p->mInterpolationT){}
    void Inputs(){*interpreter->mRsp=rsp;*interpreter->mRdp=rdp;}
    ~Restore(){Inputs();interpreter->mCurMtxReplacements=replacements;Mk64NativeCourseUseReference(false);
        interpreter->mInterpolationIndex=interpolationIndex;
        interpreter->mInterpolationIndexTarget=interpolationTarget;interpreter->mInterpolationT=interpolationT;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        Mk64NativeParticleUseReference(false);Mk64NativeParticleUseEagerExits(false);
#endif
    }
};
}

void Mk64NativeComparisonRun(Fast::Interpreter* interpreter,
        Fast::GfxRenderingAPICitro3D* renderer,void* commands,size_t bytes,uint64_t frame) {
    if(attempted || frame!=240)return;
    attempted=true;
    if(!interpreter || !renderer || !commands || !bytes || bytes>4*1024*1024)
        throw std::runtime_error("Native comparison command span");
    const uint64_t initialHash=Hash(commands,bytes);commandHash=initialHash;
    // Keep the large snapshot off the game thread's bounded stack. Allocate
    // once outside all timed scopes, then restore the interpreter's inputs and
    // prior replacement-map pointer before the ordinary frame resumes.
    auto restore=std::make_unique<Restore>(interpreter);
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    Mk64NativeParticleValidateComparison(commands,bytes);particlePackets=Mk64NativeParticlePacketCount();
    imageBytes=renderer->NativeComparisonFrameBytes();
    if(!imageBytes || imageBytes>800u*240u*4 || (imageBytes&127))
        throw std::runtime_error("Native particle comparison framebuffer size");
    // Two bounded, completion-owned images. Transfers/hash/SD writes are outside
    // CPU recording/end timers. The source queue completes before readback.
    std::unique_ptr<uint8_t,decltype(&linearFree)> images(
        static_cast<uint8_t*>(linearMemAlign(imageBytes*2,128)),linearFree);
    if(!images)throw std::bad_alloc();
    mkdir("sdmc:/3ds/MK64/native-lab",0777);
#endif
    const std::unordered_map<Mtx*,MtxF> noReplacements;
    for(unsigned pair=0;pair<kWarmupPairs+kMeasuredPairs;++pair)for(unsigned order=0;order<2;++order) {
        restore->Inputs();
        auto& sample=samples[used];sample.pair=pair;sample.order=order;
        sample.reference=(pair+order)%2;sample.warmup=pair<kWarmupPairs;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        Mk64NativeCourseUseReference(false); // Same optimized native course in both routes.
        // N19 isolates compatibility-boundary work inside the native route.
        // Both passes skip the same source spans and use identical GPU batches.
        Mk64NativeParticleUseReference(false);Mk64NativeParticleUseEagerExits(sample.reference);
        const uint64_t particleDraws=Mk64NativeParticleDrawCount(),references=Mk64NativeParticleReferenceCount();
        const uint64_t particleBatches=Mk64NativeParticleBatchCount(),geometryCleans=Mk64NativeParticleGeometryCleanCount(),exits=Mk64NativeParticleExitCount();
#else
        Mk64NativeCourseUseReference(sample.reference);
#endif
        const uint64_t roots=Mk64NativeCourseDrawCount();
        const uint64_t draws=renderer->GetDrawCallCount(),triangles=renderer->GetTriangleCount(),
            uploads=renderer->GetTextureCacheUploadCount();
        interpreter->mInterpolationIndex=interpreter->mInterpolationIndexTarget=1;
        interpreter->mInterpolationT=1.f;
        interpreter->StartFrame();
        const uint64_t begin=svcGetSystemTick();
        interpreter->Run(static_cast<Gfx*>(commands),noReplacements);
        const uint64_t ran=svcGetSystemTick();
        sample.waitUs=renderer->GetFrameBeginWaitMicroseconds();
        interpreter->EndFrame();
        const uint64_t end=svcGetSystemTick();
        sample.runTicks=ran-begin;sample.endTicks=end-ran;
        sample.draws=renderer->GetDrawCallCount()-draws;
        sample.triangles=renderer->GetTriangleCount()-triangles;
        sample.uploads=renderer->GetTextureCacheUploadCount()-uploads;
        sample.nativeRoots=Mk64NativeCourseDrawCount()-roots;
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        sample.particleDraws=Mk64NativeParticleDrawCount()-particleDraws;
        sample.particleReferences=Mk64NativeParticleReferenceCount()-references;
        sample.particleBatches=Mk64NativeParticleBatchCount()-particleBatches;
        sample.geometryCleans=Mk64NativeParticleGeometryCleanCount()-geometryCleans;
        sample.exitApplications=Mk64NativeParticleExitCount()-exits;
        const bool invalidRoute=!sample.nativeRoots ||
            sample.particleDraws!=particlePackets || sample.particleReferences || !sample.particleBatches ||
            sample.exitApplications!=(sample.reference?particlePackets:sample.particleBatches);
        const uint64_t captureStart=svcGetSystemTick();
        uint8_t* image=images.get()+(sample.reference?imageBytes:0);
        if(!renderer->CaptureNativeComparisonPixels(image,imageBytes))
            throw std::runtime_error("Native particle comparison readback failed");
        sample.captureTicks=svcGetSystemTick()-captureStart;
        const float gpuMs=C3D_GetDrawingTime();
        if(!std::isfinite(gpuMs) || gpuMs<0 || gpuMs>10000)
            throw std::runtime_error("Native particle comparison GPU time invalid");
        sample.gpuQueueUs=uint32_t(gpuMs*1000.f+.5f);
        sample.imageHash=Hash(image,imageBytes);
        nonuniformImages=nonuniformImages && mk64_3ds::NativeFrameHasVariation(image,imageBytes);
        if(!sample.warmup) {
            const unsigned route=sample.reference?1:0;
            if(hasImageHash[route])deterministicImages=deterministicImages && stableImageHash[route]==sample.imageHash;
            else {stableImageHash[route]=sample.imageHash;hasImageHash[route]=true;}
        }
        if(order==1 && !mk64_3ds::CompareNativeFrames(images.get(),images.get()+imageBytes,imageBytes,imageDifferences[pair]))
            throw std::runtime_error("Native particle comparison image bounds");
#else
#ifdef MK64_3DS_NATIVE_BATCH_COMPARE_LAB
        const bool invalidRoute=!sample.nativeRoots;
#else
        const bool invalidRoute=(sample.reference && sample.nativeRoots) || (!sample.reference && !sample.nativeRoots);
#endif
#endif
        if(invalidRoute ||
           Hash(commands,bytes)!=initialHash)
            throw std::runtime_error("Native comparison route/input identity");
        ++used;
    }
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    SaveImage("sdmc:/3ds/MK64/native-lab/n19-boundary.rgba",images.get(),imageBytes);
    SaveImage("sdmc:/3ds/MK64/native-lab/n19-eager.rgba",images.get()+imageBytes,imageBytes);
#endif
    complete=true;
}

void Mk64NativeComparisonSave(unsigned frame) {
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    const char* path="sdmc:/3ds/MK64/native-lab/n19-comparison.csv";
    const char* reference="native_eager_exits";
    const char* candidate="native_boundary_exits";
#elif defined(MK64_3DS_NATIVE_BATCH_COMPARE_LAB)
    const char* path="sdmc:/3ds/MK64/native-lab/n16-comparison.csv";
    const char* reference="native_unbatched";
    const char* candidate="native_batched";
#else
    const char* path="sdmc:/3ds/MK64/native-lab/n14-comparison.csv";
    const char* reference="interpreted_course";
    const char* candidate="native_course";
#endif
    FILE* file=std::fopen(path,"wb");
    if(!file)return;
    std::fprintf(file,"frame,%u\ncomplete,%u\ncommand_hash,%016llx\nticks_per_second,%llu\nsnapshot_bytes,%u\n",
        frame,complete,(unsigned long long)commandHash,
        (unsigned long long)Mk64System3DSTicksPerSecond(),unsigned(sizeof(Restore)));
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    std::fprintf(file,"particle_packets,%u\nimage_bytes,%u\ndeterministic_images,%u\nnonuniform_images,%u\n",
        particlePackets,unsigned(imageBytes),deterministicImages,nonuniformImages);
    std::fprintf(file,"pair,order,route,warmup,run_ticks,begin_wait_us,end_ticks,draws,triangles,texture_uploads,native_roots,particle_draws,particle_references,gpu_queue_us,capture_ticks,image_hash,particle_batches,geometry_cleans,particle_exit_applications\n");
#else
    std::fprintf(file,"pair,order,route,warmup,run_ticks,begin_wait_us,end_ticks,draws,triangles,texture_uploads,native_roots\n");
#endif
    for(unsigned i=0;i<used;++i){const auto& s=samples[i];
        std::fprintf(file,"%u,%u,%s,%u,%llu,%u,%llu,%llu,%llu,%llu,%llu",s.pair,s.order,
            s.reference?reference:candidate,s.warmup,
            (unsigned long long)s.runTicks,s.waitUs,(unsigned long long)s.endTicks,
            (unsigned long long)s.draws,(unsigned long long)s.triangles,
            (unsigned long long)s.uploads,(unsigned long long)s.nativeRoots);
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
        std::fprintf(file,",%llu,%llu,%u,%llu,%016llx,%llu,%llu,%llu",(unsigned long long)s.particleDraws,
            (unsigned long long)s.particleReferences,s.gpuQueueUs,(unsigned long long)s.captureTicks,
            (unsigned long long)s.imageHash,(unsigned long long)s.particleBatches,(unsigned long long)s.geometryCleans,(unsigned long long)s.exitApplications);
#endif
        std::fprintf(file,"\n");
    }
#ifdef MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB
    std::fprintf(file,"image_pair,pixels,channels,max_delta,absolute_delta\n");
    for(unsigned i=0;i<used/2;++i){const auto& d=imageDifferences[i];
        std::fprintf(file,"%u,%u,%u,%u,%llu\n",i,d.pixels,d.channels,d.maxDelta,(unsigned long long)d.absoluteDelta);}
#endif
    std::fclose(file);
}
