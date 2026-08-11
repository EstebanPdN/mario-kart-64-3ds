#include "ship/resource/ResourceManager.h"

#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>

#include <cstring>
#include <vector>

extern "C" bool GameEngine_OTRSigCheck(const char* data);

namespace {
constexpr const char* kOtrSignature = "__OTR__";

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

    const std::string key(normalized);
    if (const auto found = mResources.find(key); found != mResources.end()) {
        return found->second;
    }

    Mk64TextureResource3DS textureData = {};
    if (!Mk64Resource3DSGetTexture(normalized, &textureData)) {
        return nullptr;
    }

    auto initData = std::make_shared<ResourceInitData>();
    initData->Path = key;
    auto texture = std::make_shared<Fast::Texture>(initData);
    texture->Type = static_cast<Fast::TextureType>(textureData.type);
    texture->Width = textureData.width;
    texture->Height = textureData.height;
    texture->ImageDataSize = static_cast<uint32_t>(textureData.size);
    texture->ImageData = const_cast<uint8_t*>(textureData.data);
    // The compact resource runtime owns the backing allocation.
    texture->mImageBuffer = BorrowedImageMarker();
    mResources[key] = texture;
    return texture;
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
