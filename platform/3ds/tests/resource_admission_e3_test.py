#!/usr/bin/env python3
"""Check optional preparation with actual Pack/O2R/process recovery and malloc faults."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
port = Path(__file__).resolve().parents[1]
root = port.parents[1]
out = args.output.resolve()
assert out != root and root not in out.parents
out.mkdir(parents=True, exist_ok=True)


def extract(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


pack_source = (port/'include/improved_texture_pack_3ds.hpp').read_text()
# Only the allocator call sites are redirected. All candidate ownership,
# ordering, validation, publication and production recovery code is unchanged.
assert pack_source.count('std::malloc(') == 3
instrumented = out/'instrumented'
instrumented.mkdir(exist_ok=True)
(instrumented/'improved_texture_pack_3ds.hpp').write_text(
    pack_source.replace('std::malloc(', 'ResourcePreparationMalloc('))
fixture_source = (port/'tests/resources_cache_regression_test.cpp').read_text()
prefix = fixture_source[:fixture_source.index('static Pack* active=nullptr;')]
prefix = prefix.replace('static size_t failSize=', 'static unsigned handlerEntries=0;\nstatic size_t failSize=')
prefix = prefix.replace(
    'if(n==failSize){failSize=SIZE_MAX;throw std::bad_alloc();}',
    'if(n==failSize){failSize=SIZE_MAX;auto handler=std::get_new_handler();'
    'if(!handler)throw std::bad_alloc();++handlerEntries;handler();}')
runtime = (port/'source/resource_runtime_3ds.cpp').read_text()
main_source = (port/'source/game_main_3ds.cpp').read_text()
manager = (port/'source/resource_manager_3ds.cpp').read_text()
production = '\n'.join(extract(runtime, signature) for signature in (
    'ReclaimedCache ReclaimOptionalCaches(uint32_t allowed) {',
    'extern "C" bool Mk64Resource3DSReclaimPerformanceCache(void) {',
    'extern "C" const char* Mk64Resource3DSLastReclaimedCache(void) {'))
production += '\n' + extract(main_source, 'void MemoryPressureHandler() {')
production += '\n' + extract(manager, 'extern "C" bool Mk64Improved3DSPrepare(size_t budget, bool newModel) {')
prelude = r'''
#include <cstddef>
#include <cstdlib>
#include <new>
static void* ResourcePreparationMalloc(size_t);
'''
state = r'''
#include "o2r_archive_reader.hpp"
#include "resource_runtime_3ds.h"
#include "improved_resources_3ds.h"
#include <atomic>
static Pack* current=nullptr;
std::unique_ptr<O2rArchiveReader> sArchive;
enum class ReclaimedCache:uint8_t {None,Improved,Retained,Native,Archive,Streaming};
ReclaimedCache sLastReclaimedCache=ReclaimedCache::None;
bool sReclaimingCaches=false;
std::atomic_flag sMemoryRecoveryActive=ATOMIC_FLAG_INIT;
extern "C" bool Mk64Improved3DSReclaim() __attribute__((weak));
extern "C" bool Mk64NativeKartReclaimCache() __attribute__((weak));
extern "C" bool Mk64Improved3DSReclaim(){return current && current->Reclaim();}
extern "C" bool Mk64NativeKartReclaimCache(){return false;}
bool ReleaseRetainedTextures(){return false;}
static const char* lastCheckpoint=nullptr;
extern "C" void Mk64Diagnostics3DSCheckpoint(const char* text){lastCheckpoint=text;}
static unsigned candidateCalls=0, failCandidate=0, releases=0;
static size_t candidateSizes[3]{};
static void* ResourcePreparationMalloc(size_t bytes) {
    assert(candidateCalls<3);
    candidateSizes[candidateCalls++]=bytes;
    if(candidateCalls==failCandidate) {
#ifdef E3_ADMISSION_NEGATIVE_CONTROL
        // Deliberately restore standard-semantics process recovery at this
        // allocation boundary. nothrow new must still run the real handler.
        failSize=bytes;
        return ::operator new(bytes,std::nothrow);
#else
        return nullptr;
#endif
    }
    return std::malloc(bytes);
}
static void Released(const void* pointer){assert(pointer);++releases;}
static void CheckPixels(const mk64_3ds::improved::Image& image,unsigned value) {
    assert(image && image.pixels->size()==512*128*4);
    for(char byte:*image.pixels)assert(uint8_t(byte)==value);
}
struct OldState {
    std::weak_ptr<std::vector<char>> unborrowed;
    mk64_3ds::improved::Image borrower;
    size_t cached=0,pages=0;
    uint32_t generation=0;
    unsigned releases=0;
};
static OldState Warm() {
    failCandidate=candidateCalls=handlerEntries=0;failSize=SIZE_MAX;lastCheckpoint=nullptr;
    current->Close();Save("native-karts.nsp",native);assert(current->Open("fixture.mui"));
    current->SetReleaseCallback(Released);
    assert(current->PreparePages(4,4*kSpritePageBytes));
    assert(current->Load("kart0/wheel0",64,64)); // Keep one valid decoded page.
    auto hot=current->Load("direct0",512,128,true);assert(hot);
    std::weak_ptr<std::vector<char>> weak=hot.pixels;hot={}; // Cache-only owner.
    auto live=current->Load("direct1",512,128,true);assert(live);
    return {weak,live,current->CachedBytes(),current->PageCacheBytes(),current->Generation(),releases};
}
static void CheckOld(const OldState& old) {
    assert(sArchive->IsResident() && !current->ResidentBytes());
    CheckPixels(old.borrower,2);
    assert(!old.unborrowed.expired() && old.unborrowed.lock()->size()==262144);
    assert(current->CachedBytes()==old.cached && current->PageCacheBytes()==old.pages);
    assert(current->Generation()==old.generation && releases==old.releases);
    assert(current->Load("direct1",512,128,true).pixels==old.borrower.pixels);
    const auto reads=current->Stats().fileBytes,decodes=current->Stats().pageDecodes;
    assert(current->Load("kart0/wheel0",64,64));
    assert(current->Stats().fileBytes==reads && current->Stats().pageDecodes==decodes);
    auto fallback=current->Load("kart7/wheel2",64,64);assert(fallback);
    assert(current->Stats().fileBytes>reads); // Valid ordinary source fallback.
    assert(handlerEntries==0 && !lastCheckpoint);
}
'''
checks = r'''
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);MakeFixture();
    std::filesystem::create_directories("romfs:");
    Save("romfs:/improved-textures.bin",mui);Save("romfs:/native-karts.nsp",native);
    current=&mk64_3ds::improved::LocalPack();assert(current->IsOpen());
    sArchive=std::make_unique<O2rArchiveReader>(argv[1]);assert(sArchive->Open()==O2rReadResult::Ok);
    assert(sArchive->MakeResident(SIZE_MAX));
    for(unsigned stage=1;stage<=3;++stage) {
#ifdef E3_ADMISSION_NEGATIVE_CONTROL
        if(argc==3 && stage!=unsigned(std::strtoul(argv[2],nullptr,10)))continue;
#endif
        auto old=Warm();assert(old.cached==2*262144 && old.pages==4*kSpritePageBytes);
        failCandidate=stage;
        const size_t expected[]={current->RequiredResidentBytes(),specs.size()*sizeof(uint32_t),kSpritePageBytes};
        failSize=expected[stage-1]; // An accidental C++ allocation runs the real handler.
        auto previous=std::set_new_handler(MemoryPressureHandler);
        const bool ready=current->MakeResident(current->RequiredPreparationBytes(8),8);
        std::set_new_handler(previous);failSize=SIZE_MAX;
        if(ready || !sArchive->IsResident() || handlerEntries) {
            std::fprintf(stderr,"FAIL R2 stage=%u ready=%u resident=%u handler_entries=%u checkpoint=%s\n",
                stage,unsigned(ready),unsigned(sArchive->IsResident()),handlerEntries,lastCheckpoint?lastCheckpoint:"none");
#ifdef E3_ADMISSION_NEGATIVE_CONTROL
            return 17;
#else
            assert(false);
#endif
        }
        assert(candidateCalls==stage);
        for(unsigned i=0;i<stage;++i)assert(candidateSizes[i]==expected[i]);
        CheckOld(old);
        std::printf("PASS: candidate %u fails locally; production handler active, O2R/262144 cache-only bytes/borrower/4 warm pages preserved.\n",stage);
    }
    // Public normal preparation must not clear/grow a valid fallback page LRU
    // after any candidate failure, even though an 8-page budget was supplied.
    for(unsigned stage=1;stage<=3;++stage) {
        auto old=Warm();failCandidate=stage;
        auto previous=std::set_new_handler(MemoryPressureHandler);
        assert(!Mk64Improved3DSPrepare(current->RequiredPreparationBytes(8),true));
        std::set_new_handler(previous);CheckOld(old);
    }
    for(unsigned failure=0;failure<2;++failure) {
        auto old=Warm();
        if(!failure){auto bad=native;bad.back()^=1;Save("native-karts.nsp",bad);}
        else std::filesystem::resize_file("native-karts.nsp",native.size()-1);
        auto previous=std::set_new_handler(MemoryPressureHandler);
        assert(!current->MakeResident(current->RequiredPreparationBytes(8),8));
        std::set_new_handler(previous);assert(candidateCalls==(!failure?3u:1u));
        // Restore only the synthetic backing fixture for a fallback read.
        Save("native-karts.nsp",native);CheckOld(old);
    }
    {
        auto old=Warm();
        assert(current->RequiredPreparationBytes(SIZE_MAX)==SIZE_MAX);
        auto previous=std::set_new_handler(MemoryPressureHandler);
        assert(!current->MakeResident(current->RequiredPreparationBytes(8)-1,8));
        std::set_new_handler(previous);assert(!candidateCalls);CheckOld(old);
    }
    {
        auto old=Warm();auto previous=std::set_new_handler(MemoryPressureHandler);
        assert(current->MakeResident(current->RequiredPreparationBytes(8),8));
        std::set_new_handler(previous);assert(candidateCalls==3 && !handlerEntries && sArchive->IsResident());
        assert(!old.unborrowed.expired() && current->Load("direct1",512,128,true).pixels==old.borrower.pixels);
        current->ResetStats();for(const auto& e:specs)if(e.flags)assert(current->Load(e.name.c_str(),64,64));
        assert(current->Stats().fileBytes==0 && current->PageCacheBytes()==8*kSpritePageBytes);
    }
    current->Close();sArchive.reset();
    std::puts("PASS: all optional candidates, public fallback, CRC/short read/budget failure rollback and successful zero-file-read residency with actual process recovery.");
}
'''
source = out/'admission.cpp'
source.write_text(prelude+prefix+state+production+checks)
fixture = out/'fixture.zip'
with zipfile.ZipFile(fixture, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for index in range(5000):
        archive.writestr(f'textures/karts/test/{index:04}', bytes((i*13+index*7)&255 for i in range(208)))
flags = ['clang++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
         '-fno-omit-frame-pointer', '-DMK64_3DS_GAME_TEXTURE_PACK',
         '-I'+str(instrumented), '-I'+str(port/'include'),
         '-I'+str(root/'third_party/SpaghettiKart/torch/lib')]
commands = []
with (out/'results.log').open('w') as log:
    def run(command):
        command = [str(item) for item in command]
        commands.append(shlex.join(command))
        result = subprocess.run(command, cwd=out, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log.write(result.stdout);log.flush();print(result.stdout, end='')
        return result
    for negative in (False, True):
        binary = out/('admission-negative-control' if negative else 'admission')
        command = [*flags, *(['-DE3_ADMISSION_NEGATIVE_CONTROL'] if negative else []), source,
                   port/'source/o2r_archive_reader.cpp', '-lz', '-o', binary]
        run(command).check_returncode()
        if negative:
            for stage in (1,2,3):
                result = run([binary, fixture, stage])
                assert result.returncode == 17 and f'FAIL R2 stage={stage}' in result.stdout and 'archive-residency-released-for-memory' in result.stdout
            print('PASS: three negative controls restore nothrow-new recovery and reproduce R2.')
        else:
            run([binary, fixture]).check_returncode()
(out/'commands.txt').write_text('\n'.join(commands)+'\n')
identity = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest() for path in (
    port/'include/improved_texture_pack_3ds.hpp', port/'source/resource_manager_3ds.cpp',
    port/'source/o2r_archive_reader.cpp', port/'source/resource_runtime_3ds.cpp', port/'source/game_main_3ds.cpp')}
(out/'summary.json').write_text(json.dumps(dict(passed=True, candidate_failures=['mirror','order','validation_page'],
    production_new_handler=True, archive='actual O2rArchiveReader/miniz, 5000 synthetic entries',
    old_images_and_pages_preserved=True, public_prepare_failure_preserves_pages=True,
    negative_control_reproduces_R2=True, allocator_seam='Only three std::malloc call sites redirected to failure wrapper; no cache/recovery algorithm rewritten.',
    source_sha256=identity), indent=2)+'\n')
