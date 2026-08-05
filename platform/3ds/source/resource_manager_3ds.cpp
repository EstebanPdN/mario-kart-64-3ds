#include "ship/resource/ResourceManager.h"

#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>

#include <cstring>
#include <vector>

extern "C" int32_t GameEngine_ResourceGetTexTypeByName(const char* name);
extern "C" bool GameEngine_OTRSigCheck(const char* data);

namespace {
constexpr const char* kOtrSignature = "__OTR__";

const char* NormalizeResourceName(const char* name) {
    if (name != nullptr && std::strncmp(name, kOtrSignature, 7) == 0) {
        return name + 7;
    }
    return name;
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
        if (auto resource = found->second.lock()) {
            return resource;
        }
    }

    auto* imageData = static_cast<uint8_t*>(ResourceGetDataByName(normalized));
    const size_t imageSize = ResourceGetSizeByName(normalized);
    const uint16_t width = ResourceGetTexWidthByName(normalized);
    const uint16_t height = ResourceGetTexHeightByName(normalized);
    if (imageData == nullptr || imageSize == 0 || width == 0 || height == 0) {
        return nullptr;
    }

    auto initData = std::make_shared<ResourceInitData>();
    initData->Path = key;
    auto texture = std::make_shared<Fast::Texture>(initData);
    texture->Type = static_cast<Fast::TextureType>(GameEngine_ResourceGetTexTypeByName(normalized));
    texture->Width = width;
    texture->Height = height;
    texture->ImageDataSize = static_cast<uint32_t>(imageSize);
    texture->ImageData = imageData;
    // The compact resource runtime owns the backing allocation.
    texture->mImageBuffer = std::make_shared<std::vector<char>>();
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
