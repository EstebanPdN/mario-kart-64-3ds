#include "ship/resource/ResourceManager.h"

#include "resource_runtime_3ds.h"
#include "improved_texture_pack_3ds.hpp"
#include "improved_resources_3ds.h"
#include "native_texture_storage_3ds.hpp"
#ifdef __3DS__
#include "heap_policy_3ds.hpp"
#include "diagnostics_3ds.h"
#include "audio_runtime_3ds.h"
#include <3ds.h>
#include <malloc.h>
#endif

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>

#include <cstring>
#include <array>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>

extern "C" bool GameEngine_OTRSigCheck(const char* data);

namespace {
constexpr const char* kOtrSignature = "__OTR__";
constexpr size_t kTransientTextureReserve = 512;
bool (*sImprovedTexturePolicy)(const char*) = nullptr;

#ifdef MK64_3DS_GAME_TEXTURE_PACK
struct ImprovedTextureEntry {
    std::weak_ptr<Ship::IResource> resource;
    const char* canonicalName = nullptr;
    uint32_t generation = 0, packGeneration = 0, type = 0;
    bool absent = false;
    uint16_t width = 0, height = 0;
};
#ifdef __3DS__
static_assert(sizeof(ImprovedTextureEntry) <= 32);
#endif
using ImprovedTextureCache = std::array<ImprovedTextureEntry, 1024>;
ImprovedTextureCache& ImprovedTextures() {
    // Keep immutable format metadata after the weak wrapper expires. This
    // avoids decoding Original pixels only to recover replacement dimensions.
    // Canonical names belong to the archive: check generation before reading
    // them. Entries retain no image allocation; collisions are ordinary misses.
    static auto* cache = new ImprovedTextureCache{};
    return *cache;
}

bool EligibleImprovedTexture(const char* name) {
    return std::strncmp(name,"textures/karts/",15)==0 ||
           std::strncmp(name,"textures/common_data/",21)==0 ||
           std::strncmp(name,"textures/player_selection/",26)==0 ||
           std::strncmp(name,"textures/texture_tkmk00/",24)==0 ||
           std::strncmp(name,"textures/texture_data_2/",24)==0 ||
           std::strncmp(name,"textures/other_textures/minimap_",32)==0 ||
           std::strstr(name,"/logo_mario_kart_64");
}
std::shared_ptr<Ship::IResource> LoadImprovedTexture(const char* name,
                                                   uint16_t width, uint16_t height,
                                                   uint32_t type) {
    if (!EligibleImprovedTexture(name)) return nullptr;
    const unsigned bits[]={0,32,16,4,8,4,8,4,8,16};
    if (type>=10 || !bits[type]) return nullptr;
    auto& pack=mk64_3ds::improved::LocalPack();
    const uint64_t tag=uint64_t(Mk64Resource3DSGeneration())<<32 | type;
    if(auto owner=pack.CachedOwner(name,width,height,tag))return std::static_pointer_cast<Ship::IResource>(owner);
    const auto enhanced=pack.Load(name,width,height,true);
    if (!enhanced || enhanced.width!=width || enhanced.height!=height) return nullptr;
    auto initData=std::make_shared<Ship::ResourceInitData>();initData->Path=name;
    auto texture=std::make_shared<Fast::Texture>(initData);
    texture->Type=Fast::TextureType::RGBA32bpp;
    texture->Width=enhanced.width;texture->Height=enhanced.height;
    texture->Flags=TEX_FLAG_LOAD_AS_RAW | (enhanced.nativeGpuTiled?mk64_3ds::kNativeGpuTextureFlag:0);
    // N64 tile strides describe indexed Original data, not RGBA replacement.
    texture->HByteScale=32.f/bits[type];
    texture->VPixelScale=1.f;
    texture->mImageBuffer=enhanced.pixels;
    texture->ImageData=reinterpret_cast<uint8_t*>(enhanced.pixels->data());
    texture->ImageDataSize=enhanced.pixels->size();
    pack.RememberOwner(name,width,height,tag,texture);
    return texture;
}
#endif

void ThrowIfOutOfMemory(Mk64ResourceLoadResult3DS result) {
    if (result == MK64_RESOURCE_LOAD_OUT_OF_MEMORY_3DS) {
        throw std::bad_alloc();
    }
}

const char* NormalizeResourceName(const char* name) {
    if (name != nullptr && std::strncmp(name, kOtrSignature, 7) == 0) {
        return name + 7;
    }
    return name;
}

const std::shared_ptr<std::vector<char>>& BorrowedImageMarker() {
    static const auto marker = std::make_shared<std::vector<char>>();
    return marker;
}

using TransientTextureMap =
    std::unordered_map<uint64_t, std::weak_ptr<Ship::IResource>>;

TransientTextureMap& TransientTextures() {
    // Deliberately process-lifetime: a custom shared_ptr deleter can run while
    // other translation-unit statics are being torn down.
    static auto* resources = [] {
        auto* result = new TransientTextureMap();
        result->reserve(kTransientTextureReserve);
        return result;
    }();
    return *resources;
}

class TextureLifetimeLease {
  public:
    explicit TextureLifetimeLease(uint64_t token) : mToken(token) {
    }

