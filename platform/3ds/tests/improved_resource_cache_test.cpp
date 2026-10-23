// Exercise the production resource manager and pack with synthetic pixels.
#include "ship/resource/ResourceManager.h"
#include "fast/resource/type/Texture.h"
#include "resource_runtime_3ds.h"
#include "improved_texture_pack_3ds.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace {
bool improved = true;
bool Enabled(const char*) { return improved; }
unsigned acquires = 0, releases = 0;
uint64_t nextToken = 0;
std::map<uint64_t, std::vector<uint8_t>> originals;
std::set<std::string> canonicalNames;
uint32_t generation = 1, originalType = 2;
uint16_t originalWidth = 2, originalHeight = 2;

void Put(std::vector<uint8_t>& out, size_t at, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) out[at+i] = uint8_t(value >> (i*8));
}
template<size_t Count>
void Fixture(std::array<std::string, Count> names, unsigned width = 2, unsigned height = 2) {
    using mk64_3ds::improved::Hash;
    std::sort(names.begin(), names.end(), [](const auto& a, const auto& b) {
        return Hash(a.c_str()) < Hash(b.c_str());
    });
    std::vector<uint8_t> out(16 + names.size()*32);
    std::memcpy(out.data(), "MUI3", 4);
    Put(out, 4, 1, 4); Put(out, 8, names.size(), 4); Put(out, 12, 32, 4);
    for (size_t i = 0; i < names.size(); ++i) {
        std::vector<uint8_t> pixels(width*height*4, uint8_t(i+100));
        std::vector<uint8_t> packed(compressBound(pixels.size()));
        uLongf bytes = packed.size();
        assert(compress2(packed.data(), &bytes, pixels.data(), pixels.size(), 1) == Z_OK);
        packed.resize(bytes);
        size_t at = 16+i*32;
        Put(out, at, Hash(names[i].c_str()), 8); Put(out, at+8, out.size(), 4);
        Put(out, at+12, bytes, 4); Put(out, at+16, crc32(0, pixels.data(), pixels.size()), 4);
        for (size_t offset : {20U, 24U}) Put(out, at+offset, width, 2);
        for (size_t offset : {22U, 26U}) Put(out, at+offset, height, 2);
        out.insert(out.end(), packed.begin(), packed.end());
    }
    std::filesystem::create_directories("romfs:");
    std::ofstream file("romfs:/improved-textures.bin", std::ios::binary);
    file.write(reinterpret_cast<const char*>(out.data()), out.size());
}
}

extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSAcquireTexture(
    const char* name, Mk64TextureResource3DS* result) {
    ++acquires;
    if (!generation) return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    if (!std::strcmp(name, "missing")) return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
    if (!std::strcmp(name, "oom")) return MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS;
    auto token = ++nextToken;
    auto& bytes = originals[token]; bytes.assign(8, 42);
    const auto canonical = canonicalNames.emplace(name).first;
    *result = {bytes.data(), bytes.size(), originalWidth, originalHeight, originalType, canonical->c_str(), token};
    return MK64_RESOURCE_LOAD_OK_3DS;
}
extern "C" uint32_t Mk64Resource3DSGeneration() { return generation; }
extern "C" void Mk64Resource3DSReleaseTexture(uint64_t token) {
    ++releases; assert(originals.erase(token) == 1);
}
extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByName(const char*, void**) {
    return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
}
extern "C" Mk64ResourceLoadResult3DS Mk64Resource3DSResolveDataByCrc(uint64_t, void**) {
    return MK64_RESOURCE_LOAD_NOT_FOUND_3DS;
}
extern "C" const char* ResourceGetNameByCrc(uint64_t) { return nullptr; }
extern "C" bool GameEngine_OTRSigCheck(const char*) { return false; }

