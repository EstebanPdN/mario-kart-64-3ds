#include "resource_runtime_3ds.h"

#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/resourcebridge.h>
#include <ship/resource/ResourceManager.h>

#include <cstdio>

struct CtlEntry;
struct AudioSequenceData;

extern "C" CtlEntry* GameEngine_LoadBank(uint8_t bankId);
extern "C" AudioSequenceData* GameEngine_LoadSequence(uint8_t sequenceId);
extern "C" uint32_t GameEngine_GetSequenceCount(void);

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
    constexpr const char* vertex = "models/ceremony_data/ceremony_data_seg11_vtx_340";
    constexpr const char* displayList = "models/ceremony_data/silver_trophy_dl";
    constexpr const char* audioSample = "sound/samples/sample_0";
    constexpr uint32_t expectedSequenceCount = 36;
    Ship::ResourceManager resourceManager;
    const auto wrappedTexture =
        std::static_pointer_cast<Fast::Texture>(resourceManager.LoadResourceProcess(texture));
    const uint64_t displayListCrc = ResourceGetCrcByName(displayList);
    const bool ok = wrappedTexture != nullptr && wrappedTexture->ImageData != nullptr &&
                    wrappedTexture->Width == ResourceGetTexWidthByName(texture) &&
                    wrappedTexture->Height == ResourceGetTexHeightByName(texture) &&
                    resourceManager.GetArchiveManager()->HashToCString(displayListCrc) != nullptr &&
                    resourceManager.GetResourceRawPointer(displayListCrc) != nullptr &&
                    ResourceGetDataByName(texture) != nullptr && ResourceGetTexWidthByName(texture) > 0 &&
                    ResourceGetTexHeightByName(texture) > 0 && ResourceGetDataByName(vertex) != nullptr &&
                    ResourceGetSizeByName(vertex) > 0 && ResourceGetDataByName(displayList) != nullptr &&
                    ResourceGetSizeByName(displayList) > 0 && ResourceGetDataByName(audioSample) != nullptr &&
                    ResourceGetDataByCrc(displayListCrc) != nullptr &&
                    GameEngine_LoadBank(0) != nullptr && GameEngine_LoadSequence(0) != nullptr &&
                    GameEngine_GetSequenceCount() == expectedSequenceCount;

    std::printf("entries=%zu loaded=%zu result=%s\n", Mk64Resource3DSArchiveEntryCount(),
                Mk64Resource3DSLoadedCount(), ok ? "ok" : "failed");
    Mk64Resource3DSShutdown();
    return ok ? 0 : 1;
}