    ~TextureLifetimeLease() {
        if (mToken != 0) {
            Mk64Resource3DSReleaseTexture(mToken);
        }
    }

    void Transfer() {
        mToken = 0;
    }

  private:
    uint64_t mToken;
};

struct TextureLifetimeDeleter {
    uint64_t token;

    void operator()(Fast::Texture* resource) const noexcept {
        delete resource;
        TransientTextures().erase(token);
        Mk64Resource3DSReleaseTexture(token);
    }
};
}

extern "C" void Mk64Resource3DSSetImprovedTexturePolicy(bool (*enabled)(const char*)) {
    sImprovedTexturePolicy = enabled;
}

namespace Ship {

const char* ArchiveManager3DSProbe::HashToCString(uint64_t crc) const {
    return ResourceGetNameByCrc(crc);
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const char* name) {
    const char* normalized = NormalizeResourceName(name);
    if (normalized == nullptr || normalized[0] == '\0') {
        return nullptr;
    }

    #ifdef MK64_3DS_GAME_TEXTURE_PACK
    const bool useImproved = EligibleImprovedTexture(normalized) &&
        sImprovedTexturePolicy && sImprovedTexturePolicy(normalized);
    const uint32_t generation = useImproved ? Mk64Resource3DSGeneration() : 0;
    const uint32_t packGeneration = useImproved ? mk64_3ds::improved::LocalPack().Generation() : 0;
    bool knownAbsent = false;
    const size_t improvedSlot = useImproved
        ? mk64_3ds::improved::Hash(normalized) % ImprovedTextures().size() : 0;
    if (useImproved) {
        auto& entry = ImprovedTextures()[improvedSlot];
        if (generation && entry.generation == generation && entry.packGeneration == packGeneration) {
            if (auto cached = entry.resource.lock();
                cached && cached->GetInitData()->Path == normalized) {
                // A live image already owns the exact requested pixels.
                // Live wrapper hits must refresh the same bounded pixel LRU.
                mk64_3ds::improved::LocalPack().TouchImage(normalized, entry.width, entry.height);
                return cached;
            }
            if (entry.canonicalName && !std::strcmp(entry.canonicalName, normalized)) {
                knownAbsent = entry.absent;
                if (!knownAbsent) {
                    if (auto texture = LoadImprovedTexture(normalized, entry.width, entry.height, entry.type)) {
                        entry.resource = texture;
                        return texture;
                    }
                }
            }
        }
    }
    #endif

    Mk64TextureResource3DS textureData = {};
    const Mk64ResourceLoadResult3DS loadResult =
        Mk64Resource3DSAcquireTexture(normalized, &textureData);
    ThrowIfOutOfMemory(loadResult);
    if (loadResult != MK64_RESOURCE_LOAD_OK_3DS) {
        return nullptr;
    }
    TextureLifetimeLease lifetime(textureData.lifetimeToken);

    #ifdef MK64_3DS_GAME_TEXTURE_PACK
    if (useImproved && !knownAbsent) {
        if (auto texture = LoadImprovedTexture(normalized, textureData.width, textureData.height, textureData.type)) {
            auto& entry = ImprovedTextures()[improvedSlot];
            entry = {texture, textureData.canonicalName, generation, packGeneration, textureData.type,
                     false, textureData.width, textureData.height};
            return texture;
        }
        // Only immutable absence is negative-cached. Read/decode/allocation
        // failure remains retryable, and Original palettes keep their leases.
        const auto& pack = mk64_3ds::improved::LocalPack();
        const bool unsupported = textureData.type == 0 || textureData.type >= 10;
        if (unsupported || !pack.Contains(normalized, textureData.width, textureData.height)) {
            ImprovedTextures()[improvedSlot] = {{}, textureData.canonicalName, generation,
                packGeneration, textureData.type, true, textureData.width, textureData.height};
        }
    }

    #endif
    auto& resources = TransientTextures();
    if (const auto found = resources.find(textureData.lifetimeToken);
        found != resources.end()) {
        if (auto resource = found->second.lock()) {
            return resource;
        }
        resources.erase(found);
    }

    auto initData = std::make_shared<ResourceInitData>();
    initData->Path = textureData.canonicalName != nullptr ? textureData.canonicalName : normalized;
    std::unique_ptr<Fast::Texture, TextureLifetimeDeleter> ownedTexture(
        new Fast::Texture(initData), TextureLifetimeDeleter{ textureData.lifetimeToken });
    lifetime.Transfer();
    std::shared_ptr<Fast::Texture> texture(std::move(ownedTexture));
    texture->Type = static_cast<Fast::TextureType>(textureData.type);
    texture->Width = textureData.width;
    texture->Height = textureData.height;
    texture->ImageDataSize = static_cast<uint32_t>(textureData.size);
    texture->ImageData = const_cast<uint8_t*>(textureData.data);
    // The compact resource runtime owns the backing allocation.
    texture->mImageBuffer = BorrowedImageMarker();
    resources.emplace(textureData.lifetimeToken, texture);
    return texture;
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    void* data = nullptr;
    ThrowIfOutOfMemory(Mk64Resource3DSResolveDataByCrc(crc, &data));
    return data;
}

void* ResourceManager::GetResourceRawPointer(const char* name) {
    void* data = nullptr;
    ThrowIfOutOfMemory(Mk64Resource3DSResolveDataByName(name, &data));
    return data;
}

bool ResourceManager::OtrSignatureCheck(const char* data) const {
    return GameEngine_OTRSigCheck(data);
}

} // namespace Ship

