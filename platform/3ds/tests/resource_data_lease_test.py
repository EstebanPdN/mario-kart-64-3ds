#!/usr/bin/env python3
"""Exercise actual decoded-resource pin/eviction functions with fake storage."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
port = Path(__file__).resolve().parents[1]
source = (port/'source/resource_runtime_3ds.cpp').read_text()

def function(signature):
    begin = source.index(signature)
    brace = source.index('{', begin)
    level = 1
    end = brace+1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]

functions = '\n'.join(function(name) for name in (
    'template <typename Loader>\nMk64ResourceLoadResult3DS ResolveResource(',
    'LoadedResource* RetainedLookup(', 'bool EvictReleasedTexture(',
    'bool ReleaseRetainedTextures() {', 'void RetainOrEvict(',
    'void RequestEviction(size_t archiveIndex) {', 'uint64_t TextureLifetimeToken(',
    'extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireDataByCrc(',
    'extern "C" void Mk64Resource3DSReleaseTexture(',
    'extern "C" void Mk64Resource3DSReleaseData('))
test = r'''
#include "resource_runtime_3ds.h"
#include "texture_retention_3ds.hpp"
#include <cassert>
#include <algorithm>
#include <utility>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <vector>
struct LoadedResource;
struct CrcEntry {LoadedResource* loaded=nullptr;};
unsigned destroyed=0;
constexpr unsigned kTypeDisplayList=3;
std::vector<std::pair<uintptr_t,size_t>> sDisplayListAddresses;
struct LoadedResource {
    unsigned type=0;
    size_t archiveIndex=0;
    uint32_t textureOwners=0;
    bool evictWhenUnused=false;
    bool evictionRequested=false;
    bool retained=false;
    uint32_t retainStamp=0;
    size_t allocatedBytes=64;
    CrcEntry* crcEntry=nullptr;
    unsigned payload=42;
    void* pointer=&payload;
    ~LoadedResource(){++destroyed;}
};
struct Archive {
    std::vector<std::string> entries{"models/vertices"};
    const auto& Entries() const {return entries;}
} archive;
Archive* sArchive=&archive;
std::vector<std::unique_ptr<LoadedResource>> sLoadedResources(1);
size_t sLoadedResourceCount=0;
uint32_t sRuntimeGeneration=7;
mk64_3ds::TextureRetention3DS<512> sRetainedTextures(256U * 1024U);
void RequestEviction(size_t archiveIndex);
bool IsSoundPath(const std::string&) {return false;}
LoadedResource* LoadByCrc(uint64_t crc) {
    if(crc==2)throw std::bad_alloc();
    if(crc==3)throw 17;
    return crc==1?sLoadedResources[0].get():nullptr;
}
FUNCTIONS
int main() {
    CrcEntry entry;
    auto reset=[&]{sLoadedResources[0]=std::make_unique<LoadedResource>();
        sLoadedResources[0]->crcEntry=&entry;entry.loaded=sLoadedResources[0].get();sLoadedResourceCount=1;};
    reset();
    void* data=reinterpret_cast<void*>(1);uint64_t one=123,two=456;
    assert(Mk64Resource3DSAcquireDataByCrc(1,nullptr,&one)==MK64_RESOURCE_LOAD_NOT_FOUND_3DS && one==0);
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,nullptr)==MK64_RESOURCE_LOAD_NOT_FOUND_3DS && !data);
    for(auto crc:{0,2,3}) {
        data=reinterpret_cast<void*>(1);one=123;
        auto result=Mk64Resource3DSAcquireDataByCrc(crc,&data,&one);
        assert(result==(crc==2?MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS:MK64_RESOURCE_LOAD_NOT_FOUND_3DS));
        assert(!data && !one && sLoadedResources[0]->textureOwners==0);
    }
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&one)==MK64_RESOURCE_LOAD_OK_3DS);
    assert(*static_cast<unsigned*>(data)==42 && one);
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&two)==MK64_RESOURCE_LOAD_OK_3DS && one==two);
    RequestEviction(0);
    assert(!entry.loaded && sLoadedResources[0] && destroyed==0);
    Mk64Resource3DSReleaseData(one);
    assert(sLoadedResources[0] && sLoadedResources[0]->textureOwners==1);
    assert(*static_cast<unsigned*>(data)==42);
    Mk64Resource3DSReleaseData(two);
    assert(!sLoadedResources[0] && destroyed==1 && sLoadedResourceCount==0);
    Mk64Resource3DSReleaseData(two); // Empty-slot release is harmless.
    ++sRuntimeGeneration;reset();
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&two)==MK64_RESOURCE_LOAD_OK_3DS);
    Mk64Resource3DSReleaseData(one); // Previous archive generation cannot unpin this resource.
    assert(sLoadedResources[0]->textureOwners==1);
    Mk64Resource3DSReleaseData(two);
    assert(sLoadedResources[0] && sLoadedResources[0]->textureOwners==0);
    sLoadedResources[0]->textureOwners=UINT32_MAX;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&two)==MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS && !data && !two);
    assert(sLoadedResources[0]->textureOwners==UINT32_MAX);
    sLoadedResources[0]->textureOwners=0;sRuntimeGeneration=0;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&two)==MK64_RESOURCE_LOAD_NOT_FOUND_3DS && !data && !two);
    sLoadedResources[0]->pointer=nullptr;sRuntimeGeneration=9;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&two)==MK64_RESOURCE_LOAD_NOT_FOUND_3DS && !data && !two);
    RequestEviction(0);assert(!sLoadedResources[0] && destroyed==2);
    // A texture released by its last temporary owner stays decoded for reuse;
    // memory pressure (or an explicit eviction) still frees it exactly once.
    reset();sRuntimeGeneration=11;sLoadedResources[0]->evictWhenUnused=true;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&one)==MK64_RESOURCE_LOAD_OK_3DS);
    Mk64Resource3DSReleaseData(one);
    assert(sLoadedResources[0] && sLoadedResources[0]->retained && entry.loaded && destroyed==2);
    assert(sRetainedTextures.Bytes()==64);
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&one)==MK64_RESOURCE_LOAD_OK_3DS);
    assert(!sLoadedResources[0]->retained && sRetainedTextures.Bytes()==0);
    assert(!ReleaseRetainedTextures() && sLoadedResources[0]); // Owned: never freed.
    Mk64Resource3DSReleaseData(one);
    assert(ReleaseRetainedTextures() && !sLoadedResources[0] && destroyed==3 && sRetainedTextures.Bytes()==0);
    reset();sLoadedResources[0]->evictWhenUnused=true;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&one)==MK64_RESOURCE_LOAD_OK_3DS);
    RequestEviction(0);Mk64Resource3DSReleaseData(one);
    assert(!sLoadedResources[0] && destroyed==4 && sRetainedTextures.Bytes()==0);
    reset();sLoadedResources[0]->evictWhenUnused=true;
    assert(Mk64Resource3DSAcquireDataByCrc(1,&data,&one)==MK64_RESOURCE_LOAD_OK_3DS);
    Mk64Resource3DSReleaseData(one);RequestEviction(0);
    assert(!sLoadedResources[0] && destroyed==5 && sRetainedTextures.Bytes()==0);
    assert(!ReleaseRetainedTextures()); // Stale slot after eviction is skipped.
    std::puts("PASS: production decoded-resource pins retain live data through eviction, release exactly once, retain released textures within budget, reject stale archive tokens and preserve OOM results.");
}
'''.replace('FUNCTIONS', functions)
cpp=a.output/'resource-data-lease.cpp'
cpp.write_text(test)
binary=a.output/'resource-data-lease'
subprocess.run(['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-I',str(port/'include'),str(cpp),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
