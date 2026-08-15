#include "ship/resource/ResourceManager.h"

#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>

#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>

extern "C" bool GameEngine_OTRSigCheck(const char* data);

namespace {
constexpr const char* kOtrSignature = "__OTR__";
constexpr size_t kTransientTextureReserve = 512;

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

namespace Ship {

const char* ArchiveManager3DSProbe::HashToCString(uint64_t crc) const {
    return ResourceGetNameByCrc(crc);
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const char* name) {
    const char* normalized = NormalizeResourceName(name);
    if (normalized == nullptr || normalized[0] == '\0') {
        return nullptr;
    }

    Mk64TextureResource3DS textureData = {};
    const Mk64ResourceLoadResult3DS loadResult =
        Mk64Resource3DSAcquireTexture(normalized, &textureData);
    ThrowIfOutOfMemory(loadResult);
    if (loadResult != MK64_RESOURCE_LOAD_OK_3DS) {
        return nullptr;
    }
    TextureLifetimeLease lifetime(textureData.lifetimeToken);

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
