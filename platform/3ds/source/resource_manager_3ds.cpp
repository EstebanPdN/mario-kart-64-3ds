#include "ship/resource/ResourceManager.h"

#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" bool GameEngine_OTRSigCheck(const char* data);

namespace {
constexpr const char* kOtrSignature = "__OTR__";
constexpr const char* kKartTexturePrefix = "textures/karts/";

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

bool IsTransientKartTexture(const char* name) {
    return name != nullptr && std::strncmp(name, kKartTexturePrefix,
                                           std::strlen(kKartTexturePrefix)) == 0;
}

using TransientTextureMap =
    std::unordered_map<std::string, std::weak_ptr<Ship::IResource>>;

TransientTextureMap& TransientTextures() {
    // Deliberately process-lifetime: a custom shared_ptr deleter can run while
    // other translation-unit statics are being torn down.
    static auto* resources = new TransientTextureMap();
    return *resources;
}
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

    const bool transient = IsTransientKartTexture(normalized);
    try {
        const std::string key(normalized);
        if (transient) {
            auto& resources = TransientTextures();
            if (const auto found = resources.find(key); found != resources.end()) {
                if (auto resource = found->second.lock()) {
                    return resource;
                }
                resources.erase(found);
            }
        } else if (const auto found = mResources.find(key); found != mResources.end()) {
            return found->second;
        }

        Mk64TextureResource3DS textureData = {};
        if (!Mk64Resource3DSGetTexture(normalized, &textureData)) {
            return nullptr;
        }

        auto initData = std::make_shared<ResourceInitData>();
        initData->Path = key;
        std::shared_ptr<Fast::Texture> texture;
        if (transient) {
            texture = std::shared_ptr<Fast::Texture>(
                new Fast::Texture(initData),
                [key](Fast::Texture* resource) noexcept {
                    delete resource;
                    try {
                        TransientTextures().erase(key);
                    } catch (...) {
                    }
                    ResourceDirtyByName(key.c_str());
                });
        } else {
            texture = std::make_shared<Fast::Texture>(initData);
        }
        texture->Type = static_cast<Fast::TextureType>(textureData.type);
        texture->Width = textureData.width;
        texture->Height = textureData.height;
        texture->ImageDataSize = static_cast<uint32_t>(textureData.size);
        texture->ImageData = const_cast<uint8_t*>(textureData.data);
        // The compact resource runtime owns the backing allocation.
        texture->mImageBuffer = BorrowedImageMarker();
        if (transient) {
            TransientTextures().emplace(key, texture);
        } else {
            mResources[key] = texture;
        }
        return texture;
    } catch (const std::bad_alloc&) {
        if (transient) {
            ResourceDirtyByName(normalized);
        }
        return nullptr;
    }
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    return ResourceGetDataByCrc(crc);
}

void* ResourceManager::GetResourceRawPointer(const char* name) {
    return ResourceGetDataByName(name);
}

bool ResourceManager::OtrSignatureCheck(const char* data) const {
    return GameEngine_OTRSigCheck(data);
}

} // namespace Ship