extern "C" bool Mk64Improved3DSConfigure(int textureMode) {
#ifdef MK64_3DS_GAME_TEXTURE_PACK
    const bool aliases = textureMode == 1;
    size_t budget = SIZE_MAX;
#ifdef __3DS__
    const auto allowance = [] {return mk64_3ds::ArchiveResidencyBudget(
        envGetHeapSize(), static_cast<size_t>(mallinfo().uordblks));};
    budget = allowance();
    auto& pack = mk64_3ds::improved::GlobalPack();
    if (pack.IsOpen() && aliases && !pack.KartAliasesEnabled() && pack.FullMetadataBytes() > budget) {
        // Expansion must fit before allocating its transactional pending index.
        // Replace optional caches first; never spend the 8 MiB reserve or
        // allocate the full catalog alongside a tight resident O2R mirror.
        pack.Reclaim();
        budget = allowance();
        if (pack.FullMetadataBytes() > budget && Mk64Resource3DSIsResident()) {
            if (!Mk64Resource3DSDropResidency()) return false;
            Mk64Diagnostics3DSCheckpoint("archive-residency-released-for-artwork-mode");
            budget = allowance();
        }
        if (pack.FullMetadataBytes() > budget) return false;
    }
#endif
    return mk64_3ds::improved::ConfigureLocalCatalog(aliases, budget);
#else
    return false;
#endif
}
extern "C" bool Mk64Improved3DSPrepare(size_t budget, bool newModel) {
#ifdef MK64_3DS_GAME_TEXTURE_PACK
    auto& pack = mk64_3ds::improved::LocalPack();
    const size_t pages = newModel && pack.RequiredPreparationBytes(8) <= budget ? 8 : 4;
#ifdef __3DS__
    const auto progress = [](unsigned) { Mk64GameAudio3DSServiceLoading(); };
    if (pack.MakeResident(budget, pages, progress)) return true;
#else
    if (pack.MakeResident(budget, pages)) return true;
#endif
    // A failed mirror candidate must not replace existing valid decoded pages.
    // If there is no page working set yet, source fallback may initialize one.
    if (!pack.PageCacheBytes()) pack.PreparePages(pages, budget);
#endif
    return false;
}
extern "C" bool Mk64Improved3DSReclaim(void) {
    return mk64_3ds::improved::GlobalPack().Reclaim();
}
extern "C" size_t Mk64Improved3DSMetadataBytes(void) {
    return mk64_3ds::improved::GlobalPack().MetadataBytes();
}
extern "C" size_t Mk64Improved3DSResidentBytes(void) {
    return mk64_3ds::improved::GlobalPack().ResidentBytes();
}
extern "C" size_t Mk64Improved3DSPageBytes(void) {
    return mk64_3ds::improved::GlobalPack().PageCacheBytes();
}
extern "C" size_t Mk64Improved3DSPreload(const char* const* names, size_t count) {
    if (!names) return 0;
    Ship::ResourceManager manager;
    size_t prepared = 0;
    for (size_t i = 0; i < count; ++i) {
        try { if (manager.LoadResourceProcess(names[i])) ++prepared; }
        catch (const std::bad_alloc&) { break; }
    }
    return prepared;
}

extern "C" bool Mk64Improved3DSWriteHistory(const char* directory) {
    if (!directory) return false;
    char path[1024];
    const auto length = std::snprintf(path, sizeof(path), "%s/artwork-requests.csv", directory);
    if (length < 0 || size_t(length) >= sizeof(path)) return false;
    FILE* file = std::fopen(path, "wb");
    if (!file) return false;
    bool ok = std::fputs("hash,generation,index,offset,packed,width,height,lane,outcome,read_us,decode_us\n", file) >= 0;
    const auto& pack = mk64_3ds::improved::GlobalPack();
    for (size_t i = 0; ok && i < pack.HistorySize(); ++i) {
        const auto& e = pack.HistoryAt(i);
        ok = std::fprintf(file, "%016llX,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%lu,%lu\n",
            static_cast<unsigned long long>(e.hash), static_cast<unsigned long>(e.generation),
            static_cast<unsigned long>(e.index), static_cast<unsigned long>(e.offset), static_cast<unsigned long>(e.packed),
            unsigned(e.width), unsigned(e.height), unsigned(e.lane), unsigned(e.outcome),
            static_cast<unsigned long>(e.readUs), static_cast<unsigned long>(e.decodeUs)) >= 0;
    }
    const bool closed = std::fclose(file) == 0;
    return ok && closed;
}
