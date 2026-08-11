#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "ship/resource/Resource.h"

namespace Ship {

class ArchiveManager3DSProbe {
  public:
#ifdef MK64_3DS_RESOURCE_MANAGER
    const char* HashToCString(uint64_t crc) const;
#else
    const char* HashToCString(uint64_t) const {
        return nullptr;
    }
#endif
};

class ResourceManager {
  public:
#ifdef MK64_3DS_RESOURCE_MANAGER
    std::shared_ptr<IResource> LoadResourceProcess(const char* name);
    void* GetResourceRawPointer(uint64_t crc);
    void* GetResourceRawPointer(const char* name);
#else
    std::shared_ptr<IResource> LoadResourceProcess(const char*) {
        return nullptr;
    }

    void* GetResourceRawPointer(uint64_t) {
        return nullptr;
    }

    void* GetResourceRawPointer(const char*) {
        return nullptr;
    }
#endif

    ArchiveManager3DSProbe* GetArchiveManager() {
        return &mArchiveManager;
    }

#ifdef MK64_3DS_RESOURCE_MANAGER
    bool OtrSignatureCheck(const char* data) const;
#else
    bool OtrSignatureCheck(const char*) const {
        return false;
    }
#endif

  private:
    ArchiveManager3DSProbe mArchiveManager;
#ifdef MK64_3DS_RESOURCE_MANAGER
    // Texture wrappers reference immutable storage owned by the compact 3DS
    // resource runtime. Keep them strongly cached so repeated Fast3D texture
    // commands do not allocate and destroy the same wrapper every frame.
    std::unordered_map<std::string, std::shared_ptr<IResource>> mResources;
#endif
};

} // namespace Ship
