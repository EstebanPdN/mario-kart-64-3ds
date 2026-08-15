#include "resource_runtime_3ds.h"

#include "o2r_archive_reader.hpp"

#include <fast/resource/type/Texture.h>
#include <libultraship/libultra/gbi.h>
#include <libultraship/bridge/resourcebridge.h>
#include <ship/resource/ResourceManager.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#if !defined(__3DS__)
namespace {
bool sFailLinearAllocation = false;
}

extern "C" void* linearAlloc(size_t size) {
    return sFailLinearAllocation ? nullptr : std::malloc(size);
}

extern "C" void linearFree(void* memory) {
    std::free(memory);
}
#endif

struct CtlEntry;
struct AudioSequenceData;

extern "C" CtlEntry* GameEngine_LoadBank(uint8_t bankId);
extern "C" AudioSequenceData* GameEngine_LoadSequence(uint8_t sequenceId);
extern "C" uint32_t GameEngine_GetSequenceCount(void);

namespace {
bool ValidateStartupDisplayList(Gfx* commands, size_t commandCount) {
    if (commands == nullptr || commandCount < 3 ||
        static_cast<uint8_t>(commands[0].words.w0 >> 24) != 0x33 ||
        static_cast<uint8_t>(commands[commandCount - 1].words.w0 >> 24) != 0xB8) {
        return false;
    }
    for (size_t i = 0; i < commandCount; ++i) {
        const uint8_t opcode = static_cast<uint8_t>(commands[i].words.w0 >> 24);
        if (opcode != 0x32 && opcode != 0x33) {
            continue;
        }
        if (++i >= commandCount) {
            return false;
        }
        const uint64_t hash = (static_cast<uint64_t>(commands[i].words.w0) << 32) |
                              static_cast<uint32_t>(commands[i].words.w1);
        if (ResourceGetDataByCrc(hash) == nullptr) {
            return false;
        }
    }
    return true;
}

bool ValidateCrcIndex(const char* archivePath) {
    if (!Mk64Resource3DSValidateCrcIndex()) {
        return false;
    }
    mk64_3ds::O2rArchiveReader archive(archivePath);
    if (archive.Open() != mk64_3ds::O2rReadResult::Ok ||
        archive.Entries().size() != Mk64Resource3DSArchiveEntryCount()) {
        return false;
    }
    for (const std::string& path : archive.Entries()) {
        const uint64_t crc = ResourceGetCrcByName(path.c_str());
        const char* resolved = ResourceGetNameByCrc(crc);
        size_t nameSize = 0;
        size_t crcSize = 0;
        if (resolved == nullptr || std::strcmp(resolved, path.c_str()) != 0 ||
            !Mk64Resource3DSGetArchiveEntrySizeByName(path.c_str(), &nameSize) ||
            !Mk64Resource3DSGetArchiveEntrySizeByCrc(crc, &crcSize) || nameSize != crcSize) {
            return false;
        }
    }
    return true;
}

bool ValidateTransientTexture(Ship::ResourceManager& resourceManager, const char* texture) {
    const size_t baseline = Mk64Resource3DSLoadedCount();
    auto first = std::static_pointer_cast<Fast::Texture>(resourceManager.LoadResourceProcess(texture));
    if (first == nullptr || first->ImageData == nullptr || first->ImageDataSize == 0 ||
        Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    const uint8_t firstByte = first->ImageData[0];
    auto second = std::static_pointer_cast<Fast::Texture>(resourceManager.LoadResourceProcess(texture));
    std::weak_ptr<Ship::IResource> firstLifetime = first;
    if (second.get() != first.get() || Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }

    ResourceUnloadDirectory("textures/common");
    if (Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    ResourceUnloadDirectory("__OTR__textures/common_data");
    if (Mk64Resource3DSLoadedCount() != baseline + 1U || first->ImageData[0] != firstByte) {
        return false;
    }
    second.reset();
    if (firstLifetime.expired()) {
        return false;
    }
    first.reset();
    if (!firstLifetime.expired() || Mk64Resource3DSLoadedCount() != baseline) {
        return false;
    }

    auto reloaded = std::static_pointer_cast<Fast::Texture>(resourceManager.LoadResourceProcess(texture));
    std::weak_ptr<Ship::IResource> reloadedLifetime = reloaded;
    if (reloaded == nullptr || reloaded->ImageData == nullptr ||
        Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    ResourceDirtyDirectory("textures/common_data/");
    if (Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    reloaded.reset();
    return reloadedLifetime.expired() && Mk64Resource3DSLoadedCount() == baseline;
}

bool ValidateTrackTextureRetention(Ship::ResourceManager& resourceManager,
                                   const char* texture) {
    const size_t baseline = Mk64Resource3DSLoadedCount();
    auto resource = std::static_pointer_cast<Fast::Texture>(
        resourceManager.LoadResourceProcess(texture));
    if (resource == nullptr || resource->ImageData == nullptr ||
        Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    resource.reset();
    if (Mk64Resource3DSLoadedCount() != baseline + 1U) {
        return false;
    }
    ResourceUnloadDirectory("textures/tracks/luigi_raceway/");
    return Mk64Resource3DSLoadedCount() == baseline;
}

bool ValidateOutOfMemoryPropagation(Ship::ResourceManager& resourceManager,
                                    const char* texture) {
#if defined(__3DS__)
    (void)resourceManager;
    (void)texture;
    return true;
#else
    const size_t baseline = Mk64Resource3DSLoadedCount();
    sFailLinearAllocation = true;
    bool propagated = false;
    try {
        (void)resourceManager.LoadResourceProcess(texture);
    } catch (const std::bad_alloc&) {
        propagated = true;
    }
    sFailLinearAllocation = false;
    if (!propagated || Mk64Resource3DSLoadedCount() != baseline) {
        return false;
    }
    auto recovered = std::static_pointer_cast<Fast::Texture>(
        resourceManager.LoadResourceProcess(texture));
    return recovered != nullptr && recovered->ImageData != nullptr;
#endif
}

bool ValidateDirectoryEviction(const char* vertex, const char* displayList,
                               const char* unrelated) {
    const size_t baseline = Mk64Resource3DSLoadedCount();
    const uint64_t vertexCrc = ResourceGetCrcByName(vertex);
    const uint64_t displayListCrc = ResourceGetCrcByName(displayList);
    void* vertexData = ResourceGetDataByName(vertex);
    void* displayListData = ResourceGetDataByName(displayList);
    void* unrelatedData = ResourceGetDataByName(unrelated);
    if (vertexData == nullptr || displayListData == nullptr || unrelatedData == nullptr ||
        Mk64Resource3DSLoadedCount() != baseline + 3U) {
        return false;
    }

    ResourceUnloadDirectory("models/ceremony");
    if (Mk64Resource3DSLoadedCount() != baseline + 3U ||
        ResourceGetDataByName(unrelated) != unrelatedData) {
        return false;
    }
    ResourceUnloadDirectory("__OTR__models/ceremony_data");
    if (Mk64Resource3DSLoadedCount() != baseline + 1U ||
        ResourceGetDataByName(unrelated) != unrelatedData) {
        return false;
    }
    if (ResourceGetDataByCrc(vertexCrc) == nullptr || ResourceGetDataByName(displayList) == nullptr ||
        Mk64Resource3DSLoadedCount() != baseline + 3U) {
        return false;
    }

    ResourceDirtyDirectory("models/ceremony_data/");
    if (Mk64Resource3DSLoadedCount() != baseline + 1U ||
        ResourceGetDataByName(vertex) == nullptr || ResourceGetDataByCrc(displayListCrc) == nullptr ||
        Mk64Resource3DSLoadedCount() != baseline + 3U) {
        return false;
    }

    CtlEntry* bank = GameEngine_LoadBank(0);
    AudioSequenceData* sequence = GameEngine_LoadSequence(0);
    void* sample = ResourceGetDataByName("sound/samples/sample_0");
    const size_t beforeSoundUnload = Mk64Resource3DSLoadedCount();
    ResourceUnloadDirectory("sound/");
    return bank != nullptr && sequence != nullptr && sample != nullptr &&
           GameEngine_LoadBank(0) == bank && GameEngine_LoadSequence(0) == sequence &&
           ResourceGetDataByName("sound/samples/sample_0") == sample &&
           Mk64Resource3DSLoadedCount() == beforeSoundUnload;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <mk64.o2r>\n", argv[0]);
        return 2;
    }
    if (!Mk64Resource3DSInit(argv[1])) {
        std::fprintf(stderr, "failed to initialize resource runtime\n");
        return 1;
    }

    constexpr const char* texture = "textures/common_data/common_texture_item_box_question_mark";
    constexpr const char* streamingTexture =
        "textures/karts/bowser_kart/bowser_kart_frame000_wheel0";
    constexpr const char* trackTexture =
        "textures/tracks/luigi_raceway/luigi_raceway_data/d_course_luigi_raceway_sign_left";
    constexpr const char* vertex = "models/ceremony_data/ceremony_data_seg11_vtx_340";
    constexpr const char* displayList = "models/ceremony_data/silver_trophy_dl";
    constexpr const char* startupDisplayList = "models/startup_logo/dl1";
    constexpr const char* audioSample = "sound/samples/sample_0";
    constexpr uint32_t expectedSequenceCount = 36;
    Ship::ResourceManager resourceManager;
    const bool crcIndexValid = ValidateCrcIndex(argv[1]);
    const bool transientTextureValid = ValidateTransientTexture(resourceManager, streamingTexture);
    const bool trackTextureRetentionValid =
        ValidateTrackTextureRetention(resourceManager, trackTexture);
    const bool outOfMemoryValid =
        ValidateOutOfMemoryPropagation(resourceManager, streamingTexture);
    const bool directoryEvictionValid =
        ValidateDirectoryEviction(vertex, displayList, startupDisplayList);
    const uint64_t displayListCrc = ResourceGetCrcByName(displayList);
    auto* startupCommands = static_cast<Gfx*>(ResourceGetDataByName(startupDisplayList));
    const size_t startupCommandCount = ResourceGetSizeByName(startupDisplayList) / sizeof(Gfx);
    const bool startupDisplayListValid = ValidateStartupDisplayList(startupCommands, startupCommandCount);
    const bool ok = crcIndexValid && transientTextureValid && trackTextureRetentionValid &&
                    outOfMemoryValid && directoryEvictionValid &&
                    resourceManager.GetArchiveManager()->HashToCString(displayListCrc) != nullptr &&
                    resourceManager.GetResourceRawPointer(displayListCrc) != nullptr &&
                    ResourceGetDataByName(texture) != nullptr && ResourceGetTexWidthByName(texture) > 0 &&
                    ResourceGetTexHeightByName(texture) > 0 && ResourceGetDataByName(vertex) != nullptr &&
                    ResourceGetSizeByName(vertex) > 0 && ResourceGetDataByName(displayList) != nullptr &&
                    ResourceGetSizeByName(displayList) > 0 && startupDisplayListValid &&
                    ResourceGetDataByName(audioSample) != nullptr &&
                    ResourceGetDataByCrc(displayListCrc) != nullptr &&
                    GameEngine_LoadBank(0) != nullptr && GameEngine_LoadSequence(0) != nullptr &&
                    GameEngine_GetSequenceCount() == expectedSequenceCount;

    std::printf("entries=%zu loaded=%zu result=%s\n", Mk64Resource3DSArchiveEntryCount(),
                Mk64Resource3DSLoadedCount(), ok ? "ok" : "failed");
    Mk64Resource3DSShutdown();
    return ok ? 0 : 1;
}