int main() {
    using mk64_3ds::improved::Hash;
    const std::string first = "textures/karts/synthetic";
    std::string collision;
    for (unsigned i = 0; i < 100000; ++i) {
        auto candidate = "textures/karts/collision" + std::to_string(i);
        if (Hash(candidate.c_str()) % 1024 == Hash(first.c_str()) % 1024) {
            collision = candidate; break;
        }
    }
    assert(!collision.empty());
    Fixture(std::array{first, collision});
    Mk64Resource3DSSetImprovedTexturePolicy(Enabled);
    Ship::ResourceManager manager;
    auto image = manager.LoadResourceProcess(first.c_str());
    assert(image && acquires == 1 && releases == 1 && originals.empty());
    for (unsigned i = 0; i < 128; ++i) {
        assert(manager.LoadResourceProcess(("__OTR__" + first).c_str()) == image);
    }
    assert(acquires == 1 && releases == 1);
    std::puts("PASS: 128 live Improved lookups issue zero additional original acquisitions.");

    improved = false;
    auto original = std::static_pointer_cast<Fast::Texture>(manager.LoadResourceProcess(first.c_str()));
    assert(original && original != image && original->Type == Fast::TextureType::RGBA16bpp);
    assert(original->ImageData[0] == 42 && originals.size() == 1);
    original.reset(); assert(originals.empty());
    improved = true;
    assert(manager.LoadResourceProcess(first.c_str()) == image);

    auto other = manager.LoadResourceProcess(collision.c_str());
    assert(other && other != image && other->GetInitData()->Path == collision);
    assert(std::memcmp(other->GetRawPointer(), image->GetRawPointer(), 16) != 0);
    auto reloaded = manager.LoadResourceProcess(first.c_str());
    assert(reloaded && reloaded->GetInitData()->Path == first);
    assert(std::memcmp(reloaded->GetRawPointer(), image->GetRawPointer(), 16) == 0);
    assert(std::memcmp(reloaded->GetRawPointer(), other->GetRawPointer(), 16) != 0);
    std::weak_ptr<Ship::IResource> weak = reloaded;
    image.reset(); reloaded.reset();
    assert(!weak.expired()); // Wrapper shares the pack's existing bounded pixel entry.
    assert(mk64_3ds::improved::GlobalPack().CachedBytes() <= 2*1024*1024);
    auto before = acquires;
    reloaded = manager.LoadResourceProcess(first.c_str());
    assert(reloaded && acquires == before);
    reloaded.reset();
    for (unsigned i = 0; i < 1000; ++i) {
        auto temporary = manager.LoadResourceProcess(first.c_str());
        assert(temporary && acquires == before);
    }
    std::puts("PASS: 1000 bounded pixel-owner hits reuse the wrapper without Original decodes.");

    // Archive names are freed on close. A generation check must precede any
    // comparison with the borrowed canonical pointer, even with a live image.
    auto previousGeneration = manager.LoadResourceProcess(first.c_str());
    generation = 0;
    canonicalNames.clear();
    assert(!manager.LoadResourceProcess(first.c_str()));
    before = acquires;
    generation = 2; originalType = 3;
    reloaded = manager.LoadResourceProcess(first.c_str());
    assert(reloaded && reloaded != previousGeneration && acquires == before+1);
    assert(std::static_pointer_cast<Fast::Texture>(reloaded)->HByteScale == 8.f);
    previousGeneration.reset();

    // Negative metadata is qualified by both archive and pack generation.
    const char* absent = "textures/karts/absent_from_improved";
    auto& pack = mk64_3ds::improved::GlobalPack();
    before = pack.Stats().requests;
    for (unsigned i = 0; i < 1000; ++i) assert(manager.LoadResourceProcess(absent));
    assert(pack.Stats().requests == before+1);
    assert(pack.SetKartAliasesEnabled(false));
    assert(manager.LoadResourceProcess(absent));
    assert(pack.Stats().requests == before+2);
    assert(pack.SetKartAliasesEnabled(true));
    assert(manager.LoadResourceProcess(absent));
    assert(pack.Stats().requests == before+3);

    assert(!manager.LoadResourceProcess(nullptr));
    assert(!manager.LoadResourceProcess(""));
    assert(!manager.LoadResourceProcess("missing"));
    bool oom = false;
    try { manager.LoadResourceProcess("oom"); } catch (const std::bad_alloc&) { oom = true; }
    assert(oom && originals.empty());
    other.reset(); reloaded.reset();
    // The manager's live-wrapper shortcut must also touch the bounded LRU.
    // Two 1 MiB images fill it: touching A, then adding C, must evict B.
    const std::array<std::string,3> lruNames = {"textures/karts/lruA", "textures/karts/lruB", "textures/karts/lruC"};
    assert(Hash(lruNames[0].c_str())%1024 != Hash(lruNames[1].c_str())%1024);
    assert(Hash(lruNames[0].c_str())%1024 != Hash(lruNames[2].c_str())%1024);
    Fixture(lruNames,512,512);assert(pack.Open("romfs:/improved-textures.bin"));
    originalWidth=originalHeight=512;originalType=2;
    auto a=manager.LoadResourceProcess(lruNames[0].c_str());
    auto b=manager.LoadResourceProcess(lruNames[1].c_str());
    assert(a && b && pack.CachedBytes()==2*1024*1024);
    assert(manager.LoadResourceProcess(lruNames[0].c_str())==a);
    auto c=manager.LoadResourceProcess(lruNames[2].c_str());assert(c);
    const uint64_t ownerTag=uint64_t(generation)<<32 | originalType;
    assert(pack.CachedOwner(lruNames[0].c_str(),512,512,ownerTag)==a);
    assert(!pack.CachedOwner(lruNames[1].c_str(),512,512,ownerTag));
    assert(b->GetRawPointer()); // Eviction keeps an external borrower alive.
    a.reset();b.reset();c.reset();
    mk64_3ds::improved::GlobalPack().Close();
    assert(weak.expired()); // Pack eviction also releases its wrapper owner.
    std::filesystem::remove("romfs:/improved-textures.bin");
    std::puts("PASS: exact names across collisions, mode changes, weak expiry, closed/reopened archives, fresh format metadata, missing/OOM and balanced leases.");
}
